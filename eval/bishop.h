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
			if (PRINT && index > 0) cout << colorToString(COLOR) << " double bishop: "
				<< std::right << std::setw(12) << value << endl;

			bitBoard_t passThroughBB = position.getPieceBB(QUEEN + OPPONENT) | position.getPieceBB(ROOK + OPPONENT);
			bitBoard_t occupiedBB = position.getAllPiecesBB();
			bitBoard_t removeMask = (~position.getPiecesOfOneColorBB<COLOR>() | passThroughBB)
				& ~results.pawnAttack[OPPONENT];
			occupiedBB &= ~passThroughBB;

			while (bishops)
			{
				const Square bishopSquare = BitBoardMasks::lsb(bishops);
				bishops &= bishops - 1;
				value += calcMobility<COLOR, PRINT>(results, bishopSquare, occupiedBB, removeMask);
				if (isPinned(position.pinnedMask[COLOR], bishopSquare)) {
					value += EvalValue(_pinned);
					if (PRINT) cout << "<pin>";
				}
				if (PRINT) cout << endl;
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
			const EvalValue value = BISHOP_MOBILITY_MAP[BitBoardMasks::popCount(attackBB)];
			if (PRINT) cout << colorToString(COLOR)
				<< " bishop (" << squareToString(square) << ") mobility: "
				<< std::right << std::setw(5) << value;
			return value;
		}


		static inline EvalValue getFromIndexMap(uint32_t index) {
			return EvalValue(_indexToValue[index * 2], _indexToValue[index * 2 + 1]);
		}

		/**
		 * Returns true, if the bishop is pinned
		 */
		static inline bool isPinned(bitBoard_t pinnedBB, Square square) {
			return (pinnedBB & (1ULL << square)) != 0;
		}

		/**
		 * Bitmap to white fields, to check if bishops are on opposit colors
		 */
		static const bitBoard_t WHITE_FIELDS = 0x55AA55AA55AA55AA;

		/**
		 * Additional value for two bishops on different colors
		 */
		static constexpr value_t _doubleBishop[2] = { 20, 10 };
		static constexpr value_t _pinned[2] = { 0, 0 };
		static constexpr value_t _indexToValue[4] = { 0, 0, _doubleBishop[0], _doubleBishop[1] };

		/**
		 * Mobility of a bishop. Having negative values for 0 or 1 fields to move to did not bring ELO
		 * Still I kept it.
		 */
		static constexpr value_t BISHOP_MOBILITY_MAP[15][2] = {
			{ -15, -25 }, { -10, -15 }, { 0, 0 }, { 5, 5 }, { 10, 10 }, { 15, 15 }, { 20, 20 }, { 22, 22 },
			{ 24, 24 }, { 26, 26 }, { 28, 28 }, { 30, 30 }, { 30, 30 }, { 30, 30 }, { 30, 30 }
		};

	};
}


#endif

