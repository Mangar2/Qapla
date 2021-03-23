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
		 * @param bitbaseSizeInBit number of positions stored in the bitbase
		 */
		Bitbase(uint64_t bitBaseSizeInBit) : _sizeInBit(bitBaseSizeInBit), _loaded(true) {
			_bitbase.resize(_sizeInBit / BITS_IN_ELEMENT + 1);
			clear();
		};

		Bitbase(const BitbaseIndex& index) : Bitbase(index.getSizeInBit()) {};

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
		void storeToFile(string fileName) {
			vector<bbt_t> compressed;
			vector<bbt_t> uncompressed;
			cout << "compressing " << endl;
			compress(_bitbase, compressed);
			cout << "finished compressing, testing compression" << endl;
			uncompress(compressed, uncompressed);
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
			ofstream fout(fileName, ios::out | ios::binary);
			uint64_t size = compressed.size();
			fout.write((char*)&size, sizeof(size));
			fout.write((char*)&compressed[0], size * sizeof(bbt_t));
			fout.close();
			cout << "finished testing, file " << fileName << " written" << endl;
		}

		/**
		 * Reads a _bitbase from the file
		 */
		bool readFromFile(string fileName, size_t sizeInBit) {
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
			uncompress(compressed, _bitbase);
			fin.close();
			cout << "Read: " << fileName << endl;
			_loaded = true;
			return true;
		}

		/**
		 * Reads a _bitbase from the file having a piece string
		 */
		bool readFromFile(string pieceString, string extension = ".btb", string path = "./") {
			PieceList list(pieceString);
			BitbaseIndex index(list);
			size_t size = index.getSizeInBit();
			bool success = readFromFile(path + pieceString + extension, size);
			return success;
		}

		/**
		 * Returns true, if the _bitbase is _loaded
		 */
		bool isLoaded() { return _loaded; }

	private:
		typedef uint8_t bbt_t;
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