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
		static constexpr value_t QUEEN_MOBILITY_MAP[30] = { -50, -30, -20, -10, -5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
		static constexpr value_t ROOK_MOBILITY_MAP[15] = { -15, -15, -15, -15, -5, 0, 0, 0, 0, 0, 0, 0, 10, 20, 30 };
		static constexpr value_t BISHOP_MOBILITY_MAP[15] = { -10, -10, -5, -2, 0, 0, 0, 5, 10, 15, 20, 25, 30, 35, 40 };
		static constexpr value_t KNIGHT_MOBILITY_MAP[9] = { -30, -30, -10, -5, 0, 0, 0, 5, 10 };
	};

	class EvalMobility {
	public:

		static value_t print(MoveGenerator& board, EvalResults& mobility) {
			value_t evalValue = eval(board, mobility);
			printf("Mobility:\n");
			printf("White Knight        : %ld\n", calcKnightMobility<WHITE>(board, mobility));
			printf("Black Knight        : %ld\n", -calcKnightMobility<BLACK>(board, mobility));
			printf("White Bishop        : %ld\n", calcBishopMobility<WHITE>(board, mobility));
			printf("Black Bishop        : %ld\n", -calcBishopMobility<BLACK>(board, mobility));
			printf("White Rook          : %ld\n", calcRookMobility<WHITE>(board, mobility));
			printf("Black Rook          : %ld\n", -calcRookMobility<BLACK>(board, mobility));
			printf("White Queen         : %ld\n", calcQueenMobility<WHITE>(board, mobility));
			printf("Black Queen         : %ld\n", -calcQueenMobility<BLACK>(board, mobility));
			printf("Mobility total      : %ld\n", evalValue);
			return evalValue;

		}

		/**
		 * Evaluates the mobility of all pieces (not pawns) on the board
		 */
		static value_t eval(MoveGenerator& board, EvalResults& mobility) {
			init(board, mobility);

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

			if (evalResults.midgameInPercent > 50  && abs(evalResults.materialValue) < 100) {
				result["Queen attack"] = evalResults.queenAttackFactor[COLOR];
				result["Rook attack"] = evalResults.rookAttackFactor[COLOR];
				result["Bishop attack"] = evalResults.bishopAttackFactor[COLOR];
				result["Knight attack"] = evalResults.knightAttackFactor[COLOR];
			}
			return result;
		}



	private:

		static inline void init(MoveGenerator& board, EvalResults& mobility) {
			mobility.queensBB = board.getPieceBB(WHITE_QUEEN) | board.getPieceBB(BLACK_QUEEN);
		}

		/**
		 * Evaluate mobility for all pieces of one color
		 */
		template <Piece COLOR>
		static value_t eval(MoveGenerator& board, EvalResults& mobility) {
			value_t evalResult = 0;
			evalResult += calcKnightMobility<COLOR>(board, mobility);
			evalResult += calcBishopMobility<COLOR>(board, mobility);
			evalResult += calcRookMobility<COLOR>(board, mobility);
			evalResult += calcQueenMobility<COLOR>(board, mobility);
			return evalResult;
		}

		/**
		 * Calculates the mobility values of bishops
		 */
		template <Piece COLOR>
		static value_t calcBishopMobility(MoveGenerator& board, EvalResults& mobility)
		{
			constexpr Piece OPPONENT = COLOR == WHITE ? BLACK : WHITE;
			bitBoard_t bishops = board.getPieceBB(BISHOP + COLOR);
			mobility.bishopAttack[COLOR] = 0;
			if (bishops == 0) {
				return 0;
			}

			bitBoard_t passThrough = mobility.queensBB | board.getPieceBB(ROOK + OPPONENT);
			bitBoard_t occupied = board.getAllPiecesBB();
			bitBoard_t removeMask = (~board.getPiecesOfOneColorBB<COLOR>() | passThrough) 
				& ~mobility.pawnAttack[OPPONENT];
			occupied &= ~passThrough;

			Square departureSquare;
			value_t result = 0;
			while (bishops)
			{
				departureSquare = BitBoardMasks::lsb(bishops);
				bishops &= bishops - 1;
				bitBoard_t attack = Magics::genBishopAttackMask(departureSquare, occupied);
				mobility.bishopAttack[COLOR] |= attack;
				attack &= removeMask;
				result += EvalMobilityValues::BISHOP_MOBILITY_MAP[BitBoardMasks::popCount(attack)];
				// mobility.bishopAttackFactor[COLOR] = BitBoardMasks::popCount(attack);

			}
			return result;
		}

		/**
		 * Calculates the mobility values of rooks
		 */
		template <Piece COLOR>
		static value_t calcRookMobility(MoveGenerator& board, EvalResults& mobility)
		{
			constexpr Piece OPPONENT = COLOR == WHITE ? BLACK : WHITE;
			bitBoard_t rooks = board.getPieceBB(ROOK + COLOR);
			mobility.rookAttack[COLOR] = 0;
			mobility.doubleRookAttack[COLOR] = 0;
			if (rooks == 0) {
				return 0;
			}

			bitBoard_t passThrough = mobility.queensBB | rooks;
			bitBoard_t occupied = board.getAllPiecesBB();
			bitBoard_t removeMask = (~board.getPiecesOfOneColorBB<COLOR>() | passThrough) & 
				~mobility.pawnAttack[OPPONENT];
			occupied &= ~passThrough;

			Square departureSquare;
			value_t result = 0;
			while (rooks)
			{
				departureSquare = BitBoardMasks::lsb(rooks);
				rooks &= rooks - 1;
				bitBoard_t attack = Magics::genRookAttackMask(departureSquare, occupied);
				mobility.doubleRookAttack[COLOR] |= mobility.rookAttack[COLOR] & attack;
				mobility.rookAttack[COLOR] |= attack;
				attack &= removeMask;
				result += EvalMobilityValues::ROOK_MOBILITY_MAP[BitBoardMasks::popCount(attack)];
				// mobility.rookAttackFactor[COLOR] = BitBoardMasks::popCount(attack);
			}
			return result;
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
				// mobility.queenAttackFactor[COLOR] = BitBoardMasks::popCount(attack);
			}
			return result;
		}

		/**
		 * Calculates the mobility value for Knights
		 */
		template <Piece COLOR>
		static value_t calcKnightMobility(MoveGenerator& board, EvalResults& mobility)
		{
			constexpr Piece OPPONENT = COLOR == WHITE ? BLACK : WHITE;
			mobility.knightAttack[COLOR] = 0;
			mobility.doubleKnightAttack[COLOR] = 0;

			bitBoard_t knights = board.getPieceBB(KNIGHT + COLOR);
			bitBoard_t no_destination = ~board.getPiecesOfOneColorBB<COLOR>() & ~mobility.pawnAttack[OPPONENT];

			Square departureSquare;
			value_t result = 0;
			while (knights)
			{
				departureSquare = BitBoardMasks::lsb(knights);
				knights &= knights - 1;
				bitBoard_t attack = BitBoardMasks::knightMoves[departureSquare];
				mobility.doubleKnightAttack[COLOR] |= mobility.knightAttack[COLOR] & attack;
				mobility.knightAttack[COLOR] |= attack;
				attack &= no_destination;
				result += EvalMobilityValues::KNIGHT_MOBILITY_MAP[BitBoardMasks::popCountForSparcelyPopulatedBitBoards(attack)];
				// mobility.knightAttackFactor[COLOR] = BitBoardMasks::popCount(attack);
			}
			return result;
		}
	};
}

#endif // __EVALMOBILITY_H