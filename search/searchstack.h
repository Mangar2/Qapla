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

		void initSearchAtRoot(MoveGenerator& board, value_t alpha, value_t beta, int32_t searchDepth) {
			_stack[0].initSearchAtRoot(board, alpha, beta, searchDepth);
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
		 * Takes over everything a search starting at ply + 1 reads from a stack: the nodes up to
		 * and including ply + 1 in full - hashes for the repetition check, evals for isImproving,
		 * node types, the root depth and the move already applied at ply + 1 - and the move
		 * ordering memory of the plies below.
		 */
		void copyForHandover(const SearchStack& from, ply_t ply) {
			for (ply_t index = 0; index <= ply + 1; index++) {
				copyNode(from, index);
			}
			copyMoveOrdering(from, ply + 2);
		}

		/**
		 * Takes over the move ordering memory - killers and the move of the previous iteration -
		 * of the plies from fromPly on. A search continued on another stack must order its moves
		 * exactly as this one would have, and what it set on its way must be seen here. Needed
		 * only while the node count is to stay identical to a single threaded search.
		 */
		void copyMoveOrdering(const SearchStack& from, ply_t fromPly) {
			for (ply_t index = fromPly; index < ply_t(_stack.size()); index++) {
				_stack[index].moveProvider.copyMoveOrdering(from._stack[index].moveProvider);
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
		/**
		 * Copies one node. The pv needs its own copy: the assignment of PV copies from index 0
		 * up to the first empty move, but a node's line starts at its own ply.
		 */
		void copyNode(const SearchStack& from, ply_t ply) {
			_stack[ply] = from._stack[ply];
			_stack[ply].pv.copyFromPV(from._stack[ply].pv, ply);
		}

		TT* ttPtr;
		// We sometimes access the next ply thus we need to have one spare to write data in 
		array<SearchNode, SearchConfig::MAX_SEARCH_DEPTH + 1> _stack;
	};

}

#endif // __SEARCHSTACK_H
