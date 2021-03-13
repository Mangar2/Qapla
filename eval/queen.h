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
 * Implements evaluation for queens
 */

#ifndef __QUEEN_H
#define __QUEEN_H

#include <map>
#include "../movegenerator/movegenerator.h"
#include "evalresults.h"

using namespace ChessMoveGenerator;

namespace ChessEval {

	class Queen {
	public:
		/**
		 * Evaluates the evaluation value for queens
		 */
		template <bool PRINT>
		static EvalValue eval(MoveGenerator& position, EvalResults& results) {
			EvalValue evalResult = eval<WHITE, PRINT>(position, results) - eval<BLACK, PRINT>(position, results);
			return evalResult;
		}

	private:

		/**
		 * Calculates the evaluation value for Queens
		 */
		template <Piece COLOR, bool PRINT>
		static EvalValue eval(MoveGenerator& position, EvalResults& results)
		{
			constexpr Piece OPPONENT = COLOR == WHITE ? BLACK : WHITE;
			bitBoard_t queens = position.getPieceBB(QUEEN + COLOR);
			results.queenAttack[COLOR] = 0;
			if (queens == 0) {
				return 0;
			}

			bitBoard_t occupied = position.getAllPiecesBB();
			bitBoard_t removeMask = ~position.pawnAttack[OPPONENT] & ~position.getPiecesOfOneColorBB<COLOR>();

			EvalValue value = 0;
			while (queens)
			{
				const Square square = lsb(queens);
				queens &= queens - 1;
				value += calcMobility<COLOR, PRINT>(position, results, square, occupied, removeMask);
				if (isPinned<PRINT>(position.pinnedMask[COLOR], square)) {
					value += _pinned;
				}
				if (PRINT) cout << endl;
			}
			if (PRINT) cout 
				<< colorToString(COLOR) << " queens: "
				<< std::right << std::setw(19) << value << endl;
			return value;
		}

		/**
		 * Calculates the mobility of a queen
		 */
		template<Piece COLOR, bool PRINT>
		static value_t calcMobility(MoveGenerator& position,
			EvalResults& results, Square square, bitBoard_t occupiedBB, bitBoard_t removeBB)
		{
			bitBoard_t attackBB = Magics::genRookAttackMask(square, occupiedBB & ~position.getPieceBB(ROOK + COLOR));
			attackBB |= Magics::genBishopAttackMask(square, occupiedBB & ~position.getPieceBB(BISHOP + COLOR));
			results.piecesDoubleAttack[COLOR] |= results.piecesAttack[COLOR] & attackBB;
			results.piecesAttack[COLOR] |= attackBB;

			results.queenAttack[COLOR] |= attackBB;
			attackBB &= removeBB;
			const value_t value = QUEEN_MOBILITY_MAP[popCount(attackBB)];

			if (PRINT) cout << colorToString(COLOR)
				<< " queen (" << squareToString(square) << ") mobility: "
				<< std::right << std::setw(7) << value;
			return value;
		}

		/**
		 * Returns true, if the queen is pinned
		 */
		template<bool PRINT>
		static inline bool isPinned(bitBoard_t pinnedBB, Square square) {
			bool result = (pinnedBB & (1ULL << square)) != 0;
			if (PRINT && result) cout << " <pin>";
			return result;
		}

		static constexpr value_t _pinned[2] = { 0, 0 };

		static constexpr value_t QUEEN_MOBILITY_MAP[30] = { 
			-10, -10, -10, -5, 0, 2, 4, 5, 6, 10, 10, 10, 10, 10, 10, 
			10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 
		};

	};
}

#endif // __QUEEN_H