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

#include "evalpawn.h"

using namespace ChessEval;

value_t EvalPawn::isolatedPawnAmountLookup[EvalPawn::LOOKUP_TABLE_SIZE];
bitBoard_t EvalPawn::kingInfluenceTable[COLOR_AMOUNT][COLOR_AMOUNT][BOARD_SIZE];
bitBoard_t EvalPawn::kingSupportPawnTable[COLOR_AMOUNT][BOARD_SIZE];

EvalPawn::InitStatics EvalPawn::_staticConstructor;

EvalPawn::InitStatics::InitStatics() {
	computeKingInfluenceTable();
	computeIsolatedPawnLookupTable();
	computeKingSupportTable();
}

bool EvalPawn::kingReachesPawn(Square kingPos, Square pawnPos, bool atMove) {
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

bool EvalPawn::kingSupportsPassedPawn(Square kingPos, Square pawnPos, bool atMove) {
	bool result = false;
	if (getRank(kingPos) >= Rank::R7) {
		bool kingOnAdjacentFileOfPawn = getFile(kingPos) == getFile(pawnPos) + 1 || getFile(kingPos) == getFile(pawnPos) - 1;
		bool kingInFrontOfPawn = getRank(kingPos) > getRank(pawnPos) && getRank(kingPos) <= getRank(pawnPos) + 2;
		bool pawnOnRow6AndKingAdjacent = getRank(kingPos) == Rank::R7 && getRank(pawnPos) == Rank::R7;
		result = kingOnAdjacentFileOfPawn && (pawnOnRow6AndKingAdjacent || kingInFrontOfPawn);
	}

	return result;
}

bitBoard_t EvalPawn::computeKingInfluence(Square kingPos, bool atMove, testFunction_t testFunction) {
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

void EvalPawn::computeKingInfluenceTable() {
	for (Square kingPos = A1; kingPos <= H8; ++kingPos) {
		kingInfluenceTable[BLACK][WHITE][kingPos] = computeKingInfluence(kingPos, false, kingReachesPawn);
		kingInfluenceTable[BLACK][BLACK][kingPos] = computeKingInfluence(kingPos, true, kingReachesPawn);
	}
	for (Square kingPos = A1; kingPos <= H8; ++kingPos) {
		kingInfluenceTable[WHITE][WHITE][kingPos] = BitBoardMasks::axialReflection(kingInfluenceTable[BLACK][BLACK][kingPos ^ 0x38]);
		kingInfluenceTable[WHITE][BLACK][kingPos] = BitBoardMasks::axialReflection(kingInfluenceTable[BLACK][WHITE][kingPos ^ 0x38]);
	}
}

void EvalPawn::computeKingSupportTable() {
	for (Square kingPos = A1; kingPos <= H8; ++kingPos) {
		kingSupportPawnTable[WHITE][kingPos] = computeKingInfluence(kingPos, false, kingSupportsPassedPawn);
	}
	for (Square kingPos = A1; kingPos <= H8; ++kingPos) {
		kingSupportPawnTable[BLACK][kingPos] = BitBoardMasks::axialReflection(kingSupportPawnTable[WHITE][kingPos ^ 0x38]);
	}
}

void EvalPawn::computeIsolatedPawnLookupTable() {
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

value_t EvalPawn::print(MoveGenerator& board) {
	EvalResults mobility;
	init<WHITE>(board, mobility);
	init<BLACK>(board, mobility);
	printf("Pawns\n");
	printf("White advanced pawn : %ld\n", computeAdvancedPawnValue<WHITE>());
	printf("Black advanced pawn : %ld\n", computeAdvancedPawnValue<BLACK>());
	printf("White isolated pawn : %ld\n", computeIsolatedPawnValue<WHITE>());
	printf("Black isolated pawn : %ld\n", computeIsolatedPawnValue<BLACK>());
	printf("White double   pawn : %ld\n", computeDoublePawnValue<WHITE>());
	printf("Black double   pawn : %ld\n", computeDoublePawnValue<BLACK>());
	printf("White passed   pawn : %ld\n", computePassedPawnValue<WHITE>(board));
	printf("Black passed   pawn : %ld\n", computePassedPawnValue<BLACK>(board));
	printf("Pawn total          : %ld\n", eval(board, mobility));
	return eval(board, mobility);
}
