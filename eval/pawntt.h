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
 * Implements a transposition table for pawns evaluations in chess
 */

#ifndef __PAWNTT_H
#define __PAWNTT_H

#include <vector>
#include <iterator>
#include "../basics/hashconstants.h"
#include "../basics/evalvalue.h"

using namespace std;
using namespace QaplaBasics;

namespace ChessEval {
	struct PawnTTEntry {
		int16_t _mgValue;
		int16_t _egValue;
		hash_t _hash;
		colorBB_t _passedPawns;

		void setEmpty() { _hash = 0; }
		bool isEmpty() const { return _hash == 0; }
		void set(hash_t hash, EvalValue value, colorBB_t passedPawns) {
			_hash = hash;
			_mgValue = value.midgame();
			_egValue = value.endgame();
			_passedPawns = passedPawns;
		}
		EvalValue getValue() const {
			return EvalValue(_mgValue, _egValue);
		}
	};
	class PawnTT
	{
	public:

		PawnTT() { clear(); }

		/**
		 * Clears the transposition table
		 */
		void clear()
		{
			for (PawnTTEntry& entry : _tt) {
				entry.setEmpty();
			}
		}

		/**
		 * Gets the size of the transposition table in bytes
		 */
		size_t getSizeInBytes() { 
			return _tt.capacity() * sizeof(PawnTTEntry);
		}

		/**
		 * Computes the hash index of a hash key 
		 */
		int32_t	computeEntryIndex(hash_t hashKey) {
			return int32_t(hashKey % _tt.capacity());
		}

		/**
		 * Resizes the tt so that it has a certain amount of kilobytes
		 */
		void setSizeInKilobytes(int32_t sizeInKiloBytes)
		{
			uint64_t newCapacitiy = ( 1024ULL * sizeInKiloBytes) / sizeof(PawnTTEntry);
			setCapacity(newCapacitiy);
		}

		/**
		 * Sets a hash entry 
		 */
		uint32_t setEntry(hash_t hashKey, EvalValue value, colorBB_t passedPawns)
		{
			uint32_t index = computeEntryIndex(hashKey);
			_tt[index].set(hashKey, value, passedPawns);
			return index;
		}

		/**
		 * Gets a valid tt entry index
		 * @returns tt entry index with the correct hash signature or INVALID_INDEX
		 */
		uint32_t getTTEntryIndex(hash_t hashKey)
		{
			uint32_t index = computeEntryIndex(hashKey);
			return _tt[index]._hash == hashKey ? index : INVALID_INDEX;
		}

		/**
		 * Gets a element by index
		 */
		inline const PawnTTEntry& getEntry(uint32_t index) const {
			return _tt[index];
		}

		/**
	     * Gets a element by index
		 */
		inline PawnTTEntry& getEntry(uint32_t index) {
			return _tt[index];
		}

		inline value_t getValue2(uint32_t index) const {
			return _tt[index]._mgValue;
		}

		inline EvalValue getValue(uint32_t index) const {
			return _tt[index].getValue();
		}


		/**
		 * Calculates an optimized amount of entries
		 */
		int32_t optimizeHashEntryAmount(int32_t sizeInKiloBytes)
		{
			int32_t newEntryAmount = sizeInKiloBytes * (1024 / sizeof(PawnTTEntry));
			if (newEntryAmount % 2 == 1) newEntryAmount--;
			if (newEntryAmount < 16) {
				newEntryAmount = 16;
			}
			return newEntryAmount;
		}

		static const uint32_t INVALID_INDEX = UINT32_MAX;

	private:

		/**
		 * Sets the transposition table capacity
		 */
		void setCapacity(uint64_t newCapacity) {
			// _tt.reserve(newCapacity);
			_tt.resize(newCapacity);
			clear();
		}

		// Transposition table
		vector<PawnTTEntry> _tt;
	};

}

#endif // __PAWNTT_H
