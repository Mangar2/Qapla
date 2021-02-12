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
 * Implements a stack for chess search
 */

#ifndef __SEARCHSTACK_H
#define __SEARCHSTACK_H

#include "searchvariables.h"
#include "tt.h"
// #include "HistoryTable.h"

namespace ChessSearch {

	class SearchStack {
	public:

		SearchStack(TT* tt) :
			ttPtr(tt) {
			for (uint32_t ply = 0; ply < MAX_SEARCH_DEPTH; ply++) {
				_stack[ply].ply = ply;
				searchVariablePtr[ply] = &_stack[ply];
				_stack[ply].setTT(tt);
			}
			referenceCount = 1;
		}

		SearchStack(const SearchStack& searchStack)
			: SearchStack(searchStack.getTT())
		{
		}

		~SearchStack() {
		}

		inline const SearchVariables& operator[](uint32_t index) const { return *searchVariablePtr[index]; }
		inline  SearchVariables& operator[](uint32_t index) { return *searchVariablePtr[index]; }
		TT* getTT() const { return ttPtr; }

		void initSearch(MoveGenerator& board, value_t alpha, value_t beta, int32_t searchDepth) {
			_stack[0].init(board, alpha, beta, searchDepth);
		}

		Move getMoveFromPVMovesStore(SearchVariables::pvIndex_t ply) {
			return _stack[0].getMoveFromPVMovesStore(ply);
		}

		const PV& getPV() const { return _stack[0].pvMovesStore; }

		/**
		 * Sets the PV moves store
		 */
		void setPV(PV& pv) {
			for (uint32_t ply = 0; ply < MAX_SEARCH_DEPTH; ply++) {
				_stack[ply].setPVMove(pv.getMove(ply));
				if (pv.getMove(ply) == Move::EMPTY_MOVE) {
					break;
				}
			}
		}

		/**
		 * Copy killer moves from one ply to another
		 */
		void copyKillers(SearchStack& foreignStack, ply_t fromPly) {
			for (ply_t ply = fromPly; ply < MAX_SEARCH_DEPTH; ply++) {
				_stack[ply].moveProvider.setKillerMove(foreignStack[ply].moveProvider);
				if (_stack[ply].getKillerMove()[0] == Move::EMPTY_MOVE) {
					break;
				}
			}
		}

		void initForParallelSearch(SearchStack& foreignStack, ply_t ply) {
			for (ply_t index = 0; index <= ply; index++) {
				searchVariablePtr[index] = foreignStack.searchVariablePtr[index];
			}
			copyKillers(foreignStack, ply + 1);
		}

		/**
		 * Check, if there is a two fold repetition in the search tree.
		 */
		bool isDrawByRepetitionInSearchTree(const Board& board, ply_t ply) {
			bool drawByRepetition = false;
			ply_t checkPly = ply - 4;
			ply_t minPly = ply - board.getHalfmovesWithoutPawnMoveOrCapture();
			if (minPly < 0) { minPly = 0; }
			for (ply_t checkPly = ply - 4; checkPly >= minPly; checkPly -= 2) {
				if (searchVariablePtr[checkPly]->positionHashSignature == searchVariablePtr[ply]->positionHashSignature) {
					drawByRepetition = true;
					break;
				}
			}
			return drawByRepetition;
		}

		/**
		 * Prints the moves of the stack
		 */
		void printMoves(Move currentMove, ply_t ply) const {
			for (ply_t index = 1; index <= ply + 1; index++) {
				if ((index - 1) % 2 == 0) {
					printf("%ld. ", (index / 2 + 1));
				}
				if (index <= ply) {
					printf("%s ", _stack[index].previousMove.getLAN().c_str());
				}
				else if (currentMove != Move::EMPTY_MOVE) {
					printf("%s ", currentMove.getLAN().c_str());
				}
			}
		}

	private:
		TT* ttPtr;
		static const uint8_t MAX_SEARCH_DEPTH = 128;
		array<SearchVariables*, MAX_SEARCH_DEPTH> searchVariablePtr;
		array<SearchVariables, MAX_SEARCH_DEPTH> _stack;
		uint32_t referenceCount;
	};

}

#endif // __SEARCHSTACK_H