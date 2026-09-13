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
#include <atomic>
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
        CursedWin = 3,   ///< won, but the fifty move rule takes the win away
        BlessedLoss = 4, ///< lost, but the fifty move rule saves the loss
        Unknown = 5
    };

    std::string to_string(BitbaseResult result);

    /**
     * @brief What the byte of the generation state holds once distances are computed.
     *
     * The codes are de Man's, taken from rtbgenp.c of his generator, so that the two
     * generators speak the same language and a value can be held against his without a
     * translation in between. His pawn generator is the wider of the two: the pawnless
     * one simply never writes the PAWN_ values.
     *
     * Unlike everything else in this generator the value is seen from the side to move,
     * because that is how he writes it - he keeps a table per colour where we keep the
     * colour in the lowest bit of the index. The two boundaries convert.
     *
     * A distance is the number of plies to the move that zeroes the fifty move counter:
     * a capture, a pawn move, or the mate that ends the game. WIN_IN_ONE + i is a win in
     * i + 1 plies, LOSS_IN_ONE - i a loss in i + 1, and MATE a loss in none.
     */
    namespace Dtz {
        constexpr uint8_t ILLEGAL     = 0;
        constexpr uint8_t CAPT_WIN    = 1;     ///< a capture wins: one ply to the zeroing
        constexpr uint8_t PAWN_WIN    = 2;     ///< a pawn move wins: one ply to the zeroing
        constexpr uint8_t WIN_IN_ONE  = 3;     ///< win in one ply, + i for i + 1 plies
        constexpr uint8_t LOSS_IN_ONE = 0xf8;  ///< loss in one ply, - i for i + 1 plies
        constexpr uint8_t MATE        = 0xf9;  ///< loss in no ply at all
        constexpr uint8_t CAPT_DRAW   = 0xfa;  ///< a capture holds the draw
        constexpr uint8_t PAWN_DRAW   = 0xfb;  ///< a pawn move holds the draw
        constexpr uint8_t CAPT_CLOSS  = 0xfc;  ///< the only escape is into a loss the rule saves
        constexpr uint8_t CHANGED     = 0xfd;  ///< marker of his iteration
        constexpr uint8_t UNKNOWN     = 0xfe;  ///< nothing proven, which in the end is a draw
        constexpr uint8_t BROKEN      = 0xff;

        /// The fifty move rule in plies: a win that needs longer is a cursed win.
        constexpr int DRAW_RULE = 100;

        /// The longest distance the byte holds. His generator goes further by saving a
        /// layer to disk and rescaling the byte, which is not built here: beyond this
        /// the distance saturates, which keeps the win or loss and its cursed state but
        /// not the exact number.
        constexpr int MAX_PLIES = 120;

        /// A capture or a pawn move that wins, but only so slowly that the rule takes
        /// the win away. They sit where the distances 101 and 102 sit, which is his
        /// numbering, and mean the least a cursed win can be.
        constexpr uint8_t CAPT_CWIN = uint8_t(WIN_IN_ONE + DRAW_RULE);
        constexpr uint8_t PAWN_CWIN = uint8_t(CAPT_CWIN + 1);

        /// Won by the side to move.
        inline bool isWin(uint8_t value) {
            return value >= CAPT_WIN && value <= WIN_IN_ONE + MAX_PLIES - 1;
        }
        /// Lost by the side to move.
        inline bool isLoss(uint8_t value) {
            return value >= MATE - MAX_PLIES && value <= MATE;
        }
        /// Carries a distance, which every won and every lost entry does.
        inline bool hasDistance(uint8_t value) {
            return isWin(value) || isLoss(value);
        }

        /// Plies to the zeroing move.
        inline int plies(uint8_t value) {
            if (value == CAPT_WIN || value == PAWN_WIN) return 1;
            if (value > MATE) return 0;
            if (value >= MATE - MAX_PLIES) return MATE - value;
            return value - WIN_IN_ONE + 1;
        }

        /// Whether the fifty move rule takes this win or loss away.
        inline bool isCursed(uint8_t value) {
            return hasDistance(value) && plies(value) > DRAW_RULE;
        }

        /**
         * Nothing more to compute here.
         *
         * A win that a capture or a pawn move reaches only as a cursed one is not done:
         * a quiet move may still win it inside the rule, which is shorter and therefore
         * better. The same holds for the escape into a loss the rule saves.
         */
        inline bool isDone(uint8_t value) {
            return value == ILLEGAL
                || (hasDistance(value) && value != CAPT_CWIN && value != PAWN_CWIN);
        }

        /// Win, draw or loss of the entry with the rule applied, seen from the side to move.
        inline BitbaseResult toResult(uint8_t value) {
            if (isWin(value))
                return isCursed(value) ? BitbaseResult::CursedWin : BitbaseResult::Win;
            if (isLoss(value))
                return isCursed(value) ? BitbaseResult::BlessedLoss : BitbaseResult::Loss;
            if (value == CAPT_CLOSS) return BitbaseResult::BlessedLoss;
            return value == ILLEGAL ? BitbaseResult::Unknown : BitbaseResult::Draw;
        }
    }

    class Bitbase {
    public:
        /**
         * @brief Constructs an empty Bitbase.
         */
        explicit Bitbase();

        /**
         * @brief Constructs a Bitbase with a given entry count and bits per entry.
         * @param entryCount Number of entries the bitbase should hold.
         * @param bitsPerEntry Number of bits per entry (1, 2 or 8).
		 * @param sig Signature of the bitbase.
         */
        explicit Bitbase(uint64_t entryCount, uint32_t bitsPerEntry, uint32_t sig);

        /**
         * @brief Constructs a Bitbase from a BitbaseIndex.
         * @param index Index providing the entry count.
         * @param bitsPerEntry Number of bits per entry (1, 2 or 8).
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
         * @brief Sets all bits in the bitbase to 1.
         */
        void fillAll();

        /**
         * @brief Fills every entry of a byte wide bitbase with one value.
         * @param value Value to write into every entry.
         */
        void fillAll(BitbaseResult value);

        /**
         * @brief Fills every entry of a byte wide bitbase with one raw value.
         * @param value Value to write into every entry.
         */
        void fillAll(uint8_t value);

        /**
         * @brief Clears every bit whose index has the given lowest bit.
         *
         * The lowest bit of a bitbase index is the side to move, so this clears the
         * candidates of one colour and leaves the other colour's untouched.
         *
         * @param parity 0 clears the even indexes (white to move), 1 the odd ones.
         */
        void clearBitsOfParity(int parity);

        /**
         * @brief Eight candidate bits at once, for scanning over empty stretches.
         * @param byteIndex Index of the byte, that is the entry index divided by eight.
         */
        bbt_t getBitByte(uint64_t byteIndex) const { return _bitbase[byteIndex]; }

        /**
         * @brief Sets a specific bit to 1.
         * @param index Bit index to set.
         */
        void setBit(uint64_t index);

        /**
         * @brief Sets a specific bit to 1 atomically (thread-safe, no mutex required).
         * Uses fetch_or so multiple threads can write concurrently.
         * @param index Bit index to set.
         */
        void setBitAtomic(uint64_t index) {
            const uint64_t elem = index / BITS_IN_ELEMENT;
            const bbt_t    mask = bbt_t(1) << (index % BITS_IN_ELEMENT);
            std::atomic_ref<bbt_t>(_bitbase[elem]).fetch_or(mask, std::memory_order_relaxed);
        }

        /**
         * @brief Reads one bit atomically (thread-safe, no mutex required).
         * Uses a relaxed load — prevents compiler from caching the value in a register.
         * @param index Bit index to read.
         * @returns true if the bit is set.
         */
        bool getBitAtomic(uint64_t index) const {
            const uint64_t elem = index / BITS_IN_ELEMENT;
            const bbt_t    mask = bbt_t(1) << (index % BITS_IN_ELEMENT);
            // atomic_ref<const T> is C++26; the storage itself is non-const, so cast is safe.
            return (std::atomic_ref<bbt_t>(const_cast<bbt_t&>(_bitbase[elem])).load(std::memory_order_relaxed) & mask) != 0;
        }

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
         * @brief Reads one entry of a byte wide bitbase.
         *
         * One entry, one byte. That is what the generation state uses while it computes:
         * a byte is a memory location of its own, so threads that write different entries
         * do not touch each other - with two bit entries four of them share a byte and a
         * write is a read-modify-write of all four.
         *
         * @param index Entry index.
         */
        BitbaseResult getByte(uint64_t index) const {
            return BitbaseResult(_bitbase[index]);
        }

        /**
         * @brief Writes one entry of a byte wide bitbase.
         * @param index Entry index.
         * @param value Value to store.
         */
        void setByte(uint64_t index, BitbaseResult value) {
            _bitbase[index] = bbt_t(value);
        }

        /** @brief The raw byte of an entry, for the distance encoding. */
        uint8_t getRawByte(uint64_t index) const { return _bitbase[index]; }

        /** @brief Writes the raw byte of an entry, for the distance encoding. */
        void setRawByte(uint64_t index, uint8_t value) { _bitbase[index] = value; }

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
         * @brief Returns all indexes where the bit is set to 1.
         * @param indexes Output vector of indices.
         */
        void getAllSetIndexes(std::vector<uint64_t>& indexes) const;

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
         * @brief Prints debug information about the current bitbase.
         */
        void print() const;

		static void setCacheSize(uint32_t sizeInMB) {
			uint64_t numCluster = static_cast<uint64_t>(sizeInMB) * 1024 * 1024 / DEFAULT_CLUSTER_SIZE_IN_BYTES;
			numCluster = std::clamp(numCluster, static_cast<uint64_t>(2), static_cast<uint64_t>(UINT32_MAX));
			cache.resize(numCluster);
		}

        /**
         * @brief Get the Bitbase Data object   
         * 
         * @return const std::vector<bbt_t>& 
         */
        const std::vector<bbt_t>& getBitbaseData() const {
            return _bitbase;
        }

        /**
         * @brief Get the number of bits in one data element (bbt_t).
         * @return Number of bits in a data element.
         */
        const uint32_t getBitsInDataElement() const {
            return BITS_IN_ELEMENT;
        }


    private:

        bool loadHeader(const std::filesystem::path& path);
        int getBitsFromLoadedData(uint64_t bitIndex, bbt_t mask) const;
        int getBitsFromClusterData(uint64_t bitIndex, bbt_t mask);

        // Caching
        uint32_t _signature;
        static inline ClusterCache cache{ 511 };

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

