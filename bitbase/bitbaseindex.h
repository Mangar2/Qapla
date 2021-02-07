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

#include "bitbase.h"
#include "piecelist.h"
#include "../basics/types.h"
#include "../movegenerator/movegenerator.h"

using namespace ChessBasics;
using namespace ChessMoveGenerator;

namespace ChessBitbase {

	class BitbaseIndex
	{
	public:
		BitbaseIndex() { clear(); }

		/**
		 * Clears the index
		 */
		void clear() {
			_index = 0;
			_mapType = 0;
			_squares.clear();
		}

		/**
		 * Initializes the index calculation
		 */
		void initialize(bool wtm, Square whiteKingSquare, Square blackKingSquare, bool hasPawn) {
			_hasPawn = hasPawn;
			_wtm = wtm;
			_index = wtm ? 0 : 1;
			_sizeInBit = COLOR_AMOUNT;
			_mapType = computeSquareMapType(whiteKingSquare, blackKingSquare, _hasPawn);
			whiteKingSquare = mapSquare(whiteKingSquare, _mapType);
			blackKingSquare = mapSquare(blackKingSquare, _mapType);
			addPieceSquare(whiteKingSquare);
			addPieceSquare(blackKingSquare);
			computeKingIndex(_wtm, whiteKingSquare, blackKingSquare, _hasPawn);
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
		 * Sets the positions of pieces by having an index an a list of piece types
		 */
		bool setPieceSquaresByIndex(uint64_t index, const PieceList& pieceList) {
			return setPieceSquaresByIndex(index,
				pieceList.getNumberOfPawns(), pieceList.getNumberOfPiecesWithoutPawns());
		}

		/**
		 * Gets the square of a piece
		 */
		Square getPieceSquare(uint32_t pieceNo) const {
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
		uint64_t computeIndex() const {
			return _index;
		}

		/**
		 * Gets the number of pieces (all pieces incl. kings and pawns)
		 */
		uint32_t getNumberOfPieces() const {
			return uint32_t(_squares.size());
		}

		/**
		 * @returns true, if its whites turn to move
		 */
		bool isWhiteToMove() const {
			return _wtm;
		}

		/**
		 * Changing the index by moving a piece
		 */
		void doMove(Move move) {
			Square startSquare = move.getDeparture();
			for (auto &pieceSquare : _squares) {
				if (pieceSquare == startSquare) {
					pieceSquare = move.getDestination();
					break;
				}
			}
		}

	private:

		/**
		 * Calculates and sets the squares of all pieces giving an index and the amount of pawns and pieces
		 */
		bool setPieceSquaresByIndex(uint64_t index, uint32_t pawnAmount, uint32_t nonPawnPieceAmount);

		/**
		 * Set the Squares of kings by having an index
		 */
		uint64_t setKingSquaresByIndex(uint64_t index, bool hasPawn);

		/**
		 * Computes the next possible king position for positions with pawns
		 */
		static Square computeNextKingSquareForPositionsWithPawn(Square currentSquare) {
			Square nextSquare = getFile(currentSquare) < File::D ? currentSquare + 1 : currentSquare + 5;
			return nextSquare;
		}

		/**
  		 * Checks, if two squares are adjacent
		 */
		static bool isAdjacent(Square pos1, Square pos2);

		/**
		 * Computes the real square of a piece from a index-square ("raw") by adding one 
		 * for every piece on a "lower" square
		 */
		Square computesRealSquare(bitBoard_t checkPieces, Square rawSquare) {
			Square realSquare = rawSquare;
			while (true) {
				bitBoard_t mapOfAllFieldBeforeAndIncludingCurrentField = (1ULL << realSquare);
				mapOfAllFieldBeforeAndIncludingCurrentField |= mapOfAllFieldBeforeAndIncludingCurrentField - 1;
				if ((mapOfAllFieldBeforeAndIncludingCurrentField & checkPieces) != 0) {
					++realSquare;
					checkPieces &= checkPieces - 1;
				}
				else {
					break;
				}
			}
			return realSquare;
		}

		/**
		 * Maps a Square to a symetric square. 
		 * A chess position without pawns has 8 symetric positions. 
		 * Example: B1, G1 (File-Map), B8 (Row-Map) G8 (Row and File-Map), A2 (Triangle map), ...
		 * @param originalSquare - square the piece is currently located
		 * @param mapType - the symetric map, TRF (Triangle map bit, Rank map bit, File map bit)
		 * @returns symmetric mapped square
		 */
		Square mapSquare(Square originalSquare, uint32_t mapType) {
			uint32_t resultSquare = originalSquare;
			if (mapType != 0) {
				if ((mapType & MAP_FILE) != 0) {
					resultSquare ^= 0x7;
				}
				if ((mapType & MAP_RANK) != 0) {
					resultSquare ^= 0x38;
				}
				if ((mapType & MAP_TO_A1_D1_D4_TRIANGLE) != 0) {
					resultSquare = (resultSquare >> 3) | ((resultSquare & 7) << 3);
				}
			}
			return Square(resultSquare);
		}

		/**
		 * Computes the mapping type of the current position
		 */
		uint32_t computeSquareMapType(Square whiteKingSquare, Square blackKingSquare, bool hasPawn) {
			uint32_t mapType = 0;
			if (getFile(whiteKingSquare) >= File::E) {
				mapType |= MAP_FILE;
				whiteKingSquare ^= 0x7;
				blackKingSquare ^= 0x7;
			}
			if (!hasPawn) {
				if (whiteKingSquare >= A5) {
					mapType |= MAP_RANK;
					whiteKingSquare ^= 0x38;
					blackKingSquare ^= 0x38;
				}
				if (getFile(whiteKingSquare) < File(getRank(whiteKingSquare))) {
					mapType |= MAP_TO_A1_D1_D4_TRIANGLE;
				}
				else if (getFile(whiteKingSquare) == File(getRank(whiteKingSquare)) && 
					(getRank(blackKingSquare) > Rank(getFile(blackKingSquare)))) {
					mapType |= MAP_TO_A1_D1_D4_TRIANGLE;
				}
			}
			return mapType;
		}

		/**
		 * Adds a pawn to the index
		 */
		void addPawnToIndex(Square pawnSquare) {
			Square mappedSquare = mapSquare(pawnSquare, _mapType);
			uint64_t indexValueBasedOnPawnSquare = mappedSquare - A2;
			// Reduces the index for every square the pawn cannot exist because there is already 
			// another piece
			for (auto pieceSquare : _squares) {
				if (pieceSquare < mappedSquare && pieceSquare >= A2) {
					indexValueBasedOnPawnSquare--;
				}
			}
			_index += (int64_t)indexValueBasedOnPawnSquare * _sizeInBit;
			_sizeInBit *= AMOUT_OF_PAWN_POSITIONS - getNumberOfPieces();
			addPieceSquare(mappedSquare);
		}

		/**
		 * Add any piece except pawn to the index. Pawns are special, because they can only
		 * walk in one direction
		 */
		void addNonPawnPieceToIndex(Square pieceSquare) {
			Square mappedSquare = mapSquare(pieceSquare, _mapType);
			uint64_t indexValueBasedOnPieceSquare = mappedSquare;
			for (auto pieceSquare : _squares) {
				if (pieceSquare < mappedSquare) {
					indexValueBasedOnPieceSquare--;
				}
			}
			_index += (int64_t)indexValueBasedOnPieceSquare * _sizeInBit;
			_sizeInBit *= (int64_t)BOARD_SIZE - getNumberOfPieces();
			addPieceSquare(mappedSquare);
		}

		/**
		 * Adds the square of a piece to the piece square list
		 */
		void addPieceSquare(Square square) {
			_squares.push_back(square);
			_piecesBB |= 1ULL << square;
		}

		/**
		 * Changes the square of a piece
		 */
		void changePieceSquare(uint32_t pieceNo, Square newSquare) {
			if (pieceNo < getNumberOfPieces()) {
				Square oldSquare = _squares[pieceNo];
				_squares[pieceNo] = newSquare;
				_piecesBB ^= 1ULL << oldSquare;
				_piecesBB |= 1ULL << newSquare;
			}
		}

		/**
		 * Initializes static lookup maps
		 */
		static struct InitStatic {
			InitStatic();
		} _staticConstructor;


	private:

		/**
		 * Computes the index for two kings and moving right
		 * @param index current index, adding the king positions
		 */
		void computeKingIndex(bool wtm, Square whiteKingSquare, Square blackKingSquare, bool hasPawn) {
			uint32_t kingIndexNotShrinkedBySymetries = whiteKingSquare + blackKingSquare * BOARD_SIZE;
			if (hasPawn) {
				_index += mapTwoKingsToIndexWithPawn[kingIndexNotShrinkedBySymetries] * COLOR_AMOUNT;
			}
			else {
				_index += mapTwoKingsToIndexWithoutPawn[kingIndexNotShrinkedBySymetries] * COLOR_AMOUNT;
			}
			_sizeInBit *= hasPawn ? AMOUNT_OF_TWO_KING_POSITIONS_WITH_PAWN : AMOUNT_OF_TWO_KING_POSITIONS_WITHOUT_PAWN;
		}

		static const uint32_t MAP_FILE = 1;
		static const uint32_t MAP_RANK = 2;
		static const uint32_t MAP_TO_A1_D1_D4_TRIANGLE = 4;
		static const uint64_t AMOUNT_OF_TWO_KING_POSITIONS_WITH_PAWN = 1806;
		static const uint64_t AMOUNT_OF_TWO_KING_POSITIONS_WITHOUT_PAWN = 462;
		static const uint64_t AMOUT_OF_PAWN_POSITIONS = BOARD_SIZE - 2 * NORTH;
		static const uint64_t AMOUT_OF_PIECE_POSITIONS = BOARD_SIZE;
		static const uint64_t COLORS = 2;

		static array<uint32_t, BOARD_SIZE* BOARD_SIZE> mapTwoKingsToIndexWithPawn;
		static array<uint32_t, BOARD_SIZE* BOARD_SIZE> mapTwoKingsToIndexWithoutPawn;
		static array<uint32_t, AMOUNT_OF_TWO_KING_POSITIONS_WITH_PAWN> mapIndexToKingSquaresWithPawn;
		static array<uint32_t, AMOUNT_OF_TWO_KING_POSITIONS_WITHOUT_PAWN> mapIndexToKingSquaresWithoutPawn;
		vector<Square> _squares;
		bitBoard_t _piecesBB;
		uint64_t _index;
		uint64_t _sizeInBit;
		uint8_t _mapType;

		bool _wtm;
		bool _hasPawn;
	};

}

#endif // __BITBASEINDEX_H