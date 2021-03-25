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
 * State of the generation for a single piece combination
 */

#ifndef __GENERATIONSTATE_H
#define __GENERATIONSTATE_H

#include "bitbase.h"
#include "bitbaseindex.h"

namespace QaplaBitbase {

	class GenerationState
	{
	public:
		/**
		 * Creates a generation state object holding information which positions are already decided
		 * and where to look further
		 */
		GenerationState(PieceList& pieceList) 
			: _wonPositions(true), _computedPositions(true), _candidates(true)
		{
			BitbaseIndex bitbaseIndexType(pieceList);
			_sizeInBit = bitbaseIndexType.getSizeInBit();
			_wonPositions.setSize(_sizeInBit);
			_computedPositions.setSize(_sizeInBit);
			_candidates.setSize(_sizeInBit);
			_pieceList = pieceList;
			_illegal = 0;
			_lossOrDraw = 0;
			_won = 0;
		}

		/**
		 * Gets the current piece list
		 */
		const PieceList& getPieceList() { return _pieceList; }

		/**
		 * Returns true, if the current position is a position to check
		 * @param index index of the position
		 * @param onlyCandidates if true, only candidates are provided to be checked
		 */
		bool isPositionToCheck(uint64_t index, bool onlyCandidates) {
			return !_computedPositions.getBit(index) &&
				(!onlyCandidates || _candidates.getBit(index));
		}

		/**
		 * Retrieves a list of items to check
		 */
		vector<uint64_t> getWork(uint64_t index, uint64_t count, bool onlyCandidates) {
			vector<uint64_t> result;
			for (; count > 0; --count, ++index) {
				if (isPositionToCheck(index, onlyCandidates)) {
					result.push_back(index);
				}
			}
			return result;
		}

		/**
		 * Gets the bitbase size in bits
		 */
		uint64_t getSizeInBit() { return _sizeInBit; }

		const Bitbase& getWonPositions() { return _wonPositions; }

		/**
		 * Sets a position to a candidate for lookup
		 */
		void setCandidate(uint64_t index) { 
			_candidates.setBit(index); 
		}

		/**
		 * Sets a list of candidates 
		 */
		void setCandidates(const vector<uint64_t>& candidates, uint64_t debugIndex) {
			for (auto index : candidates) {
				setCandidate(index);
				if (index == debugIndex) {
					cout << endl << index << " set to candidates " << endl;
				}
			}
		}

		/**
		 * True, if the position is a candidate for further investigation
		 */
		bool isCandidate(uint64_t index) {
			return _candidates.getBit(index);
		}

		/**
		 * Clears all candidates
		 */
		void clearAllCandidates() {
			_candidates.clear();
		}

		/**
		 * Clears a position to be a candidate for lookup
		 */
		void clearCandidate(uint64_t index) {
			_candidates.clearBit(index);
		}

		/**
		 * Sets a position to win
		 */
		void setWin(uint64_t index) {
			_won++;
			_wonPositions.setBit(index);
			_computedPositions.setBit(index);
		}

		/**
		 * Sets a position to loss or draw
		 */
		void setLossOrDraw(uint64_t index) {
			_lossOrDraw++;
			_computedPositions.setBit(index);
		}

		/**
		 * Sets the position to illegal
		 */
		void setIllegal(uint64_t index) {
			_illegal++;
			_computedPositions.setBit(index);
		}

		/**
		 * Prints a statistic 
		 */
		void printStatistic() {
			cout << "Won: " << _won << " Loss or Draw: " << _lossOrDraw << " Illegal: " << _illegal << endl;
		}

		/**
		 * Stores the result (bitbase with winning positions) to a file
		 */
		void storeToFile(string fileName) { _wonPositions.storeToFile(fileName); }

	private:
		uint64_t _sizeInBit;
		uint64_t _illegal;
		uint64_t _lossOrDraw;
		uint64_t _won;
		Bitbase _wonPositions;
		Bitbase _computedPositions;
		Bitbase _candidates;
		PieceList _pieceList;
	};

}

#endif // __GENERATIONSTATE_H
