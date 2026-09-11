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
 * Workpackage for a thread in bitbase generation
 */

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <fstream>
#include <vector>
#include <filesystem>
#include "bitbase-file.h"

namespace QaplaBitbase {

    std::optional<BitbaseFile::FileInfo>
        BitbaseFile::readFileInfo(const std::string& filePath) {
        std::ifstream in(filePath, std::ios::binary);
        if (!in) {
            if (errno == ENOENT) {
                return std::nullopt; // Datei fehlt � erwartbarer Zustand
            }
            else {
                throw std::runtime_error("Failed to open bitbase file: " + filePath);
            }
        }

        BitbaseHeader header = BitbaseHeader::read(in);
        const size_t offsetCount = static_cast<size_t>(header.clusterCount()) + 1;

        std::vector<uint64_t> offsets(offsetCount);
        in.read(reinterpret_cast<char*>(offsets.data()), offsetCount * sizeof(uint64_t));
        if (!in) {
            throw std::runtime_error("Failed to read offset table from file: " + filePath);
        }

        return FileInfo{ 
            .offsets = std::move(offsets), 
            .clusterSize = header.clusterSize(), 
            .compression = header.compression(),
            .sizeInBits = header.sizeInBits(),
            .bitsPerEntry = (header.entryFormat() >= 1) ? 2u : 1u
        };
    }

    std::vector<bbt_t> BitbaseFile::readCluster(
        const std::string& filePath,
        uint64_t sizeInBits,
        uint32_t clusterSizeBytes,
        uint32_t clusterIndex,
        const std::vector<uint64_t>& offsets,
        const QaplaCompress::DecompressFn& decompressFn
    ) {
        if (clusterIndex + 1 >= offsets.size()) {
            throw std::out_of_range("Invalid cluster index (offsets out of bounds)");
        }

        const uint64_t startOffset = offsets[clusterIndex];
        const uint64_t endOffset = offsets[clusterIndex + 1];
        const size_t compressedSize = static_cast<size_t>(endOffset - startOffset);

        std::ifstream in(filePath, std::ios::binary);
        if (!in) {
            throw std::runtime_error("Failed to open bitbase file: " + filePath);
        }

        in.seekg(startOffset, std::ios::beg);
        if (!in) {
            throw std::runtime_error("Failed to seek to cluster offset");
        }

        std::vector<uint8_t> compressed(compressedSize);
        in.read(reinterpret_cast<char*>(compressed.data()), compressedSize);
        if (!in) {
            throw std::runtime_error("Failed to read cluster data");
        }

        // Berechne tats�chliche unkomprimierte Gr��e (in Bytes) f�r diesen Cluster
        const uint64_t startBit = static_cast<uint64_t>(clusterIndex) * static_cast<uint64_t>(clusterSizeBytes) * 8;
        const uint64_t remainingBits = (startBit >= sizeInBits) ? 0 : (sizeInBits - startBit);
        const uint64_t actualBits = std::min<uint64_t>(remainingBits, static_cast<uint64_t>(clusterSizeBytes) * 8);
        const size_t expectedBytes = static_cast<size_t>((actualBits + 7) / 8); // Aufrunden bei Nicht-Byte-Ausrichtung

        std::vector<uint8_t> decompressed = decompressFn(compressed.data(), compressed.size(), expectedBytes);

        if (decompressed.size() % sizeof(bbt_t) != 0) {
            throw std::runtime_error("Invalid decompressed cluster size");
        }

        std::vector<bbt_t> result(decompressed.size() / sizeof(bbt_t));
        std::memcpy(result.data(), decompressed.data(), decompressed.size());
        return result;
    }


    std::vector<bbt_t> BitbaseFile::readAll(
        const std::string& fileNameWithPath,
		uint64_t sizeInBits,
		uint32_t clusterSizeInBytes,
        const std::vector<uint64_t>& offsets,
        const QaplaCompress::DecompressFn& decompressFn
    ) {
        if (offsets.size() < 2) {
            throw std::runtime_error("Offset table is too short");
        }

        const size_t clusterCount = offsets.size() - 1;
        std::vector<bbt_t> result;

        for (size_t i = 0; i < clusterCount; ++i) {
            std::vector<bbt_t> cluster = readCluster(fileNameWithPath, sizeInBits, clusterSizeInBytes, 
                static_cast<uint32_t>(i), offsets, decompressFn);
            result.insert(result.end(), cluster.begin(), cluster.end());
        }

        return result;
    }



}

