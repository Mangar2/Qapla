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
 * @copyright Copyright (c) 2025 Volker Böhm
 * @Overview
 * Implements a stack for chess search
 */

#ifndef __SEARCHSTACK_H
#define __SEARCHSTACK_H

#include "search-node.h"
#include "tt.h"
// #include "HistoryTable.h"

namespace QaplaSearch {

	class SearchStack {
	public:

		SearchStack(TT* tt) 
			: ttPtr(tt) 
		{
			for (uint32_t ply = 0; ply < _stack.size(); ply++) {
				_stack[ply].ply = ply;
				_stack[ply].setTT(tt);
			}
		}

		SearchStack(const SearchStack&) = delete;
		SearchStack& operator=(const SearchStack&) = delete;

		void clear() {
			for (uint32_t ply = 0; ply < _stack.size(); ply++) {
				_stack[ply].clearMoveProvider();
			}
		}

		inline const SearchNode& operator[](uint32_t index) const { return _stack[index]; }
		inline  SearchNode& operator[](uint32_t index) { return _stack[index]; }
		TT* getTT() const { return ttPtr; }

		void initSearchAtRoot(MoveGenerator& board, value_t alpha, value_t beta, int32_t searchDepth, ChessEval::PawnTT* pawnTT) {
			_stack[0].initSearchAtRoot(board, alpha, beta, searchDepth, pawnTT);
		}

		Move getMoveFromPVMovesStore(SearchNode::pvIndex_t ply) {
			return _stack[0].getMoveFromPVMovesStore(ply);
		}

		const PV& getPV() const { return _stack[0].pv; }

		/**
		 * Sets the PV moves store
		 */
		void setPV(const PV& pv) {
			for (uint32_t ply = 0; ply < _stack.size(); ply++) {
				_stack[ply].setPVMove(pv.getMove(ply));
				if (pv.getMove(ply) == Move::EMPTY_MOVE) {
					break;
				}
			}
		}

		/**
		 * Fetches what a thread searching a move of the node at ply reads from the node's
		 * stack and cannot derive itself: the line of moves that leads to the node, to be
		 * replayed with replayLine, and of the node what a child reads from its parent, see
		 * SearchNode::copyForHandover, the eval of ply - 1 for the child's isImproving and the
		 * root depth. No lock: the node's thread stays in the node while the helper works.
		 * @param line receives the moves of the plies 1 to ply
		 */
		void fetchForHandover(const SearchStack& from, ply_t ply, Move* line) {
			for (ply_t index = 1; index <= ply; index++) {
				line[index] = from._stack[index].previousMove;
			}
			_stack[ply].copyForHandover(from._stack[ply]);
			if (ply > 0) _stack[ply - 1].adjustedEval = from._stack[ply - 1].adjustedEval;
			_stack[0].remainingDepth = from._stack[0].remainingDepth;
		}

		/**
		 * Plays the line fetched by fetchForHandover from the root, position must be at the
		 * root. Sets the hashes on the way, the repetition check below reads them.
		 */
		void replayLine(MoveGenerator& position, const Move* line, ply_t ply) {
			_stack[0].positionHash = position.computeBoardHash();
			for (ply_t index = 1; index <= ply; index++) {
				_stack[index].doMove(position, line[index]);
				_stack[index].positionHash = position.computeBoardHash();
			}
		}

		/**
		 * Takes the line back, position is at the root afterwards
		 */
		void undoLine(MoveGenerator& position, ply_t ply) {
			for (ply_t index = ply; index >= 1; index--) {
				_stack[index].undoMove(position);
			}
		}

		/**
		 * Check, if there is a two fold repetition in the search tree.
		 */
		bool isDrawByRepetitionInSearchTree(const Board& board, ply_t ply) {
			bool drawByRepetition = false;
			ply_t minPly = ply - board.getHalfmovesWithoutPawnMoveOrCapture();
			if (minPly < 0) { minPly = 0; }
			for (ply_t checkPly = ply - 4; checkPly >= minPly; checkPly -= 2) {
				if (_stack[checkPly].positionHash == _stack[ply].positionHash) {
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
					std::cout << (index / 2 + 1) << ". ";
				}
				if (index <= ply) {
					std::cout << _stack[index].previousMove.getLAN() << " ";
				} else if (currentMove != Move::EMPTY_MOVE) {
					std::cout << currentMove.getLAN() << " ";
				}
			}
		}

		int32_t size() const {
			return static_cast<int32_t>(_stack.size());
		}

	private:
		TT* ttPtr;
		// We sometimes access the next ply thus we need to have one spare to write data in 
		array<SearchNode, SearchConfig::MAX_SEARCH_DEPTH + 1> _stack;
	};

}

#endif // __SEARCHSTACK_H
