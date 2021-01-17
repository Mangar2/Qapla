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

#include "../basics/types.h"
#include "../movegenerator/movegenerator.h"
#include "evalresults.h"
#include "evalendgame.h"
#include "evalpawn.h"
#include "evalmobility.h"

namespace ChessEval {

	struct KingAttackValues {
		static constexpr array<value_t, 20> attackWeight = 
			{ 0,  -20, -40, -80, -150, -250, -350, -450, -500, -500, -500, -500, -500, -500, -500, -500, -500, -500, -500, -500 };
	};

	class KingAttack {

	public:

		/**
		 * Calculates an evaluation for the current board position
		 */
		static value_t eval(MoveGenerator& board, EvalResults& mobility) {
			computeAttacks<WHITE>(mobility);
			computeAttacks<BLACK>(mobility);
			return computeAttackCount<WHITE>(board.getKingSquare<WHITE>(), mobility) - 
				computeAttackCount<BLACK>(board.getKingSquare<BLACK>(), mobility);
		}

		/**
		 * Prints the evaluation results
		 */
		static value_t printEval(MoveGenerator& board, EvalResults& mobility) {
			value_t result = eval(board, mobility);
			printf("King attack\n");
			printf("White (pressure %ld) : %ld\n", mobility.kingPressureCount[WHITE], mobility.kingAttackValue[WHITE]);
			printf("Black (pressure %ld) : %ld\n", mobility.kingPressureCount[BLACK], -mobility.kingAttackValue[BLACK]);
			printf("King total           : %ld\n", result);
			return result;
		}


	private:

		/**
		 * Counts the undefended or under-defended attacks for squares near king
		 * King itself is not counted as defending piece
		 */
		template <Piece COLOR>
		inline static value_t computeAttackCount(Square kingSquare, EvalResults& mobility) {
			const Piece OPPONENT = COLOR == WHITE ? BLACK : WHITE;
			bitBoard_t attackArea = _kingAttackBB[COLOR][kingSquare];
			bitBoard_t kingAttacks = attackArea & mobility.undefendedAttack[OPPONENT];
			bitBoard_t kingDoubleAttacks = attackArea & mobility.undefendedDoubleAttack[OPPONENT];
			mobility.kingPressureCount[COLOR] =
				BitBoardMasks::popCount(kingAttacks) + BitBoardMasks::popCount(kingDoubleAttacks) * 3;
			mobility.kingAttackValue[COLOR] = KingAttackValues::attackWeight[mobility.kingPressureCount[COLOR]];
			return mobility.kingAttackValue[COLOR];
		}

		/**
		 * Computes attack bitboards for all pieces of one color - single and double attacks
		 */
		template <Piece COLOR>
		inline static void computeAttacks(EvalResults& mobility) {
			const Piece OPPONENT = COLOR == WHITE ? BLACK : WHITE;
			bitBoard_t doubleAttack = mobility.doubleRookAttack[COLOR] | mobility.doubleKnightAttack[COLOR];
			bitBoard_t attack = mobility.queenAttack[COLOR];
			doubleAttack |= attack & mobility.rookAttack[COLOR];
			attack |= mobility.rookAttack[COLOR];
			doubleAttack |= attack & mobility.bishopAttack[COLOR];
			attack |= mobility.bishopAttack[COLOR];
			doubleAttack |= attack & mobility.knightAttack[COLOR];
			attack |= mobility.knightAttack[COLOR];
			doubleAttack |= attack & mobility.pawnAttack[COLOR];
			attack |= mobility.pawnAttack[COLOR];
			mobility.piecesAttack[COLOR] = attack;
			mobility.piecesDoubleAttack[COLOR] = doubleAttack;
			mobility.undefendedAttack[COLOR] = (mobility.piecesAttack[COLOR] & ~mobility.piecesAttack[OPPONENT]) |
				(mobility.piecesDoubleAttack[COLOR] & ~mobility.piecesDoubleAttack[OPPONENT]);
			mobility.undefendedDoubleAttack[COLOR] = mobility.piecesDoubleAttack[COLOR] & ~mobility.piecesAttack[OPPONENT];
		}

		static struct InitStatics {
			InitStatics();
		} _staticConstructor;

		static array<bitBoard_t, BOARD_SIZE> _kingAttackBB[2];

	};
}

#endif // __KINGATTACK_H