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
 * Lets a Qapla board be used where a book expects a position. It is the only
 * place in the book code that knows a Qapla board, everything else works on the
 * BookPosition concept - which is what makes the same book code usable from a
 * chess GUI with a board of its own.
 */

#pragma once

#include "../../basics/board.h"
#include "packed-move.h"

namespace QaplaBook {

	/**
	 * A view on a Qapla board that offers what a book needs of it.
	 */
	class BoardPosition {
	public:
		explicit BoardPosition(const QaplaBasics::Board& board) : _board(board) {}

		bool isEmpty(QaplaBasics::Square square) const {
			return _board[square] == QaplaBasics::Piece::NO_PIECE;
		}

	private:
		const QaplaBasics::Board& _board;
	};

	static_assert(BookPosition<BoardPosition>);
}
