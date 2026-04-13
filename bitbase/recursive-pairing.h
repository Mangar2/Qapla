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
 * Recursive Pairing (Re-Pair) + Huffman compression for bitbases.
 *
 * The algorithm encodes a WDL sequence as a context-free grammar by iteratively
 * replacing the most frequent adjacent symbol pair with a new non-terminal.
 * The resulting Re-Pair symbol stream is Huffman-coded and split into fixed-size
 * compressed blocks.  Each block stores a variable number of symbols.
 *
 * Terminal indices (0..NUM_TERMINALS-1) map to WDLValue enum values.
 * Currently only Draw/Win/Loss (0..2) appear in practice; CursedWin and
 * BlessedLoss (3..4) are reserved for future use.
 * Unknown is not supported by this compressor.
 *
 * Primary access pattern: probe(idx) retrieves the WDL value at a single
 * position index without decompressing the entire table.  This is implemented
 * by navigating via sparseIndex -> blockLength -> Huffman decode -> btree walk,
 * analogous to the decompress_pairs() approach used in Syzygy tablebases.
 *
 * Serialized binary layout (little-endian):
 *   uint16_t  numRules
 *   uint8_t   btree[numRules * 2]            (left, right each 1 byte)
 *   uint8_t   maxSymLen                      (maximum Huffman code length in bits)
 *   uint8_t   minSymLen                      (minimum Huffman code length in bits)
 *   uint16_t  symCount[maxSymLen-minSymLen+1]  (symCount[li]  = number of symbols at relative length li)
 *   uint8_t   symOrder[sum(symCount)]          (symbols sorted by (length asc, orig_index asc))
 *   uint32_t  sizeofBlock                    (compressed block size in bytes)
 *   uint32_t  span                           (approx. positions between sparse index entries)
 *   uint32_t  blocksNum
 *   uint16_t  blockLength[blocksNum]         (terminals per block stored as count-1; range 0..65535 = 1..65536)
 *   uint8_t   encoded[blocksNum * sizeofBlock] (bit-packed Huffman codes, MSB first)
 *
 * Derived at load time (not stored):
 *   base64[]      (64-bit left-padded decode table, one entry per length; from symCount)
 *   groupStart[]  (start index in symOrder[] for each relative length group; from symCount)
 *   symlen[]      (per-symbol terminal expansion count minus 1, from btree)
 *   sparseIndex[] (random-access index, from blockLength + span)
 */

#pragma once

#include <cstdint>
#include <vector>
#include "bitbase.h"

namespace QaplaRePair {

    /// Maximum combined vocabulary size (terminals + non-terminals).
    /// Terminal symbols occupy indices 0..NUM_TERMINALS-1.
    /// Grammar rules use indices NUM_TERMINALS..MAX_VOCAB_SIZE-1.
    /// Yields at most MAX_VOCAB_SIZE - NUM_TERMINALS = 251 rules for the default of 256.
    static constexpr int16_t MAX_VOCAB_SIZE = 256;

    /// Default compressed block size in bytes.
    /// Stored as sizeofBlock in RePairData so it can vary per table.
    static constexpr uint32_t COMPRESSED_BLOCK_BYTES = 4;

    /// Maximum Huffman code length in bits.
    static constexpr int MAX_HUFF_LEN = 16;

    static_assert(MAX_VOCAB_SIZE <= 256,
        "MAX_VOCAB_SIZE must be <= 256 to fit symbols in uint8_t");

    /// Number of terminal symbols: Draw, Win, Loss, CursedWin, BlessedLoss.
    /// Indices 0..2 align with QaplaBitbase::BitbaseResult for safe casting.
    /// Indices 3..4 are reserved and currently unused.
    static constexpr int16_t NUM_TERMINALS = 5;

    /// WDL result values with extended precision for DTZ-style tables.
    /// Values 0..2 are identical to QaplaBitbase::BitbaseResult.
    enum class WDLValue : uint8_t {
        Draw        = 0,
        Win         = 1,
        Loss        = 2,
        CursedWin   = 3,  ///< Reserved; currently unused
        BlessedLoss = 4,  ///< Reserved; currently unused
    };

    /**
     * @brief One grammar rule: non-terminal symbol -> (left child, right child).
     *
     * btree[i] describes symbol with index (NUM_TERMINALS + i).
     * Both left and right are symbol indices (0..MAX_VOCAB_SIZE-1):
     *   0..NUM_TERMINALS-1  = terminal (leaf)
     *   NUM_TERMINALS..255  = previously defined non-terminal
     * Each field is 1 byte; the rule is 2 bytes in memory and on disk.
     */
    struct PairRule {
        uint8_t left;
        uint8_t right;
    };

    /**
     * @brief Sparse index entry for fast random-access position lookup.
     *
     * sparseIndex[k] stores the block number and offset-within-block of the
     * position at index I(k) = k * span + span/2, allowing probe() to jump
     * close to the target block without scanning all of blockLength[].
     * Analogous to SparseEntry in Syzygy's PairsData.
     */
    struct SparseEntry {
        uint32_t block;   ///< Block index (0-based)
        uint32_t offset;  ///< Offset within block in units of terminal positions.
                          ///< For real entries: 0..blockLength[block] (fits uint16_t).
                          ///< For phantom entries beyond totalTerminals: can exceed 65535,
                          ///< so uint32_t is required to avoid truncation.
    };

    /**
     * @brief Re-Pair + Huffman compressed representation of a bitbase.
     *
     * Fields are split into three groups:
     *   Serialized   - written to / read from disk
     *   Derived      - computed at load time; not serialized (marked below)
     *
     * To look up the WDL value at a position index, call probe().
     * To decompress a full block or the entire table, call decompressBlock() / decompress().
     */
    struct RePairData {

        // ── Grammar ──────────────────────────────────────────────────────────
        /// btree[i]: children of non-terminal symbol (NUM_TERMINALS + i).
        /// Terminals (0..NUM_TERMINALS-1) are leaves; they have no btree entries.
        std::vector<PairRule>    btree;

        // ── Huffman ───────────────────────────────────────────────────────────
        /// Minimum and maximum Huffman code lengths.  SERIALIZED to disk.
        uint8_t minSymLen = 0;
        uint8_t maxSymLen = 0;

        /// symCount[li] = number of symbols with code length (li + minSymLen).
        /// Used to reconstruct base64[] and groupStart[] at load time.
        /// SERIALIZED as uint16_t array.
        std::vector<uint16_t>    symCount;

        /// symOrder[pos] = original grammar symbol index of the pos-th entry in
        /// the globally sorted order (Huffman length asc, then symbol index asc).
        /// Size = sum(symCount).  During probe(), the offset within length group li
        /// maps to symOrder[groupStart[li] + offset].  SERIALIZED as uint8_t array.
        std::vector<uint8_t>     symOrder;

        // ── Fast decode table (for probe()) ──────────────────────────────────
        /// base64[li] = left-padded 64-bit value of the lowest canonical code
        /// at relative length li = len - minSymLen.  Monotonically increasing
        /// (standard canonical Huffman: shorter codes have smaller bit-patterns).
        /// Computed from symCount[] at load time.  DERIVED; not serialized.
        std::vector<uint64_t>    base64;

        /// groupStart[li] = index into symOrder[] of the first symbol at relative
        /// length li.  groupStart[li] = sum(symCount[0..li-1]).
        /// DERIVED from symCount[]; not serialized.
        std::vector<uint32_t>    groupStart;

        // ── Expansion info ────────────────────────────────────────────────────
        /// symlen[s] = (number of terminal values symbol s expands to) - 1.
        /// symlen[s] == 0: leaf (single terminal, value == s).
        /// Indexed 0..NUM_TERMINALS+btree.size()-1.  DERIVED from btree; not serialized.
        ///
        /// Used by probe() to navigate the btree tree without fully expanding
        /// every symbol, analogous to PairsData::symlen in Syzygy tbprobes.
        std::vector<uint32_t>    symlen;

        // ── Block layout ──────────────────────────────────────────────────────
        /// Compressed block size in bytes.  Every block in encoded[] is exactly
        /// sizeofBlock bytes.  Default is COMPRESSED_BLOCK_BYTES.
        size_t   sizeofBlock = COMPRESSED_BLOCK_BYTES;

        /// Approximately every span terminal positions there is one sparseIndex entry.
        /// Larger span = smaller index, slower probe(); smaller span = opposite.
        uint32_t span = 128;

        /// Number of compressed blocks.  DERIVED as blockLength.size(); not stored
        /// separately on disk, but kept here for fast access.
        uint32_t blocksNum = 0;

        /// sparseIndex[k] is the entry for position I(k) = k*span + span/2.
        /// Enables probe() to jump close to the target block in O(1).
        /// Analogous to PairsData::sparseIndex in Syzygy tbprobes.
        /// DERIVED from blockLength + span; not serialized.
        std::vector<SparseEntry> sparseIndex;

        /// blockLength[b] = (number of terminals in block b) - 1.
        /// Stored as count-1 so that 0..65535 covers 1..65536 actual terminals.
        /// Positions never begin at 0 terminals, so 0 is never a wasted code.
        std::vector<uint16_t>    blockLength;

        /// Bit-packed Huffman-coded Re-Pair symbol stream.
        /// Total size = blocksNum * sizeofBlock bytes.
        /// Within each block: Huffman codes are packed MSB-first; unused
        /// trailing bits of the last symbol in a block are zero-padded.
        std::vector<uint8_t>     encoded;
    };

    /**
     * @brief Compress a bitbase sequence using Re-Pair + Huffman.
     *
     * Input values must be QaplaBitbase::BitbaseResult (Draw/Win/Loss).
     * BitbaseResult::Unknown is not supported.
     *
     * @param input       Sequence of BitbaseResult values to compress.
     * @param blockBytes  Block size in bytes (default COMPRESSED_BLOCK_BYTES).
     * @param span        Positions between sparse index entries (default 128).
     * @return            Compressed RePairData ready for serialization or direct use.
     */
    RePairData compress(const std::vector<QaplaBitbase::BitbaseResult>& input,
                        size_t blockBytes = COMPRESSED_BLOCK_BYTES,
                        uint32_t span = 128);

    /**
     * @brief Serialize RePairData to a flat byte stream (little-endian).
     */
    std::vector<uint8_t> serialize(const RePairData& data);

    /**
     * @brief Deserialize RePairData from a byte stream produced by serialize().
     *
     * Also computes all derived fields (minSymLen, maxSymLen, symlen, sparseIndex).
     *
     * @throws std::runtime_error if the stream is truncated or malformed.
     */
    RePairData deserialize(const uint8_t* data, size_t size);

    /**
     * @brief Look up the WDL value at a single position index without full decompression.
     *
     * Uses sparseIndex to locate the relevant block, then Huffman-decodes symbols
     * until the target offset is reached, and navigates the btree to find the leaf.
     * Analogous to decompress_pairs() in Syzygy tbprobes.
     *
     * @param data  Deserialized RePairData (must have derived fields populated).
     * @param idx   Zero-based position index into the full uncompressed sequence.
     * @return      WDL value at that position.
     * @throws std::runtime_error on corrupt data or out-of-range index.
     */
    WDLValue probe(const RePairData& data, uint64_t idx);

    /**
     * @brief Decompress one block back to its original terminal values.
     *
     * @param data        Deserialized Re-Pair data.
     * @param blockIndex  Zero-based block index.
     * @return            Original terminals for that block, or empty if out of range.
     */
    std::vector<QaplaBitbase::BitbaseResult> decompressBlock(
        const RePairData& data, uint32_t blockIndex);

    /**
     * @brief Fully decompress all encoded data to the original terminal sequence.
     *
     * Equivalent to calling decompressBlock() for every block and concatenating
     * the results, but more efficient (single pass).
     *
     * @param data  Deserialized Re-Pair data.
     * @return      Reconstructed sequence of BitbaseResult values.
     */
    std::vector<QaplaBitbase::BitbaseResult> decompress(const RePairData& data);

} // namespace QaplaRePair
