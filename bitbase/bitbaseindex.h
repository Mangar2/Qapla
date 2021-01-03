#pragma once

#include "BitBase.h"
#include "GenBoard.h"

typedef pos_t(*getPieceMethod)(GenBoard& board);

class BoardAccess {
public:
	static pos_t getBlackKingPos(GenBoard& board) { return board.kingPos[BLACK]; }
	static pos_t getWhiteKingPos(GenBoard& board) { return board.kingPos[WHITE]; }
	static pos_t getWhitePawnPos(GenBoard& board) { return BitBoardMasks::lsb(board.getPieceBitBoard(WHITE_PAWN)); }
	static pos_t getBlackPawnPos(GenBoard& board) { return BitBoardMasks::lsb(board.getPieceBitBoard(BLACK_PAWN)); }
	static bool getWhiteToMove(GenBoard& board) { return board.whiteToMove; }
	
};

class BitBaseIndex
{
public:

	static uint64_t computeKPKSize() {
		uint64_t maxIndex = computeKingIndex(POS_D8, POS_H8, 0);
		return (maxIndex + 1) * AMOUT_OF_PAWN_POSITIONS * COLORS;
	}

	static uint64_t computeKRKPSize() {
		uint64_t maxIndex = computeKingIndex(POS_D8, POS_H8, 0);
		return (maxIndex + 1) * AMOUT_OF_PAWN_POSITIONS * COLORS * BOARD_SIZE;
	}


	static inline pos_t getWhiteKingPos(GenBoard& board) { return BoardAccess::getWhiteKingPos(board); }
	static inline pos_t getBlackKingPos(GenBoard& board) { return BoardAccess::getBlackKingPos(board); }
	static inline pos_t getWhitePawnPos(GenBoard& board) { return BoardAccess::getWhitePawnPos(board); }
	static inline pos_t getBlackPawnPos(GenBoard& board) { return BoardAccess::getBlackPawnPos(board); }

	static inline pos_t getMappedWhiteKingPos(GenBoard& board) { return mapPosToOtherColor(BoardAccess::getWhiteKingPos(board)); }
	static inline pos_t getMappedBlackKingPos(GenBoard& board) { return mapPosToOtherColor(BoardAccess::getBlackKingPos(board)); }
	static inline pos_t getMappedWhitePawnPos(GenBoard& board) { return mapPosToOtherColor(BoardAccess::getWhitePawnPos(board)); }
	static inline pos_t getMappedBlackPawnPos(GenBoard& board) { return mapPosToOtherColor(BoardAccess::getBlackPawnPos(board)); }

	static uint64_t kpkIndex(GenBoard& board) {
		return computeIndexThreePiecesWithPawn<getWhiteKingPos, getBlackKingPos, getWhitePawnPos, false>(board);
	}

	static uint64_t kkpIndex(GenBoard& board) {
		return computeIndexThreePiecesWithPawn<getMappedBlackKingPos, getMappedWhiteKingPos, getMappedBlackPawnPos, true>(board);
	}
	
	static pos_t computeNextKingPosForPositionsWithPawn(pos_t currentPos) {
		pos_t nextPos = getCol(currentPos) < 3 ?  currentPos + 1 : currentPos + 5;
		return nextPos;
	}

	static bool isAdjacent(pos_t pos1, pos_t pos2) {
		bitBoard_t map = 1ULL << pos1;
		bool result;

		map |= (map & ~BitBoardMasks::COL_A_BITMASK) >> 1;
		map |= (map & ~BitBoardMasks::COL_H_BITMASK) << 1;
		map |= (map & ~BitBoardMasks::ROW_1_BITMASK) >> 8;
		map |= (map & ~BitBoardMasks::ROW_8_BITMASK) << 8;

		result = ((1ULL << pos2) & map) != 0;

		return result;
	}

	static void initStatics() {
		initKingIndexMap();
	}


private:
	BitBaseIndex(void);

	template <getPieceMethod WhiteKing, getPieceMethod BlackKing, getPieceMethod Pawn, bool invertSideToMove>
	static uint64_t computeIndexThreePiecesWithPawn(GenBoard& board) {
		pos_t whiteKingPos = WhiteKing(board);
		pos_t blackKingPos = BlackKing(board);
		pos_t whitePawnPos = Pawn(board);
		bool whiteToMove = BoardAccess::getWhiteToMove(board);
		if (invertSideToMove) { whiteToMove = !whiteToMove; }

		uint32_t mapType = computePosMapType(whiteKingPos, true);
		uint64_t index = computeKingIndex(whiteKingPos, blackKingPos, mapType);
		index *= AMOUT_OF_PAWN_POSITIONS;
		index += computePawnIndex(whitePawnPos, mapType);
		index *= COLORS;
		index += whiteToMove ? 0 : 1;
		return index;
	}


	static void initKingIndexMap() {
		uint32_t index = 0;
		for (pos_t whiteKingPos = 0; whiteKingPos < BOARD_SIZE; whiteKingPos = computeNextKingPosForPositionsWithPawn(whiteKingPos)) {
			for (pos_t blackKingPos = 0; blackKingPos < BOARD_SIZE; blackKingPos++) {
				uint32_t lookupIndex = whiteKingPos + blackKingPos * BOARD_SIZE;
				assert(lookupIndex < BOARD_SIZE * BOARD_SIZE);
				kingIndexMap[lookupIndex] = index;
				if (!isAdjacent(whiteKingPos, blackKingPos)) {
					index++;
				} 
			}
		}
		//printf("%ld\n", index);
	}

	static uint32_t computePosMapType(pos_t pos, bool hasPawn) {
		uint32_t mapType = 0;
		if (getCol(pos) > POS_COL_4) {
			mapType |= 1;
		}
		if (!hasPawn) {
		}
		return mapType;
	}

	static pos_t mapPos(pos_t originalPos, uint32_t mapType) {
		pos_t col = getCol(originalPos);
		pos_t row = getRow(originalPos);
		if ((mapType & 1) == 1) {
			col = BOARD_COL_AMOUNT - 1 - col;
		}
		if ((mapType & 2) == 2) {
			row = BOARD_ROW_AMOUNT - 1 - row;
		}
		return calculatePosInBoard(col, row);
	}

	static pos_t mapPosToOtherColor(pos_t pos) {
		return BOARD_SIZE - 1 - pos;
	}

	static uint64_t computeKingIndex(pos_t whiteKingPos, pos_t blackKingPos, uint32_t mapType) {
		whiteKingPos = mapPos(whiteKingPos, mapType);
		blackKingPos = mapPos(blackKingPos, mapType);
		return kingIndexMap[whiteKingPos + blackKingPos * BOARD_SIZE];
	}

	static uint64_t computePawnIndex(pos_t whitePawnPos, pos_t mapType) {
		return mapPos(whitePawnPos, mapType) - POS_A2;
	}


public:
	static const uint32_t mapToLeftHalf = 1;
	static const uint32_t mapToLowerHalf = 2;
	static const uint32_t mapToTriangle = 4;
	static const uint64_t AMOUNT_OF_TWO_KING_POSITIONS = 3612;
	static const uint64_t AMOUT_OF_PAWN_POSITIONS = BOARD_SIZE - 2 * BOARD_COL_AMOUNT;
	static const uint64_t COLORS = 2;

	static uint32_t kingIndexMap[BOARD_SIZE * BOARD_SIZE];

};

