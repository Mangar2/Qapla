/**
 * @license
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Volker Böhm
 * @copyright Copyright (c) 2025 Volker Böhm
 * @Overview
 * Provides an array of bits in a
 */

#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <atomic>
#include <array>
#include <algorithm>
#include <memory>
#include <cstring>


namespace QaplaBitbase {

    /**
     * @class CacheEntry
     * @brief Compact metadata for one cached cluster. Data lives in a separate pool.
     *
     * The key field combines signature and clusterNumber into a single atomic uint64_t.
     * key == 0 means the slot is empty. Readers check the key lock-free; writers
     * invalidate (key=0) before writing data and publish (key=real) after.
     */
    struct CacheEntry {
        std::atomic<uint64_t> key{0};
        uint64_t ageCounter{0};
        uint64_t usageCounter{0};

        static uint64_t makeKey(uint32_t sig, uint32_t clusterIdx) {
            return (static_cast<uint64_t>(sig) << 32) | clusterIdx;
        }

        uint64_t computeValue(uint64_t nowAge) const {
            uint64_t age = nowAge - ageCounter;
            uint64_t usageEffect = usageCounter * 64;
            return (usageEffect >= age) ? 0 : (age - usageEffect);
        }
    };


    /**
     * @class ClusterCache
     * @brief Fixed-size cache with lock-free reads and striped write locks.
     *
     * Metadata (CacheEntry) and cluster data are stored separately:
     * - _entries[]: compact array for fast probe-loop scanning
     * - _dataPool:  pre-allocated buffer, slot i at offset i * _clusterSizeBytes
     *
     * Reads are completely lock-free using a double-check on the atomic key.
     * Writes use a per-stripe mutex for writer-writer safety and invalidate
     * the key before modifying data.
     */
    class ClusterCache {
    public:
        static constexpr std::size_t PROBE_COUNT = 100;
        static constexpr std::size_t STRIPE_COUNT = 256;
        static constexpr uint32_t DEFAULT_CLUSTER_SIZE = 16 * 1024;

        ClusterCache(std::size_t capacity, uint32_t clusterSizeBytes = DEFAULT_CLUSTER_SIZE)
            : _clusterSizeBytes(clusterSizeBytes)
        {
            allocate(capacity);
        }

        void resize(std::size_t newCapacity) {
            allocate(newCapacity);
        }

        /**
         * @brief Lock-free lookup of a single byte from a cached cluster.
         *
         * Uses double-check on the atomic key to detect torn reads from
         * a concurrent writer. On torn read, returns -1 (cache miss).
         *
         * @return The byte value (0-255) on hit, or -1 on miss.
         */
        int getEntryByte(uint32_t sig, uint32_t clusterIdx, uint32_t byteIndex) {
            if (_capacity == 0 || byteIndex >= _clusterSizeBytes) return -1;
            const uint64_t searchKey = CacheEntry::makeKey(sig, clusterIdx);
            const std::size_t segSize = _capacity / STRIPE_COUNT;
            const std::size_t h = hash(sig, clusterIdx);
            const std::size_t stripe = h % STRIPE_COUNT;
            const std::size_t segStart = stripe * segSize;
            const std::size_t offset = (h / STRIPE_COUNT) % segSize;
            const std::size_t probeCount = std::min(PROBE_COUNT, segSize);

            for (std::size_t i = 0; i < probeCount; ++i) {
                std::size_t idx = segStart + (offset + i) % segSize;
                uint64_t k = _entries[idx].key.load(std::memory_order_acquire);
                if (k == searchKey) {
                    uint8_t byte = _dataPool[idx * _clusterSizeBytes + byteIndex];
                    // Double-check: key still valid after reading data
                    uint64_t k2 = _entries[idx].key.load(std::memory_order_acquire);
                    if (k2 == searchKey) return byte;
                    return -1;  // torn read — treat as miss
                }
            }
            return -1;
        }

        /**
         * @brief Insert or replace a cluster entry.
         *
         * Writer-writer safety via stripe mutex. The atomic key is invalidated
         * before writing data and published after, so concurrent readers
         * see either the old valid entry or a miss — never partially written data.
         */
		void setEntry(const std::vector<uint8_t>& data,
			uint32_t sig,
			uint32_t clusterIdx) {
            if (_capacity == 0) return;
            const uint64_t age = ++_nowAge;
            const uint64_t newKey = CacheEntry::makeKey(sig, clusterIdx);
            const std::size_t segSize = _capacity / STRIPE_COUNT;
            const std::size_t h = hash(sig, clusterIdx);
            const std::size_t stripe = h % STRIPE_COUNT;
            const std::size_t segStart = stripe * segSize;
            const std::size_t offset = (h / STRIPE_COUNT) % segSize;
            const std::size_t probeCount = std::min(PROBE_COUNT, segSize);
            const uint32_t copySize = std::min(static_cast<uint32_t>(data.size()), _clusterSizeBytes);

            std::lock_guard<std::mutex> lock(_stripeMutexes[stripe].mtx);

            std::size_t victim = segStart + offset;
            uint64_t bestValue = _entries[victim].computeValue(age);
            for (std::size_t i = 1; i < probeCount; ++i) {
                std::size_t idx = segStart + (offset + i) % segSize;
                uint64_t v = _entries[idx].computeValue(age);
                if (v > bestValue) {
                    bestValue = v;
                    victim = idx;
                }
            }
            bool wasEmpty = (_entries[victim].key.load(std::memory_order_relaxed) == 0);

            // Invalidate before writing data — readers see key=0 → miss
            _entries[victim].key.store(0, std::memory_order_release);

            // Copy cluster data into pre-allocated buffer
            std::memcpy(&_dataPool[victim * _clusterSizeBytes], data.data(), copySize);

            // Update metadata
            _entries[victim].ageCounter = age;
            _entries[victim].usageCounter = 0;

            // Publish — readers can now match this entry
            _entries[victim].key.store(newKey, std::memory_order_release);

            if (wasEmpty) ++_fillCount;
            else ++_numOverwrites;
		}

        uint32_t fillInPercent() const {
            if (_capacity == 0) return 0;
            return static_cast<uint32_t>(_fillCount.load() * 100 / _capacity);
        }

		void print() const {
			std::cout << "Cache: " << _capacity << " entries, "
				<< (_fillCount.load() * 100) / _capacity << "% filled, "
				<< (_numOverwrites.load() * 100) / _capacity << "% overwrites" << std::endl;
		}

    private:
        struct alignas(64) StripeMutex {
            std::mutex mtx;
        };

        std::unique_ptr<CacheEntry[]> _entries;
        std::unique_ptr<uint8_t[]> _dataPool;
        std::size_t _capacity{0};
        uint32_t _clusterSizeBytes;
        std::atomic<uint64_t> _nowAge{0};
        std::atomic<uint32_t> _fillCount{0};
        std::atomic<uint32_t> _numOverwrites{0};
        std::array<StripeMutex, STRIPE_COUNT> _stripeMutexes;

        void allocate(std::size_t capacity) {
            capacity = roundToStripeMultiple(capacity);
            _capacity = capacity;
            _entries = std::make_unique<CacheEntry[]>(capacity);
            _dataPool = std::make_unique<uint8_t[]>(
                static_cast<std::size_t>(capacity) * _clusterSizeBytes);
            _fillCount = 0;
            _numOverwrites = 0;
        }

        static std::size_t roundToStripeMultiple(std::size_t n) {
            return ((n + STRIPE_COUNT - 1) / STRIPE_COUNT) * STRIPE_COUNT;
        }

        static std::size_t hash(uint32_t sig, uint32_t clusterIdx) {
            uint64_t h = static_cast<uint64_t>(clusterIdx);
            h ^= sig + 0x9e3779b97f4a7c15 + (h << 6) + (h >> 2);
            return static_cast<size_t>(h);
        }
    };

    

} // namespace QaplaBitbase

