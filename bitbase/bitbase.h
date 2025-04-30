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
#include <ostream>
#include <filesystem>

namespace QaplaBitbase {

    using bbt_t = uint8_t;

    /**
     * @class Bitbase
     * @brief Stores and manages bit-level data for chess endgame databases.
     */
    class Bitbase {
    public:
        /**
         * @brief Constructs an empty Bitbase.
         * @param loaded Whether the Bitbase is initially considered loaded.
         */
        explicit Bitbase(bool loaded = false);

        /**
         * @brief Constructs a Bitbase with a given size in bits.
         * @param sizeInBit Number of bits the bitbase should hold.
         */
        explicit Bitbase(uint64_t sizeInBit);

        /**
         * @brief Constructs a Bitbase from a BitbaseIndex.
         * @param index Index providing the bitbase size.
         */
        Bitbase(const class BitbaseIndex& index);

        /**
         * @brief Sets the number of bits in the bitbase.
         * @param sizeInBit New size in bits.
         */
        void setSize(uint64_t sizeInBit);

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
         * @return True if bit is set, false otherwise.
         */
        bool getBit(uint64_t index) const;

        /**
         * @brief Gets the size of the bitbase in bits.
         * @return Bit count.
         */
        uint64_t getSizeInBit() const;

		/**
		 * @brief Gets the size of the bitbase (internal vector structure) in bytes.
		 * @return Size in bytes.
		 */
        uint64_t getSize() const;

        /**
         * @brief Returns a string describing number of won and non-won positions.
         * @return Descriptive string.
         */
        std::string getStatistic() const;

        /**
         * @brief Saves the bitbase uncompressed to file.
         * @param fileName Output file path.
         */
        void storeUncompressed(const std::string& fileName);

        /**
         * @brief Saves the bitbase compressed to file, with optional C++ export.
         * @param fileName File path to save binary.
         * @param signature Optional C++ array name to generate header.
         * @param test Whether to verify compression integrity.
         * @param verbose Enable output.
         */
        void storeToFile(std::string fileName, std::string signature, bool first, bool test = false, bool verbose = false);

        /**
         * @brief Loads a bitbase from disk based on a piece string.
         * @param pieceString Piece string identifying the position set.
         * @param extension File extension to use.
         * @param path Directory path.
         * @param verbose Enable output.
         * @return True on success.
         */
        bool readFromFile(std::string pieceString, std::string extension = ".btb", std::filesystem::path path = "./", bool verbose = true);

        /**
         * @brief Checks if bitbase data has been successfully loaded.
         * @return True if loaded.
         */
        bool isLoaded() const;

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
        void writeCompressedVectorAsCppFile(const std::vector<uint8_t>& data, const std::string& varName, const std::string& filename);

        /**
         * @brief Loads a compressed bitbase from a compiled-in uint32_t array.
         * @param data32 Input data.
         * @param byteSize Total size in bytes.
         * @param sizeInBit Original size in bits.
         * @param verbose Enable output.
         */
        void loadFromEmbeddedData(const uint32_t* data32, uint32_t byteSize, uint64_t sizeInBit, bool verbose = false);

        /**
         * @brief Prints debug information about the current bitbase.
         */
        void print() const;


    private:
        bool readFromFile(std::filesystem::path fileName, size_t sizeInBit, bool verbose);
        uint32_t computeVectorSize();

        bool _loaded;
        static const uint64_t BITS_IN_ELEMENT = sizeof(bbt_t) * 8;
        std::vector<bbt_t> _bitbase;
        uint64_t _sizeInBit;
    };

    std::ostream& operator<<(std::ostream& stream, const Bitbase& bitBase);

} // namespace QaplaBitbase

