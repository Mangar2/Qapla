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
 * Implements the evaluation of a king attack based on attack bitboards
 * The king attack area is two rows to north, one row to south and one file east and west,
 * thus a rectangle of 3x4 fields
 */

#ifndef __KINGATTACK_H
#define __KINGATTACK_H

 // Idee 1: Evaluierung in der Ruhesuche ohne positionelle Details
 // Idee 2: Zugsortierung nach lookup Tabelle aus reduziertem Board-Hash
 // Idee 3: Beweglichkeit einer Figur aus der Suche evaluieren. Speichern, wie oft eine Figur von einem Startpunkt erfolgreich bewegt wurde.

#include <map>
#include "../basics/types.h"
#include "../movegenerator/movegenerator.h"
#include "evalresults.h"
#include "evalendgame.h"
#include "evalpawn.h"
#include "evalmobility.h"

using namespace std;

namespace ChessEval {

	struct KingAttackValues {
		static const uint32_t MAX_WEIGHT_COUNT = 19;
		static constexpr array<value_t, MAX_WEIGHT_COUNT + 1> attackWeight =
			{ 0,  -20, -40, -60, -90, -130, -180, -240, -310, -490, -500, -500, -500, -500, -500, -500, -500, -500, -500, -500 };
	};

	class KingAttack {

	public:

		static void printBB(bitBoard_t bb) {
			uint32_t lineBreak = 8;
			for (uint64_t i = 1ULL << 63; i > 0; i /= 2) {
				cout << ((bb & i) ? "X " : ". ");
				lineBreak--;
				if (lineBreak == 0) {
					cout << endl;
					lineBreak = 8;
				}
			}
			cout << endl;
		}

		/**
		 * Calculates an evaluation for the current board position
		 */
		static value_t eval(MoveGenerator& board, EvalResults& evalResults) {
			computeAttacks<WHITE>(evalResults);
			computeAttacks<BLACK>(evalResults);
			computeUndefendedAttacks<WHITE>(evalResults);
			computeUndefendedAttacks<BLACK>(evalResults);
			value_t result = computeAttackValue<WHITE>(board.getKingSquare<WHITE>(), evalResults) -
				computeAttackValue<BLACK>(board.getKingSquare<BLACK>(), evalResults);
			return result;
		}

		/**
		 * Prints the evaluation results
		 */
		static void print(EvalResults& evalResults) {
			printf("King attack\n");
			printf("White (pressure %1ld)  : %ld\n", evalResults.kingPressureCount[WHITE], evalResults.kingAttackValue[WHITE]);
			printf("Black (pressure %1ld)  : %ld\n", evalResults.kingPressureCount[BLACK], -evalResults.kingAttackValue[BLACK]);
		}

		/**
		 * Gets a list of detailed evaluation information
		 */
		template <Piece COLOR>
		static map<string, value_t> factors(MoveGenerator& board, EvalResults& evalResults) {
			map<string, value_t> result;
			eval(board, evalResults);
			const Piece OPPONENT = COLOR == WHITE ? BLACK : WHITE;
			Square kingSquare = board.getKingSquare<OPPONENT>();
			bitBoard_t attackArea = _kingAttackBB[OPPONENT][kingSquare];
			bitBoard_t kingAttacks = attackArea & evalResults.undefendedAttack[COLOR];
			bitBoard_t kingDoubleAttacks = attackArea & evalResults.undefendedDoubleAttack[COLOR];
			if (evalResults.midgameInPercent > 50) {
				result["King pawn defended attack"] =
					BitBoardMasks::popCount(evalResults.piecesAttack[COLOR] &
						evalResults.pawnAttack[OPPONENT] & attackArea);
				result["King defended attack"] =
					BitBoardMasks::popCount(evalResults.piecesAttack[COLOR] & 
						evalResults.piecesAttack[OPPONENT] & attackArea);
				result["King non pawn defended attack"] =
					BitBoardMasks::popCount(evalResults.piecesAttack[COLOR] &
						evalResults.piecesAttack[OPPONENT] & ~evalResults.pawnAttack[OPPONENT] & attackArea);
				result["King single undefended attack"] = 
					BitBoardMasks::popCount(kingAttacks & ~kingDoubleAttacks);
				result["King single attack without pawn defended fields"] =
					BitBoardMasks::popCount(evalResults.piecesAttack[COLOR] &
						~evalResults.pawnAttack[OPPONENT] & attackArea);
				result["King double attack defended twice"] =
					BitBoardMasks::popCount(evalResults.piecesDoubleAttack[COLOR] & 
						evalResults.piecesDoubleAttack[OPPONENT] & attackArea);
				result["King double attack defended once"] =
					BitBoardMasks::popCount(evalResults.piecesDoubleAttack[COLOR] & 
						evalResults.piecesAttack[OPPONENT] & attackArea);
				result["King double attack defended by pawns"] =
					BitBoardMasks::popCount(evalResults.piecesDoubleAttack[COLOR] &
						evalResults.pawnAttack[OPPONENT] & attackArea);
				result["King double undefended attack"] =
					BitBoardMasks::popCount(kingDoubleAttacks);
				result["King single no pawn defence + king protected double + 2 * king unprotected double"] =
					BitBoardMasks::popCount(evalResults.piecesAttack[COLOR] &
						~evalResults.pawnAttack[OPPONENT] & attackArea) +
					BitBoardMasks::popCount(evalResults.piecesDoubleAttack[COLOR] & attackArea &
						evalResults.piecesAttack[OPPONENT]) +
					BitBoardMasks::popCount(evalResults.piecesDoubleAttack[COLOR] & attackArea & 
						~evalResults.piecesAttack[OPPONENT]) * 2;

				result["King pressure"] = evalResults.kingPressureCount[OPPONENT];
			}
			return result;
		}


	private:

		/**
		 * Counts the undefended or under-defended attacks for squares near king
		 * King itself is not counted as defending piece
		 */
		template <Piece COLOR>
		inline static value_t computeAttackValue(Square kingSquare, EvalResults& evalResults) {
			const Piece OPPONENT = COLOR == WHITE ? BLACK : WHITE;
			bitBoard_t attackArea = _kingAttackBB[COLOR][kingSquare];
			bitBoard_t kingAttacks = attackArea & evalResults.undefendedAttack[OPPONENT];
			bitBoard_t kingDoubleAttacks = attackArea & evalResults.undefendedDoubleAttack[OPPONENT];
			evalResults.kingPressureCount[COLOR] =
				BitBoardMasks::popCount(kingAttacks) + BitBoardMasks::popCount(kingDoubleAttacks) * 2;
			if (evalResults.kingPressureCount[COLOR] > KingAttackValues::MAX_WEIGHT_COUNT) {
				evalResults.kingPressureCount[COLOR] = KingAttackValues::MAX_WEIGHT_COUNT;
			}
			evalResults.kingAttackValue[COLOR] = 
				(KingAttackValues::attackWeight[evalResults.kingPressureCount[COLOR]] * evalResults.midgameInPercent) / 100;
			return evalResults.kingAttackValue[COLOR];
		}

		/**
		 * Computes attack bitboards for all pieces of one color - single and double attacks
		 */
		template <Piece COLOR>
		inline static void computeAttacks(EvalResults& evalResults) {
			const Piece OPPONENT = COLOR == WHITE ? BLACK : WHITE;
			bitBoard_t doubleAttack = evalResults.doubleRookAttack[COLOR] | evalResults.doubleKnightAttack[COLOR];
			bitBoard_t attack = evalResults.queenAttack[COLOR];
			doubleAttack |= attack & evalResults.rookAttack[COLOR];
			attack |= evalResults.rookAttack[COLOR];
			doubleAttack |= attack & evalResults.bishopAttack[COLOR];
			attack |= evalResults.bishopAttack[COLOR];
			doubleAttack |= attack & evalResults.knightAttack[COLOR];
			attack |= evalResults.knightAttack[COLOR];
			doubleAttack |= attack & evalResults.pawnAttack[COLOR];
			attack |= evalResults.pawnAttack[COLOR];
			evalResults.piecesAttack[COLOR] = attack;
			evalResults.piecesDoubleAttack[COLOR] = doubleAttack;
		}

		template <Piece COLOR>
		inline static void computeUndefendedAttacks(EvalResults& evalResults) {
			const Piece OPPONENT = COLOR == WHITE ? BLACK : WHITE;
			evalResults.undefendedAttack[COLOR] = (evalResults.piecesAttack[COLOR] & ~evalResults.piecesAttack[OPPONENT]) |
				(evalResults.piecesDoubleAttack[COLOR] & ~evalResults.piecesDoubleAttack[OPPONENT]);
			evalResults.undefendedDoubleAttack[COLOR] = evalResults.piecesDoubleAttack[COLOR] & ~evalResults.piecesAttack[OPPONENT];
		}

		static struct InitStatics {
			InitStatics();
		} _staticConstructor;

		static array<bitBoard_t, BOARD_SIZE> _kingAttackBB[2];

	};
}

#endif // __KINGATTACK_H