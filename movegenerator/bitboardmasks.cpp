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

#include "bitboardmasks.h"

using namespace QaplaMoveGenerator;

// map from pawn-target position to adjacent bits on ep row
bitBoard_t BitBoardMasks::mEPMask[BOARD_SIZE];

// LSB Version 2, Lookup table
int8_t BitBoardMasks::mLeastSignificantBit[LOOKUP_SIZE];

// Knight move table
bitBoard_t BitBoardMasks::knightMoves[BOARD_SIZE];

// King move table
bitBoard_t BitBoardMasks::kingMoves[BOARD_SIZE];

// PawnAttack table
bitBoard_t BitBoardMasks::pawnCaptures[2][BOARD_SIZE];
//bitBoard_t BitBoardMasks::pawnCaptures[BLACK][BOARD_SIZE];

// Ray tables
bitBoard_t BitBoardMasks::mRay[BOARD_SIZE * BOARD_SIZE];
bitBoard_t BitBoardMasks::mFullRay[BOARD_SIZE * BOARD_SIZE];

uint8_t BitBoardMasks::popCountLookup[LOOKUP_SIZE];

BitBoardMasks::InitStatics BitBoardMasks::_staticConstructor;


// -------------------------- GenKingTargetBoard ------------------------------
bitBoard_t BitBoardMasks::genKingTargetBoard(Square square)
{
	bitBoard_t squareBB = 1ULL << square;

	bitBoard_t aResult;
	aResult = shift<NW>(squareBB) | shift<NORTH>(squareBB) | shift<NE>(squareBB) |
		shift<EAST>(squareBB) | shift<WEST>(squareBB) |
		shift<SW>(squareBB) | shift<SOUTH>(squareBB) | shift<SE>(squareBB);

	return aResult;
}

// -------------------------- GenKnightTargetBoard ----------------------------
bitBoard_t BitBoardMasks::genKnightTargetBoard(Square square)
{
	bitBoard_t squareBB = 1ULL << square;

	return
		shift<NW>(shift<NORTH>(squareBB)) |
		shift<NE>(shift<NORTH>(squareBB)) |
		shift<NW>(shift<WEST>(squareBB)) |
		shift<NE>(shift<EAST>(squareBB)) |
		shift<SW>(shift<SOUTH>(squareBB)) |
		shift<SE>(shift<SOUTH>(squareBB)) |
		shift<SW>(shift<WEST>(squareBB)) |
		shift<SE>(shift<EAST>(squareBB));
}

// -------------------------- InitStatics -------------------------------------

void BitBoardMasks::initPopCount() {
	popCountLookup[0] = 0;
	for (uint32_t i = 1; i < LOOKUP_SIZE; i++) {
		popCountLookup[i] = popCountLookup[i / 2] + (i & 1);
	}
}

void BitBoardMasks::initAttackRay() {
	Square square;
	Square square2;
	int32_t dir;

	const int32_t MOVE_DIRECTION[8][2] =
		{ { 1,0 }, { -1,0 }, { 0,1 }, { 0,-1 }, { 1,1 }, { 1,-1 }, { -1,1 }, { -1,-1 } };

	square = A1;

	for (square = A1; square <= H8; ++square)
	{
		for (square2 = A1; square2 <= H8; ++square2)
		{
			// first initializes the ray, only setting target bit
			mRay[square + square2 * 64] = 1ULL << (square + square2 * 64);
			// full ray ist set to zero
			mFullRay[square + square2 * 64] = 0ULL;
		}
	}
	for (square = A1; square <= H8; ++square)
	{
		// Second loop through colums and diagonals
		for (dir = 0; dir < 8; dir++)
		{
			bitBoard_t aBoard = 0;
			File file = getFile(square) + MOVE_DIRECTION[dir][0];
			Rank rank = getRank(square) + MOVE_DIRECTION[dir][1];
			for (; isFileInBoard(file) && isRankInBoard(rank); file += MOVE_DIRECTION[dir][0], rank += MOVE_DIRECTION[dir][1])
			{
				aBoard |= 1ULL << computeSquare(file, rank);
				mRay[square + computeSquare(file, rank) * 64] = aBoard;
			}
			// The board has stored the full ray to the border of the board, set it to every field in ray
			file = getFile(square) + MOVE_DIRECTION[dir][0];
			rank = getRank(square) + MOVE_DIRECTION[dir][1];
			for (; isFileInBoard(file) && isRankInBoard(rank); file += MOVE_DIRECTION[dir][0], rank += MOVE_DIRECTION[dir][1])
			{
				mFullRay[square + computeSquare(file, rank) * 64] = aBoard;
			}

		}
	}
}

BitBoardMasks::InitStatics::InitStatics()
{

	Square square;

	initPopCount();
	initAttackRay();
	for (square = A1; square <= H8; ++square)
	{
		knightMoves[square] = genKnightTargetBoard(square);
		kingMoves[square] = genKingTargetBoard(square);
		// Set pawn capture masks
		pawnCaptures[WHITE][square] = 0;
		pawnCaptures[BLACK][square] = 0;
		mEPMask[square] = 0;
		if (getRank(square) > Rank::R1 && getRank(square) < Rank::R8)
		{
			pawnCaptures[WHITE][square] = 5ULL << (square + 7);
			pawnCaptures[BLACK][square] = 5ULL << (square - 9);
			mEPMask[square] = 5ULL << (square - 1);
			if (getFile(square) == File::A || getFile(square) == File::H) 
			{
				pawnCaptures[WHITE][square] &= ~(FILE_H_BITMASK | FILE_A_BITMASK);
				pawnCaptures[BLACK][square] &= ~(FILE_H_BITMASK | FILE_A_BITMASK);
				mEPMask[square] &= ~(FILE_H_BITMASK | FILE_A_BITMASK);
			}
		}
		mEPMask[square] = 0ULL;
	}
	for (square = A4; square < A6; ++square)
	{
		mEPMask[square] = 0x05ULL << (square - 1);
		mEPMask[square] &= 0XFFULL << (square - (square % NORTH));
	}

}
