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
 * The smallest board a 9 bit book needs: the piece placement and the side to
 * move, and playing a move on it. A packed move depends on the position, so
 * adding a line means following it on a board - this one lets the book do that
 * on its own, without a move generator and without knowing a Qapla board.
 *
 * Playing a move needs three rules beyond moving a piece: a king moving onto
 * its own rook castles, a pawn capturing onto an empty square takes en passant,
 * and a promotion replaces the pawn.
 */

#pragma once

#include <array>
#include <cstdint>

#include "packed-move.h"

namespace QaplaBook9 {

	class BookBoard {
	public:
		/**
		 * The start position of a standard game, white to move.
		 */
		static constexpr BookBoard startPosition() {
			constexpr std::array<Piece, 8> BACK_RANK = {
				Piece::ROOK, Piece::KNIGHT, Piece::BISHOP, Piece::QUEEN,
				Piece::KING, Piece::BISHOP, Piece::KNIGHT, Piece::ROOK };
			BookBoard board;
			for (int32_t file = 0; file < 8; ++file) {
				board.setPiece(detail::squareOf(file, 0), Piece(BACK_RANK[file] | Piece::WHITE));
				board.setPiece(detail::squareOf(file, 1), Piece::WHITE_PAWN);
				board.setPiece(detail::squareOf(file, 6), Piece::BLACK_PAWN);
				board.setPiece(detail::squareOf(file, 7), Piece(BACK_RANK[file] | Piece::BLACK));
			}
			return board;
		}

		constexpr Piece pieceAt(Square square) const {
			return _board[square];
		}

		constexpr bool isWhiteToMove() const {
			return _whiteToMove;
		}

		constexpr void setPiece(Square square, Piece piece) {
			_board[square] = piece;
		}

		constexpr void setWhiteToMove(bool whiteToMove) {
			_whiteToMove = whiteToMove;
		}

		/**
		 * Plays a move as unpackMove hands it out, castling as the king moving
		 * onto its own rook. The move is not checked for legality.
		 */
		constexpr void doMove(const BookMove& move) {
			const Piece piece = _board[move.from];
			const Piece color = QaplaBasics::getPieceColor(piece);
			const Piece target = _board[move.to];
			if (isCastling(move, *this)) {
				const bool kingSide = detail::fileOf(move.to) > detail::fileOf(move.from);
				const int32_t rank = detail::rankOf(move.from);
				_board[move.from] = Piece::NO_PIECE;
				_board[move.to] = Piece::NO_PIECE;
				_board[detail::squareOf(kingSide ? 6 : 2, rank)] = piece;
				_board[detail::squareOf(kingSide ? 5 : 3, rank)] = target;
			}
			else {
				const bool isEnPassant = QaplaBasics::isPawn(piece) && target == Piece::NO_PIECE
					&& detail::fileOf(move.to) != detail::fileOf(move.from);
				if (isEnPassant) {
					_board[detail::squareOf(detail::fileOf(move.to), detail::rankOf(move.from))] = Piece::NO_PIECE;
				}
				_board[move.from] = Piece::NO_PIECE;
				_board[move.to] = move.promotion == Piece::NO_PIECE ? piece
					: Piece(QaplaBasics::getPieceType(move.promotion) | color);
			}
			_whiteToMove = !_whiteToMove;
		}

		constexpr bool operator==(const BookBoard& boardToCompare) const = default;

	private:
		std::array<Piece, 64> _board{};
		bool _whiteToMove = true;
	};

	static_assert(BookPosition<BookBoard>);
}
