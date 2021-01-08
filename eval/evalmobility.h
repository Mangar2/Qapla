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

#include "../movegenerator/movegenerator.h"

using namespace ChessMoveGenerator;

namespace ChessEval {
	class EvalMobility {

	public:
		EvalMobility(MoveGenerator& board) {
			whitePawnAttack = board.pawnAttackMask[WHITE];
			blackPawnAttack = board.pawnAttackMask[BLACK];

			occupied = board.getAllPiecesBB();
			queens = board.getPieceBB(WHITE_QUEEN) + board.getPieceBB(BLACK_QUEEN);
		}


		value_t print(MoveGenerator& board) {
			printf("Mobility:\n");
			printf("White Knight        : %ld\n", calcWhiteKnightMobility(board));
			printf("Black Knight        : %ld\n", calcBlackKnightMobility(board));
			printf("White Bishop        : %ld\n", calcWhiteBishopMobility(board));
			printf("Black Bishop        : %ld\n", calcBlackBishopMobility(board));
			printf("White Rook          : %ld\n", calcWhiteRookMobility(board));
			printf("Black Rook          : %ld\n", calcBlackRookMobility(board));
			printf("White Queen         : %ld\n", calcWhiteQueenMobility(board));
			printf("Black Queen         : %ld\n", calcBlackQueenMobility(board));
			printf("Mobility total      : %ld\n", eval(board));
			return eval(board);

		}

		/**
		 * Evaluates the mobility of all pieces (not pawns) on the board
		 */
		value_t eval(MoveGenerator& board) {
			value_t evalResult = 0;
			evalResult += calcWhiteKnightMobility(board);
			evalResult += calcBlackKnightMobility(board);
			evalResult += calcWhiteBishopMobility(board);
			evalResult += calcBlackBishopMobility(board);
			evalResult += calcWhiteRookMobility(board);
			evalResult += calcBlackRookMobility(board);
			evalResult += calcWhiteQueenMobility(board);
			evalResult += calcBlackQueenMobility(board);
			return evalResult;
		}


	private:

		value_t calcBishopMobility(bitBoard_t bishops, bitBoard_t occupied, bitBoard_t removeMask) const
		{
			Square departureSquare;
			value_t result = 0;
			while (bishops)
			{
				departureSquare = BitBoardMasks::lsb(bishops);
				bishops &= bishops - 1;
				bitBoard_t attack = Magics::genBishopAttackMask(departureSquare, occupied);
				attack &= removeMask;
				result += BISHOP_MOBILITY_MAP[BitBoardMasks::popCount(attack)];

			}
			return result;
		}

		value_t calcRookMobility(bitBoard_t rooks, bitBoard_t occupied, bitBoard_t removeMask) const
		{
			Square departureSquare;
			value_t result = 0;
			while (rooks)
			{
				departureSquare = BitBoardMasks::lsb(rooks);
				rooks &= rooks - 1;
				bitBoard_t attack = Magics::genRookAttackMask(departureSquare, occupied);
				attack &= removeMask;
				result += ROOK_MOBILITY_MAP[BitBoardMasks::popCount(attack)];

			}
			return result;
		}

		value_t calcQueenMobility(bitBoard_t queens, bitBoard_t occupied, bitBoard_t removeMask) const
		{
			Square departureSquare;
			value_t result = 0;
			while (queens)
			{
				departureSquare = BitBoardMasks::lsb(queens);
				queens &= queens - 1;
				bitBoard_t attack = Magics::genQueenAttackMask(departureSquare, occupied);
				attack &= removeMask;
				result += QUEEN_MOBILITY_MAP[BitBoardMasks::popCount(attack)];

			}
			return result;
		}

		value_t calcKnightMobility(bitBoard_t knights, bitBoard_t occupied, bitBoard_t removeMask) const
		{
			Square departureSquare;
			value_t result = 0;
			while (knights)
			{
				departureSquare = BitBoardMasks::lsb(knights);
				knights &= knights - 1;
				bitBoard_t attack = BitBoardMasks::knightMoves[departureSquare];
				attack &= removeMask;
				result += KNIGHT_MOBILITY_MAP[BitBoardMasks::popCountForSparcelyPopulatedBitBoards(attack)];

			}
			return result;
		}


		value_t calcWhiteBishopMobility(MoveGenerator& board) {
			value_t value = 0;
			if (board.getPieceBB(WHITE_BISHOP) != 0) {
				bitBoard_t passThrough = queens + board.getPieceBB(BLACK_ROOK);
				value = calcBishopMobility(board.getPieceBB(WHITE_BISHOP), (occupied & ~passThrough), (~occupied | passThrough) & ~blackPawnAttack);
			}
			return value;
		}

		value_t calcBlackBishopMobility(MoveGenerator& board) {
			value_t value = 0;
			if (board.getPieceBB(BLACK_BISHOP) != 0) {
				bitBoard_t passThrough = queens + board.getPieceBB(WHITE_ROOK);
				value = calcBishopMobility(board.getPieceBB(BLACK_BISHOP), (occupied & ~passThrough), (~occupied | passThrough) & ~whitePawnAttack);
			}
			return -value;
		}

		value_t calcWhiteRookMobility(MoveGenerator& board) {
			value_t value = 0;
			if (board.getPieceBB(WHITE_ROOK) != 0) {
				bitBoard_t passThrough = queens + board.getPieceBB(WHITE_ROOK);
				value = calcRookMobility(board.getPieceBB(WHITE_ROOK), (occupied & ~passThrough), (~occupied | passThrough) & ~blackPawnAttack);
			}
			return value;
		}

		value_t calcBlackRookMobility(MoveGenerator& board) {
			value_t value = 0;
			if (board.getPieceBB(BLACK_ROOK) != 0) {
				bitBoard_t passThrough = queens + board.getPieceBB(BLACK_ROOK);
				value = calcRookMobility(board.getPieceBB(BLACK_ROOK), (occupied & ~passThrough), (~occupied | passThrough) & ~whitePawnAttack);
			}
			return -value;
		}

		value_t calcWhiteQueenMobility(MoveGenerator& board) {
			value_t value = 0;
			if (board.getPieceBB(WHITE_QUEEN) != 0) {
				bitBoard_t passThrough = board.getPieceBB(WHITE_ROOK) + board.getPieceBB(WHITE_BISHOP);
				value = calcQueenMobility(board.getPieceBB(WHITE_QUEEN), (occupied & ~passThrough), (~occupied | passThrough) & ~blackPawnAttack);
			}
			return value;
		}

		value_t calcBlackQueenMobility(MoveGenerator& board) {
			value_t value = 0;
			if (board.getPieceBB(BLACK_QUEEN) != 0) {
				bitBoard_t passThrough = board.getPieceBB(BLACK_ROOK) + board.getPieceBB(BLACK_BISHOP);
				value = calcQueenMobility(board.getPieceBB(BLACK_QUEEN), (occupied & ~passThrough), (~occupied | passThrough) & ~whitePawnAttack);
			}
			return -value;
		}

		value_t calcWhiteKnightMobility(MoveGenerator& board) {
			value_t value = calcKnightMobility(board.getPieceBB(WHITE_KNIGHT), occupied, ~occupied & ~blackPawnAttack);
			return value;
		}

		value_t calcBlackKnightMobility(MoveGenerator& board) {
			value_t value = calcKnightMobility(board.getPieceBB(BLACK_KNIGHT), occupied, ~occupied & ~whitePawnAttack);
			return -value;
		}


		bitBoard_t blackPawnAttack;
		bitBoard_t whitePawnAttack;
		bitBoard_t occupied;
		bitBoard_t queens;
		const static value_t MOBILITY_VALUE = 2;
		static constexpr value_t QUEEN_MOBILITY_MAP[30] = { -10, -10, -10, -5, 0, 2, 4, 5, 6, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 };
		static constexpr value_t ROOK_MOBILITY_MAP[15] = { 0, 0, 0, 0, 0, 10, 15, 20, 25, 30, 30, 30, 30, 30, 30 };
		static constexpr value_t BISHOP_MOBILITY_MAP[15] = { 0, 0, 0, 5, 10, 15, 20, 22, 24, 26, 28, 30, 30, 30, 30 };
		static constexpr value_t KNIGHT_MOBILITY_MAP[9] = { -30, -20, -10, 0, 10, 20, 25, 25, 25 };

	};

}