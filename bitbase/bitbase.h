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
 * @author Volker B�hm
 * @copyright Copyright (c) 2021 Volker B�hm
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
#include "bitbase-file.h"
#include "compress.h"
#include "cluster-cache.h"

namespace QaplaBitbase {

    /**
     * @class Bitbase
     * @brief Stores and manages bit-level data for chess endgame databases.
     */
    class Bitbase {
    public:
        /**
         * @brief Constructs an empty Bitbase.
         */
        explicit Bitbase();

        /**
         * @brief Constructs a Bitbase with a given size in bits.
         * @param sizeInBit Number of bits the bitbase should hold.
		 * @param sig Signature of the bitbase.
         */
        explicit Bitbase(uint64_t sizeInBit, uint32_t sig);

        /**
         * @brief Constructs a Bitbase from a BitbaseIndex.
         * @param index Index providing the bitbase size.
		 * @param sig Signature of the bitbase.
         */
        Bitbase(const class BitbaseIndex& index, uint32_t sig);

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
         * @brief Sets the number of bits in the bitbase.
         * @param sizeInBit New size in bits.
         */
        void setSize(uint64_t sizeInBit) {
            _sizeInBits = sizeInBit;
        }

        void resize(uint64_t sizeInBit) {
            setSize(sizeInBit);
			_bitbase.resize(getSize());
        }

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
         * @brief Clears a specific bit (sets to 0).
         * @param index Bit index to clear.
         */
        void clearBit(uint64_t index);

        /**
         * @brief Gets the value of a specific bit.
         * @param index Bit index to retrieve.
         * @return 1 if bit is set, 0, if not and -1 on error.
         */
        int getBit(uint64_t index);

        /**
         * @brief Gets the size of the bitbase in bits.
         * @return Bit count.
         */
        uint64_t getSizeInBit() const;

		/**
		 * @brief Gets the size of the bitbase (internal vector structure) in Elements.
		 * @return Size in Elements.
		 */
        uint64_t getSize() const {
            return (_sizeInBits + BITS_IN_ELEMENT - 1) / BITS_IN_ELEMENT;
        }

        /**
         * @brief Returns a string describing number of won and non-won positions.
         * @return Descriptive string.
         */
        std::string getStatistic();

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
         * @brief Counts the number of set bits (won positions).
         * @param begin Optional starting index.
         * @return Count of set bits.
         */
        uint64_t computeWonPositions(uint64_t begin = 0) const;

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

        // Caching
        uint32_t _signature;
        static inline ClusterCache cache{ 511 };

        static constexpr uint32_t DEFAULT_CLUSTER_SIZE_IN_BYTES = 16 * 1024; 

        static const uint64_t BITS_IN_ELEMENT = sizeof(bbt_t) * 8;
        uint64_t _sizeInBits;
        
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

