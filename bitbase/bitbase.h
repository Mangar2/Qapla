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
#include "../basics/move.h"
#include "../movegenerator/bitboardmasks.h"
#include "../movegenerator/movegenerator.h"

namespace ChessBitbase {

	class BitBase {
	public:
		BitBase() : loaded(false) {}

		BitBase(uint64_t bitBaseSizeInBit) : sizeInBit(bitBaseSizeInBit), loaded(false) {
			bitBase.resize(sizeInBit / BITS_IN_ELEMENT + 1);
			clear();
		};

		/**
		 * Sets all elements in the bitbase to zero
		 */
		void clear() {
			for (auto& element : bitBase) { element = 0;  }
		}

		/**
		 * Sets a single bit in the bitbase
		 */
		void setBit(uint64_t index) {
			if (index < sizeInBit) {
				bitBase[index / BITS_IN_ELEMENT] |= 1UL << (index % BITS_IN_ELEMENT);
			}
		}

		/**
		 * Gets a single bit from the bitbase
		 */
		bool getBit(uint64_t index) const {
			bool result = false;
			if (loaded && index < sizeInBit) {
				result = (bitBase[index / BITS_IN_ELEMENT] & 1UL << (index % BITS_IN_ELEMENT)) != 0;
			}
			return result;
		}

		/**
		 * Stores a bitbase to a file
		 */
		void storeToFile(string fileName) {
			ofstream fout(fileName, ios::out | ios::binary);
			fout.write((char*)&bitBase[0], bitBase.size() * sizeof(uint32_t));
			fout.close();
		}

		/**
		 * Reads a bitbase from the file
		 */
		void readFromFile(string fileName) {
			ifstream fin(fileName, ios::in | ios::binary);
			fin.read((char*)&bitBase[0], bitBase.size() * sizeof(uint32_t));
			fin.close();
			loaded = true;
		}

		/**
		 * Returns true, if the bitbase is loaded
		 */
		bool isLoaded() { return loaded; }

	private:
		bool loaded;
		static const uint64_t BITS_IN_ELEMENT = sizeof(uint32_t) * 8;
		vector<uint32_t> bitBase;
		uint64_t sizeInBit;
	};

}

#endif // BITBASE_H