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
 * Stores and reads a bitbase using Re-Pair + Huffman compression.
 *
 * Write side: compresses a sequence of BitbaseResult values with
 * QaplaRePair::compress() + serialize() and writes a small binary file.
 *
 * Read side: opens the file with a memory-mapped view (OS manages page
 * caching), deserializes the grammar and Huffman tables once, and then
 * provides O(1)-ish random access to individual positions via probe().
 *
 * File format (little-endian):
 *   uint32_t  magic1        = 0x42425052  ('RPBB')
 *   uint32_t  magic2        = 0x00000001  (version 1)
 *   uint64_t  entryCount    (number of positions in uncompressed table)
 *   uint8_t[] serialized Re-Pair payload (QaplaRePair::serialize() output)
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "bitbase.h"
#include "recursive-pairing.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace QaplaBitbase {

/**
 * @class BitbaseRePairFile
 * @brief Writes and reads Re-Pair + Huffman compressed bitbase files.
 *
 * The class is not copyable because it owns a memory-mapped file handle.
 * Typical usage:
 *
 *   // Compression + write
 *   BitbaseRePairFile::write("KPK.rpb", results);
 *
 *   // Random-access read
 *   BitbaseRePairFile reader;
 *   reader.open("KPK.rpb");
 *   BitbaseResult r = reader.probe(idx);
 *   reader.close();                // or let destructor handle it
 */
class BitbaseRePairFile {
public:
    BitbaseRePairFile()  = default;
    ~BitbaseRePairFile() { close(); }

    BitbaseRePairFile(const BitbaseRePairFile&)            = delete;
    BitbaseRePairFile& operator=(const BitbaseRePairFile&) = delete;

    /**
     * Compress a full WDL sequence and write it to disk.
     *
     * @param fileName  Destination path (e.g. "KPK.rpb").
     * @param results   WDL values to compress (Draw/Win/Loss; Unknown not supported).
     * @throws std::runtime_error on compression or I/O failure.
     */
    static void write(const std::string& fileName,
                      const std::vector<BitbaseResult>& results);

    /**
     * Open a compressed bitbase file for random-access probing.
     *
     * The file is memory-mapped so the OS controls which pages are resident;
     * grammar tables and Huffman decode structures are deserialized once from
     * the mapped view.
     *
     * @param fileName  Path of a file written by write().
     * @returns true on success, false if the file cannot be opened or the
     *          header / payload is invalid.
     */
    bool open(const std::string& fileName);

    /**
     * Close the memory mapping and release all OS handles.
     * Safe to call multiple times or when already closed.
     */
    void close();

    /**
     * Look up the WDL value at a single position index.
     *
     * Uses the sparse index and Huffman decode from the deserialized Re-Pair
     * data; no full decompression is performed.
     *
     * @param index  Zero-based position index into the uncompressed table.
     * @returns Corresponding BitbaseResult (Draw / Win / Loss).
     * @throws std::runtime_error if the file is not open or index is out of range.
     */
    BitbaseResult probe(uint64_t index) const;

    /** Total number of positions in the compressed table. */
    uint64_t getEntryCount() const { return _entryCount; }

    /** Returns true after a successful open() and before close(). */
    bool isOpen() const { return _isOpen; }

private:
    // ── File format constants ──────────────────────────────────────────────
    static constexpr uint32_t MAGIC1       = 0x42425052u; // 'RPBB'
    static constexpr uint32_t MAGIC2       = 0x00000001u; // version 1
    static constexpr size_t   HEADER_BYTES = 16;          // 4 + 4 + 8

    // ── Memory-mapped file state ───────────────────────────────────────────
#ifdef _WIN32
    HANDLE _fileHandle    = INVALID_HANDLE_VALUE;
    HANDLE _mappingHandle = nullptr;
#else
    int    _fd            = -1;
#endif
    const uint8_t* _mappedData = nullptr;
    size_t         _mappedSize = 0;

    // ── Deserialized Re-Pair working structures ────────────────────────────
    QaplaRePair::RePairData _repairData;
    uint64_t                _entryCount = 0;
    bool                    _isOpen     = false;
};

} // namespace QaplaBitbase
