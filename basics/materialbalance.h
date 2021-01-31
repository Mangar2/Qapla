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
 * Implements an incremental algorithm to compute the material balance of a board
 */

#ifndef __MATERIALBALANCE_H
#define __MATERIALBALANCE_H

#include <array>
#include "../basics/types.h"
#include "../basics/evalvalue.h"
#include "../basics/move.h"

using namespace std;

namespace ChessBasics {

	class MaterialBalance {

	public:
		MaterialBalance() {
			pieceValues.fill(0);
			pieceValues[WHITE_PAWN] = PAWN_VALUE;
			pieceValues[BLACK_PAWN] = -PAWN_VALUE;
			pieceValues[WHITE_KNIGHT] = KNIGHT_VALUE;
			pieceValues[BLACK_KNIGHT] = -KNIGHT_VALUE;
			pieceValues[WHITE_BISHOP] = BISHOP_VALUE;
			pieceValues[BLACK_BISHOP] = -BISHOP_VALUE;
			pieceValues[WHITE_ROOK] = ROOK_VALUE;
			pieceValues[BLACK_ROOK] = -ROOK_VALUE;
			pieceValues[WHITE_QUEEN] = QUEEN_VALUE;
			pieceValues[BLACK_QUEEN] = -QUEEN_VALUE;
			pieceValues[WHITE_KING] = MAX_VALUE;
			pieceValues[BLACK_KING] = -MAX_VALUE;
			for (Piece piece = NO_PIECE; piece <= BLACK_KING; ++piece) {
				absolutePieceValues[piece] = abs(pieceValues[piece]);
			}
		}

		/**
		 * Clears the material values
		 */
		void clear() {
			materialValue = 0;
		}

		/**
		 * Adds a piece to the material value
		 */
		inline void addPiece(Piece piece) {
			materialValue += pieceValues[piece];
		}

		/**
		 * Removes the piece from the material value
		 */
		inline void removePiece(Piece piece) {
			materialValue -= pieceValues[piece];
		}

		/**
		 * Gets the value of a piece
		 */
		inline value_t getPieceValue(Piece piece) const {
			return pieceValues[piece];
		}

		/**
		 * Gets the value of a piece
		 */
		inline value_t getPieceValueForMoveSorting(Piece piece) const {
			return pieceValuesForMoveSorting[piece];
		}

		/**
		 * Gets the value of a piece
		 */
		inline value_t getAbsolutePieceValue(Piece piece) const {
			return absolutePieceValues[piece];
		}

		/**
		 * Gets the material value of the board - positive values indicates
		 * white positions is better
		 */
		inline value_t getMaterialValue() const {
			return materialValue;
		}

		/**
		 * Gets the material value of the current board from the player to move view
		 * @param whiteToMove true, if it is the turn of white to play a move
		 */
		inline value_t getMaterialValue(bool whiteToMove) const {
			return whiteToMove ? materialValue : -materialValue;
		}

		const static value_t PAWN_VALUE = 100;
		const static value_t KNIGHT_VALUE = 325;
		const static value_t BISHOP_VALUE = 325;
		const static value_t ROOK_VALUE = 500;
		const static value_t QUEEN_VALUE = 975;
	
	private:

		value_t materialValue;
		array<value_t, PIECE_AMOUNT> pieceValues;
		array<value_t, PIECE_AMOUNT> absolutePieceValues;

		static constexpr array<value_t, PIECE_AMOUNT> pieceValuesForMoveSorting =
		{ 0, 0, 100, -100, 300, -300, 300, -300, 500, -500, 900, -900 , MAX_VALUE, -MAX_VALUE };

	};

}

#endif // __MATERIALBALANCE_H
