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
 */

#include "bitbaseindex.h"

using namespace ChessBitbase;

array<uint32_t, BOARD_SIZE* BOARD_SIZE> BitbaseIndex::mapTwoKingsToIndexWithPawn;
array<uint32_t, BOARD_SIZE* BOARD_SIZE> BitbaseIndex::mapTwoKingsToIndexWithoutPawn;
array<uint32_t, BitbaseIndex::NUMBER_OF_TWO_KING_POSITIONS_WITH_PAWN> BitbaseIndex::mapIndexToKingSquaresWithPawn;
array<uint32_t, BitbaseIndex::NUMBER_OF_TWO_KING_POSITIONS_WITHOUT_PAWN> BitbaseIndex::mapIndexToKingSquaresWithoutPawn;
BitbaseIndex::InitStatic BitbaseIndex::_staticConstructor;


bool isOnDiagonal(Square square) {
	return getFile(square) == File(getRank(square));
}

bool isAboveDiagonal(Square square) {
	return getFile(square) < File(getRank(square));
}

bool BitbaseIndex::isAdjacent(Square pos1, Square pos2) {
	bitBoard_t map = 1ULL << pos1;
	bool result;

	map |= BitBoardMasks::shift<WEST>(map);
	map |= BitBoardMasks::shift<EAST>(map);
	map |= BitBoardMasks::shift<SOUTH>(map);
	map |= BitBoardMasks::shift<NORTH>(map);

	result = ((1ULL << pos2) & map) != 0;
	return result;
}

BitbaseIndex::InitStatic::InitStatic() {
	uint32_t index = 0;
	for (Square whiteKingSquare = A1; whiteKingSquare <= H8; whiteKingSquare = computeNextKingSquareForPositionsWithPawn(whiteKingSquare)) {
		for (Square blackKingSquare = A1; blackKingSquare <= H8; ++blackKingSquare) {
			uint32_t lookupIndex = whiteKingSquare + blackKingSquare * BOARD_SIZE;
			assert(lookupIndex < uint32_t(BOARD_SIZE * BOARD_SIZE));
			mapTwoKingsToIndexWithPawn[lookupIndex] = index;
			mapIndexToKingSquaresWithPawn[index] = lookupIndex;
			if (!isAdjacent(whiteKingSquare, blackKingSquare)) {
				index++;
			}
		}
	}
	const Square whiteKingSquareWithoutPawn[] = { A1, B1, C1, D1, B2, C2, D2, C3, D3, D4 };
	index = 0;
	for (uint32_t posNo = 0; posNo < 10; posNo++) {
		Square whiteKingSquare = whiteKingSquareWithoutPawn[posNo];
		for (Square blackKingSquare = A1; blackKingSquare <= H8; ++blackKingSquare) {
			if (getFile(whiteKingSquare) == File(getRank(whiteKingSquare)) 
				&& getFile(blackKingSquare) < File(getRank(blackKingSquare))) {
				continue;
			}
			uint32_t lookupIndex = whiteKingSquare + blackKingSquare * BOARD_SIZE;
			assert(lookupIndex < uint32_t(BOARD_SIZE* BOARD_SIZE));
			assert(index < NUMBER_OF_TWO_KING_POSITIONS_WITHOUT_PAWN);
			mapTwoKingsToIndexWithoutPawn[lookupIndex] = index;
			mapIndexToKingSquaresWithoutPawn[index] = lookupIndex;
			if (!isAdjacent(whiteKingSquare, blackKingSquare)) {
				index++;
			}
		}
	}
}


uint64_t BitbaseIndex::setKingSquaresByIndex(uint64_t index, bool hasPawn) {
	uint32_t posIndex;
	uint64_t amountOfKingPositions;
	if (hasPawn) {
		amountOfKingPositions = NUMBER_OF_TWO_KING_POSITIONS_WITH_PAWN;
		posIndex = mapIndexToKingSquaresWithPawn[index % amountOfKingPositions];
	}
	else
	{
		amountOfKingPositions = NUMBER_OF_TWO_KING_POSITIONS_WITHOUT_PAWN;
		posIndex = mapIndexToKingSquaresWithoutPawn[index % amountOfKingPositions];
	}
	addPieceSquare(Square(posIndex % BOARD_SIZE));
	addPieceSquare(Square(posIndex / BOARD_SIZE));
	index /= amountOfKingPositions;
	_sizeInBit = amountOfKingPositions * _sizeInBit;
	return index;
}

bool BitbaseIndex::setPieceSquaresByIndex(uint64_t index, uint32_t pawnCount, uint32_t nonPawnPieceAmount) {
	clear();
	uint32_t posIndex;
	bool legalPosition = true;
	_index = index;
	_wtm = (_index % COLORS) == 0;
	_index /= COLORS;
	_sizeInBit = COLORS;
	_index = setKingSquaresByIndex(_index, pawnCount > 0);

	for (uint32_t pawn = 0; pawn < pawnCount; pawn++) {
		const uint32_t relevantPawnPositionNumber = NUMBER_OF_PAWN_POSITIONS - _piecesCount;
		posIndex = _index % relevantPawnPositionNumber;
		_index /= relevantPawnPositionNumber;
		_sizeInBit *= relevantPawnPositionNumber;
		addPieceSquare(computesRealSquare(_piecesBB >> 8, Square(posIndex)) + A2);
		if (_squares[_piecesCount - 1] >= BOARD_SIZE) {
			legalPosition = false;
		}
	}

	const uint32_t kingAmount = 2;
	bool allOnDiagonal = (pawnCount == 0) && isOnDiagonal(getSquare(0)) && isOnDiagonal(getSquare(1));
	for (uint32_t piece = 0; piece < nonPawnPieceAmount - kingAmount; piece++) {
		posIndex = _index % (AMOUT_OF_PIECE_POSITIONS - _piecesCount);
		_index /= AMOUT_OF_PIECE_POSITIONS - _piecesCount;
		_sizeInBit *= AMOUT_OF_PIECE_POSITIONS - _piecesCount;
		addPieceSquare(computesRealSquare(_piecesBB, Square(posIndex)));
		if (_squares[_piecesCount - 1] >= BOARD_SIZE) {
			legalPosition = false;
		}
		// This index is invalidated, because it is unused. The first piece must be below the
		// diagonal. 
		if (allOnDiagonal && isAboveDiagonal(_squares[_piecesCount - 1])) {
			legalPosition = false;
		}
		if (!isOnDiagonal(_squares[_piecesCount - 1])) {
			allOnDiagonal = false;
		}
	}
	return legalPosition;
}

void BitbaseIndex::initialize(bool wtm, Square whiteKingSquare, Square blackKingSquare, bool hasPawn) {
	_numberOfDiagonalSquares = 0;
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

Square BitbaseIndex::computeNextKingSquareForPositionsWithPawn(Square currentSquare) {
	Square nextSquare = getFile(currentSquare) < File::D ? currentSquare + 1 : currentSquare + 5;
	return nextSquare;
}

Square BitbaseIndex::computesRealSquare(bitBoard_t checkPieces, Square rawSquare) {
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

Square BitbaseIndex::mapSquare(Square originalSquare, uint32_t mapType) {
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

uint32_t BitbaseIndex::computeSquareMapType(Square whiteKingSquare, Square blackKingSquare, bool hasPawn) {
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

		if (isAboveDiagonal(whiteKingSquare)) {
			mapType |= MAP_TO_A1_D1_D4_TRIANGLE;
			_numberOfDiagonalSquares = 0;
		}
		else if (isOnDiagonal(whiteKingSquare)) {
			_numberOfDiagonalSquares = 1;
			if (isAboveDiagonal(blackKingSquare)) {
				mapType |= MAP_TO_A1_D1_D4_TRIANGLE;
			}
			else if (isOnDiagonal(blackKingSquare)) {
				_numberOfDiagonalSquares = 2;
			}
		} 
	}
	return mapType;
}

void BitbaseIndex::addPawnToIndex(Square pawnSquare) {
	Square mappedSquare = mapSquare(pawnSquare, _mapType);

	// Reduces the index for every square the pawn cannot exist because there is already 
	// another piece
	const bitBoard_t RANK1 = 0xFF;
	const bitBoard_t belowBB = ((1ULL << mappedSquare) - 1) & _piecesBB & ~RANK1;
	uint64_t indexValueBasedOnPawnSquare = uint64_t(mappedSquare - A2) - popCount(belowBB);

	_index += (int64_t)indexValueBasedOnPawnSquare * _sizeInBit;
	_sizeInBit *= NUMBER_OF_PAWN_POSITIONS - getNumberOfPieces();
	addPieceSquare(mappedSquare);
}

uint32_t BitbaseIndex::popCount(bitBoard_t bitBoard) {
	uint32_t result = 0;
	for (; bitBoard; bitBoard &= (bitBoard - 1)) {
		result++;
	}
	return result;
}

void BitbaseIndex::addNonPawnPieceToIndex(Square square) {
	Square mappedSquare = mapSquare(square, _mapType);
	if (_numberOfDiagonalSquares == _piecesCount) {
		if (isAboveDiagonal(mappedSquare)) {
			_mapType |= MAP_TO_A1_D1_D4_TRIANGLE;
			mappedSquare = mapSquare(square, _mapType);
		}
		else if (isOnDiagonal(mappedSquare)) {
			_numberOfDiagonalSquares++;
		}
	}
	
	const bitBoard_t belowBB = ((1ULL << mappedSquare) - 1) & _piecesBB;
	uint64_t indexValueBasedOnPieceSquare = uint64_t(mappedSquare) - popCount(belowBB);

	_index += (int64_t)indexValueBasedOnPieceSquare * _sizeInBit;
	_sizeInBit *= (int64_t)BOARD_SIZE - getNumberOfPieces();
	addPieceSquare(mappedSquare);
}

void BitbaseIndex::addPieceSquare(Square square) {
	_squares[_piecesCount] = square;
	_piecesCount++;
	_piecesBB |= 1ULL << square;
}

void BitbaseIndex::changePieceSquare(uint32_t pieceNo, Square newSquare) {
	if (pieceNo < getNumberOfPieces()) {
		Square oldSquare = _squares[pieceNo];
		_squares[pieceNo] = newSquare;
		_piecesBB ^= 1ULL << oldSquare;
		_piecesBB |= 1ULL << newSquare;
	}
}

void BitbaseIndex::computeKingIndex(bool wtm, Square whiteKingSquare, Square blackKingSquare, bool hasPawn) {
	uint32_t kingIndexNotShrinkedBySymetries = whiteKingSquare + blackKingSquare * BOARD_SIZE;
	if (hasPawn) {
		_index += uint64_t(mapTwoKingsToIndexWithPawn[kingIndexNotShrinkedBySymetries]) * COLOR_AMOUNT;
	}
	else {
		_index += uint64_t(mapTwoKingsToIndexWithoutPawn[kingIndexNotShrinkedBySymetries]) * COLOR_AMOUNT;
	}
	_sizeInBit *= hasPawn ? NUMBER_OF_TWO_KING_POSITIONS_WITH_PAWN : NUMBER_OF_TWO_KING_POSITIONS_WITHOUT_PAWN;
}
