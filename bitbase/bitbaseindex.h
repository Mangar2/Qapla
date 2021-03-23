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
 * Calculates an index from a board position to a _bitbase. The index is calculated by multiplying 
 * 1. One bit for white to move / black to move
 * 2. An index for the positions of the two kings supressing illegal positions where kings are adjacent
 * and calculating a symetry
 * 3. The indexes of pawns and 
 * The indes has no information about which pieces are on the board. It must only know whether a piece
 * is a pawn or not a pawn. 
 */

#ifndef __BITBASEINDEX_H
#define __BITBASEINDEX_H

#include "piecelist.h"
#include "../basics/types.h"
#include "../movegenerator/movegenerator.h"

using namespace ChessBasics;
using namespace ChessMoveGenerator;

namespace QaplaBitbase {

	class BitbaseIndex
	{
	public:
		BitbaseIndex() {
			clear();
		}

		/**
		 * Creates a bitbase index and sets the pieces from a piece list
		 * @param pieceList list of pieces to be used in the bitbase index
		 */
		BitbaseIndex(const PieceList& pieceList) { 
			clear();
			_pieceCount = pieceList.getNumberOfPieces();
			_pawnCount = pieceList.getNumberOfPawns();
			computeSize();
		}

		/**
		 * Creates a bitbase index and sets the pieces and the squares from a piece list
		 * @param pieceList list of pieces to be used in the bitbase index
		 * @param wtm true, if white to move
		 */
		BitbaseIndex(const PieceList& pieceList, bool wtm) {
			set(pieceList, wtm);
		}

		/**
		 * Creates a bitbase index, sets the piece counts from a piece list 
		 * and the piece squares from an index
		 * @param index index having the coded squares information
		 * @param pieceList list of pieces to be used in the bitbase index
		 */
		BitbaseIndex(uint64_t index, const PieceList& pieceList) {
			clear();
			_index = index;
			setSquares(pieceList);
			if (_isLegal && hasUnorderdDoublePiece(pieceList)) {
				_isLegal = false;
			}
		}

		/**
		 * Sets the bitbase index from a piece List
		 */
		void set(const PieceList& pieceList, bool wtm) {
			clear();
			Square whiteKingPos = pieceList.getSquare(0);
			Square blackKingPos = pieceList.getSquare(1);
			initialize(wtm, whiteKingPos, blackKingPos, pieceList.getNumberOfPawns() > 0);
			for (uint32_t index = 2; index < pieceList.getNumberOfPieces(); index++) {
				addPieceToIndex(pieceList.getSquare(index), pieceList.getPiece(index));
			}
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
		 * Gets the maximum possible index + 1
		 */
		uint64_t getSizeInBit() const {
			return _sizeInBit;
		}

		/**
		 * Gets the index of the current position
		 */
		uint64_t getIndex() const {
			return _index;
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

		/**
		 * @returns true, if the index is a legal index
		 */
		bool isLegal() const { return _isLegal; }

	private:

		/**
		 * @returns true, if the bitbase has two pieces of the same kind that are badly ordered
		 * (the first piece is on a larger square than the second piece).
		 */
		bool hasUnorderdDoublePiece(const PieceList& pieceList) {
			bool unordered = false;
			Piece lastPiece = Piece::NO_PIECE;
			for (uint32_t index = 2; index < pieceList.getNumberOfPieces(); index++) {
				const bool samePiece = pieceList.getPiece(index) == pieceList.getPiece(index - 1);
				if (samePiece) {
					unordered = getSquare(index) <= getSquare(index - 1);
					if (unordered) {
						break;
					}
				}
			}
			return unordered;
		};

		/**
		 * Clears the index
		 */
		void clear() {
			_index = 0;
			_mapType = 0;
			_piecesBB = 0;
			_pawnsBB = 0;
			_pieceCount = 0;
			_pawnCount = 0;
			_numberOfDiagonalSquares = 0;
			_isLegal = false;
		}

		/**
		 * Checks, if a square is legal
		 */
		bool isLegalSquare(Square square, bool allOnDiagonal, const PieceList& pieceList);

		/**
		 * Sets all squares by an index
		 */
		void setSquares(const PieceList& pieceList);

		/**
		 * Sets all pawns based on the remaining index after setting the Kings
		 * @retunrns remaining index after setting the pawns
		 */
		uint64_t setPawns(uint64_t index, const PieceList& pieceList);
		void setPieces(uint64_t index, const PieceList& pieceList);

		/**
		 * Computes the size of the bitbase index
		 */
		void computeSize() {
			_sizeInBit = _pawnCount == 0 ?
				NUMBER_OF_TWO_KING_POSITIONS_WITHOUT_PAWN * COLOR_COUNT :
				NUMBER_OF_TWO_KING_POSITIONS_WITH_PAWN * COLOR_COUNT;
			uint32_t remainingPawnPositions = NUMBER_OF_PAWN_POSITIONS;
			for (uint32_t pawns = _pawnCount; pawns > 0; pawns--, remainingPawnPositions--) {
				_sizeInBit *= remainingPawnPositions;
			}
			uint64_t remainingPiecePositions = NUMBER_OF_PIECE_POSITIONS - KING_COUNT - _pawnCount;
			for (uint32_t pieces = _pieceCount - KING_COUNT - _pawnCount; pieces > 0; pieces--) {
				_sizeInBit *= remainingPiecePositions;
				remainingPiecePositions--;
			}
		}


		/**
		 * Returns true, if a field is already occupied
		 */
		bool isOccupied(Square square) {
			return _piecesBB & (1ULL << square);
		}

		/**
		 * Adds another piece to the index
		 */
		void addPieceToIndex(Square square, Piece piece) {
			if (isPawn(piece)) {
				addPawnToIndex(square);
			}
			else {
				addNonPawnPieceToIndex(square);
			}
		}


		/**
		 * Initializes the index calculation
		 */
		void initialize(bool wtm, Square whiteKingSquare, Square blackKingSquare, bool hasPawn);

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
		 * Maps a Square to a symetric square. 
		 * A chess position without pawns has 8 symetric positions. 
		 * Example: B1, G1 (File-Map), B8 (Row-Map) G8 (Row and File-Map), A2 (Triangle map), ...
		 * @param originalSquare - square the piece is currently located
		 * @param mapType - the symetric map, TRF (Triangle map bit, Rank map bit, File map bit)
		 * @returns symmetric mapped square
		 */
		Square mapSquare(Square originalSquare, uint32_t mapType);

		/**
		 * Computes the mapping type of the current position
		 */
		uint32_t computeSquareMapType(Square whiteKingSquare, Square blackKingSquare, bool hasPawn);

		/**
		 * Adds a pawn to the index
		 */
		void addPawnToIndex(Square pawnSquare);

		/**
		 * Pop count for sparcely populated bitboards
		 */
		uint32_t popCount(bitBoard_t bitBoard);

		/**
		 * Add any piece except pawn to the index. Pawns are special, because they can only
		 * walk in one direction
		 */
		void addNonPawnPieceToIndex(Square pieceSquare);

		/**
		 * Adds the square of a pawn to the piece square list
		 */
		void addPawnSquare(Square square);

		/**
		 * Adds the square of a piece to the piece square list
		 */
		void addPieceSquare(Square square);

		/**
		 * Changes the square of a piece
		 */
		void changePieceSquare(uint32_t pieceNo, Square newSquare);

		/**
		 * Initializes static lookup maps
		 */
		static struct InitStatic {
			InitStatic();
		} _staticConstructor;

		/**
		 * Computes the index for two kings and moving right
		 * @param index current index, adding the king positions
		 */
		void computeKingIndex(bool wtm, Square whiteKingSquare, Square blackKingSquare, bool hasPawn);

		static const uint32_t MAP_FILE = 1;
		static const uint32_t MAP_RANK = 2;
		static const uint32_t MAP_TO_A1_D1_D4_TRIANGLE = 4;
		static const uint32_t NUMBER_OF_TWO_KING_POSITIONS_WITH_PAWN = 1806;
		static const uint32_t NUMBER_OF_TWO_KING_POSITIONS_WITHOUT_PAWN = 462;
		static const uint64_t NUMBER_OF_PAWN_POSITIONS = BOARD_SIZE - 2 * NORTH;
		static const uint64_t NUMBER_OF_PIECE_POSITIONS = BOARD_SIZE;
		static const uint64_t COLOR_COUNT = 2;
		static const uint64_t KING_COUNT = 2;

		static array<uint32_t, BOARD_SIZE* BOARD_SIZE> mapTwoKingsToIndexWithPawn;
		static array<uint32_t, BOARD_SIZE* BOARD_SIZE> mapTwoKingsToIndexWithoutPawn;
		static array<uint32_t, NUMBER_OF_TWO_KING_POSITIONS_WITH_PAWN> mapIndexToKingSquaresWithPawn;
		static array<uint32_t, NUMBER_OF_TWO_KING_POSITIONS_WITHOUT_PAWN> mapIndexToKingSquaresWithoutPawn;
		static const uint32_t MAX_PIECES_COUNT = 10;
		uint32_t _pieceCount;
		uint32_t _pawnCount;
		uint32_t _numberOfDiagonalSquares;
		array<Square, MAX_PIECES_COUNT> _squares;
		bitBoard_t _piecesBB;
		bitBoard_t _pawnsBB;
		uint64_t _index;
		uint64_t _sizeInBit;
		uint8_t _mapType;
		bool _isLegal;
		bool _wtm;

	};

}

#endif // __BITBASEINDEX_H