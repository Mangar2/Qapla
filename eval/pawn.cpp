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

#include "pawn.h"

using namespace ChessEval;

value_t Pawn::isolatedPawnAmountLookup[Pawn::LOOKUP_TABLE_SIZE];
bitBoard_t Pawn::kingInfluenceTable[COLOR_AMOUNT][COLOR_AMOUNT][BOARD_SIZE];
bitBoard_t Pawn::kingSupportPawnTable[COLOR_AMOUNT][BOARD_SIZE];
PawnTT Pawn::_tt;

Pawn::InitStatics Pawn::_staticConstructor;

Pawn::InitStatics::InitStatics() {
	computeKingInfluenceTable();
	computeIsolatedPawnLookupTable();
	computeKingSupportTable();
	_tt.setSizeInKilobytes(1024);
}

bool Pawn::kingReachesPawn(Square kingPos, Square pawnPos, bool atMove) {
	Rank kingRankAfterFirstHalfmove = getRank(kingPos);
	int32_t distanceToPromote = int32_t(Rank::R8 - uint32_t(getRank(pawnPos)));
	int32_t colKingPawnDistance = abs(int32_t(getFile(kingPos)) - int32_t(getFile(pawnPos)));
	if (atMove) {
		colKingPawnDistance--;
		++kingRankAfterFirstHalfmove;
	}
	bool result = kingRankAfterFirstHalfmove >= getRank(pawnPos) && colKingPawnDistance <= distanceToPromote;
	return result;
}

bool Pawn::kingSupportsPassedPawn(Square kingPos, Square pawnPos, bool atMove) {
	bool result = false;
	if (getRank(kingPos) >= Rank::R7) {
		bool kingOnAdjacentFileOfPawn = getFile(kingPos) == getFile(pawnPos) + 1 || getFile(kingPos) == getFile(pawnPos) - 1;
		bool kingInFrontOfPawn = getRank(kingPos) > getRank(pawnPos) && getRank(kingPos) <= getRank(pawnPos) + 2;
		bool pawnOnRow6AndKingAdjacent = getRank(kingPos) == Rank::R7 && getRank(pawnPos) == Rank::R7;
		result = kingOnAdjacentFileOfPawn && (pawnOnRow6AndKingAdjacent || kingInFrontOfPawn);
	}

	return result;
}

bitBoard_t Pawn::computeKingInfluence(Square kingPos, bool atMove, testFunction_t testFunction) {
	bitBoard_t kingInfluence = 1ULL << kingPos;
	for (Square pawnPos = A3; pawnPos <= H8; ++pawnPos) {
		if (testFunction(kingPos, pawnPos, atMove)) {
			kingInfluence |= 1ULL << pawnPos;
			if (pawnPos < A4) {
				kingInfluence |= 1ULL << (pawnPos );
			}
		}
	}
	return kingInfluence;
}

void Pawn::computeKingInfluenceTable() {
	for (Square kingPos = A1; kingPos <= H8; ++kingPos) {
		kingInfluenceTable[BLACK][WHITE][kingPos] = computeKingInfluence(kingPos, false, kingReachesPawn);
		kingInfluenceTable[BLACK][BLACK][kingPos] = computeKingInfluence(kingPos, true, kingReachesPawn);
	}
	for (Square kingPos = A1; kingPos <= H8; ++kingPos) {
		kingInfluenceTable[WHITE][WHITE][kingPos] = BitBoardMasks::axialReflection(kingInfluenceTable[BLACK][BLACK][kingPos ^ 0x38]);
		kingInfluenceTable[WHITE][BLACK][kingPos] = BitBoardMasks::axialReflection(kingInfluenceTable[BLACK][WHITE][kingPos ^ 0x38]);
	}
}

void Pawn::computeKingSupportTable() {
	for (Square kingPos = A1; kingPos <= H8; ++kingPos) {
		kingSupportPawnTable[WHITE][kingPos] = computeKingInfluence(kingPos, false, kingSupportsPassedPawn);
	}
	for (Square kingPos = A1; kingPos <= H8; ++kingPos) {
		kingSupportPawnTable[BLACK][kingPos] = BitBoardMasks::axialReflection(kingSupportPawnTable[WHITE][kingPos ^ 0x38]);
	}
}

void Pawn::computeIsolatedPawnLookupTable() {
	uint32_t pawnMask;
	isolatedPawnAmountLookup[0] = 0;
	for (pawnMask = 1; pawnMask < LOOKUP_TABLE_SIZE; pawnMask++) {
		uint32_t leftPawnMask = pawnMask / 2;
		value_t isolatedPawnAmount = isolatedPawnAmountLookup[leftPawnMask];
		if ((pawnMask & 1) == 1) {
			if ((leftPawnMask & 1) == 0) {
				isolatedPawnAmount++;
			}
			else if ((leftPawnMask & 2) == 0) {
				isolatedPawnAmount--;
			}
		}
		isolatedPawnAmountLookup[pawnMask] = isolatedPawnAmount;
	}
}

