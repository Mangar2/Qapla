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
 * Implements a static class to evaluate bishops 
 */

#ifndef __BISHOP_H
#define __BISHOP_H

#include <cstdint>
#include "../basics/types.h"
#include "../movegenerator/bitboardmasks.h"
#include "../movegenerator/movegenerator.h"
#include "../basics/evalvalue.h"
#include "evalresults.h"

using namespace ChessBasics;
using namespace ChessMoveGenerator;

namespace ChessEval {
	class Bishop {
	public:
		template <bool PRINT>
		static EvalValue eval(const MoveGenerator& position, EvalResults& results) {
			return eval<WHITE, PRINT>(position, results) - eval<BLACK, PRINT>(position, results);
		}
	private:

		/**
		 * Evaluates Bishops
		 */
		template<Piece COLOR, bool PRINT>
		static EvalValue eval(const MoveGenerator& position, EvalResults& results) {
			EvalValue value;
			constexpr Piece OPPONENT = COLOR == WHITE ? BLACK : WHITE;
			bitBoard_t bishops = position.getPieceBB(BISHOP + COLOR);
			results.bishopAttack[COLOR] = 0;
			if (bishops == 0) {
				return 0;
			}
			uint32_t index = 0;
			index += hasDoubleBishop(bishops);
			value = getFromIndexMap(index);

			bitBoard_t passThroughBB = results.queensBB | position.getPieceBB(ROOK + OPPONENT);
			bitBoard_t occupiedBB = position.getAllPiecesBB();
			bitBoard_t removeMask = (~position.getPiecesOfOneColorBB<COLOR>() | passThroughBB)
				& ~results.pawnAttack[OPPONENT];
			occupiedBB &= ~passThroughBB;

			while (bishops)
			{
				const Square bishopSquare = BitBoardMasks::lsb(bishops);
				bishops &= bishops - 1;
				value += calcMobility<COLOR, PRINT>(results, bishopSquare, occupiedBB, removeMask);
			}

			if (PRINT) cout << colorToString(COLOR) << " bishops: " 
				<< std::right << std::setw(18) << value << endl;
			return value;
		}

		/**
		 * Checks, if we have at least two bishops on different colored fields
		 */
		static bool hasDoubleBishop(bitBoard_t bishops) {
			return ((bishops & WHITE_FIELDS) != 0 && (bishops & (~WHITE_FIELDS)) != 0);
		}

		/**
		 * Calculates the mobility of a bishop
		 */
		template<Piece COLOR, bool PRINT>
		static EvalValue calcMobility(
			EvalResults& results, Square square, bitBoard_t occupiedBB, bitBoard_t removeBB)
		{
			bitBoard_t attackBB = Magics::genBishopAttackMask(square, occupiedBB);
			results.bishopAttack[COLOR] |= attackBB;
			attackBB &= removeBB;
			EvalValue value = BISHOP_MOBILITY_MAP[BitBoardMasks::popCount(attackBB)];
			if (PRINT) cout << colorToString(COLOR)
				<< " bishop (" << squareToString(square) << ") mobility: "
				<< std::right << std::setw(5) << value << endl;
			return value;
		}


		static inline EvalValue getFromIndexMap(uint32_t index) {
			return EvalValue(indexToValue[index * 2], indexToValue[index * 2 + 1]);
		}

		static constexpr value_t doubleBishop[2] = { 0, 0 };
		static const bitBoard_t WHITE_FIELDS = 0x55AA55AA55AA55AA;

		// Mobility Map for bishops
		static constexpr value_t BISHOP_MOBILITY_MAP[15][2] = { 
			{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 5, 5 }, { 10, 10 }, { 15, 15 }, { 20, 20 }, { 22, 22 },
			{ 24, 24 }, { 26, 26 }, { 28, 28 }, { 30, 30 }, { 30, 30 }, { 30, 30 }, { 30, 30 } 
		};

#ifdef _TEST0 
		static constexpr value_t doubleBishop[2] = { 50, 0 };
#endif
#ifdef _TEST1 
		static constexpr value_t doubleBishop[2] = { 50, 25 };
#endif
#ifdef _TEST2 
		static constexpr value_t doubleBishop[2] = { 40, 25 };
#endif
#ifdef _T3 
		static constexpr value_t doubleBishop[2] = { 60, 25 };
#endif
#ifdef _T4 
		static constexpr value_t doubleBishop[2] = { 50, 30 };
#endif
#ifdef _T5
		static constexpr value_t doubleBishop[2] = { 50, 10 };
#endif

		static constexpr value_t indexToValue[4] = { 0, 0, doubleBishop[0], doubleBishop[1] };

	};
}


#endif

