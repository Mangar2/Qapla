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

namespace QaplaBasics {

	class MaterialBalance {

	public:
		MaterialBalance() {
			pieceValues.fill(0);
			pieceValues[WHITE_PAWN] = EvalValue(PAWN_VALUE_MG, PAWN_VALUE_EG);
			pieceValues[BLACK_PAWN] = EvalValue(-PAWN_VALUE_MG, -PAWN_VALUE_EG);
			pieceValues[WHITE_KNIGHT] = EvalValue(KNIGHT_VALUE_MG, KNIGHT_VALUE_EG);
			pieceValues[BLACK_KNIGHT] = EvalValue(-KNIGHT_VALUE_MG, -KNIGHT_VALUE_EG);
			pieceValues[WHITE_BISHOP] = EvalValue(BISHOP_VALUE_MG, BISHOP_VALUE_EG);
			pieceValues[BLACK_BISHOP] = EvalValue(-BISHOP_VALUE_MG, -BISHOP_VALUE_EG);
			pieceValues[WHITE_ROOK] = EvalValue(ROOK_VALUE_MG, ROOK_VALUE_EG);
			pieceValues[BLACK_ROOK] = EvalValue(-ROOK_VALUE_MG, -ROOK_VALUE_EG);
			pieceValues[WHITE_QUEEN] = EvalValue(QUEEN_VALUE_MG, QUEEN_VALUE_EG);
			pieceValues[BLACK_QUEEN] = EvalValue(-QUEEN_VALUE_MG, -QUEEN_VALUE_EG);
			pieceValues[WHITE_KING] = EvalValue(MAX_VALUE, MAX_VALUE);
			pieceValues[BLACK_KING] = EvalValue(-MAX_VALUE, -MAX_VALUE);
			for (Piece piece = NO_PIECE; piece <= BLACK_KING; ++piece) {
				absolutePieceValues[piece] = abs(pieceValues[piece].midgame());
			}
		}

		/**
		 * Clears the material values
		 */
		void clear() {
			_materialValue = 0;
		}

		/**
		 * Adds a piece to the material value
		 */
		inline void addPiece(Piece piece) {
			_materialValue += pieceValues[piece];
		}

		/**
		 * Removes the piece from the material value
		 */
		inline void removePiece(Piece piece) {
			_materialValue -= pieceValues[piece];
		}

		/**
		 * Gets the value of a piece
		 */
		inline EvalValue getPieceValue(Piece piece) const {
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
		inline EvalValue getMaterialValue() const {
			return _materialValue;
		}

		const array<EvalValue, PIECE_AMOUNT>& getPieceValues() const {
			return pieceValues;
		}
		array<EvalValue, PIECE_AMOUNT>& getPieceValues() {
			return pieceValues;
		}

		const static value_t PAWN_VALUE_MG = 80;
		const static value_t PAWN_VALUE_EG = 95;
		const static value_t KNIGHT_VALUE_MG = 360;
		const static value_t KNIGHT_VALUE_EG = 325;
		const static value_t BISHOP_VALUE_MG = 360;
		const static value_t BISHOP_VALUE_EG = 330;
		const static value_t ROOK_VALUE_MG = 550;
		const static value_t ROOK_VALUE_EG = 550;
		const static value_t QUEEN_VALUE_MG = 975;
		const static value_t QUEEN_VALUE_EG = 1025;

	
	private:

		EvalValue _materialValue;
		array<EvalValue, PIECE_AMOUNT> pieceValues;
		array<value_t, PIECE_AMOUNT> absolutePieceValues;

		static constexpr array<value_t, PIECE_AMOUNT> pieceValuesForMoveSorting =
		{ 0, 0, 100, -100, 300, -300, 300, -300, 500, -500, 900, -900 , MAX_VALUE, -MAX_VALUE };

	};

}

#endif // __MATERIALBALANCE_H
