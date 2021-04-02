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

#include "reverseindex.h"

using namespace QaplaBitbase;

array<uint32_t, BOARD_SIZE* BOARD_SIZE> ReverseIndex::mapTwoKingsToIndexWithPawn;
array<uint32_t, BOARD_SIZE* BOARD_SIZE> ReverseIndex::mapTwoKingsToIndexWithoutPawn;
array<uint32_t, ReverseIndex::NUMBER_OF_TWO_KING_POSITIONS_WITH_PAWN> ReverseIndex::mapIndexToKingSquaresWithPawn;
array<uint32_t, ReverseIndex::NUMBER_OF_TWO_KING_POSITIONS_WITHOUT_PAWN> ReverseIndex::mapIndexToKingSquaresWithoutPawn;
ReverseIndex::InitStatic ReverseIndex::_staticConstructor;


bool ReverseIndex::isAdjacent(Square pos1, Square pos2) {
	bitBoard_t map = 1ULL << pos1;
	bool result;

	map |= BitBoardMasks::shift<WEST>(map);
	map |= BitBoardMasks::shift<EAST>(map);
	map |= BitBoardMasks::shift<SOUTH>(map);
	map |= BitBoardMasks::shift<NORTH>(map);

	result = ((1ULL << pos2) & map) != 0;
	return result;
}

ReverseIndex::InitStatic::InitStatic() {
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

uint64_t ReverseIndex::setKingSquaresByIndex(uint64_t index, bool hasPawn) {
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

void ReverseIndex::setPiecesByIndex(uint64_t index, const PieceList& pieceList) {
	bool hasPawns = pieceList.getNumberOfPawns() > 0;
	uint32_t piecesToAdd = pieceList.getNumberOfPiecesWithoutPawns() - KING_COUNT;
	uint64_t remainingPiecePositions = NUMBER_OF_PIECE_POSITIONS - KING_COUNT - pieceList.getNumberOfPawns();
	for (; piecesToAdd > 0; --piecesToAdd, --remainingPiecePositions) {
		const Square square = computesRealSquare(_piecesBB, Square(index % remainingPiecePositions));
		index /= remainingPiecePositions;
		addPieceSquare(square);
	}
}

uint64_t ReverseIndex::setPawnsByIndex(uint64_t index, const PieceList& pieceList) {
	const bool hasPawns = pieceList.getNumberOfPawns() > 0;
	uint64_t remainingPawnPositions = NUMBER_OF_PAWN_POSITIONS;
	for (uint32_t pawn = 0; pawn < pieceList.getNumberOfPawns(); ++pawn, --remainingPawnPositions) {
		const Square square = computesRealSquare(_pawnsBB, A2 + Square(index % remainingPawnPositions));

		index /= remainingPawnPositions;
		addPawnSquare(square);
	}
	return index;
}

void ReverseIndex::setSquares(uint64_t index, const PieceList& pieceList) {
	_wtm = (index % COLOR_COUNT) == 0;
	index /= COLOR_COUNT;

	uint64_t numberOfKingPositions = setKingSquaresByIndex(index, pieceList.getNumberOfPawns() > 0);
	index /= numberOfKingPositions;

	index = setPawnsByIndex(index, pieceList);
	setPiecesByIndex(index, pieceList);
}


Square ReverseIndex::computeNextKingSquareForPositionsWithPawn(Square currentSquare) {
	Square nextSquare = getFile(currentSquare) < File::D ? currentSquare + 1 : currentSquare + 5;
	return nextSquare;
}

Square ReverseIndex::computesRealSquare(bitBoard_t checkPieces, Square rawSquare) {
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

uint32_t ReverseIndex::popCount(bitBoard_t bitBoard) {
	uint32_t result = 0;
	for (; bitBoard; bitBoard &= (bitBoard - 1)) {
		result++;
	}
	return result;
}

void ReverseIndex::addPawnSquare(Square square) {
	_pawnCount++;
	_pawnsBB |= 1ULL << square;
	addPieceSquare(square);
}

void ReverseIndex::addPieceSquare(Square square) {
	_squares[_pieceCount] = square;
	_pieceCount++;
	_piecesBB |= 1ULL << square;
}

