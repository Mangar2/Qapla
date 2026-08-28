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
 * Fills the tablebase handover structure from a Qapla board.
 *
 * This is the engine side of the interface and the only file here that includes
 * an engine header - tbprobe.h and tbprobe.cpp know nothing about the board.
 * Derived from bitbase/piecelist.h, reduced to what the tablebase index needs.
 */

#pragma once

#include "tbprobe.h"
#include "../../movegenerator/movegenerator.h"

namespace QaplaSyzygy {

	/**
	 * Translates a Qapla piece into the handover encoding.
	 * Qapla stores the type in bits 1..3 and the colour in bit 0, the interface
	 * numbers white pieces 0..5 and black pieces 6..11.
	 */
	constexpr uint8_t toTbPieceCode(QaplaBasics::Piece piece) {
		const int type = (int(piece) >> 1) - 1;      // PAWN -> 0 ... KING -> 5
		const int colour = int(piece) & 1;           // 0 = white, 1 = black
		return uint8_t(type + colour * 6);
	}

	static_assert(toTbPieceCode(QaplaBasics::WHITE_PAWN) == WhitePawn);
	static_assert(toTbPieceCode(QaplaBasics::WHITE_KNIGHT) == WhiteKnight);
	static_assert(toTbPieceCode(QaplaBasics::WHITE_BISHOP) == WhiteBishop);
	static_assert(toTbPieceCode(QaplaBasics::WHITE_ROOK) == WhiteRook);
	static_assert(toTbPieceCode(QaplaBasics::WHITE_QUEEN) == WhiteQueen);
	static_assert(toTbPieceCode(QaplaBasics::WHITE_KING) == WhiteKing);
	static_assert(toTbPieceCode(QaplaBasics::BLACK_PAWN) == BlackPawn);
	static_assert(toTbPieceCode(QaplaBasics::BLACK_KNIGHT) == BlackKnight);
	static_assert(toTbPieceCode(QaplaBasics::BLACK_BISHOP) == BlackBishop);
	static_assert(toTbPieceCode(QaplaBasics::BLACK_ROOK) == BlackRook);
	static_assert(toTbPieceCode(QaplaBasics::BLACK_QUEEN) == BlackQueen);
	static_assert(toTbPieceCode(QaplaBasics::BLACK_KING) == BlackKing);

	/**
	 * Builds the handover structure from a board position.
	 * The pieces are walked in ascending handover order, so the list comes out
	 * sorted without a sorting pass.
	 *
	 * @param position board to read, must hold at most TB_MAX_PIECES pieces
	 * @param tbPosition structure to fill
	 * @returns false if the position holds more pieces than the format supports
	 */
	inline bool buildTbPosition(const QaplaMoveGenerator::MoveGenerator& position,
		TbPosition& tbPosition) {

		using namespace QaplaBasics;

		// Ascending in the handover encoding: white pawn to king, then black pawn to king
		static constexpr Piece walkOrder[] = {
			WHITE_PAWN, WHITE_KNIGHT, WHITE_BISHOP, WHITE_ROOK, WHITE_QUEEN, WHITE_KING,
			BLACK_PAWN, BLACK_KNIGHT, BLACK_BISHOP, BLACK_ROOK, BLACK_QUEEN, BLACK_KING
		};

		uint8_t amount = 0;

		for (const Piece piece : walkOrder) {
			bitBoard_t pieces = position.getPieceBB(piece);
			for (; pieces != 0; pieces &= pieces - 1) {
				if (amount >= TB_MAX_PIECES) return false;
				tbPosition.square[amount] = uint8_t(lsb(pieces));
				tbPosition.piece[amount] = toTbPieceCode(piece);
				++amount;
			}
		}

		tbPosition.pieceCount = amount;
		tbPosition.whiteToMove = position.isWhiteToMove();
		return true;
	}

}
