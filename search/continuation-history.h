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

namespace QaplaSearch {

	/**
	 * One entry of type T per moving piece and destination square of a move, in one flat array.
	 * Index = (moving piece - WHITE_PAWN) * 64 + destination: the pieces run from WHITE_PAWN (2) to
	 * BLACK_KING (13), so 12 x 64 = 768 entries without gaps. The constant -128 folds into the
	 * address offset.
	 */
	template<typename T>
	class MoveKeyedTable {
	public:
		static constexpr uint32_t SIZE = (QaplaBasics::BLACK_KING - QaplaBasics::WHITE_PAWN + 1) * QaplaBasics::BOARD_SIZE;

		T& operator[](QaplaBasics::Move move) { return _table[computeIndex(move)]; }
		const T& operator[](QaplaBasics::Move move) const { return _table[computeIndex(move)]; }

		void fill(const T& value) { _table.fill(value); }

		/**
		 * @returns true, if the move can key the table. An empty move (root, start of the search)
		 * and the null move carry no piece. Checked by assert only: the caller knows these cases
		 * and does not access the table for them.
		 */
		static constexpr bool isKey(QaplaBasics::Move move) {
			return !move.isEmpty() && !move.isNullMove();
		}

	private:
		static constexpr uint32_t computeIndex(QaplaBasics::Move move) {
			assert(isKey(move));
			return (uint32_t(move.getMovingPiece()) - QaplaBasics::WHITE_PAWN) * QaplaBasics::BOARD_SIZE
				+ uint32_t(move.getDestination());
		}

		std::array<T, SIZE> _table;
	};

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
	 * All tables keyed by earlier moves of the search line. The caller hands over a best move
	 * and the search line; which tables exist and which earlier move keys each of them is
	 * decided here. For now it holds the follow-up move only.
	 */
	class ContinuationHistory {
	public:
		void clear() {
			_followUpMove.clear();
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

	private:
		FollowUpMove _followUpMove;
	};

}
