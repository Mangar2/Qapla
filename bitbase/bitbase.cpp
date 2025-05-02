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

	template<bool Clustered>
    Bitbase<Clustered>::Bitbase() : _loaded(false), _sizeInBit(0) {}

    template<bool Clustered>
    Bitbase<Clustered>::Bitbase(uint64_t sizeInBit) : _loaded(false), _sizeInBit(sizeInBit) {
    }

    template<bool Clustered>
    Bitbase<Clustered>::Bitbase(const BitbaseIndex& index) : Bitbase(index.getSizeInBit()) {}

    template<bool Clustered>
    void Bitbase<Clustered>::clear() {
        for (auto& element : _bitbase) {
            element = 0;
        }
    }

    template<bool Clustered>
    void Bitbase<Clustered>::setBit(uint64_t index) {
        if (index >= _sizeInBit) return;
        if constexpr (Clustered) {
            // todo
        } else {
            _bitbase[index / BITS_IN_ELEMENT] |= bbt_t(1) << (index % BITS_IN_ELEMENT);
        }
    }

    template<bool Clustered>
    void Bitbase<Clustered>::clearBit(uint64_t index) {
        if (index >= _sizeInBit) return;

        if constexpr (Clustered) {
            // TODO: implement cluster-aware clearBit
        }
        else {
            _bitbase[index / BITS_IN_ELEMENT] &= ~(bbt_t(1) << (index % BITS_IN_ELEMENT));
        }
    }

    template<bool Clustered>
    bool Bitbase<Clustered>::getBit(uint64_t index) const {
        if (index >= _sizeInBit) return false;

        if constexpr (Clustered) {
            // TODO: implement cluster-aware getBit (with optional fallback)
            return false;
        }
        else {
            return (_bitbase[index / BITS_IN_ELEMENT] & (bbt_t(1) << (index % BITS_IN_ELEMENT))) != 0;
        }
    }

    template<bool Clustered>
    uint64_t Bitbase<Clustered>::getSizeInBit() const {
        return _sizeInBit;
    }

    template<bool Clustered>
    std::string Bitbase<Clustered>::getStatistic() const {
        uint64_t win = 0;
        uint64_t draw = 0;
        for (uint64_t index = 0; index < _sizeInBit; ++index) {
            if (getBit(index)) win++; else draw++;
        }
        return " win: " + to_string(win) + " draw, loss or error: " + to_string(draw);
    }

    template<bool Clustered>
    void Bitbase<Clustered>::storeToFile(const std::string& fileName, QaplaCompress::CompressionType compression) {
        if constexpr (Clustered) {
            // Clustered-Modus: kein Schreiben über diese Methode
            return;
        }

        try {
            const uint32_t clusterElements = DEFAULT_CLUSTER_SIZE_IN_BYTES / sizeof(bbt_t);

            BitbaseFile::write(
                fileName,
                _bitbase,
                clusterElements,
                compression,
                QaplaCompress::Compress::getCompressor(compression)
            );
        }
        catch (const std::exception& ex) {
            std::cerr << "Error: Failed to write uncompressed bitbase file '" << fileName
                << "': " << ex.what() << std::endl;
        }
    }

    template<bool Clustered>
    void Bitbase<Clustered>::loadHeader(const std::filesystem::path& path) {
        auto [offsets, clusterSizeBytes, compression] = BitbaseFile::readOffsets(path.string());

        _offsets = std::move(offsets);
        _clusterSizeBytes = clusterSizeBytes;
        _compression = compression;
    }

    template<bool Clustered>
    void Bitbase<Clustered>::attachFromFile(std::string pieceString, std::string extension, std::filesystem::path path) {
        _filePath = path / (pieceString + extension);
        loadHeader(_filePath);
    }

    template<bool Clustered>
    std::tuple<bool, std::string> Bitbase<Clustered>::readAll() {
        try {
            const auto decompressFn = QaplaCompress::Compress::getDecompressor(_compression);
            std::vector<bbt_t> data = BitbaseFile::readAll(_filePath.string(), _clusterSizeBytes, _offsets, decompressFn);

            if constexpr (Clustered) {
                // TODO: Clustered-Ladepfad implementieren
            }
            else {
                _bitbase = std::move(data);
                _loaded = true;
            }

            return { true, "" };
        }
        catch (const std::exception& ex) {
			return { false, ex.what() };
        }
    }

    template<bool Clustered>
    void Bitbase<Clustered>::getAllIndexes(const Bitbase& andNot, vector<uint64_t>& indexes) const {
		if (Clustered) throw std::runtime_error("Clustered bitbase does not support getAllIndexes");
        for (uint64_t index = 0; index < _sizeInBit; index += BITS_IN_ELEMENT) {
            uint64_t bbIndex = index / BITS_IN_ELEMENT;
            bbt_t value = _bitbase[bbIndex] & ~andNot._bitbase[bbIndex];
            for (uint64_t sub = 0; value; ++sub, value >>= 1) {
                if (value & 1) indexes.push_back(index + sub);
            }
        }
    }

    template<bool Clustered>
    uint64_t Bitbase<Clustered>::computeWonPositions(uint64_t begin) const {
        if (Clustered) throw std::runtime_error("Clustered bitbase does not support computeWonPositions");
        uint64_t result = 0;
        for (uint64_t i = begin; i < _bitbase.size(); ++i) {
            result += std::popcount(_bitbase[i]);
        }
        return result;
    }

    template<bool Clustered>
    void Bitbase<Clustered>::writeAsCppFile(const string& varName, const string& filename) {
        if (Clustered) throw std::runtime_error("Clustered bitbase does not support writeAsCppFile");

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

    template<bool Clustered>
    void Bitbase<Clustered>::loadFromEmbeddedData(const uint32_t* data32, bool verbose) {
        if (Clustered) throw std::runtime_error("Clustered bitbase does not support loadFromEmbeddedData");
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
            cout << "Bitbase loaded from embedded data, sizeInBit = " << _sizeInBit << endl;
        }
    }

    template<bool Clustered>
    void Bitbase<Clustered>::Bitbase::print() const {
        std::cout << "Bitbase Information:\n";
        std::cout << "  Loaded: " << std::boolalpha << _loaded << '\n';
        std::cout << "  Size in bits: " << _sizeInBit << '\n';

        const uint64_t expectedSize = getSize();
        std::cout << "  Expected memory size (bytes): " << expectedSize << '\n';

        if (_bitbase.size() != expectedSize) {
            std::cout << " Size mismatch detected between bit count and memory size.\n";
        }
    }

	template class Bitbase<false>;
	template class Bitbase<true>;

} // namespace QaplaBitbase

