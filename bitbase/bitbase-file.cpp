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
#include <stdexcept>
#include <fstream>
#include <ostream>
#include <istream>
#include <vector>
#include <filesystem>
#include "bitbase-file.h"

namespace QaplaBitbase {

    void BitbaseFile::write(
        const std::string& fileNameWithPath,
        const std::vector<bbt_t>& data,
        uint32_t clusterElements,
        QaplaCompress::CompressionType compression,
        const QaplaCompress::CompressFn& compressFn
    ) {
        if (clusterElements == 0) {
            throw std::invalid_argument("Cluster size must be > 0");
        }

        const uint32_t clusterSizeBytes = clusterElements * sizeof(bbt_t);
        const size_t totalClusters = (data.size() + clusterElements - 1) / clusterElements;

        std::vector<std::vector<uint8_t>> compressedClusters = compressClusters(data, clusterElements, compressFn);

        std::vector<uint64_t> offsets;
        computeOffsets(compressedClusters, offsets, sizeof(BitbaseHeader) + (totalClusters + 1) * sizeof(uint64_t));

        BitbaseHeader header(compression, clusterSizeBytes, static_cast<uint32_t>(totalClusters));

        std::filesystem::path finalFile = fileNameWithPath;
        std::filesystem::path tempFile = finalFile;
        tempFile += ".tmp";

        writeToFile(tempFile.string(), header, offsets, compressedClusters);
        std::filesystem::rename(tempFile, finalFile);

    }

    std::vector<std::vector<uint8_t>> BitbaseFile::compressClusters(
        const std::vector<bbt_t>& data,
        uint32_t clusterElements,
        const QaplaCompress::CompressFn& compressFn
    ) {
        const size_t totalClusters = (data.size() + clusterElements - 1) / clusterElements;
        std::vector<std::vector<uint8_t>> result;
        result.reserve(totalClusters);

        for (size_t i = 0; i < totalClusters; ++i) {
            const size_t start = i * clusterElements;
            const size_t count = std::min(clusterElements, static_cast<uint32_t>(data.size() - start));

            const uint8_t* inPtr = reinterpret_cast<const uint8_t*>(&data[start]);
            const size_t inSize = count * sizeof(bbt_t);

            std::vector<uint8_t> compressed = compressFn(inPtr, inSize);

            result.push_back(std::move(compressed));
        }

        return result;
    }

    void BitbaseFile::computeOffsets(
        const std::vector<std::vector<uint8_t>>& compressedClusters,
        std::vector<uint64_t>& outOffsets,
        size_t headerSize
    ) {
        outOffsets.reserve(compressedClusters.size() + 1);
        uint64_t offset = static_cast<uint64_t>(headerSize);
        outOffsets.push_back(offset);

        for (const auto& cluster : compressedClusters) {
            offset += cluster.size();
            outOffsets.push_back(offset);
        }
    }

    void BitbaseFile::writeToFile(
        const std::string& tempFile,
        const BitbaseHeader& header,
        const std::vector<uint64_t>& offsets,
        const std::vector<std::vector<uint8_t>>& compressedClusters
    ) {
        std::ofstream out(tempFile, std::ios::binary | std::ios::trunc);
        if (!out) {
            throw std::runtime_error("Failed to open temporary file: " + tempFile);
        }

        header.write(out);

        out.write(reinterpret_cast<const char*>(offsets.data()), offsets.size() * sizeof(uint64_t));
        if (!out) {
            throw std::runtime_error("Failed to write offset table");
        }

        for (const auto& cluster : compressedClusters) {
            out.write(reinterpret_cast<const char*>(cluster.data()), cluster.size());
            if (!out) {
                throw std::runtime_error("Failed to write compressed cluster");
            }
        }

        out.close();
        if (!out) {
            throw std::runtime_error("Failed to finalize file output");
        }

    }

    std::tuple<std::vector<uint64_t>, uint32_t, QaplaCompress::CompressionType>
        BitbaseFile::readOffsets(const std::string& fileNameWithPath) {
        std::ifstream in(fileNameWithPath, std::ios::binary);
        if (!in) {
            throw std::runtime_error("Failed to open bitbase file: " + fileNameWithPath);
        }

        BitbaseHeader header = BitbaseHeader::read(in);
        const size_t offsetCount = static_cast<size_t>(header.clusterCount()) + 1;

        std::vector<uint64_t> offsets(offsetCount);
        in.read(reinterpret_cast<char*>(offsets.data()), offsetCount * sizeof(uint64_t));
        if (!in) {
            throw std::runtime_error("Failed to read offset table");
        }

        return { std::move(offsets), header.clusterSize(), header.compression() };
    }

    std::vector<bbt_t> BitbaseFile::readCluster(
        const std::string& fileNameWithPath,
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

        std::ifstream in(fileNameWithPath, std::ios::binary);
        if (!in) {
            throw std::runtime_error("Failed to open bitbase file: " + fileNameWithPath);
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

		std::vector<uint8_t> decompressed = decompressFn(compressed.data(), compressed.size(), clusterSizeBytes);

        if (decompressed.size() % sizeof(bbt_t) != 0) {
            throw std::runtime_error("Invalid decompressed cluster size");
        }

        std::vector<bbt_t> result(decompressed.size() / sizeof(bbt_t));
        std::memcpy(result.data(), decompressed.data(), decompressed.size());
        return result;
    }

    std::vector<bbt_t> BitbaseFile::readAll(
        const std::string& fileNameWithPath,
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
            std::vector<bbt_t> cluster = readCluster(fileNameWithPath, clusterSizeInBytes, static_cast<uint32_t>(i), offsets, decompressFn);
            result.insert(result.end(), cluster.begin(), cluster.end());
        }

        return result;
    }



}

