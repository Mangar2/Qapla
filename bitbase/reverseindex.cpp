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

array<uint32_t, ReverseIndex::NUMBER_OF_TWO_KING_POSITIONS_WITH_PAWN> ReverseIndex::mapIndexToKingSquaresWithPawn;
array<uint32_t, ReverseIndex::NUMBER_OF_TWO_KING_POSITIONS_WITHOUT_PAWN> ReverseIndex::mapIndexToKingSquaresWithoutPawn;
array<uint16_t, ReverseIndex::NUMBER_OF_DOUBLE_PAWN_POSITIONS> ReverseIndex::mapIndexToTwoPawnSquares;
array<uint16_t, ReverseIndex::NUMBER_OF_DOUBLE_PIECE_POSITIONS> ReverseIndex::mapIndexToTwoPieceSquares;

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

void ReverseIndex::computeTwoPawnIndexLookup() {
	uint32_t index = 0;
	for (uint16_t pawn1 = 0; pawn1 < NUMBER_OF_PAWN_POSITIONS - 1; ++pawn1) {
		for (uint16_t pawn2 = pawn1 + 1; pawn2 < NUMBER_OF_PAWN_POSITIONS; ++pawn2) {
			mapIndexToTwoPawnSquares[index] = 
				pawn1 * uint16_t(NUMBER_OF_PAWN_POSITIONS) + pawn2;
			index++;
		}
	}
}

void ReverseIndex::computeTwoPieceIndexLookup() {
	uint32_t index = 0;
	for (uint16_t piece1 = 0; piece1 < REMAINING_PIECE_POSITIONS - 1; ++piece1) {
		for (uint16_t piece2 = piece1 + 1; piece2 < REMAINING_PIECE_POSITIONS; ++piece2) {
			mapIndexToTwoPieceSquares[index] =
				uint16_t(piece1) * uint16_t(REMAINING_PIECE_POSITIONS) + piece2;
			index++;
		}
	}
}

ReverseIndex::InitStatic::InitStatic() {
	uint32_t index = 0;
	computeTwoPawnIndexLookup();
	computeTwoPieceIndexLookup();
	for (Square whiteKingSquare = A1; whiteKingSquare <= H8; whiteKingSquare = computeNextKingSquareForPositionsWithPawn(whiteKingSquare)) {
		for (Square blackKingSquare = A1; blackKingSquare <= H8; ++blackKingSquare) {
			uint32_t lookupIndex = whiteKingSquare + blackKingSquare * BOARD_SIZE;
			assert(lookupIndex < uint32_t(BOARD_SIZE * BOARD_SIZE));
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
	uint32_t piecesToAdd = pieceList.getNumberOfPiecesWithoutPawns() - NUMBER_OF_KINGS;
	uint64_t remainingPiecePositions = NUMBER_OF_PIECE_POSITIONS - NUMBER_OF_KINGS - pieceList.getNumberOfPawns();
	while (pieceList.getNumberOfPieces() > _pieceCount) {
		const uint32_t count = pieceList.getNumberOfSamePieces(_pieceCount);
		if (count == 2) {
			uint16_t doublePosition = mapIndexToTwoPieceSquares[index % NUMBER_OF_DOUBLE_PIECE_POSITIONS];
			index /= NUMBER_OF_DOUBLE_PIECE_POSITIONS;
			Square square1 = computeRealSquare(_piecesBB, Square(doublePosition / REMAINING_PIECE_POSITIONS));
			Square square2 = computeRealSquare(_piecesBB, Square(doublePosition % REMAINING_PIECE_POSITIONS));
			remainingPiecePositions -= count;
			addPieceSquare(square1);
			addPieceSquare(square2);
		}
		else {
			for (uint32_t piece = 0; piece < count; ++piece, --remainingPiecePositions) {
				const Square square = computeRealSquare(_piecesBB, Square(index % remainingPiecePositions));
				index /= remainingPiecePositions;
				_isLegal = _isLegal && !(square > H8);
				addPieceSquare(square);
			}
		}
	}
}

uint64_t ReverseIndex::setPawnsByIndex(uint64_t index, const PieceList& pieceList) {
	uint32_t numberOfPawns = pieceList.getNumberOfPawns();
	if (numberOfPawns == 0) {
		return index;
	}
	uint64_t remainingPawnPositions = NUMBER_OF_PAWN_POSITIONS;
	while (isPawn(pieceList.getPiece(_pieceCount))) {
		const uint32_t count = pieceList.getNumberOfSamePieces(_pieceCount);
		if (count == 2) {
			uint16_t doublePosition = mapIndexToTwoPawnSquares[index % NUMBER_OF_DOUBLE_PAWN_POSITIONS];
			index /= NUMBER_OF_DOUBLE_PAWN_POSITIONS;
			addPieceSquare(Square(doublePosition / NUMBER_OF_PAWN_POSITIONS) + A2);
			addPieceSquare(Square(doublePosition % NUMBER_OF_PAWN_POSITIONS) + A2);
			remainingPawnPositions -= count;
		}
		else {
			for (uint32_t pawn = 0; pawn < count; ++pawn, --remainingPawnPositions) {
				bitBoard_t pawnsBB = _piecesBB & 0x00FFFFFFFFFFFF00;
				const Square square = computeRealSquare(pawnsBB, A2 + Square(index % remainingPawnPositions));
				index /= remainingPawnPositions;
				_isLegal = _isLegal && !(square > H7);
				addPieceSquare(square);
			}
		}
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

Square ReverseIndex::computeRealSquare(bitBoard_t checkPieces, Square rawSquare) {
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

void ReverseIndex::addPieceSquare(Square square) {
	_squares[_pieceCount] = square;
	_pieceCount++;
	bitBoard_t squareBB = 1ULL << square;
	_isLegal = _isLegal && (_piecesBB & squareBB) == 0;
	_piecesBB |= squareBB;
}

