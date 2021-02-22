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
 * @author Volker B�hm
 * @copyright Copyright (c) 2021 Volker B�hm
 * @Overview
 * Implements a transposition table entry class storing position information
 * - the current best move of the position
 * - the position value
 * - an information about the value precision (exact, lower-bound, upper-bound)
 * - the calculated depth
 * - a flag indicating a nullmove thread
 * - an age information
 * Depth, Nullmove thread, presions flags and age are packed in a 16 bit integer:
 * msb AAAAPPNDDDDDDDDD lsb
 * Where
 * - D is the calculated depth
 * - N is the nullmove thread flags
 * - P are the precision flags
 * - A is the age indicator
 */

#ifndef __HASHENTRYINFO_H
#define __HASHENTRYINFO_H

#include <assert.h>
#include "../basics/hashconstants.h"
#include "../basics/move.h"
#include "searchdef.h"

using namespace ChessBasics;

namespace ChessSearch {

	class TTEntry {
	public:
		TTEntry() { clear();  };

		inline void setTT(hash_t hash) { _hash = hash; }
		inline hash_t getHash() { return _hash; }

		bool isEmpty() { return _hash == 0; }
		void clear() { 
			_hash = 0;  
			_info = 0;
			setEntryAgeIndicator(ENTRY_AGE_INDICATOR_MASK - 1);
		}

		/**
		 * Sets the computed Depth, the entry age and the nullmove thread flag
		 */
		void setInfo(uint32_t computedDepth, uint32_t entryAge, uint32_t nullmoveThreat) {
			_info &= PRECISION_MASK;
			if (computedDepth > MAX_DEPTH) computedDepth = MAX_DEPTH;
			_info += computedDepth;
			_info += nullmoveThreat << NULLMOVE_THREAT_SHIFT;
			setEntryAgeIndicator(entryAge);
		}

		/**
		 * Sets the calculated position value including the precision based
		 * on the alpha/beta window
		 */
		void setValue(value_t positionValue, value_t alpha, value_t beta, ply_t ply) {
			_info &= ~PRECISION_MASK;
			if (positionValue <= alpha) {
				_info |= LESSER_OR_EQUAL << PRECISION_SHIFT;
			}
			else if (positionValue >= beta) {
				_info |= GREATER_OR_EQUAL << PRECISION_SHIFT;
			}
			else {
				_info |= EXACT << PRECISION_SHIFT;
			}

			setPositionValue(positionValue, ply);
		}

		/**
		 * Gets the value of the hash entry and adjusts the alpha/beta window.
		 * @param positionValue returned position value
		 * @param alpha returned alpha bound
		 * @param beta returned beta bound
		 * @param remainingDepth remaining calculation depth
		 * @param ply current calculation ply
		 * @returns true, if the value of the hash causes a Hash cutoff
		 */
		value_t getValue(value_t alpha, value_t beta, ply_t remainingDepth, ply_t ply) const
		{
			value_t positionValue = NO_VALUE;

			if ((_info != 0) && (getComputedDepth() >= remainingDepth)) {
				const value_t storedValue = getPositionValue(ply);
				switch (getComputedPrecision())
				{
				case EXACT: positionValue = storedValue; break;
				case LESSER_OR_EQUAL: if (storedValue < beta) beta = storedValue; break;
				case GREATER_OR_EQUAL: if (storedValue > alpha) alpha = storedValue; break;
				default:assert(false);
				}

				if (alpha >= beta)
				{
					positionValue = storedValue;
				}
			}

			return positionValue;
		}

		/**
		 * Checks, if the tt value is an upper bound value (thus the value was <= alpha)
		 */
		bool isValueLesserOrEqual() const {
			return getComputedPrecision() == LESSER_OR_EQUAL;
		}

		/**
		 * Checks, if the tt value is an upper bound value (thus the value was <= alpha)
		 */
		bool isValueGreaterOrEqual() const {
			return getComputedPrecision() == GREATER_OR_EQUAL;
		}

		/**
		 * Checks, if the tt value is an upper bound value (thus the value was <= alpha)
		 */
		bool isValueExact() const {
			return getComputedPrecision() == EXACT;
		}

		/**
		 * Checks, if the stored hash value is below a beta value
		 */
		bool isTTValueBelowBeta(value_t probeBeta, ply_t ply) {
			value_t ttValue = getValue(probeBeta-1, probeBeta, 0, ply);
			bool result = ttValue != NO_VALUE && (ttValue < probeBeta);
			return result;
		}

		void updateEntryAgeIndicator(hash_t _ageIndicator)
		{
			_info &= ~(ENTRY_AGE_INDICATOR_MASK << ENTRY_AGE_INDICATOR_SHIFT);
			_info += (_ageIndicator << ENTRY_AGE_INDICATOR_SHIFT) & ENTRY_AGE_INDICATOR_MASK;
		}

		Move getMove() const { return _move;  }
		void setMove(Move move) { _move = move; }

		uint32_t getAgeIndicator() const {
			return ((_info & ENTRY_AGE_INDICATOR_MASK) >> ENTRY_AGE_INDICATOR_SHIFT);
		}

		ply_t getComputedDepth() const {
			return ply_t((_info & DEPTH_MASK) >> DEPTH_SHIFT);
		}
		inline bool isNullmoveThreadPosition() const { 
			return (_info & NULLMOVE_THREAT_MASK) != 0; 
		}
		inline uint32_t getComputedPrecision() const { 
			return (_info & PRECISION_MASK) >> PRECISION_SHIFT; 
		}

		inline bool hasExactValue() const {
			return getComputedPrecision() == EXACT;
		}

		/**
		 * Check, if the value stored MUST be used always
		 */
		inline bool alwaysUseValue() const {
			return getComputedDepth() == MAX_DEPTH;
		}

		/**
		 * Decides, if a value is good enough to overwrite a "always replace" entry
		 * (It is not "always overwrite" only "mostly overwrite")
		 */
		inline bool doOverwriteAlwaysReplaceEntry(
			value_t positionValue, value_t alpha, value_t beta, ply_t computedDepth) const
		{
			bool result = true;
			if (hasExactValue()) {
				bool newValueIsPVValue = beta > positionValue && positionValue > alpha;
				if (!newValueIsPVValue || computedDepth <= getComputedDepth()) {
					result = false;
				}
			}
			return result;
		}


		/**
		 * Gets the position value and adjusts mate values according
		 * the current calculation ply
		 */
		int16_t getPositionValue(int32_t ply) const
		{
			int16_t positionValue = _value;
			if (positionValue > MIN_MATE_VALUE) {
				positionValue -= ply;
			}
			else if (positionValue < -MIN_MATE_VALUE) {
				positionValue += ply;
			}
			assert(positionValue < MAX_VALUE);
			assert(positionValue > -MAX_VALUE);
			return positionValue;
		}



	private:

		/**
		 * True, if the stored value is greater or equal to the computed value
		 */
		bool isValuePrecisionGreaterOrEqual()
		{
			auto precision = getComputedPrecision();
			return (precision == GREATER_OR_EQUAL) || (precision == EXACT);
		}

		/**
		 * True, if the stored value is less or equal to the computed value
		 */
		bool isValuePrecisionLesserOrEqual()
		{
			auto precision = getComputedPrecision();
			return (precision == LESSER_OR_EQUAL) || (precision == EXACT);
		}

		// Sets the current hash entry to the current move no
		void setEntryAgeIndicator(int32_t entryAge)
		{
			_info |= (entryAge << ENTRY_AGE_INDICATOR_SHIFT) & ENTRY_AGE_INDICATOR_MASK;
		}

		/**
		 * Sets the value of the hashed position.
		 * Values indicating a mate have the value "mate - ply"
		 * This is corrected to "mate" before storing the value
		 */
		void setPositionValue(value_t positionValue, ply_t ply)
		{
			if (positionValue > MIN_MATE_VALUE)
			{
				positionValue += ply;
			}
			if (positionValue < -MIN_MATE_VALUE)
			{
				positionValue -= ply;
			}

			assert(positionValue >= -MAX_VALUE);
			assert(positionValue <= MAX_VALUE);
			_value = int16_t(positionValue);
		}

	public:
		static const uint16_t MAX_DEPTH = 0x01FF;

	private:
		static const uint16_t DEPTH_SHIFT = 0ULL;
		static const uint16_t DEPTH_MASK = MAX_DEPTH << DEPTH_SHIFT;
		static const uint16_t NULLMOVE_THREAT_MASK = 0x00000001ULL;
		static const uint16_t NULLMOVE_THREAT_SHIFT = 9ULL;
		static const uint16_t PRECISION_SHIFT = 10ULL;
		static const uint16_t PRECISION_MASK = 0x00000003ULL << PRECISION_SHIFT;
		static const uint16_t ENTRY_AGE_INDICATOR_SHIFT = 12ULL;
		static const uint16_t ENTRY_AGE_INDICATOR_MASK = 0x0000000FULL << ENTRY_AGE_INDICATOR_SHIFT;

		static const uint16_t INVALID = 0;
		static const uint16_t EXACT = 1;
		static const uint16_t LESSER_OR_EQUAL = 2;
		static const uint16_t GREATER_OR_EQUAL = 3;

	private:
		Move _move;
		int16_t _value;
		uint16_t _info;
		hash_t _hash;
	};

}

#endif // __HASHENTRYINFO_H