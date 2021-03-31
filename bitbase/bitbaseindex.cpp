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

using namespace QaplaBitbase;

array<uint32_t, BOARD_SIZE* BOARD_SIZE> BitbaseIndex::mapTwoKingsToIndexWithPawn;
array<uint32_t, BOARD_SIZE* BOARD_SIZE> BitbaseIndex::mapTwoKingsToIndexWithoutPawn;
array<uint32_t, BitbaseIndex::NUMBER_OF_TWO_KING_POSITIONS_WITH_PAWN> BitbaseIndex::mapIndexToKingSquaresWithPawn;
array<uint32_t, BitbaseIndex::NUMBER_OF_TWO_KING_POSITIONS_WITHOUT_PAWN> BitbaseIndex::mapIndexToKingSquaresWithoutPawn;
BitbaseIndex::InitStatic BitbaseIndex::_staticConstructor;


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
	uint64_t numberOfKingPositions;
	if (hasPawn) {
		numberOfKingPositions = NUMBER_OF_TWO_KING_POSITIONS_WITH_PAWN;
		posIndex = mapIndexToKingSquaresWithPawn[index % numberOfKingPositions];
	}
	else
	{
		numberOfKingPositions = NUMBER_OF_TWO_KING_POSITIONS_WITHOUT_PAWN;
		posIndex = mapIndexToKingSquaresWithoutPawn[index % numberOfKingPositions];
	}
	addPieceSquare(Square(posIndex % BOARD_SIZE));
	addPieceSquare(Square(posIndex / BOARD_SIZE));
	return numberOfKingPositions;
}

bool BitbaseIndex::isLegalSquare(Square square, bool allOnDiagonal, const PieceList& pieceList) {
	bool result = true;
	if (square > H8) {
		result = false;
	} 
	else if (allOnDiagonal && isAboveDiagonal(square)) {
		// This index is invalidated, because it is unused. The first piece must be below the
		// diagonal. 
		result = false;
	}
	else {
		// The second piece of the same kind must be on a larger square. Else it is an illegal symmetry.
		bool samePiece = pieceList.getPiece(getNumberOfPieces()) == pieceList.getPiece(getNumberOfPieces() - 1);
		bool onSquareBelow = doublePieceSortValue[square] < doublePieceSortValue[_squares[getNumberOfPieces() - 1]];
		if (samePiece && onSquareBelow) {
			result = false;
		}
	}
	return result;
}

void BitbaseIndex::setPiecesByIndex(uint64_t index, const PieceList& pieceList) {
	bool hasPawns = pieceList.getNumberOfPawns() > 0;
	uint32_t piecesToAdd = pieceList.getNumberOfPiecesWithoutPawns() - KING_COUNT;
	uint64_t remainingPiecePositions = NUMBER_OF_PIECE_POSITIONS - KING_COUNT - pieceList.getNumberOfPawns();
	for (; piecesToAdd > 0; --piecesToAdd, --remainingPiecePositions) {
		const Square square = computesRealSquare(_piecesBB, Square(index % remainingPiecePositions));
		index /= remainingPiecePositions;
		_sizeInBit *= remainingPiecePositions;
		addPieceSquare(square);
	}
}

uint64_t BitbaseIndex::setPawnsByIndex(uint64_t index, const PieceList& pieceList) {
	const bool hasPawns = pieceList.getNumberOfPawns() > 0;
	uint64_t remainingPawnPositions = NUMBER_OF_PAWN_POSITIONS;
	for (uint32_t pawn = 0; pawn < pieceList.getNumberOfPawns(); ++pawn, --remainingPawnPositions) {
		const Square square = computesRealSquare(_pawnsBB, A2 + Square(index % remainingPawnPositions));

		index /= remainingPawnPositions;
		_sizeInBit *= remainingPawnPositions;
		
		addPawnSquare(square);
	}
	return index;
}

void BitbaseIndex::setSquares(const PieceList& pieceList) {
	_isLegal = true;
	_wtm = (_index % COLOR_COUNT) == 0;
	uint64_t index = _index / COLOR_COUNT;
	_sizeInBit = COLOR_COUNT;

	uint64_t numberOfKingPositions = setKingSquaresByIndex(index, pieceList.getNumberOfPawns() > 0);
	_sizeInBit *= numberOfKingPositions;
	index /= numberOfKingPositions;

	index = setPawnsByIndex(index, pieceList);
	setPiecesByIndex(index, pieceList);
}


void BitbaseIndex::initialize(const PieceList& pieceList, bool wtm) {
	_wtm = wtm;
	_index = wtm ? 0 : 1;
	_sizeInBit = COLOR_COUNT;
	_mapType = computeSquareMapType(pieceList);

	Square whiteKingSquare = mapSquare(pieceList.getSquare(0), _mapType);
	Square blackKingSquare = mapSquare(pieceList.getSquare(1), _mapType);
	addPieceSquare(whiteKingSquare);
	addPieceSquare(blackKingSquare);
	computeKingIndex(_wtm, whiteKingSquare, blackKingSquare, pieceList.getNumberOfPawns() > 0);
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

uint32_t BitbaseIndex::computeSquareMapType(const PieceList& pieceList) {
	uint32_t mapType = 0;
	Square whiteKingSquare = pieceList.getSquare(0);
	if (getFile(whiteKingSquare) >= File::E) {
		mapType |= MAP_FILE;
	}
	if (pieceList.getNumberOfPawns() == 0) {
		if (whiteKingSquare >= A5) {
			mapType |= MAP_RANK;
		}

		// Probe the two kings, return if an is not on a diagonal
		for (uint32_t index = 0; index < 2; index++) {
			Square mappedSquare = mapSquare(pieceList.getSquare(index), mapType);
			if (isOnDiagonal(mappedSquare)) {
				continue;
			}
			if (isAboveDiagonal(mappedSquare)) {
				mapType |= MAP_TO_A1_D1_D4_TRIANGLE;
			}
			return mapType;
		}

		// It is more complicated with multiple pieces of the same kind. The following special
		// cases must be handled
		// 1. Ignore pieces on the diagonal
		// 2. Ignore pieces of the same type that are arranged in mirror symmetry according the A1:H8 diagonal
		// 3. The smalles piece of the first type decides the mapping, if not ignored

		array<Square, 10> squares;
		for (uint32_t index = 2; index < pieceList.getNumberOfPieces();) {
			uint32_t count = getSquaresOfSameKind(pieceList, index, squares);
			for (uint32_t sqIndex = 0; sqIndex < count; sqIndex++) {
				squares[sqIndex] = mapSquare(squares[sqIndex], mapType);
			}
			bubbleSortMultiplePiece(squares, count);
			for (uint32_t sqIndex = 0; sqIndex < count; sqIndex++) {
				Square mappedSquare = squares[sqIndex];
				if (isOnDiagonal(mappedSquare)) {
					continue;
				}
				if (sqIndex + 1 < count) {
					Square nextSquare = squares[sqIndex + 1];
					bool symmetricPair = mapSquare(mappedSquare, MAP_TO_A1_D1_D4_TRIANGLE) == nextSquare;
					if (symmetricPair) {
						sqIndex++;
						continue;
					}
				}
				if (isAboveDiagonal(mappedSquare)) {
					mapType |= MAP_TO_A1_D1_D4_TRIANGLE;
				}
				return mapType;
			}
			index += count;
		}
	}
	return mapType;
}

uint32_t BitbaseIndex::popCount(bitBoard_t bitBoard) {
	uint32_t result = 0;
	for (; bitBoard; bitBoard &= (bitBoard - 1)) {
		result++;
	}
	return result;
}

void BitbaseIndex::addPawnToIndex(Square mappedSquare) {
	const bitBoard_t belowBB = ((1ULL << mappedSquare) - 1) & _pawnsBB;
	uint64_t indexValueBasedOnPawnSquare = uint64_t(mappedSquare - A2) - popCount(belowBB);

	_index += (int64_t)indexValueBasedOnPawnSquare * _sizeInBit;
	_sizeInBit *= (NUMBER_OF_PAWN_POSITIONS - _pawnCount);

	addPawnSquare(mappedSquare);
}


void BitbaseIndex::addNonPawnPieceToIndex(Square mappedSquare) {
	const bitBoard_t belowBB = ((1ULL << mappedSquare) - 1) & _piecesBB;
	uint64_t indexValueBasedOnPieceSquare = uint64_t(mappedSquare) - popCount(belowBB);

	_index += (int64_t)indexValueBasedOnPieceSquare * _sizeInBit;
	_sizeInBit *= (int64_t)BOARD_SIZE - getNumberOfPieces();
	addPieceSquare(mappedSquare);
}

void bubbleSort(array<Square, 10>& squares, uint32_t count) {
	for (int32_t outerLoop = count - 1; outerLoop > 0; outerLoop--) {
		for (int32_t innerLoop = 1; innerLoop <= outerLoop; innerLoop++) {
			if (squares[innerLoop - 1] > squares[innerLoop]) {
				swap(squares[innerLoop - 1], squares[innerLoop]);
			}
		}
	}
}

void  BitbaseIndex::bubbleSortMultiplePiece(array<Square, 10>& squares, uint32_t count) {
	for (int32_t outerLoop = count - 1; outerLoop > 0; outerLoop--) {
		for (int32_t innerLoop = 1; innerLoop <= outerLoop; innerLoop++) {
			if (doublePieceSortValue[int(squares[innerLoop - 1])] > doublePieceSortValue[int(squares[innerLoop])]) {
				swap(squares[innerLoop - 1], squares[innerLoop]);
			}
		}
	}
}

/**
 * Adds another piece to the index
 */
void BitbaseIndex::addPiecesToIndex(array<Square, 10>& squares, uint32_t count, Piece piece)
{
	for (uint32_t index = 0; index < count; index++) {
		squares[index] = mapSquare(squares[index], _mapType);
	}
	if (isPawn(piece)) {
		if (count == 1) {
			addPawnToIndex(squares[0]);
		}
		else {
			bubbleSort(squares, count);
			for (uint32_t index = 0; index < count; index++) {
				addPawnToIndex(squares[index]);
			}
		}
	}
	else {
		if (count == 1) {
			addNonPawnPieceToIndex(squares[0]);
		}
		else {
			bubbleSortMultiplePiece(squares, count);
			for (uint32_t index = 0; index < count; index++) {
				addNonPawnPieceToIndex(squares[index]);
			}
		}
	}
}

/**
 * Gets a list of squares for pieces of the same kind
 * @param pieceList list of pieces
 * @param begin index to start square selection
 * @param squares list of squares to return
 * @returns amount of squares in the list
 */
uint32_t BitbaseIndex::getSquaresOfSameKind(const PieceList& pieceList, uint32_t begin, array<Square, 10>& squares) {
	Piece piece = pieceList.getPiece(begin);
	squares[0] = pieceList.getSquare(begin);
	uint32_t count;
	for (count = 1; begin + count < pieceList.getNumberOfPieces(); count++) {
		if (pieceList.getPiece(begin + count) != piece) break;
		squares[count] = pieceList.getSquare(begin + count);
	}
	return count;
}

/**
 * Sets the bitbase index from a piece List
 */
void BitbaseIndex::set(const PieceList& pieceList, bool wtm) {
	clear();
	initialize(pieceList, wtm);
	uint32_t numberOfPieces = pieceList.getNumberOfPieces();
	array<Square, 10> squares;
	for (uint32_t index = 2; index < numberOfPieces;) {
		uint32_t count = getSquaresOfSameKind(pieceList, index, squares);
		addPiecesToIndex(squares, count, pieceList.getPiece(index));
		index += count;
	}
}

void BitbaseIndex::addPawnSquare(Square square) {
	_pawnCount++;
	_pawnsBB |= 1ULL << square;
	addPieceSquare(square);
}

void BitbaseIndex::addPieceSquare(Square square) {
	_squares[_pieceCount] = square;
	_pieceCount++;
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
		_index += uint64_t(mapTwoKingsToIndexWithPawn[kingIndexNotShrinkedBySymetries]) * COLOR_COUNT;
	}
	else {
		_index += uint64_t(mapTwoKingsToIndexWithoutPawn[kingIndexNotShrinkedBySymetries]) * COLOR_COUNT;
	}
	_sizeInBit *= hasPawn ? NUMBER_OF_TWO_KING_POSITIONS_WITH_PAWN : NUMBER_OF_TWO_KING_POSITIONS_WITHOUT_PAWN;
}
