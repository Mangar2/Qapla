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
 * Implements an incremental algorithm to compute the material balance of a board
 */

#pragma once

#include <array>
#include "../basics/types.h"
#include "../basics/evalvalue.h"
#include "../interface/uci-parameter-provider.h"

#ifdef PARAM_OPTIMIZE
#define PARAM_OPTIMIZE_MATERIALBALANCE
#endif

namespace QaplaBasics {

	class MaterialBalance {

	public:
		friend class MaterialBalanceUciAccess;

		/**
		 * Get UCI parameter access interface
		 * @return Reference to UCI parameter provider
		 */
		static ChessEval::UciParameterProvider& getUciAccess();

		MaterialBalance() {
#ifdef PARAM_OPTIMIZE_MATERIALBALANCE
			ensurePieceValuesInitialized();
#else
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
#endif
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
		 * Gets the midgame/endgame evaluation value for the specified piece
		 */
		inline EvalValue getPieceValue(Piece piece) const {
			return pieceValues[piece];
		}

		/**
		 * Gets the piece value used for move sorting heuristics
		 */
		inline value_t getPieceValueForMoveSorting(Piece piece) const {
			return pieceValuesForMoveSorting[piece];
		}

		/**
		 * Gets the absolute midgame value 
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

		// Piece values reverted to 0.4.0-025a. The optimized values
		// (N 365, B 365/340, R 550/575, Q 1150/1150) performed worse than 0.4.0-025a,
		// therefore they are kept as a comment only.
		constexpr static value_t PAWN_VALUE_MG_DEFAULT = 80;
		constexpr static value_t PAWN_VALUE_EG_DEFAULT = 95;
		constexpr static value_t KNIGHT_VALUE_MG_DEFAULT = 360;
		constexpr static value_t KNIGHT_VALUE_EG_DEFAULT = 310;
		constexpr static value_t BISHOP_VALUE_MG_DEFAULT = 360;
		constexpr static value_t BISHOP_VALUE_EG_DEFAULT = 330;
		constexpr static value_t ROOK_VALUE_MG_DEFAULT = 560;
		constexpr static value_t ROOK_VALUE_EG_DEFAULT = 570;
		constexpr static value_t QUEEN_VALUE_MG_DEFAULT = 1035;
		constexpr static value_t QUEEN_VALUE_EG_DEFAULT = 1085;

	#ifndef PARAM_OPTIMIZE_MATERIALBALANCE
		constexpr static value_t PAWN_VALUE_MG = PAWN_VALUE_MG_DEFAULT;
		constexpr static value_t PAWN_VALUE_EG = PAWN_VALUE_EG_DEFAULT;
		constexpr static value_t KNIGHT_VALUE_MG = KNIGHT_VALUE_MG_DEFAULT;
		constexpr static value_t KNIGHT_VALUE_EG = KNIGHT_VALUE_EG_DEFAULT;
		constexpr static value_t BISHOP_VALUE_MG = BISHOP_VALUE_MG_DEFAULT;
		constexpr static value_t BISHOP_VALUE_EG = BISHOP_VALUE_EG_DEFAULT;
		constexpr static value_t ROOK_VALUE_MG = ROOK_VALUE_MG_DEFAULT;
		constexpr static value_t ROOK_VALUE_EG = ROOK_VALUE_EG_DEFAULT;
		constexpr static value_t QUEEN_VALUE_MG = QUEEN_VALUE_MG_DEFAULT;
		constexpr static value_t QUEEN_VALUE_EG = QUEEN_VALUE_EG_DEFAULT;
	#else

		inline static value_t PAWN_VALUE_MG = PAWN_VALUE_MG_DEFAULT;
		inline static value_t PAWN_VALUE_EG = PAWN_VALUE_EG_DEFAULT;
		inline static value_t KNIGHT_VALUE_MG = KNIGHT_VALUE_MG_DEFAULT;
		inline static value_t KNIGHT_VALUE_EG = KNIGHT_VALUE_EG_DEFAULT;
		inline static value_t BISHOP_VALUE_MG = BISHOP_VALUE_MG_DEFAULT;
		inline static value_t BISHOP_VALUE_EG = BISHOP_VALUE_EG_DEFAULT;
		inline static value_t ROOK_VALUE_MG = ROOK_VALUE_MG_DEFAULT;
		inline static value_t ROOK_VALUE_EG = ROOK_VALUE_EG_DEFAULT;
		inline static value_t QUEEN_VALUE_MG = QUEEN_VALUE_MG_DEFAULT;
		inline static value_t QUEEN_VALUE_EG = QUEEN_VALUE_EG_DEFAULT;
	#endif

	
	private:
	#ifdef PARAM_OPTIMIZE_MATERIALBALANCE
		static void rebuildPieceValues();

		static inline void ensurePieceValuesInitialized() {
			(void)_staticInitializer;
		}

		struct InitStatics {
			InitStatics() {
				rebuildPieceValues();
			}
		};

		inline static InitStatics _staticInitializer;
	#endif

		EvalValue _materialValue;
	#ifndef PARAM_OPTIMIZE_MATERIALBALANCE
		array<EvalValue, PIECE_AMOUNT> pieceValues;
		array<value_t, PIECE_AMOUNT> absolutePieceValues;
	#else
		inline static array<EvalValue, PIECE_AMOUNT> pieceValues;
		inline static array<value_t, PIECE_AMOUNT> absolutePieceValues;
	#endif

		// Mean of the midgame and the endgame value of the piece, knight and bishop kept equal.
		// The pawn keeps 100, it nearly always collects bonuses on top of its material value
		static constexpr array<value_t, PIECE_AMOUNT> pieceValuesForMoveSorting =
		{ 0, 0, 100, -100, 340, -340, 340, -340, 565, -565, 1060, -1060 , MAX_VALUE, -MAX_VALUE };

	};

}


