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

 // bitbase.cpp

#include "bitbase.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <bit>
#include "compress.h"
#include "bitbaseindex.h"
#include "piecelist.h"

using namespace std;

namespace QaplaBitbase {

    Bitbase::Bitbase(bool loaded) : _loaded(loaded), _sizeInBit(0) {}

    Bitbase::Bitbase(uint64_t sizeInBit) : _loaded(true), _sizeInBit(0) {
        setSize(sizeInBit);
    }

    Bitbase::Bitbase(const BitbaseIndex& index) : Bitbase(index.getSizeInBit()) {}

    void Bitbase::setSize(uint64_t sizeInBit) {
        _sizeInBit = sizeInBit;
        _bitbase.resize(_sizeInBit / BITS_IN_ELEMENT + 1);
        clear();
    }

    void Bitbase::clear() {
        for (auto& element : _bitbase) {
            element = 0;
        }
    }

    void Bitbase::setBit(uint64_t index) {
        if (index < _sizeInBit) {
            _bitbase[index / BITS_IN_ELEMENT] |= bbt_t(1) << (index % BITS_IN_ELEMENT);
        }
    }

    void Bitbase::clearBit(uint64_t index) {
        if (index < _sizeInBit) {
            _bitbase[index / BITS_IN_ELEMENT] &= ~(bbt_t(1) << (index % BITS_IN_ELEMENT));
        }
    }

    bool Bitbase::getBit(uint64_t index) const {
        if (_loaded && index < _sizeInBit) {
            return (_bitbase[index / BITS_IN_ELEMENT] & (bbt_t(1) << (index % BITS_IN_ELEMENT))) != 0;
        }
        return false;
    }

    uint64_t Bitbase::getSizeInBit() const {
        return _sizeInBit;
    }

    std::string Bitbase::getStatistic() const {
        uint64_t win = 0;
        uint64_t draw = 0;
        for (uint64_t index = 0; index < _sizeInBit; ++index) {
            if (getBit(index)) win++; else draw++;
        }
        return " win: " + to_string(win) + " draw, loss or error: " + to_string(draw);
    }

    void Bitbase::storeUncompressed(const std::string& fileName) {
        ofstream fout(fileName, ios::binary);
        if (!fout) {
            cerr << "Error: Cannot open file for writing: " << fileName << endl;
            return;
        }
        uint64_t size = _bitbase.size();
        uint8_t compression = 0;
        fout.write(reinterpret_cast<const char*>(&size), sizeof(size));
        fout.write(reinterpret_cast<const char*>(&compression), sizeof(compression));
        fout.write(reinterpret_cast<const char*>(_bitbase.data()), size * sizeof(bbt_t));
        if (!fout) cerr << "Error: Failed while writing to file: " << fileName << endl;
        fout.close();
    }

    void Bitbase::storeToFile(string fileName, string signature, bool first, bool test, bool verbose) {
        if (verbose) cout << "compressing\n";
        auto compressed = QaplaCompress::compress(_bitbase);

        if (!signature.empty() && first) {
            writeCompressedVectorAsCppFile(compressed, signature, signature + ".h");
        }

        if (test) {
            auto uncompressed = QaplaCompress::uncompress(compressed, _bitbase.size());
            if (_bitbase != uncompressed) {
                cerr << "compression error in file: " << fileName << endl;
                for (uint64_t i = 0; i < _bitbase.size(); ++i) {
                    if (_bitbase[i] != uncompressed[i]) {
                        cerr << " First error at index: " << i
                            << " required: " << int(_bitbase[i])
                            << " found: " << int(uncompressed[i]) << endl;
                        break;
                    }
                }
            }
            else if (verbose) {
                cout << "OK! Original file and uncompressed file are identical\n";
            }
        }

        ofstream fout(fileName, ios::binary);
        if (!fout) {
            cerr << "Error: Cannot open file for writing: " << fileName << endl;
            return;
        }
        uint64_t size = compressed.size();
        fout.write(reinterpret_cast<const char*>(&size), sizeof(size));
        fout.write(reinterpret_cast<const char*>(compressed.data()), size * sizeof(bbt_t));
        if (!fout) cerr << "Error: Failed while writing to file: " << fileName << endl;
        fout.close();
    }

    bool Bitbase::readFromFile(string pieceString, string extension, string path, bool verbose) {
        PieceList list(pieceString);
        BitbaseIndex index(list);
        return readFromFile(path + pieceString + extension, index.getSizeInBit(), verbose);
    }

    bool Bitbase::isLoaded() const {
        return _loaded;
    }

    void Bitbase::getAllIndexes(const Bitbase& andNot, vector<uint64_t>& indexes) const {
        for (uint64_t index = 0; index < _sizeInBit; index += BITS_IN_ELEMENT) {
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

    void Bitbase::writeCompressedVectorAsCppFile(const vector<uint8_t>& data, const string& varName, const string& filename) {
        ofstream out(filename);
        if (!out) throw runtime_error("Cannot open output file: " + filename);

        out << "#pragma once" << std::endl << std::endl << "#include <cstdint>" << std::endl << std::endl;
        out << "constexpr uint32_t " << varName << "_size = " << data.size() << ";" << std::endl;
        out << "constexpr uint32_t " << varName << "[] = {";

        size_t fullChunks = data.size() / 4;
        size_t remaining = data.size() % 4;

        auto read32 = [&](size_t index) -> uint32_t {
            uint32_t val = 0;
            for (size_t i = 0; i < 4; ++i) {
                size_t pos = index * 4 + i;
                val |= (pos < data.size() ? data[pos] : 0) << (i * 8);
            }
            return val;
            };

        string spacer;
        for (size_t i = 0; i < fullChunks + (remaining ? 1 : 0); ++i) {
            if (i % 10 == 0) out << std::endl;
            out << spacer << "0x" << hex << setw(8) << setfill('0') << read32(i);
            spacer = ",";
        }

        out << std::endl << "};" << std::endl;
        out.close();
    }

    void Bitbase::loadFromEmbeddedData(const uint32_t* data32, uint32_t byteSize, uint64_t sizeInBit, bool verbose) {
        vector<bbt_t> compressed;
        compressed.reserve(byteSize);

        for (uint32_t i = 0; i < byteSize / 4; ++i) {
            uint32_t val = data32[i];
            compressed.push_back(static_cast<bbt_t>(val & 0xFF));
            compressed.push_back(static_cast<bbt_t>((val >> 8) & 0xFF));
            compressed.push_back(static_cast<bbt_t>((val >> 16) & 0xFF));
            compressed.push_back(static_cast<bbt_t>((val >> 24) & 0xFF));
        }

        if (byteSize % 4 != 0) {
            uint32_t val = data32[byteSize / 4];
            for (uint32_t i = 0; i < byteSize % 4; ++i) {
                compressed.push_back(static_cast<bbt_t>((val >> (8 * i)) & 0xFF));
            }
        }

        _bitbase = QaplaCompress::uncompress(compressed, sizeInBit / BITS_IN_ELEMENT + 1);
        _sizeInBit = sizeInBit;
        _loaded = true;

        if (verbose) {
            cout << "Bitbase loaded from embedded data, sizeInBit = " << _sizeInBit << endl;
        }
    }

    bool Bitbase::readFromFile(string fileName, size_t sizeInBit, bool verbose) {
        _sizeInBit = sizeInBit;
        ifstream fin(fileName, ios::binary);
        if (!fin) {
            return false;
        }
        uint64_t size = 0;
        fin.read(reinterpret_cast<char*>(&size), sizeof(size));
        vector<bbt_t> compressed(size);
        fin.read(reinterpret_cast<char*>(compressed.data()), size * sizeof(bbt_t));
        _bitbase = QaplaCompress::uncompress(compressed, _sizeInBit);
        fin.close();
        if (verbose) cout << "Read: " << fileName << endl;
        _loaded = true;
        return true;
    }

    uint32_t Bitbase::computeVectorSize() {
        return static_cast<uint32_t>(_sizeInBit / BITS_IN_ELEMENT) + 1;
    }

    ostream& operator<<(ostream& stream, const Bitbase& bitBase) {
        return stream << bitBase.getStatistic();
    }

} // namespace QaplaBitbase

