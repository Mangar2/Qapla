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
 * @copyright Copyright (c) 2021 Volker Böhm
 * @Overview
 * Provides an array of bits in a
 */

#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <iostream>


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
     * @brief Fixed-size cache of Bitbase cluster entries with simple probing.
     */
    class ClusterCache {
    public:
        /**
         * @brief Number of slots to probe on collision.
         */
        static constexpr std::size_t PROBE_COUNT = 100;

        /**
         * @brief Construct cache with given capacity.
         * @param capacity  Number of entries to allocate.
         */
        ClusterCache(std::size_t capacity) {
            entries.resize(capacity);
        }

        /**
         * @brief Resize the cache.
         * @param newCapacity  New number of entries.
         */
        void resize(std::size_t newCapacity) {
            entries.resize(newCapacity);
        }

        /**
         * @brief Look up an entry by signature and cluster number.
         * @param sig         Bitbase signature to match.
         * @param clusterIdx  Cluster index to match.
         * @param nowAge      Current global age counter.
         * @return Pointer to matching entry, or nullptr if not found.
         */
        CacheEntry* getEntry(uint32_t sig, uint32_t clusterIdx) {
            if (entries.empty()) return nullptr;
            std::size_t idx = hash(sig, clusterIdx) % entries.size();
            for (std::size_t i = 0; i < PROBE_COUNT; ++i) {
                auto& e = entries[(idx + i) % entries.size()];
                if (e.signature == sig && e.clusterNumber == clusterIdx) {
                    nowAge++;
					e.signalUsage(nowAge);
                    return &e;
                }
            }
            return nullptr;
        }

        /**
         * @brief Insert or replace an entry using simple eviction.
         * @param entry   New CacheEntry to insert.
         * @param nowAge  Current global age counter.
         */
        void setEntry(const CacheEntry& entry) {
            if (entries.empty()) return;
            nowAge++;
            std::size_t base = hash(entry.signature, entry.clusterNumber) % entries.size();
            // find slot with smallest value among first PROBE_COUNT positions
            std::size_t victim = base;
            uint64_t bestValue = entries[base].computeValue(nowAge);
            for (std::size_t i = 1; i < PROBE_COUNT; ++i) {
                std::size_t idx = (base + i) % entries.size();
                uint64_t v = entries[idx].computeValue(nowAge);
                if (v > bestValue) {
                    bestValue = v;
                    victim = idx;
                }
            }
            if (entries[victim].signature == 0) {
                fillCount++;
			}
			else {
				numOverwrites++;
			}
            entries[victim] = entry;
        }

        /**
		 * @brief Set an entry using the current global age counter.
		 * @param data        Data vector for this cluster.
		 * @param sig         Bitbase signature.
		 * @param clusterIdx  Cluster index number.
         */
		void setEntry(const std::vector<uint8_t>& data,
			uint32_t sig,
			uint32_t clusterIdx) {
			setEntry(CacheEntry(data, sig, clusterIdx, nowAge));
		}

		/**
		 * @brief Get the current fill percentage of the cache.
		 * @return uint32_t  Fill percentage (0-100).
		 */ 
        uint32_t fillInPercent() const {
            if (entries.empty()) return 0;
            return static_cast<uint32_t>(fillCount * 100 / entries.size());
        }

		void print() const {
			std::cout << "Cache: " << entries.size() << " entries, "
				<< (fillCount * 100) / entries.size() << "% filled, "
				<< (numOverwrites * 100) / entries.size() << "% overwrites" << std::endl;
		}

    private:
        std::vector<CacheEntry> entries;
		uint64_t nowAge = 0;
        uint32_t fillCount = 0;
        uint32_t numOverwrites = 0;

        /**
         * @brief Combine signature and cluster number into a hash.
         * @param sig         Bitbase signature.
         * @param clusterIdx  Cluster index.
         * @return std::size_t  Raw hash value.
         */
        /*
        static std::size_t hash(uint32_t sig, uint32_t clusterIdx) {

            return (static_cast<std::size_t>(sig) * 31u)
                ^ static_cast<std::size_t>(clusterIdx);
        }
        */
        static std::size_t hash(uint32_t sig, uint32_t clusterIdx) {
            uint64_t h = static_cast<uint64_t>(clusterIdx);
            h ^= sig + 0x9e3779b97f4a7c15 + (h << 6) + (h >> 2); // inspired by boost::hash_combine
            return static_cast<size_t>(h);
        }
    };

    

} // namespace QaplaBitbase

