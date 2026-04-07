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


namespace QaplaBitbase {

    /**
     * @class CacheEntry
     * @brief Represents one cached cluster from a bitbase.
     */
    struct CacheEntry {
        /** Unique signature of the bitbase file. */
        uint32_t signature;

        /** Index number of the cluster within the bitbase. */
        uint32_t clusterNumber;

        /** Monotonically increasing counter for age-based eviction. */
        uint64_t ageCounter;

        /** Raw data of the cluster. */
        std::vector<uint8_t> data;

        /** Count of how often this entry was accessed. */
        uint64_t usageCounter;

		/**
		 * @brief Signals that this entry was used.
		 *
		 * Increments the usage counter and updates the age counter.
		 *
		 * @param nowAge  Current global age counter
		 */
		void signalUsage(uint64_t nowAge) {
			usageCounter++;
			ageCounter = nowAge;
		}

        /**
         * @brief Construct a new CacheEntry
         *
         * @param initData    Data vector for this cluster
         * @param sig         Bitbase signature
         * @param clusterIdx  Cluster index number
         * @param currentAge  Current global age counter
         */
        CacheEntry(const std::vector<uint8_t>& initData,
            uint32_t sig,
            uint32_t clusterIdx,
            uint64_t currentAge)
            : signature(sig),
            clusterNumber(clusterIdx),
            ageCounter(currentAge),
            data(initData),
            usageCounter(0)
        {
        }

		CacheEntry() : signature(0), clusterNumber(0), ageCounter(0), usageCounter(0) {}

        /**
         * @brief Compute the eviction "value" of this entry.
         *
         * A higher returned value means this entry is older (and thus
         * more likely to be evicted).
         *
         * @param nowAge  Current global age counter
         * @return uint64_t  Difference between nowAge and this->ageCounter
         */
        uint64_t computeValue(uint64_t nowAge) const {
            uint64_t age = nowAge - ageCounter;
            uint64_t usageEffect = usageCounter * 64;
            return (usageEffect >= age) ? 0 : (age - usageEffect);
        }
    };


    /**
     * @class ClusterCache
     * @brief Fixed-size cache of Bitbase cluster entries with striped locking.
     *
     * The entry array is partitioned into STRIPE_COUNT segments, each protected
     * by its own mutex. Probing wraps within a segment, so threads accessing
     * different segments never contend on the same lock.
     */
    class ClusterCache {
    public:
        /**
         * @brief Number of slots to probe on collision.
         */
        static constexpr std::size_t PROBE_COUNT = 100;

        /**
         * @brief Number of independent lock stripes.
         */
        static constexpr std::size_t STRIPE_COUNT = 256;

        /**
         * @brief Construct cache with given capacity (rounded up to multiple of STRIPE_COUNT).
         * @param capacity  Number of entries to allocate.
         */
        ClusterCache(std::size_t capacity) {
            capacity = roundToStripeMultiple(capacity);
            _entries.resize(capacity);
        }

        /**
         * @brief Resize the cache (rounded up to multiple of STRIPE_COUNT).
         * @param newCapacity  New number of entries.
         */
        void resize(std::size_t newCapacity) {
            newCapacity = roundToStripeMultiple(newCapacity);
            _entries.resize(newCapacity);
        }

        /**
         * @brief Look up a single byte from a cached cluster.
         *
         * Thread-safe: acquires only the stripe mutex for the target segment.
         *
         * @param sig         Bitbase signature to match.
         * @param clusterIdx  Cluster index to match.
         * @param byteIndex   Byte offset within the cluster data.
         * @return The byte value (0-255) on hit, or -1 on miss.
         */
        int getEntryByte(uint32_t sig, uint32_t clusterIdx, uint32_t byteIndex) {
            if (_entries.empty()) return -1;
            const std::size_t segSize = _entries.size() / STRIPE_COUNT;
            const std::size_t h = hash(sig, clusterIdx);
            const std::size_t stripe = h % STRIPE_COUNT;
            const std::size_t segStart = stripe * segSize;
            const std::size_t offset = (h / STRIPE_COUNT) % segSize;
            const std::size_t probeCount = std::min(PROBE_COUNT, segSize);

            std::lock_guard<std::mutex> lock(_stripeMutexes[stripe].mtx);
            for (std::size_t i = 0; i < probeCount; ++i) {
                auto& e = _entries[segStart + (offset + i) % segSize];
                if (e.signature == sig && e.clusterNumber == clusterIdx) {
                    if (byteIndex < e.data.size()) {
                        return e.data[byteIndex];
                    }
                    return -1;
                }
            }
            return -1;
        }

        /**
         * @brief Insert or replace a cluster entry.
         *
         * Thread-safe: acquires only the stripe mutex for the target segment.
         *
         * @param data        Data vector for this cluster.
         * @param sig         Bitbase signature.
         * @param clusterIdx  Cluster index number.
         */
		void setEntry(const std::vector<uint8_t>& data,
			uint32_t sig,
			uint32_t clusterIdx) {
            if (_entries.empty()) return;
            const uint64_t age = ++_nowAge;
            const std::size_t segSize = _entries.size() / STRIPE_COUNT;
            const std::size_t h = hash(sig, clusterIdx);
            const std::size_t stripe = h % STRIPE_COUNT;
            const std::size_t segStart = stripe * segSize;
            const std::size_t offset = (h / STRIPE_COUNT) % segSize;
            const std::size_t probeCount = std::min(PROBE_COUNT, segSize);

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
            if (_entries[victim].signature == 0) {
                ++_fillCount;
			}
			else {
				++_numOverwrites;
			}
            _entries[victim] = CacheEntry(data, sig, clusterIdx, age);
		}

		/**
		 * @brief Get the current fill percentage of the cache.
		 * @return uint32_t  Fill percentage (0-100).
		 */ 
        uint32_t fillInPercent() const {
            if (_entries.empty()) return 0;
            return static_cast<uint32_t>(_fillCount.load() * 100 / _entries.size());
        }

		void print() const {
			std::cout << "Cache: " << _entries.size() << " entries, "
				<< (_fillCount.load() * 100) / _entries.size() << "% filled, "
				<< (_numOverwrites.load() * 100) / _entries.size() << "% overwrites" << std::endl;
		}

    private:
        struct alignas(64) StripeMutex {
            std::mutex mtx;
        };

        std::vector<CacheEntry> _entries;
        std::atomic<uint64_t> _nowAge{0};
        std::atomic<uint32_t> _fillCount{0};
        std::atomic<uint32_t> _numOverwrites{0};
        std::array<StripeMutex, STRIPE_COUNT> _stripeMutexes;

        static std::size_t roundToStripeMultiple(std::size_t n) {
            return ((n + STRIPE_COUNT - 1) / STRIPE_COUNT) * STRIPE_COUNT;
        }

        static std::size_t hash(uint32_t sig, uint32_t clusterIdx) {
            uint64_t h = static_cast<uint64_t>(clusterIdx);
            h ^= sig + 0x9e3779b97f4a7c15 + (h << 6) + (h >> 2); // inspired by boost::hash_combine
            return static_cast<size_t>(h);
        }
    };

    

} // namespace QaplaBitbase

