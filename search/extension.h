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
 * Implements functions to calculate the search extensions
 */

#include "../movegenerator/bitboardmasks.h"
#include "../movegenerator/movegenerator.h"
#include "SearchParameter.h"

using namespace QaplaMoveGenerator;

namespace QaplaSearch {
	class Extension {
	public:

		/**
		 * Computes extensions
		 */
		static ply_t calculateExtension(MoveGenerator& board, Move move, int32_t remainingSearchDepth) {
			ply_t extension = 0;
			if (SearchParameter::DO_CHECK_EXTENSIONS && board.isInCheck()) {
				extension = 1;
			}
			else if (SearchParameter::DO_PASSED_PAWN_EXTENSIONS && isChallengingPassedPawnMove(board, move)) {
				static uint32_t passedPawnExtensions = 0;
				passedPawnExtensions++; if (passedPawnExtensions % 1000 == 0) { printf("%ld\n", passedPawnExtensions); }
				extension = 1;
			}
			return extension;
		}

		/**
		 * Computes a bit board mask containing every field where passed pawns may be located
		 */
		static bitBoard_t computeWhitePassedPawnCheckMask(Square square) {
			bitBoard_t result = 1ULL << (square + NORTH);
			result |= (result & ~BitBoardMasks::FILE_H_BITMASK) << 1;
			result |= (result & ~BitBoardMasks::FILE_A_BITMASK) >> 1;
			result |= result << NORTH;
			result |= result << NORTH * 3;
			return result;
		}

		/**
		 * Computes a bit board mask containing every field where passed pawns may be located
		 */
		static bitBoard_t computeBlackPassedPawnCheckMask(Square square) {
			bitBoard_t result = 1ULL << (square + SOUTH);
			result |= (result & ~BitBoardMasks::FILE_H_BITMASK) << 1;
			result |= (result & ~BitBoardMasks::FILE_A_BITMASK) >> 1;
			result |= result >> -SOUTH;
			result |= result >> -SOUTH * 3;
			return result;
		}

		static bool defendedByWhiteOrNotAttackedByBlack(MoveGenerator& board, Square square) {
			bitBoard_t posBitBoard = 1ULL << square;
			bitBoard_t defended = posBitBoard & board.attackMask[WHITE];
			bitBoard_t notAttacked = posBitBoard & ~board.attackMask[BLACK];
			return (defended | notAttacked) != 0;
		}

		static bool defendedByBlackOrNotAttackedByWhite(MoveGenerator& board, Square square) {
			bitBoard_t posBitBoard = 1ULL << square;
			bitBoard_t defended = posBitBoard & board.attackMask[BLACK];
			bitBoard_t notAttacked = posBitBoard & ~board.attackMask[WHITE];
			return (defended | notAttacked) != 0;
		}

		static bool isChallengingPassedPawnMove(MoveGenerator& board, Move move) {
			bool isPassedPawn = false;
			Piece movingPiece = move.getMovingPiece();
			Square destinationSquare = move.getDestination();

			if (movingPiece == WHITE_PAWN) {
				if (destinationSquare >= Square(SearchParameter::PASSED_PAWN_EXTENSION_WHITE_MIN_TARGET_RANK) * NORTH &&
					defendedByWhiteOrNotAttackedByBlack(board, destinationSquare) &&
					move.isPromote())
				{
					bitBoard_t passedPawnCheckMask = computeWhitePassedPawnCheckMask(destinationSquare);
					isPassedPawn = (passedPawnCheckMask & board.getPieceBB(BLACK_PAWN)) == 0;
				}
			}
			else if (movingPiece == BLACK_PAWN) {
				if (destinationSquare <= Square(SearchParameter::PASSED_PAWN_EXTENSION_BLACK_MIN_TARGET_RANK) * NORTH + NW &&
					defendedByBlackOrNotAttackedByWhite(board, destinationSquare) &&
					!move.isPromote())
				{
					bitBoard_t passedPawnCheckMask = computeBlackPassedPawnCheckMask(destinationSquare);
					isPassedPawn = (passedPawnCheckMask & board.getPieceBB(WHITE_PAWN)) == 0;
				}
			}

			return isPassedPawn;
		}

	};

}
