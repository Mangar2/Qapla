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
 * Class holding moves played at the root with multi-pv support
 */

#ifndef __ROOTMOVES_H
#define __ROOTMOVES_H

#include <vector>
#include <string>
#include "searchdef.h"
#include "../basics/move.h"
#include "../basics/evalvalue.h"
#include "../movegenerator/movegenerator.h"
#include "pv.h"
#include "searchstack.h"

using namespace std;
using namespace QaplaBasics;

namespace QaplaSearch {

	class RootMove
	{
	public:
		RootMove() { init(); }

		/**
		 * A root move is smaller that another
		 * If it is searched with "PV" and the other is not
		 * If both are searched with PV: if it failed low and the other did not
		 * If both are searched with PV and not failed low: the one with the least value
		 */
		bool operator<(const RootMove& rootMove) const;

		// Initializes all members
		void init();

		/**
		 * Chess move
		 */
		void setMove(Move move) { _move = move; }
		Move getMove() const { return _move; }

		/**
		 * Gets the pv of the move
		 */
		const PV& getPV() const {
			return _pvLine;
		}

		/**
		 * @returns true, if the search was a PV search
		 */
		bool isPVSearched() const { return _isPVSearched; }

		bool isFailLow() const { return _valueOfLastSearch <= _alphaOfLastSearch; }
		bool isFailHigh() const { return _valueOfLastSearch >= _betaOfLastSearch; }

		bool isPVSearchedInWindow(ply_t depth) const { 
			return isPVSearched(depth) && !isFailLow() && !isFailHigh();
		}

		bool isPVSearched(ply_t depth) const { return _isPVSearched && (_depthOfLastSearch >= depth); }

		/**
		 * Sets results after the search of a move is finished
		 */
		void set(value_t searchResult, const SearchStack& stack, bool isPVSearched);

		/**
 		 * Checks, if we need to research this root move.
		 * We do not need to search a move after an intial search window has been changed and 
		 * we can be sure, that the current move result was already inside the last window or 
		 * outside the window and the corresponding window border did not change.
		 * @returns true, if we need to search this root move
		 */ 
		bool doSearch(const SearchVariables& variables) const;

		/**
		 * Print the move (used for debugging)
	     */
		void print() const;

		value_t getValue() const { return _valueOfLastSearch; }
		ply_t getDepth() const { return _depthOfLastSearch;  }
		value_t getAlpha() const { return _alphaOfLastSearch;  }
		value_t getBeta() const { return _betaOfLastSearch; }
	private:
		Move _move;

		value_t _valueOfLastSearch;

		value_t _alphaOfLastSearch;
		value_t _betaOfLastSearch;

		bool _isPVSearched;
		ply_t _depthOfLastSearch;

		uint64_t _nodeCountOfLastSearch;
		uint64_t _totalNodeCount;
		uint64_t _totalTableBaseHits;
		uint64_t _totalBitbaseHits;
		uint64_t _timeSpendToSearchMoveInMilliseconds;

		string _pvString;
		PV _pvLine;

		bool _isExcluded;

	};

	class RootMoves {
	public:
		RootMoves() { clear(); }
		/**
		 * Searches for a move in the root move list
		 * @returns -1, if not found, else the position of the move
		 */
		RootMove& findMove(Move move);

		/**
		 * Sets the moves to be searched
		 * @param position the current position
		 * @param searchMoves the moves to be searched, if empty, all moves are searched
		 * @param butterflyBoard the butterfly board
		 */
		void setMoves(MoveGenerator& position, const std::vector<Move>& searchMoves, ButterflyBoard& butterflyBoard);

		/**
		 * Stable sort algorithm sorting all moves from first to last
		 * @param first first move to consider, do not touch moves in front of first
		 */
		void bubbleSort(uint32_t first);

		/**
		 * Gets an iterator to iterate through the moves
		 */
		vector<RootMove>& getMoves() { return _moves; }
		const vector<RootMove>& getMoves() const { return _moves; }

		/**
		 * Gets a reference to the move
		 */
		RootMove& getMove(size_t index) { return _moves[index]; }
		const RootMove& getMove(size_t index) const { return _moves[index]; }

		/**
		 * Returns the amount of moves with full pv search of current depth and a value in the search window 
		 * at the start of the move list. 
		 */
		uint32_t countPVSearchedMovesInWindow(ply_t depth) const {
			uint32_t count = 0;
			for (const RootMove& rootMove : _moves) {
				if (rootMove.isPVSearchedInWindow(depth)) {
					count++;
				} else {
					break;
				}
			}
			return count;
		}

		/**
		 * Print the root moves (used for debugging)
		 */
		void print();

		void clear() { _moves.clear(); }

	private:
		vector<RootMove> _moves;
	};


}

#endif // __PV