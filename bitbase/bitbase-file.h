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
 * Workpackage for a thread in bitbase generation
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <functional>
#include "compress.h"


namespace QaplaBitbase {

	using bbt_t = uint8_t; // Type for bitbase data

    /**
     * Static utility class for reading and writing Bitbase files.
     * Handles raw file format only — no caching, no semantics.
     */
    class BitbaseFile {
    public:
		struct FileInfo {
			std::vector<uint64_t> offsets;
			uint32_t clusterSize;
			QaplaCompress::CompressionType compression;
			uint64_t sizeInBits;
		};
        /**
         * Writes a bitbase file to disk.
         *
         * @param fileNameWithPath Destination file path (e.g. "kpk.bb").
		 * @param sizeInBits Total number of bits in the bitbase.
         * @param data Uncompressed bitbase data (vector of bbt_t).
         * @param clusterElements Number of elements per cluster.
         * @param compression Compression type identifier.
         * @param compressFn Compression function to apply per cluster.
         */
        static void write(
            const std::string& fileNameWithPath,
			uint64_t sizeInBits,
            const std::vector<bbt_t>& data,
            uint32_t clusterElements,
            QaplaCompress::CompressionType compression,
            const QaplaCompress::CompressFn& compressFn
        );

        /**
         * Reads only the file header and cluster offset table.
         *
         * @param filePath Path to the bitbase file.
		 * @return Tuple containing offsets, cluster size, and compression type.
         */
		static std::optional<BitbaseFile::FileInfo> readFileInfo(const std::string& filePath);

        /**
         * Reads and decompresses a single cluster.
         *
         * @param fileNameWithPath Path to the bitbase file.
         * @param clusterSizeInBytes Size of each uncompressed cluster in bytes.
         * @param clusterIndex Index of the cluster to read.
         * @param offsets Offset table (N+1 entries), as returned by readOffsets().
         * @param decompressFn Decompression function.
         * @return Decompressed cluster data as vector of bbt_t.
         */
        static std::vector<bbt_t> readCluster(
            const std::string& filePath,
			uint64_t sizeInBits,
			uint32_t clusterSizeBytes,
            uint32_t clusterIndex,
            const std::vector<uint64_t>& offsets,
            const QaplaCompress::DecompressFn& decompressFn
        );

        /**
         * Reads and decompresses the entire bitbase into a single flat vector.
         *
         * @param fileNameWithPath Path to the bitbase file.
		 * @param clusterSizeInBytes Size of each uncompressed cluster in bytes.
         * @param decompressFn Decompression function.
         * @return Entire bitbase as uncompressed vector of bbt_t.
         */
        static std::vector<bbt_t> readAll(
            const std::string& fileNameWithPath,
			uint64_t sizeInBits,
            uint32_t clusterSizeInBytes,
            const std::vector<uint64_t>& offsets,
            const QaplaCompress::DecompressFn& decompressFn
        );

    private:
        /**
         * Compact, binary-safe bitbase file header (32 bytes).
         * Structured as 8× uint32_t, with explicit constructor for all fields.
         */
        struct BitbaseHeader {
            static constexpr size_t WordCount = 10;
            static constexpr uint32_t MAGIC_1 = 0x4C504151; // 'Q''A''P''L'
            static constexpr uint32_t MAGIC_2 = 0x42494241; // 'A''B''I''B'
            static constexpr uint32_t CURRENT_VERSION = 1;

            uint32_t words[WordCount];

            /**
             * Constructs a BitbaseHeader with required metadata.
             *
             * @param compression CompressionType as integer.
             * @param cluster_size Size of uncompressed cluster in bytes.
             * @param cluster_count Number of clusters in file.
             * @param totalBits Total number of uncompressed bits in the entire bitbase.
             */
            BitbaseHeader(QaplaCompress::CompressionType compression, uint32_t cluster_size, uint32_t cluster_count, uint64_t sizeInBits) {
                words[0] = MAGIC_1;
                words[1] = MAGIC_2;
                words[2] = CURRENT_VERSION;
                words[3] = static_cast<uint32_t>(compression);
                words[4] = cluster_size;
                words[5] = cluster_count;

                // Split sizeInBits into two 32-bit words (little-endian order)
                words[6] = static_cast<uint32_t>(sizeInBits & 0xFFFFFFFF);
                words[7] = static_cast<uint32_t>((sizeInBits >> 32) & 0xFFFFFFFF);
                words[8] = 0;
                words[9] = 0;
            }

            BitbaseHeader() {
                std::fill(std::begin(words), std::end(words), 0u);
            }

            bool is_valid() const {
                return words[0] == MAGIC_1 && words[1] == MAGIC_2;
            }

            uint32_t version() const { return words[2]; }
            QaplaCompress::CompressionType compression() const {
                return static_cast<QaplaCompress::CompressionType>(words[3]);
            }
            uint32_t clusterSize() const { return words[4]; }
            uint32_t clusterCount() const { return words[5]; }

            uint64_t sizeInBits() const {
                return (static_cast<uint64_t>(words[7]) << 32) | static_cast<uint64_t>(words[6]);
            }

            void write(std::ostream& out) const {
                out.write(reinterpret_cast<const char*>(words), sizeof(words));
                if (!out) throw std::runtime_error("Failed to write bitbase header");
            }

            static BitbaseHeader read(std::istream& in) {
                BitbaseHeader h = BitbaseHeader();
                in.read(reinterpret_cast<char*>(h.words), sizeof(h.words));
                if (!in) throw std::runtime_error("Failed to read bitbase header");
                if (!h.is_valid()) throw std::runtime_error("Invalid bitbase file, magic signature not found");
                return h;
            }
        };

        static std::vector<std::vector<uint8_t>> compressClusters(
            const std::vector<bbt_t>& data,
            uint32_t clusterElements,
            const QaplaCompress::CompressFn& compressFn
        );

        static void computeOffsets(
            const std::vector<std::vector<uint8_t>>& compressedClusters,
            std::vector<uint64_t>& outOffsets,
            size_t headerSize
        );

        static void writeToFile(
            const std::string& tempFile,
            const BitbaseHeader& header,
            const std::vector<uint64_t>& offsets,
            const std::vector<std::vector<uint8_t>>& compressedClusters
        );
    };

}

