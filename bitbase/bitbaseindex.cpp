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
array<uint32_t, BitbaseIndex::AMOUNT_OF_TWO_KING_POSITIONS_WITH_PAWN> BitbaseIndex::mapIndexToKingSquaresWithPawn;
array<uint32_t, BitbaseIndex::AMOUNT_OF_TWO_KING_POSITIONS_WITHOUT_PAWN> BitbaseIndex::mapIndexToKingSquaresWithoutPawn;
BitbaseIndex::InitStatic BitbaseIndex::_staticConstructor;

/**
 * Checks, if two squares are adjacent
 */
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
			assert(index < AMOUNT_OF_TWO_KING_POSITIONS_WITHOUT_PAWN);
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
		amountOfKingPositions = AMOUNT_OF_TWO_KING_POSITIONS_WITH_PAWN;
		posIndex = mapIndexToKingSquaresWithPawn[index % amountOfKingPositions];
	}
	else
	{
		amountOfKingPositions = AMOUNT_OF_TWO_KING_POSITIONS_WITHOUT_PAWN;
		posIndex = mapIndexToKingSquaresWithoutPawn[index % amountOfKingPositions];
	}
	addPieceSquare(Square(posIndex % BOARD_SIZE));
	addPieceSquare(Square(posIndex / BOARD_SIZE));
	index /= amountOfKingPositions;
	_sizeInBit = amountOfKingPositions * _sizeInBit;
	return index;
}

bool BitbaseIndex::setPieceSquaresByIndex(uint64_t index, uint32_t pawnAmount, uint32_t nonPawnPieceAmount) {
	clear();
	uint32_t posIndex;
	bool legalPosition = true;
	_index = index;
	_wtm = (_index % COLORS) == 0;
	_index /= COLORS;
	_sizeInBit = COLORS;
	_index = setKingSquaresByIndex(_index, pawnAmount > 0);

	for (uint32_t pawn = 0; pawn < pawnAmount; pawn++) {
		const uint32_t numberOfPieces = getNumberOfPieces();
		const uint32_t relevantPawnPositionNumber = NUMBER_OF_PAWN_POSITIONS - numberOfPieces;
		posIndex = _index % relevantPawnPositionNumber;
		_index /= relevantPawnPositionNumber;
		_sizeInBit *= relevantPawnPositionNumber;
		addPieceSquare(computesRealSquare(_piecesBB >> 8, Square(posIndex)) + A2);
		if (_squares[numberOfPieces - 1] > H8) {
			legalPosition = false;
		}
	}

	const uint32_t kingAmount = 2;
	for (uint32_t piece = 0; piece < nonPawnPieceAmount - kingAmount; piece++) {
		const uint32_t numberOfPieces = getNumberOfPieces();
		posIndex = _index % (AMOUT_OF_PIECE_POSITIONS - numberOfPieces);
		_index /= AMOUT_OF_PIECE_POSITIONS - numberOfPieces;
		_sizeInBit *= AMOUT_OF_PIECE_POSITIONS - numberOfPieces;
		addPieceSquare(computesRealSquare(_piecesBB, Square(posIndex)));
		if (_squares[numberOfPieces - 1] >= BOARD_SIZE) {
			legalPosition = false;
		}
	}
	return legalPosition;
}
