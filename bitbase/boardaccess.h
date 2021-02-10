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
 * Provides an access to the chess position - adapter to different chess boards
 * of different chess engines
 */


#ifndef __BOARDACCESS_H
#define __BOARDACCESS_H

#include <vector>
#include <fstream>
#include "../basics/move.h"
#include "bitbaseindex.h"
#include "../movegenerator/bitboardmasks.h"
#include "../movegenerator/movegenerator.h"

using namespace ChessMoveGenerator;

namespace ChessBitbase {

	class BoardAccess {
	public:
		/**
		 * Computes a _bitbase index from a position position
		 */
		template<bool SYMETRIC>
		static uint64_t computeIndex(const MoveGenerator& position) {
			BitbaseIndex bitBaseIndex;
			PieceList pieceList(position);
			bool wtm = position.isWhiteToMove() ^ SYMETRIC;
			if (SYMETRIC) pieceList.toSymetric();
			setBitbaseIndexFromPieceList(pieceList, wtm, bitBaseIndex);
			uint64_t index = bitBaseIndex.computeIndex();
			return index;
		}

		/**
		 * Computes a _bitbase index from piece list
		 */
		static uint64_t computeIndex(bool whiteToMove, const PieceList& pieceList, Move move) {
			BitbaseIndex bitBaseIndex;
			setBitbaseIndex(whiteToMove, bitBaseIndex, pieceList, move);
			uint64_t index = bitBaseIndex.computeIndex();
			return index;
		}


	private:

		/**
		 * Sets a _bitbase Index having a position
		 */
		static void setBitbaseIndexFromPieceList(const PieceList& pieceList, bool wtm, BitbaseIndex& bitBaseIndex) {
			bool hasPawn = pieceList.getNumberOfPawns() != 0;
			Square whiteKingPos = pieceList.getSquare(0);
			Square blackKingPos = pieceList.getSquare(1);
			bitBaseIndex.initialize(wtm, whiteKingPos, blackKingPos, hasPawn);
			for (uint32_t index = 2; index < pieceList.getNumberOfPieces(); index++) {
				bitBaseIndex.addPieceToIndex(pieceList.getSquare(index), pieceList.getPiece(index));
			}
		}


		static inline Square getPiecePos(uint32_t index, const PieceList& pieceList, Move move) {
			Square square = pieceList.getSquares()[index];
			if (square == move.getDeparture()) {
				square = move.getDestination();
			}
			return square;
		}

		/**
		 * Sets a bitbase index having a piece list with pieces and squares
		 */
		static void setBitbaseIndex(bool whiteToMove, BitbaseIndex& bitBaseIndex, const PieceList& pieceList, Move move) {
			bool hasPawn = pieceList.getNumberOfPawns() != 0;
			Square whiteKingPos = getPiecePos(0, pieceList, move);
			Square blackKingPos = getPiecePos(1, pieceList, move);
			bitBaseIndex.initialize(whiteToMove, whiteKingPos, blackKingPos, hasPawn);

			for (uint32_t index = 2; index < pieceList.getNumberOfPieces(); index++) {
				Piece piece = pieceList.getPiece(index);
				Square square = getPiecePos(index, pieceList, move);
				bitBaseIndex.addPieceToIndex(square, piece);
			}
		}

		/**
		 * Adds all pieces of one type (for example all white pawns)
		 */
		template<Piece COLOR>
		static void addAllPiecesOfType(const MoveGenerator& position, Piece piece, BitbaseIndex& bitBaseIndex) {
			bitBoard_t piecesBB = position.getPieceBB(piece);
			for (; piecesBB != 0; piecesBB &= piecesBB - 1) {
				Square square = BitBoardMasks::lsb(piecesBB);
				if (COLOR == BLACK) {
					square = switchSide(square);
				}
				bitBaseIndex.addPieceToIndex(square, piece);
			}
		}
	};

}

#endif // __BOARDACCESS_H