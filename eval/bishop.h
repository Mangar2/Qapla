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
			uint32_t index = 0;
			index += hasDoubleBishop(bishops);
			value = getFromIndexMap(index);
			/*
			while (bishops)
			{
				uint16_t index = 0;
				const Square bishopSquare = BitBoardMasks::lsb(bishops);
				bishops &= bishops - 1;
			}
			*/
			if (PRINT) cout << colorToString(COLOR) << " bishop: " 
				<< std::right << std::setw(19) << value << endl;
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
		/*
		template<Piece COLOR>
		static EvalValue calcMobility(Square square, bitBoard_t occupiedBB, bitBoard_t removeBB, EvalResults& results) {
			bitBoard_t attackBB = Magics::genRookAttackMask(square, occupiedBB);
			mobility.doubleRookAttack[COLOR] |= mobility.rookAttack[COLOR] & attackBB;
			mobility.rookAttack[COLOR] |= attackBB;
			attackBB &= removeMaskBB;
			return EvalMobilityValues::ROOK_MOBILITY_MAP[BitBoardMasks::popCount(attackBB)];
		}


		*/


		static inline EvalValue getFromIndexMap(uint32_t index) {
			return EvalValue(indexToValue[index * 2], indexToValue[index * 2 + 1]);
		}

		static constexpr value_t doubleBishop[2] = { 0, 0 };
		static const bitBoard_t WHITE_FIELDS = 0x55AA55AA55AA55AA;

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

