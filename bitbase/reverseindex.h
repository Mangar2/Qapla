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
 * Calculates the piece positions from a bitbase index (number). This is used only for bitbase 
 * generation to loop through all possible positions scipping symetries.
 * The index has the following coding:
 * 1. One bit for white to move / black to move
 * 2. An index for the positions of the two kings supressing illegal positions where kings are adjacent
 * and calculating a symetry
 * 3. The indexes of pawns and 
 * The indes has no information about which pieces are on the board. It must only know whether a piece
 * is a pawn or not a pawn. 
 */

#ifndef __QAPLA_BITBASE_REVERSEINDEX_H
#define __QAPLA_BITBASE_REVERSEINDEX_H

#include "piecelist.h"
#include "../basics/types.h"
#include "../movegenerator/movegenerator.h"

using namespace ChessBasics;
using namespace ChessMoveGenerator;

namespace QaplaBitbase {

	class ReverseIndex
	{
	public:
		/**
		 * Creates a bitbase index, sets the piece counts from a piece list 
		 * and the piece squares from an index
		 * @param index index having the coded squares information
		 * @param pieceList list of pieces to be used in the bitbase index
		 */
		ReverseIndex(uint64_t index, const PieceList& pieceList) {
			clear();
			setSquares(index, pieceList);
		}

		/**
		 * Gets the square of a piece
		 */
		Square getSquare(uint32_t pieceNo) const {
			Square result = NO_SQUARE;
			if (pieceNo < getNumberOfPieces()) {
				result = _squares[pieceNo];
			}
			return result;
		}

		/**
		 * Gets the number of pieces (all pieces incl. kings and pawns)
		 */
		uint32_t getNumberOfPieces() const {
			return _pieceCount;
		}

		/**
		 * @returns true, if its whites turn to move
		 */
		bool isWhiteToMove() const {
			return _wtm;
		}

	private:

		/**
		 * Clears the index
		 */
		void clear() {
			_piecesBB = 0;
			_pawnsBB = 0;
			_pieceCount = 0;
			_pawnCount = 0;
		}

		/**
		 * Sets all squares by an index
		 */
		void setSquares(uint64_t index, const PieceList& pieceList);

		/**
		 * Sets all pawns based on the remaining index after setting the Kings
		 * @retunrns remaining index after setting the pawns
		 */
		uint64_t setPawnsByIndex(uint64_t index, const PieceList& pieceList);
		void setPiecesByIndex(uint64_t index, const PieceList& pieceList);

		/**
		 * Calculates the index of a double pawn
		 */
		uint64_t doublePawnIndex(Square square1, Square square2) {
			uint64_t index;
			if (square2 < square1) {
				swap(square1, square2);
			}
			index = ((127 - square1) * square1) /2;
			index += square2;
		}

		/**
		 * Set the Squares of kings by having an index
		 */
		uint64_t setKingSquaresByIndex(uint64_t index, bool hasPawn);

		/**
		 * Computes the next possible king position for positions with pawns
		 */
		static Square computeNextKingSquareForPositionsWithPawn(Square currentSquare);

		/**
  		 * Checks, if two squares are adjacent
		 */
		static bool isAdjacent(Square pos1, Square pos2);

		/**
		 * Computes the real square of a piece from a index-square ("raw") by adding one 
		 * for every piece on a "lower" square
		 */
		Square computesRealSquare(bitBoard_t checkPieces, Square rawSquare);

		/**
		 * Pop count for sparcely populated bitboards
		 */
		uint32_t popCount(bitBoard_t bitBoard);

		/**
		 * Adds the square of a pawn to the piece square list
		 */
		void addPawnSquare(Square square);

		/**
		 * Adds the square of a piece to the piece square list
		 */
		void addPieceSquare(Square square);

		/**
		 * Initializes static lookup maps
		 */
		static struct InitStatic {
			InitStatic();
		} _staticConstructor;


		static const uint32_t NUMBER_OF_TWO_KING_POSITIONS_WITH_PAWN = 1806;
		static const uint32_t NUMBER_OF_TWO_KING_POSITIONS_WITHOUT_PAWN = 462;
		static const uint64_t NUMBER_OF_PAWN_POSITIONS = BOARD_SIZE - 2 * NORTH;
		static const uint64_t NUMBER_OF_PIECE_POSITIONS = BOARD_SIZE;
		static const uint64_t COLOR_COUNT = 2;
		static const uint64_t KING_COUNT = 2;

		static array<uint32_t, NUMBER_OF_TWO_KING_POSITIONS_WITH_PAWN> mapIndexToKingSquaresWithPawn;
		static array<uint32_t, NUMBER_OF_TWO_KING_POSITIONS_WITHOUT_PAWN> mapIndexToKingSquaresWithoutPawn;

		static const uint32_t MAX_PIECES_COUNT = 10;
		bitBoard_t _piecesBB;
		bitBoard_t _pawnsBB;
		uint32_t _pieceCount;
		uint32_t _pawnCount;
		array<Square, MAX_PIECES_COUNT> _squares;
		bool _wtm;

	};

}

#endif // __QAPLA_BITBASE_REVERSEINDEX_H