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
 * The features of the net, HalfKA in its second shape.
 *
 * A perspective has one feature per (own king square, piece plane, piece square).
 * There are eleven planes and not twelve: the own king needs none, its square is
 * already the first part of the index, so its plane would hold the same single
 * bit twice.
 *
 * Both perspectives use the same weights and differ in what they call their own:
 * the black perspective mirrors every square top to bottom and swaps the colours
 * of every piece. Both are cheap in the piece encoding of Qapla - the colour is
 * the lowest bit of a piece, so swapping the colours of a plane index is an
 * exclusive or with one, and mirroring a square is switchSide.
 *
 * The index order is ours and not the one of any other engine. Nothing outside
 * this file and the data loader of the trainer may depend on it, and those two
 * have to agree: a feature index is a column of the weight matrix, and which
 * piece gets which column is of no interest to the net as long as it is always
 * the same one.
 */

#pragma once

#include <cstdint>

#include "nnue-arch.h"
#include "../../basics/board.h"

namespace QaplaNnue {

	/** A plane index that means the piece has none, used for the own king. */
	inline constexpr uint32_t NO_PIECE_PLANE = ~uint32_t(0);

	/**
	 * The plane of a piece as a perspective sees it.
	 * The pieces of Qapla run WHITE_PAWN, BLACK_PAWN, WHITE_KNIGHT, ... so
	 * subtracting the first one gives 0 to 11 with the colour in the lowest bit.
	 * Seen from black every colour is the other one, which is that bit flipped.
	 * What is then the own king, index 10, has no plane; the enemy king, index 11,
	 * takes the plane that is left over.
	 */
	template <QaplaBasics::Piece PERSPECTIVE>
	constexpr uint32_t pieceePlaneOf(QaplaBasics::Piece piece) {
		const uint32_t relative = uint32_t(piece - QaplaBasics::MIN_PIECE)
			^ (PERSPECTIVE == QaplaBasics::BLACK ? 1u : 0u);
		if (relative == 10) return NO_PIECE_PLANE;
		return relative == 11 ? 10u : relative;
	}

	/**
	 * The square as a perspective sees it: black mirrors the board top to bottom,
	 * so that its own first rank is the one with index zero.
	 */
	template <QaplaBasics::Piece PERSPECTIVE>
	constexpr QaplaBasics::Square squareOf(QaplaBasics::Square square) {
		return PERSPECTIVE == QaplaBasics::BLACK ? QaplaBasics::switchSide(square) : square;
	}

	/**
	 * The feature index of a piece on a square, for a perspective whose own king
	 * stands on kingSquare. Both squares are already seen from the perspective.
	 */
	constexpr uint32_t featureIndex(QaplaBasics::Square kingSquare, uint32_t piecePlane,
		QaplaBasics::Square pieceSquare) {
		return (uint32_t(kingSquare) * PIECE_PLANES + piecePlane) * SQUARE_COUNT
			+ uint32_t(pieceSquare);
	}

	/**
	 * Writes the active features of a perspective and returns how many there are.
	 * The array has to hold MAX_ACTIVE_FEATURES of them.
	 */
	template <QaplaBasics::Piece PERSPECTIVE>
	inline uint32_t computeActiveFeatures(const QaplaBasics::Board& board, uint32_t* features) {
		using namespace QaplaBasics;
		const Square kingSquare = squareOf<PERSPECTIVE>(
			PERSPECTIVE == WHITE ? board.getKingSquare<WHITE>() : board.getKingSquare<BLACK>());
		uint32_t count = 0;
		bitBoard_t pieces = board.getAllPiecesBB();
		while (pieces != 0) {
			const Square square = popLSB(pieces);
			const uint32_t plane = pieceePlaneOf<PERSPECTIVE>(board[square]);
			if (plane == NO_PIECE_PLANE) continue;
			features[count++] = featureIndex(kingSquare, plane, squareOf<PERSPECTIVE>(square));
		}
		return count;
	}
}
