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
 * Provides an array of bits with the ability to read and write it clusterwise to disk.
 */

 // bitbase.cpp

#include "bitbase.h"
#include <fstream>
#include <iostream>
#include <ostream>
#include <iomanip>
#include <stdexcept>
#include <bit>
#include "compress.h"
#include "bitbaseindex.h"
#include "piecelist.h"

using namespace std;

namespace QaplaBitbase {

    Bitbase::Bitbase() : _loaded(false), _sizeInBits(0) {}

    Bitbase::Bitbase(uint64_t sizeInBit) : _loaded(false), _sizeInBits(sizeInBit) {
    }

    Bitbase::Bitbase(const BitbaseIndex& index) : Bitbase(index.getSizeInBit()) {}

    void Bitbase::clear() {
        _bitbase.clear();
    }

    void Bitbase::setBit(uint64_t index) {
        if (index >= _sizeInBits) return;
        _bitbase[index / BITS_IN_ELEMENT] |= bbt_t(1) << (index % BITS_IN_ELEMENT);
    }

    void Bitbase::clearBit(uint64_t index) {
        if (index >= _sizeInBits) return;
        _bitbase[index / BITS_IN_ELEMENT] &= ~(bbt_t(1) << (index % BITS_IN_ELEMENT));
    }

    bool Bitbase::getBitCluster(uint64_t index) {
        if (index >= _sizeInBits) return false;

        const uint32_t bitsPerCluster = _clusterSizeBytes * 8;
        const uint32_t clusterIndex = static_cast<uint32_t>(index / bitsPerCluster);
        const uint32_t bitInCluster = static_cast<uint32_t>(index % bitsPerCluster);

        auto cluster = BitbaseFile::readCluster(
            _filePath.string(),
            _sizeInBits,
            _clusterSizeBytes,
            clusterIndex,
            _offsets,
            QaplaCompress::Compress::getDecompressor(_compression)
        );

        const bbt_t word = cluster[bitInCluster / BITS_IN_ELEMENT];
        const uint32_t bit = bitInCluster % BITS_IN_ELEMENT;
        return (word >> bit) & 1;
    }

    bool Bitbase::getBit(uint64_t index) const {
        if (index >= _sizeInBits) return false;
        return (_bitbase[index / BITS_IN_ELEMENT] & (bbt_t(1) << (index % BITS_IN_ELEMENT))) != 0;
    }

    uint64_t Bitbase::getSizeInBit() const {
        return _sizeInBits;
    }

    std::string Bitbase::getStatistic() const {
        uint64_t win = 0;
        uint64_t draw = 0;
        for (uint64_t index = 0; index < _sizeInBits; ++index) {
            if (getBit(index)) win++; else draw++;
        }
        return " win: " + to_string(win) + " draw, loss or error: " + to_string(draw);
    }

    void Bitbase::storeToFile(const std::string& fileName, QaplaCompress::CompressionType compression) {
        try {
            const uint32_t clusterElements = DEFAULT_CLUSTER_SIZE_IN_BYTES / sizeof(bbt_t);

            BitbaseFile::write(
                fileName,
				_sizeInBits,
                _bitbase,
                clusterElements,
                compression,
                QaplaCompress::Compress::getCompressor(compression)
            );

			verifyWrittenFile();
        }
        catch (const std::exception& ex) {
            std::cerr << "Error: Failed to write uncompressed bitbase file '" << fileName
                << "': " << ex.what() << std::endl;
        }
    }

    void Bitbase::verifyWrittenFile() {
        Bitbase loaded(_sizeInBits);
        loaded._filePath = _filePath;
        if (!loaded.loadHeader(_filePath)) {
			throw std::runtime_error("Error: Failed to open file to compare '" + _filePath.string() + "' ");
        }

        auto [success, errorMessage] = loaded.readAll();
        if (!success) {
			throw std::runtime_error("Error: Failed to read bitbase file '" + _filePath.string() + "' " + errorMessage);
		}

        if (loaded.getSizeInBit() != _sizeInBits) {
			throw std::runtime_error("Error: Size mismatch between original and loaded bitbase.");
        }

        if (loaded._bitbase != _bitbase) {
			throw std::runtime_error("Error: Data mismatch between original and loaded bitbase.");
        }

    }

    bool Bitbase::loadHeader(const std::filesystem::path& path) {
        auto fileInfoOpt = BitbaseFile::readFileInfo(path.string());
        if (!fileInfoOpt) return false;

		const auto& fileInfo = *fileInfoOpt;

        _offsets = std::move(fileInfo.offsets);
        _clusterSizeBytes = fileInfo.clusterSize;
        _compression = fileInfo.compression;
        if (fileInfo.sizeInBits != _sizeInBits) {
            throw std::runtime_error("Error: Size mismatch between file and bitbase.");
        }
        return true;
    }

    bool Bitbase::attachFromFile(std::string pieceString, std::string extension, std::filesystem::path path) {
        setFilename(pieceString, extension, path);
        return loadHeader(_filePath);
    }

    std::tuple<bool, std::string> Bitbase::readAll() {
        try {
            const auto decompressFn = QaplaCompress::Compress::getDecompressor(_compression);
            std::vector<bbt_t> data = BitbaseFile::readAll(_filePath.string(), _sizeInBits, _clusterSizeBytes, _offsets, decompressFn);

            _bitbase = std::move(data);
            _loaded = true;

            return { true, "" };
        }
        catch (const std::exception& ex) {
			return { false, ex.what() };
        }
    }

    void Bitbase::getAllIndexes(const Bitbase& andNot, vector<uint64_t>& indexes) const {
        for (uint64_t index = 0; index < _sizeInBits; index += BITS_IN_ELEMENT) {
            uint64_t bbIndex = index / BITS_IN_ELEMENT;
            bbt_t value = _bitbase[bbIndex] & ~andNot._bitbase[bbIndex];
            for (uint64_t sub = 0; value; ++sub, value >>= 1) {
                if (value & 1) indexes.push_back(index + sub);
            }
        }
    }

    uint64_t Bitbase::computeWonPositions(uint64_t begin) const {
        uint64_t result = 0;
        for (uint64_t i = begin; i < _bitbase.size(); ++i) {
            result += std::popcount(_bitbase[i]);
        }
        return result;
    }

    void Bitbase::writeAsCppFile(const string& varName, const string& filename) {

        const auto compressFn = QaplaCompress::Compress::getCompressor(QaplaCompress::CompressionType::Miniz);
        const uint8_t* inPtr = reinterpret_cast<const uint8_t*>(_bitbase.data());
        const size_t inSize = _bitbase.size() * sizeof(bbt_t);
        std::vector<uint8_t> compressed = compressFn(inPtr, inSize);

        ofstream out(filename);
        if (!out) throw runtime_error("Cannot open output file: " + filename);

        out << "#pragma once" << std::endl << std::endl << "#include <cstdint>" << std::endl << std::endl;
        out << "constexpr uint32_t " << varName << "_size = " << compressed.size() << ";" << std::endl;
        out << "constexpr uint32_t " << varName << "[] = {";

        size_t fullChunks = compressed.size() / 4;
        size_t remaining = compressed.size() % 4;

        auto read32 = [&](size_t index) -> uint32_t {
            uint32_t val = 0;
            for (size_t i = 0; i < 4; ++i) {
                size_t pos = index * 4 + i;
                val |= (pos < compressed.size() ? compressed[pos] : 0) << (i * 8);
            }
            return val;
            };

        string spacer;
        for (size_t i = 0; i < fullChunks + (remaining ? 1 : 0); ++i) {
            if (i % 10 == 0) out << "\n";
            out << spacer << "0x" << hex << setw(8) << setfill('0') << read32(i);
            spacer = ",";
        }

        out << std::endl << "};" << std::endl;
        out.close();
    }

    void Bitbase::loadFromEmbeddedData(const uint32_t* data32, bool verbose) {
        vector<uint8_t> compressed;
        const size_t sizeInBytes = getSize() * sizeof(bbt_t);

        compressed.reserve(sizeInBytes);

        for (uint32_t i = 0; i < sizeInBytes / 4; ++i) {
            uint32_t val = data32[i];
            compressed.push_back(static_cast<uint8_t>(val & 0xFF));
            compressed.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
            compressed.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
            compressed.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
        }

        if (sizeInBytes % 4 != 0) {
            uint32_t val = data32[sizeInBytes / 4];
            for (uint32_t i = 0; i < sizeInBytes % 4; ++i) {
                compressed.push_back(static_cast<uint8_t>((val >> (8 * i)) & 0xFF));
            }
        }

        const auto decompressFn = QaplaCompress::Compress::getDecompressor(QaplaCompress::CompressionType::Miniz);
        const size_t expectedSize = getSize();

        std::vector<uint8_t> decompressed = decompressFn(
            compressed.data(),
            compressed.size(),
            expectedSize
        );

        _bitbase.resize(expectedSize);
        std::memcpy(_bitbase.data(), decompressed.data(), decompressed.size());
        _loaded = true;

        if (verbose) {
            cout << "Bitbase loaded from embedded data, sizeInBit = " << _sizeInBits << endl;
        }
    }

    void Bitbase::Bitbase::print() const {
        std::cout << "Bitbase Information:\n";
        std::cout << "  Loaded: " << std::boolalpha << _loaded << '\n';
        std::cout << "  Size in bits: " << _sizeInBits << '\n';

        const uint64_t expectedSize = getSize();
        std::cout << "  Expected memory size (bytes): " << expectedSize << '\n';

        if (_bitbase.size() != expectedSize) {
            std::cout << " Size mismatch detected between bit count and memory size.\n";
        }
    }

} // namespace QaplaBitbase

