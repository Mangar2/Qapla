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
 * Provides an access to the chess board - adapter to different chess boards
 * of different chess engines
 */


#ifndef __BOARDACCESS_H
#define __BOARDACCESS_H

#include <vector>
#include <fstream>
#include "../basics/move.h"
#include "../movegenerator/bitboardmasks.h"
#include "../movegenerator/movegenerator.h"

using namespace ChessMoveGenerator;

namespace ChessBitbase {
	class BoardAccess {
	public:
		static uint64_t getIndex(const MoveGenerator& board) {
			BitBaseIndex bitBaseIndex;
			setBitBaseIndexFromBoard(board, bitBaseIndex);
			uint64_t index = bitBaseIndex.getIndex();
			return index;
		}

		static uint64_t getIndex(bool whiteToMove, const PieceList& pieceList, move_t move) {
			BitBaseIndex bitBaseIndex;
			setBitBaseIndex(whiteToMove, bitBaseIndex, pieceList, move);
			uint64_t index = bitBaseIndex.getIndex();
			return index;
		}

		static void setBitBaseIndexFromBoard(const MoveGenerator& board, BitBaseIndex& bitBaseIndex) {
			bool hasPawn = (board.getPieceBitBoard(WHITE_PAWN) | board.getPieceBitBoard(BLACK_PAWN)) != 0;
			bitBaseIndex.initialize(board.kingPos[WHITE], board.kingPos[BLACK], board.whiteToMove, hasPawn);
			for (piece_t piece = WHITE_PAWN; piece <= BLACK_QUEEN; piece++) {
				addPiece(board, piece, bitBaseIndex);
			}
		}

	private:

		static inline pos_t getPiecePos(uint32_t index, const PieceList& pieceList, move_t move) {
			pos_t pos = pieceList.getPos(index);
			if (pos == Move::getStartPos(move)) {
				pos = Move::getTargetPos(move);
			}
			return pos;
		}

		static void setBitBaseIndex(bool whiteToMove, BitBaseIndex& bitBaseIndex, const PieceList& pieceList, move_t move) {
			bool hasPawn = pieceList.getPawnAmount() != 0;
			pos_t whiteKingPos = getPiecePos(0, pieceList, move);
			pos_t blackKingPos = getPiecePos(1, pieceList, move);
			bitBaseIndex.initialize(whiteKingPos, blackKingPos, whiteToMove, hasPawn);

			for (uint32_t index = 2; index < pieceList.getPieceAmount(); index++) {
				piece_t piece = pieceList.getPiece(index);
				pos_t pos = getPiecePos(index, pieceList, move);
				bitBaseIndex.addPieceToIndex(pos, piece);
			}
		}


		static void addPiece(const MoveGenerator& board, piece_t piece, BitBaseIndex& bitBaseIndex) {
			bitBoard_t pieces = board.getPieceBitBoard(piece);
			for (; pieces != 0; pieces &= pieces - 1) {
				bitBaseIndex.addPieceToIndex(BitBoardMasks::lsb(pieces), piece);
			}
		}
	};

}

#endif // __BOARDACCESS_H