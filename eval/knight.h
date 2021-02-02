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
 * Implements a static class to evaluate knights 
 */

#ifndef __KNIGHT_H
#define __KNIGHT_H

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
			bitBoard_t knights = position.getPieceBB(KNIGHT + COLOR);

			while (knights)
			{
				const Square knightSquare = BitBoardMasks::lsb(knights);
				knights &= knights - 1;
			}

			if (PRINT) cout << colorToString(COLOR) << " bishops: " 
				<< std::right << std::setw(18) << value << endl;
			return value;
		}

		static inline EvalValue getFromIndexMap(uint32_t index) {
			return EvalValue(indexToValue[index * 2], indexToValue[index * 2 + 1]);
		}

		// Mobility Map for knights
		static constexpr value_t KNIGHT_MOBILITY_MAP[15][2] = { 
			{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 5, 5 }, { 10, 10 }, { 15, 15 }, { 20, 20 }, { 22, 22 },
			{ 24, 24 }, { 26, 26 }, { 28, 28 }, { 30, 30 }, { 30, 30 }, { 30, 30 }, { 30, 30 } 
		};
		
		static constexpr value_t indexToValue[4] = { 0, 0, 0, 0 };

#ifdef _TEST0 
#endif
#ifdef _TEST1 
#endif
#ifdef _TEST2 
#endif
#ifdef _T3 
#endif
#ifdef _T4 
#endif
#ifdef _T5
#endif


	};
}


#endif

