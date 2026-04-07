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
#include <ostream>
#include <filesystem>
#include <algorithm>
#include <shared_mutex>
#include "bitbase-file.h"
#include "compress.h"
#include "cluster-cache.h"

namespace QaplaBitbase {

    /**
     * @class Bitbase
     * @brief Stores and manages bit-level data for chess endgame databases.
     *
     * 2-bit entry encoding (bitsPerEntry == 2):
     */
    enum class BitbaseResult : int {
        Draw = 0,
        DrawOrLoss = 0,  ///< Used for legacy 1-bit bitbases where draw and loss are not distinguished
        Win = 1,
        Loss = 2,
        Unknown = 3
    };

    std::string to_string(BitbaseResult result);

    class Bitbase {
    public:
        /**
         * @brief Constructs an empty Bitbase.
         */
        explicit Bitbase();

        /**
         * @brief Constructs a Bitbase with a given entry count and bits per entry.
         * @param entryCount Number of entries the bitbase should hold.
         * @param bitsPerEntry Number of bits per entry (1 or 2).
		 * @param sig Signature of the bitbase.
         */
        explicit Bitbase(uint64_t entryCount, uint32_t bitsPerEntry, uint32_t sig);

        /**
         * @brief Constructs a Bitbase from a BitbaseIndex.
         * @param index Index providing the entry count.
         * @param bitsPerEntry Number of bits per entry (1 or 2).
		 * @param sig Signature of the bitbase.
         */
        Bitbase(const class BitbaseIndex& index, uint32_t bitsPerEntry, uint32_t sig);

        /**
         * @brief Sets the filename.
         *
         * @param pieceString Identifier string for the bitbase (e.g. "KPK").
         * @param extension File extension (default: ".bb").
         * @param path Directory path to the bitbase file (default: current directory).
         */
        void setFilename(std::string pieceString,
            std::string extension = ".bb",
            std::filesystem::path path = "./") {
            _filePath = path / (pieceString + extension);
        }

        /**
         * @brief Attaches the Bitbase to a file and loads its header metadata.
         *
         * @param pieceString Identifier string for the bitbase (e.g. "KPK").
         * @param extension File extension (default: ".bb").
         * @param path Directory path to the bitbase file (default: current directory).
         */
        bool attachFromFile(std::string pieceString,
            std::string extension = ".bb",
            std::filesystem::path path = "./");

        /**
         * @brief Sets the number of entries in the bitbase.
         * @param entryCount New entry count.
         */
        void setSize(uint64_t entryCount) {
            _entryCount = entryCount;
        }

        void resize(uint64_t entryCount) {
            setSize(entryCount);
			_bitbase.resize(getSize());
        }

        uint32_t getBitsPerEntry() const { return _bitsPerEntry; }

        /**
         * @brief Clears all bits in the bitbase (sets to 0).
         */
        void clear();

        /**
         * @brief Sets a specific bit to 1.
         * @param index Bit index to set.
         */
        void setBit(uint64_t index);

        /**
         * @brief Sets two bits as a combined integer value (for example, for win/draw/loss encoding). 
         * Requires that the content of the two bits is currently 0 (initial or cleared state).
         * @param index2 Index into the two-bit array (will be converted to bit position by multiplying by 2).
         * @param value BitbaseResult value to set.
         */
        void set2Bit(uint64_t index2, BitbaseResult value);

        /**
         * @brief Clears a specific bit (sets to 0).
         * @param index Bit index to clear.
         */
        void clearBit(uint64_t index);

        /**
         * @brief Clears two bits (sets to 0).
         * @param index2 Index into the two-bit array (will be converted to bit position by multiplying by 2).
         */
        void clear2Bits(uint64_t index2);

        /**
         * @brief Gets the value of a specific bit.
         * @param index Bit index to retrieve.
         * @return 1 if bit is set, 0, if not and -1 on error.
         */
        int getBit(uint64_t index);

        /**
         * @brief Gets the value of two bits as a combined integer (for example, for win/draw/loss encoding).
         * 
         * @param index2 Index into the two-bit array (will be converted to bit position by multiplying by 2).
         * @return BitbaseResult value.
         */
        BitbaseResult get2Bits(uint64_t index2);

        /**
         * @brief Gets the total number of bits in the bitbase.
         * @return Bit count (entryCount * bitsPerEntry).
         */
        uint64_t sizeInBits() const { return _entryCount * _bitsPerEntry; }

        /**
         * @brief Gets the number of entries in the bitbase.
         * @return Entry count.
         */
        uint64_t getEntryCount() const { return _entryCount; }

		/**
		 * @brief Gets the size of the bitbase (internal vector structure) in Elements.
		 * @return Size in Elements.
		 */
        uint64_t getSize() const {
            return (sizeInBits() + BITS_IN_ELEMENT - 1) / BITS_IN_ELEMENT;
        }

        /**
         * @brief Saves the bitbase uncompressed to file.
         * @param fileName Output file path.
		 * @param compression Compression type.
         */
        void storeToFile(const std::string& fileName, QaplaCompress::CompressionType compression);

        /**
         * @brief Loads a bitbase from disk
         * @return True on success.
         */
        std::tuple<bool, std::string> readAll();

        /**
         * @brief Checks if bitbase data has been successfully loaded.
         * @return True if loaded.
         */
        bool isLoaded() const {
            return _loaded;
        }

        /**
		 * @brief Checks if the header information has been loaded.
		 * @return True if header is loaded.
         */
		bool isHeaderLoaded() const {
			return _headerLoaded;
		}

		/**
		 * @brief Sets the loaded state of the bitbase.
		 */
		void setLoaded() {
			_loaded = true;
            _headerLoaded = true;
		}

        /**
         * @brief Returns all indexes where current bitbase is 1 and the given is 0.
         * @param andNot Bitbase to exclude.
         * @param indexes Output vector of indices.
         */
        void getAllIndexes(const Bitbase& andNot, std::vector<uint64_t>& indexes) const;

        /**
         * @brief Counts the number of entries matching the given result.
         * @param result BitbaseResult to match.
         * @return Count of matching entries.
         */
        uint64_t computeResults(BitbaseResult result) const;

        /**
         * @brief Writes the compressed bitbase as a C++ header file with a uint32_t array.
         * @param data Compressed data.
         * @param varName Name of the array.
         * @param filename Output header file path.
         */
        void writeAsCppFile(const std::string& varName, const std::string& filename);

        /**
         * @brief Loads a compressed bitbase from a compiled-in uint32_t array.
         * @param data32 Input data.
         * @param verbose Enable output.
         */
        void loadFromEmbeddedData(const uint32_t* data32, bool verbose = false);

        /**
         * @brief Prints debug information about the current bitbase.
         */
        void print() const;

		static void setCacheSize(uint32_t sizeInMB) {
			uint64_t numCluster = static_cast<uint64_t>(sizeInMB) * 1024 * 1024 / DEFAULT_CLUSTER_SIZE_IN_BYTES;
			numCluster = std::clamp(numCluster, static_cast<uint64_t>(2), static_cast<uint64_t>(UINT32_MAX));
			cache.resize(numCluster);
		}


    private:

        bool loadHeader(const std::filesystem::path& path);
        void verifyWrittenFile();
        void compactTo1BitIfPossible();
        int getBitsFromLoadedData(uint64_t bitIndex, bbt_t mask) const;
        int getBitsFromClusterData(uint64_t bitIndex, bbt_t mask);

        // Caching
        uint32_t _signature;
        static inline ClusterCache cache{ 511 };
        static inline std::shared_mutex _cacheMutex;

        static constexpr uint32_t DEFAULT_CLUSTER_SIZE_IN_BYTES = 16 * 1024; 

        static const uint64_t BITS_IN_ELEMENT = sizeof(bbt_t) * 8;
        uint64_t _entryCount;

        uint32_t _bitsPerEntry = 1;
        
        // Fully loaded bitbase data
        bool _loaded;
        std::vector<bbt_t> _bitbase;

        // Information to load further clusters from file
        bool _headerLoaded;
        std::filesystem::path _filePath;
        std::vector<uint64_t> _offsets;
        uint32_t _clusterSizeBytes = DEFAULT_CLUSTER_SIZE_IN_BYTES;
        QaplaCompress::CompressionType _compression;
    };

} // namespace QaplaBitbase

