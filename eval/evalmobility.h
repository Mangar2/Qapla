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
 * Implements functions to eval the mobility of chess pieces
 */

#ifndef __EVALMOBILITY_H
#define __EVALMOBILITY_H

#include <map>
#include "../movegenerator/movegenerator.h"
#include "evalresults.h"

using namespace ChessMoveGenerator;

namespace ChessEval {

	struct EvalMobilityValues {
		const static value_t MOBILITY_VALUE = 2;
		static constexpr value_t QUEEN_MOBILITY_MAP[30] = { -10, -10, -10, -5, 0, 2, 4, 5, 6, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 };
	};

	class EvalMobility {
	public:

		static value_t print(MoveGenerator& board, EvalResults& mobility) {
			value_t evalValue = eval(board, mobility);
			printf("Mobility:\n");
			printf("White Queen         : %ld\n", calcQueenMobility<WHITE>(board, mobility));
			printf("Black Queen         : %ld\n", -calcQueenMobility<BLACK>(board, mobility));
			printf("Mobility total      : %ld\n", evalValue);
			return evalValue;

		}

		/**
		 * Evaluates the mobility of all pieces (not pawns) on the board
		 */
		static value_t eval(MoveGenerator& board, EvalResults& mobility) {
			value_t evalResult = eval<WHITE>(board, mobility) - eval<BLACK>(board, mobility);
			return evalResult;
		}

		/**
		 * Gets a list of detailed evaluation information
		 */
		template <Piece COLOR>
		static map<string, value_t> factors(MoveGenerator& board, EvalResults& evalResults) {
			map<string, value_t> result;
			eval(board, evalResults);
			const Piece OPPONENT = COLOR == WHITE ? BLACK : WHITE;

			if (evalResults.midgameInPercent > 50) {
				result["Queen attack"] = evalResults.queenAttackFactor[COLOR];
			}
			return result;
		}



	private:

		/**
		 * Evaluate mobility for all pieces of one color
		 */
		template <Piece COLOR>
		static value_t eval(MoveGenerator& board, EvalResults& mobility) {
			value_t evalResult = 0;
			evalResult += calcQueenMobility<COLOR>(board, mobility);
			return evalResult;
		}


		/**
		 * Calculates the mobility value for Queens
		 */
		template <Piece COLOR>
		static value_t calcQueenMobility(MoveGenerator& board, EvalResults& mobility)
		{
			constexpr Piece OPPONENT = COLOR == WHITE ? BLACK : WHITE;
			bitBoard_t queens = board.getPieceBB(QUEEN + COLOR);
			mobility.queenAttack[COLOR] = 0;
			if (queens == 0) {
				return 0;
			}

			bitBoard_t occupied = board.getAllPiecesBB();
			bitBoard_t removeMask = ~mobility.pawnAttack[OPPONENT];

			Square departureSquare;
			value_t result = 0;
			while (queens)
			{
				departureSquare = BitBoardMasks::lsb(queens);
				queens &= queens - 1;
				bitBoard_t attack = Magics::genRookAttackMask(departureSquare, occupied & ~board.getPieceBB(ROOK + COLOR));
				attack |= Magics::genBishopAttackMask(departureSquare, occupied & ~board.getPieceBB(BISHOP + COLOR));
				mobility.queenAttack[COLOR] |= attack;
				attack &= removeMask;
				result += EvalMobilityValues::QUEEN_MOBILITY_MAP[BitBoardMasks::popCount(attack)];
			}
			return result;
		}

		
	};
}

#endif // __EVALMOBILITY_H