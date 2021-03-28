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


#ifndef __BITBASE_H
#define __BITBASE_H

#include <vector>
#include <fstream>
#include <cerrno>
#include "../basics/move.h"
#include "../movegenerator/bitboardmasks.h"
#include "../movegenerator/movegenerator.h"
#include "bitbaseindex.h"
#include "huffmancode.h"
#include "compress.h"

namespace QaplaBitbase {

	/**
	 * Basic type storing bits.
	 */
	typedef uint8_t bbt_t;

	class Bitbase {
	public:
		Bitbase(bool loaded = false) : _sizeInBit(0), _loaded(loaded) {}

		/**
		 * @param sizeInBit number of positions stored in the bitbase
		 */
		Bitbase(uint64_t sizeInBit) : _loaded(true) {
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
		uint64_t getSizeInBit() { return _sizeInBit; }

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
		 * Stores a _bitbase to a file
		 */
		void storeToFile(string fileName, bool test = false, bool verbose = false) {
			vector<bbt_t> compressed;
			vector<bbt_t> uncompressed;
			if (verbose) cout << "compressing " << endl;
			compress(_bitbase, compressed);
			if (test) {
				cout << "testing compression ... " << endl;
				uncompress(compressed, uncompressed, _bitbase.size());
				if (_bitbase != uncompressed) {
					cout << " compression error " << fileName << endl;
					for (uint64_t index = 0; index < _bitbase.size(); index++) {
						if (_bitbase[index] != uncompressed[index]) {
							cout << " First error at index: " << index
								<< " required: " << int(_bitbase[index])
								<< " found: " << int(uncompressed[index])
								<< endl;
						}
					}
				}
				else {
					cout << "OK! Original file and uncompressed file are identical" << endl;
				}
			}
			ofstream fout(fileName, ios::out | ios::binary);
			uint64_t size = compressed.size();
			fout.write((char*)&size, sizeof(size));
			fout.write((char*)&compressed[0], size * sizeof(bbt_t));
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
			for (uint64_t index = 0; index < _sizeInBit; index += 8) {
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
		 * Returns true, if the _bitbase is _loaded
		 */
		bool isLoaded() { return _loaded; }

	private:
		typedef uint8_t bbt_t;


		/**
		 * Reads a _bitbase from the file
		 */
		bool readFromFile(string fileName, size_t sizeInBit, bool verbose) {
			_sizeInBit = sizeInBit;
			ifstream fin(fileName, ios::in | ios::binary);
			if (!fin.is_open()) {
				/*
				string message = "Error reading bitbase file ";
				perror((message + fileName + ": ").c_str());
				*/
				return false;
			}
			uint64_t size;
			fin.read((char*)&size, sizeof(size));
			vector<bbt_t> compressed;
			_bitbase.clear();
			compressed.resize(size);
			fin.read((char*)&compressed[0], size * sizeof(bbt_t));
			uncompress(compressed, _bitbase, sizeInBit);
			fin.close();
			if (verbose) {
				cout << "Read: " << fileName << endl;
			}
			_loaded = true;
			return true;
		}


		void compress_test() {
			HuffmanCode huffman(_bitbase);
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

#endif // BITBASE_H