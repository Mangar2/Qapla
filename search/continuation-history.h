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
 * @copyright Copyright (c) 2026 Volker Böhm
 * @Overview
 * Tables keyed by an earlier move of the search, for move ordering by continuation
 */

#pragma once

#include <array>
#include <cassert>
#include "../basics/types.h"
#include "../basics/move.h"
#include "searchdef.h"
#include "searchstack.h"
#include "butterfly-boards.h"

namespace QaplaSearch {

	/**
	 * Follow-up move: the move that was best after the own move two plies back. Keyed by piece
	 * and destination of that own move.
	 */
	class FollowUpMove {
	public:
		void clear() { _table.fill(QaplaBasics::Move()); }

		/**
		 * Remembers bestMove as the answer to ownPreviousMove. The caller guarantees a real own
		 * move as key and decides which best moves are worth remembering.
		 */
		void set(QaplaBasics::Move ownPreviousMove, QaplaBasics::Move bestMove) {
			_table[ownPreviousMove] = bestMove;
		}

		/**
		 * @returns the remembered answer to ownPreviousMove, an empty move if there is none yet.
		 * The caller guarantees a real own move as key.
		 */
		QaplaBasics::Move get(QaplaBasics::Move ownPreviousMove) const {
			return _table[ownPreviousMove];
		}

	private:
		MoveKeyedTable<QaplaBasics::Move> _table;
	};

	/**
	 * All tables keyed by earlier moves of the search line: the follow-up move and the continuation
	 * butterfly board. The caller hands over a best move and the search line; which tables exist and
	 * which earlier move keys each of them is decided here.
	 */
	class ContinuationHistory {
	public:
		void clear() {
			_followUpMove.clear();
			_board.clear();
		}

		/**
		 * Sets the rows of the node at ply: rows[d - 1] holds the moves played after the move d plies
		 * back, stack[ply + 1 - d].previousMove. The dummy row stands in where the line is shorter than
		 * d, for the empty move at the root and for the null move.
		 */
		void setRows(const SearchStack& stack, ply_t ply, ContinuationHistoryRows& rows) {
			for (ply_t distance = 1; distance <= ply_t(rows.size()); distance++) {
				rows[distance - 1] = _board.getRow(distance <= ply + 1
					? stack[ply + 1 - distance].previousMove : QaplaBasics::Move());
			}
		}

		/**
		 * Enters the best move of a node into each of its rows of the continuation history, with the
		 * quiet moves tried before it, as the butterfly board does. The dummy row is never updated.
		 */
		void newBestMove(const ContinuationHistoryRows& rows, QaplaBasics::Move bestMove, ply_t depth,
			const QaplaBasics::Move* triedMoves, uint32_t triedMovesAmount) {
			for (ContinuationHistoryRow* row : rows) {
				if (!_board.isDummy(row)) {
					row->newBestMove(bestMove, depth, triedMoves, triedMovesAmount);
				}
			}
		}

		/**
		 * Enters bestMove, found in the node at ply, into every table.
		 * stack[ply].previousMove is the opponent's move into this node, stack[ply - 1].previousMove
		 * the own move two plies back - empty at ply 1 (the root has no move into it), the null
		 * move after a null move search.
		 */
		void setMove(const SearchStack& stack, ply_t ply, QaplaBasics::Move bestMove) {
			if (ply < 1) {
				return;
			}
			const QaplaBasics::Move ownPreviousMove = stack[ply - 1].previousMove;
			if (MoveKeyedTable<QaplaBasics::Move>::isKey(ownPreviousMove)) {
				_followUpMove.set(ownPreviousMove, bestMove);
			}
		}

		/**
		 * @returns the follow-up move of the node at ply, an empty move if there is none
		 */
		QaplaBasics::Move getFollowUpMove(const SearchStack& stack, ply_t ply) const {
			if (ply < 1) {
				return QaplaBasics::Move();
			}
			const QaplaBasics::Move ownPreviousMove = stack[ply - 1].previousMove;
			return MoveKeyedTable<QaplaBasics::Move>::isKey(ownPreviousMove)
				? _followUpMove.get(ownPreviousMove) : QaplaBasics::Move();
		}

	private:
		FollowUpMove _followUpMove;
		ContinuationButterflyBoard _board;
	};

}
