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

#pragma once

#include <vector>
#include <fstream>
#include <cerrno>
#include <bit>

#include "lz4.h"
#include "../basics/move.h"
#include "../movegenerator/bitboardmasks.h"
#include "../movegenerator/movegenerator.h"
#include "bitbaseindex.h"
#include "compress.h"

namespace QaplaBitbase {

	/**
	 * Basic type storing bits.
	 */
	using bbt_t = uint8_t;

	class Bitbase {
	public:
		explicit Bitbase(bool loaded = false) : _loaded(loaded), _sizeInBit(0) {}

		/**
		 * @param sizeInBit number of positions stored in the bitbase
		 */
		explicit Bitbase(uint64_t sizeInBit) : _loaded(true), _sizeInBit(0) {
			setSize(sizeInBit);
		};

		Bitbase(const BitbaseIndex& index) : Bitbase(index.getSizeInBit()) {};

		/**
		 * Sets the size of the bitbase
		 */
		void setSize(uint64_t sizeInBit) {
			_sizeInBit = sizeInBit;
			_bitbase.resize(_sizeInBit / BITS_IN_ELEMENT + 1);
			clear();
		}

		/**
		 * Gets the amount of stored positions
		 */
		uint64_t getSizeInBit() const { return _sizeInBit; }

		/**
		 * Sets all elements in the _bitbase to zero
		 */
		void clear() {
			for (auto& element : _bitbase) { element = 0;  }
		}

		/**
		 * Sets a single bit in the _bitbase
		 */
		void setBit(uint64_t index) {
			if (index < _sizeInBit) {
				_bitbase[index / BITS_IN_ELEMENT] |= bbt_t(1) << (index % BITS_IN_ELEMENT);
			}
		}

		/**
		 * Clears a single bit in the _bitbase
		 */
		void clearBit(uint64_t index) {
			if (index < _sizeInBit) {
				_bitbase[index / BITS_IN_ELEMENT] &= ~(bbt_t(1) << (index % BITS_IN_ELEMENT));
			}
		}

		/**
		 * Gets a single bit from the _bitbase
		 */
		bool getBit(uint64_t index) const {
			bool result = false;
			if (_loaded && index < _sizeInBit) {
				result = (_bitbase[index / BITS_IN_ELEMENT] & (bbt_t(1) << (index % BITS_IN_ELEMENT))) != 0;
			}
			return result;
		}

		/**
		 * Gets a statistic of a _bitbase
		 */
		string getStatistic() const {
			uint64_t win = 0;
			uint64_t draw = 0;

			for (uint64_t index = 0; index < _sizeInBit; index++) {
				if (getBit(index)) {
					win++;
				}
				else {
					draw++;
				}
			}
			return " win: " + to_string(win) + " draw, loss or error: " + to_string(draw);
		}

		/**
		 * Stores a bitbase uncompressed to a file
		 */
		void storeUncompressed(const std::string& fileName) {
			std::ofstream fout(fileName, std::ios::out | std::ios::binary);
			if (!fout) {
				std::cerr << "Error: Cannot open file for writing: " << fileName << std::endl;
				return;
			}

			uint64_t size = _bitbase.size();
			uint8_t compression = 0;

			fout.write(reinterpret_cast<const char*>(&size), sizeof(size));
			fout.write(reinterpret_cast<const char*>(&compression), sizeof(compression));
			fout.write(reinterpret_cast<const char*>(_bitbase.data()), size * sizeof(bbt_t));

			if (!fout) {
				std::cerr << "Error: Failed while writing to file: " << fileName << std::endl;
			}
			fout.close();
		}


		/**
		 * Stores a _bitbase to a file
		 */
		void storeToFile(string fileName, string signature, bool test = false, bool verbose = false) {
			if (verbose) cout << "compressing " << endl;
			auto compressed = std::move(QaplaCompress::compress(_bitbase));
			if (signature != "") {
				writeCompressedVectorAsCppFile(compressed, signature, signature + ".h");
			}
			if (test) {
				if (verbose) std::cout << "testing compression ... " << std::endl;
				auto uncompressed = QaplaCompress::uncompress(compressed, _bitbase.size());
				if (_bitbase != uncompressed) {
					std::cerr << "compression error in file: " << fileName << std::endl;
					for (uint64_t index = 0; index < _bitbase.size(); index++) {
						if (_bitbase[index] != uncompressed[index]) {
							std::cerr << " First error at index: " << index
								<< " required: " << int(_bitbase[index])
								<< " found: " << int(uncompressed[index])
								<< std::endl;
						}
					}
				}
				else {
					if (verbose) std::cout << "OK! Original file and uncompressed file are identical" << std::endl;
				}
			}
			ofstream fout(fileName, ios::out | ios::binary);
			if (!fout) {
				std::cerr << "Error: Cannot open file for writing: " << fileName << std::endl;
				return;
			}
			uint64_t size = compressed.size();
			fout.write((char*)&size, sizeof(size));
			fout.write((char*)&compressed[0], size * sizeof(bbt_t));
			if (!fout) {
				std::cerr << "Error: Failed while writing to file: " << fileName << std::endl;
			}
			fout.close();
		}


		/**
		 * Reads a _bitbase from the file having a piece string
		 */
		bool readFromFile(string pieceString, string extension = ".btb", string path = "./",
			bool verbose = true) 
		{
			PieceList list(pieceString);
			BitbaseIndex index(list);
			size_t size = index.getSizeInBit();
			bool success = readFromFile(path + pieceString + extension, size, verbose);
			return success;
		}

		/**
		 * Retrieves all indexes from the bitbase as vector
		 */
		void getAllIndexes(const Bitbase& andNot, vector<uint64_t>& indexes) const {
			for (uint64_t index = 0; index < _sizeInBit; index += BITS_IN_ELEMENT) {
				uint64_t bbIndex = index / BITS_IN_ELEMENT;
				bbt_t value = _bitbase[bbIndex] & ~andNot._bitbase[bbIndex] ;
				for (uint64_t sub = 0; value; sub++, value /= 2) {
					if (value & 1) {
						indexes.push_back(index + sub);
					}
				}
			}
		}

		/**
		 * Retrieves the amount of won positions in the bitbase
		 */
		uint64_t computeWonPositions(uint64_t begin = 0) const {
			uint64_t result = 0;
			for (uint64_t index = begin; index < _bitbase.size(); index ++) {
				result += std::popcount(_bitbase[index]);
			}
			return result;
		}

		void writeCompressedVectorAsCppFile(const std::vector<uint8_t>& data, const std::string& varName, const std::string& filename) {
			std::ofstream out(filename);
			if (!out.is_open()) {
				throw std::runtime_error("Cannot open output file: " + filename);
			}

			out << "#pragma once" << std::endl << std::endl;
			out << "#include <cstdint>" << std::endl << std::endl;
			out << "constexpr uint32_t " << varName << "_size = " << data.size() << ";" << std::endl;
			out << "constexpr uint32_t " << varName << "[] = {";

			size_t fullChunks = data.size() / 4;
			size_t remaining = data.size() % 4;

			auto read32 = [&](size_t index) -> uint32_t {
				uint32_t val = 0;
				for (size_t i = 0; i < 4; ++i) {
					size_t pos = index * 4 + i;
					val |= (pos < data.size() ? data[pos] : 0) << (i * 8); // little endian
				}
				return val;
				};
			std::string spacer = "";
			for (size_t i = 0; i < fullChunks + (remaining ? 1 : 0); ++i) {
				uint32_t value = read32(i);
				if (i % 10 == 0) out << std::endl << "  ";
				out << spacer << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
				spacer = ",";
			}

			out << std::endl << "};" << std::endl;
			out.close();
		}

		void loadFromEmbeddedData(const uint32_t* data32, uint32_t byteSize, uint64_t sizeInBit, bool verbose) {
			std::vector<bbt_t> compressed;
			compressed.reserve(byteSize);

			// uint32_t -> 4 bytes (little endian)
			for (uint32_t i = 0; i < byteSize / 4; ++i) {
				uint32_t val = data32[i];
				compressed.push_back(static_cast<bbt_t>(val & 0xFF));
				compressed.push_back(static_cast<bbt_t>((val >> 8) & 0xFF));
				compressed.push_back(static_cast<bbt_t>((val >> 16) & 0xFF));
				compressed.push_back(static_cast<bbt_t>((val >> 24) & 0xFF));
			}

			// Cut on not full 4 bytes
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
				std::cout << "Bitbase loaded from embedded data, sizeInBit = " << _sizeInBit << std::endl;
			}
		}


		/**
		 * Returns true, if the _bitbase is _loaded
		 */
		bool isLoaded() const { return _loaded; }

	private:
		
		/**
		 * Reads a _bitbase from the file
		 */
		bool readFromFile(string fileName, size_t sizeInBit, bool verbose) {
			_sizeInBit = sizeInBit;
			ifstream fin(fileName, ios::in | ios::binary);
			if (!fin.is_open()) {
				std::cerr << "Error: Cannot open file for reading: " << fileName << std::endl;
				return false;
			}
			uint64_t size;
			fin.read(reinterpret_cast<char*>(&size), sizeof(size));
			vector<bbt_t> compressed;
			_bitbase.clear();
			compressed.resize(size);
			fin.read(reinterpret_cast<char*>(&compressed[0]), size * sizeof(bbt_t));
			_bitbase = std::move(QaplaCompress::uncompress(compressed, _sizeInBit));
			fin.close();
			if (verbose) {
				cout << "Read: " << fileName << endl;
			}
			_loaded = true;
			return true;
		}

		uint32_t computeVectorSize() {
			return (uint32_t)(_sizeInBit / BITS_IN_ELEMENT) + 1;
		}

		bool _loaded;
		static const uint64_t BITS_IN_ELEMENT = sizeof(bbt_t) * 8;
		vector<bbt_t> _bitbase;
		uint64_t _sizeInBit;
	};

	/**
	 * Pints a bitbase statistic
	 */
	static ostream& operator<<(ostream& stream, const Bitbase& bitBase) {
		stream << bitBase.getStatistic();
		return stream;
	}

}
