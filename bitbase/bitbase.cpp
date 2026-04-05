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

using namespace std;

namespace QaplaBitbase {

    Bitbase::Bitbase() : _sizeInBits(0), _loaded(false), _headerLoaded(false) {}

    Bitbase::Bitbase(uint64_t sizeInBit, uint32_t sig) 
        : _signature(sig), _sizeInBits(sizeInBit), _loaded(false), _headerLoaded(false) {}

    Bitbase::Bitbase(const BitbaseIndex& index, uint32_t sig) 
        : Bitbase(index.getSizeInBit(), sig) {}

    void Bitbase::clear() {
        std::fill(_bitbase.begin(), _bitbase.end(), 0);
    }

    void Bitbase::setBit(uint64_t index) {
		assert(isLoaded());
        if (index >= _sizeInBits) return;
        _bitbase[index / BITS_IN_ELEMENT] |= bbt_t(1) << (index % BITS_IN_ELEMENT);
    }

    void Bitbase::or2Bit(uint64_t index2, int value) {
        assert(isLoaded());
        uint64_t index = index2 * 2;
        if (index + 1 >= _sizeInBits) return;
        _bitbase[index / BITS_IN_ELEMENT] |= (bbt_t(value) << (index % BITS_IN_ELEMENT));
    }

    void Bitbase::clearBit(uint64_t index) {
        assert(isLoaded());
        if (index >= _sizeInBits) return;
        _bitbase[index / BITS_IN_ELEMENT] &= ~(bbt_t(1) << (index % BITS_IN_ELEMENT));
    }

    void Bitbase::clear2Bits(uint64_t index2) {
        assert(isLoaded());
        uint64_t index = index2 * 2;
        if (index + 1 >= _sizeInBits) return;
        uint64_t elementIndex = index / BITS_IN_ELEMENT;
        uint64_t bitOffset = index % BITS_IN_ELEMENT;
        _bitbase[elementIndex] &= ~(bbt_t(3) << bitOffset);
    }

    int Bitbase::getBitsFromLoadedData(uint64_t bitIndex, bbt_t mask) const {
        return (_bitbase[bitIndex / BITS_IN_ELEMENT] >> (bitIndex % BITS_IN_ELEMENT)) & mask;
    }

    int Bitbase::getBitsFromClusterData(uint64_t bitIndex, bbt_t mask) {
        const uint32_t bitsPerCluster = _clusterSizeBytes * 8;
        const uint32_t clusterIndex = static_cast<uint32_t>(bitIndex / bitsPerCluster);
        const uint32_t bitInCluster = static_cast<uint32_t>(bitIndex % bitsPerCluster);

        // Prope cache
        auto cacheEntry = cache.getEntry(_signature, clusterIndex);
        if (cacheEntry) {
            const bbt_t word = cacheEntry->data[bitInCluster / BITS_IN_ELEMENT];
            const uint32_t bit = bitInCluster % BITS_IN_ELEMENT;
            return (word >> bit) & mask;
        }
        try {
            auto cluster = BitbaseFile::readCluster(
                _filePath.string(),
                _sizeInBits,
                _clusterSizeBytes,
                clusterIndex,
                _offsets,
                QaplaCompress::Compress::getDecompressor(_compression)
            );
            cache.setEntry(cluster, _signature, clusterIndex);
            const bbt_t word = cluster[bitInCluster / BITS_IN_ELEMENT];
            const uint32_t bit = bitInCluster % BITS_IN_ELEMENT;
            return (word >> bit) & mask;
        }
        catch (...) {
            return -1;
        }
    }

    int Bitbase::getBit(uint64_t index) {
        if (index >= _sizeInBits) return false;
  		if (_loaded) {
			return getBitsFromLoadedData(index, bbt_t(1));
		}

        return getBitsFromClusterData(index, bbt_t(1));

    }

    int Bitbase::get2Bits(uint64_t index2) {
        auto index = index2 * 2;
        if (index + 1 >= _sizeInBits) return false;
  		if (_loaded) {
			return getBitsFromLoadedData(index, bbt_t(3));
		}

        return getBitsFromClusterData(index, bbt_t(3));
    }

    uint64_t Bitbase::getSizeInBit() const {
        return _sizeInBits;
    }

    std::string Bitbase::getStatistic() {
        uint64_t win = 0;
        uint64_t draw = 0;
        for (uint64_t index = 0; index < _sizeInBits; ++index) {
            if (getBit(index)) win++; else draw++;
        }
        return " win: " + to_string(win) + " draw, loss or error: " + to_string(draw);
    }

    void Bitbase::storeToFile(const std::string& fileName, QaplaCompress::CompressionType compression) {
		if (!isLoaded()) {
			throw std::runtime_error("Error: Bitbase is not loaded.");
		}
        compactTo1BitIfPossible();
        const uint32_t clusterElements = DEFAULT_CLUSTER_SIZE_IN_BYTES / sizeof(bbt_t);

        BitbaseFile::write(
            fileName,
			_sizeInBits,
            _bitbase,
            clusterElements,
            compression,
            QaplaCompress::Compress::getCompressor(compression),
            _bitsPerEntry
        );

		verifyWrittenFile();
    }

    void Bitbase::compactTo1BitIfPossible() {
        if (_bitsPerEntry != 1) return;

        // Check if any entry uses the upper bit (value >= 2)
        // In 2-bit layout, the upper bits form a mask: 0xAA for uint8_t elements
        constexpr bbt_t upperBitMask = bbt_t(0xAA);  // bits 1,3,5,7 = upper bit of each 2-bit pair
        for (const auto& element : _bitbase) {
            if (element & upperBitMask) return;
        }

        // No upper bit set anywhere — compact to 1-bit
        const uint64_t entryCount = _sizeInBits / 2;
        const uint64_t newSizeInBits = entryCount;
        const uint64_t newSizeInElements = (newSizeInBits + BITS_IN_ELEMENT - 1) / BITS_IN_ELEMENT;
        std::vector<bbt_t> compacted(newSizeInElements, 0);

        for (uint64_t i = 0; i < entryCount; ++i) {
            const uint64_t srcBitIndex = i * 2;
            const int value = (_bitbase[srcBitIndex / BITS_IN_ELEMENT] >> (srcBitIndex % BITS_IN_ELEMENT)) & 1;
            if (value) {
                compacted[i / BITS_IN_ELEMENT] |= bbt_t(1) << (i % BITS_IN_ELEMENT);
            }
        }

        _bitbase = std::move(compacted);
        _sizeInBits = newSizeInBits;
        _bitsPerEntry = 0;
        std::cout << "Compacted 2-bit bitbase to 1-bit (no upper bits used)" << std::endl;
    }

    void Bitbase::verifyWrittenFile() {
		std::cout << "Verifying written file, bitbase size: " << _bitbase.size() << " path: " << _filePath.string() << std::endl;
        Bitbase loaded(_sizeInBits, _signature);
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
        try {
            auto fileInfoOpt = BitbaseFile::readFileInfo(path.string());
            if (!fileInfoOpt) return false;

            const auto& fileInfo = *fileInfoOpt;

            _offsets = std::move(fileInfo.offsets);
            _clusterSizeBytes = fileInfo.clusterSize;
            _compression = fileInfo.compression;
            _bitsPerEntry = fileInfo.bitsPerEntry;
            if (fileInfo.sizeInBits != _sizeInBits) {
                throw std::runtime_error("Error: Size mismatch between file and bitbase.");
            }
            _headerLoaded = true;
        }
		catch (const std::exception& ex) {
			std::cerr << "Error loading header: " << ex.what() << std::endl;
			return false;
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
            setLoaded();

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
        setLoaded();

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

