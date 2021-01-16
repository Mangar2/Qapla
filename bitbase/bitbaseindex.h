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
 * Calculates an index from a board position to a bitbase. The index is calculated by multiplying 
 * 1. An index for the positions of the two kings supressing illegal positions where kings are adjacent.
 * 
 */

#include "bitbase.h"
#include "../basics/types.h"
#include "../movegenerator/movegenerator.h"

using namespace ChessBasics;
using namespace ChessMoveGenerator;

typedef Square(*getPieceMethod)(MoveGenerator& board);

class BoardAccess {
public:
	template <Piece COLOR>
	static Square getKingPos(MoveGenerator& board) { return board.getKingSquare<COLOR>(); }
	template <Piece COLOR>
	static Square getPawnPos(MoveGenerator& board) { return BitBoardMasks::lsb(board.getPieceBB(COLOR + PAWN)); }
	static bool getWhiteToMove(MoveGenerator& board) { return board.isWhiteToMove(); }
};

class BitBaseIndex
{
public:

	/**
	 * Computes the size of a KPK bitbase
	 */
	static uint64_t computeKPKSize() {
		uint64_t maxIndex = computeKingIndex(D8, H8, 0);
		return (maxIndex + 1) * AMOUT_OF_PAWN_POSITIONS * COLORS;
	}

	/**
	 * Computes the size of a KRKP bitbase
	 */
	static uint64_t computeKRKPSize() {
		uint64_t maxIndex = computeKingIndex(D8, H8, 0);
		return (maxIndex + 1) * AMOUT_OF_PAWN_POSITIONS * COLORS * BOARD_SIZE;
	}

	static inline Square getWhiteKingPos(MoveGenerator& board) { return BoardAccess::getKingPos<WHITE>(board); }
	static inline Square getBlackKingPos(MoveGenerator& board) { return BoardAccess::getKingPos<BLACK>(board); }
	static inline Square getWhitePawnPos(MoveGenerator& board) { return BoardAccess::getPawnPos<WHITE>(board); }
	static inline Square getBlackPawnPos(MoveGenerator& board) { return BoardAccess::getPawnPos<BLACK>(board); }

	static inline Square getMappedWhiteKingPos(MoveGenerator& board) { return mapPosToOtherColor(BoardAccess::getKingPos<WHITE>(board)); }
	static inline Square getMappedBlackKingPos(MoveGenerator& board) { return mapPosToOtherColor(BoardAccess::getKingPos<BLACK>(board)); }
	static inline Square getMappedWhitePawnPos(MoveGenerator& board) { return mapPosToOtherColor(BoardAccess::getPawnPos<WHITE>(board)); }
	static inline Square getMappedBlackPawnPos(MoveGenerator& board) { return mapPosToOtherColor(BoardAccess::getPawnPos<BLACK>(board)); }

	static uint64_t kpkIndex(MoveGenerator& board) {
		return computeIndexThreePiecesWithPawn<getWhiteKingPos, getBlackKingPos, getWhitePawnPos, false>(board);
	}

	static uint64_t kkpIndex(MoveGenerator& board) {
		return computeIndexThreePiecesWithPawn<getMappedBlackKingPos, getMappedWhiteKingPos, getMappedBlackPawnPos, true>(board);
	}
	
	/**
	 * Computes the next possible king position for positions with pawns
	 */
	static Square computeNextKingPosForPositionsWithPawn(Square currentPos) {
		Square nextPos = getFile(currentPos) < File::D ?  currentPos + 1 : currentPos + 5;
		return nextPos;
	}

	/**
	 * Checks, if two squares are adjacent
	 */
	static bool isAdjacent(Square pos1, Square pos2) {
		bitBoard_t map = 1ULL << pos1;
		bool result;

		map |= BitBoardMasks::shift<WEST>(map);
		map |= BitBoardMasks::shift<EAST>(map);
		map |= BitBoardMasks::shift<SOUTH>(map);
		map |= BitBoardMasks::shift<NORTH>(map);

		result = ((1ULL << pos2) & map) != 0;
		return result;
	}

private:
	BitBaseIndex(void);

	template <getPieceMethod WhiteKing, getPieceMethod BlackKing, getPieceMethod Pawn, bool invertSideToMove>
	static uint64_t computeIndexThreePiecesWithPawn(MoveGenerator& board) {
		Square whiteKingPos = WhiteKing(board);
		Square blackKingPos = BlackKing(board);
		Square whitePawnPos = Pawn(board);
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


	/**
	 * One factor of the index is the position of the two kings. A map is used to calculate the
	 * right index for the king positions to remove all illegal positions - with two adjacent kings - 
	 * from the index. 
	 */
	static void initKingIndexMap() {
		uint32_t index = 0;
		for (Square whiteKingPos = A1; whiteKingPos <= H8; whiteKingPos = computeNextKingPosForPositionsWithPawn(whiteKingPos)) {
			for (Square blackKingPos = A1; blackKingPos <= H8; ++blackKingPos) {
				uint32_t lookupIndex = whiteKingPos + blackKingPos * BOARD_SIZE;
				assert(lookupIndex < BOARD_SIZE * BOARD_SIZE);
				kingIndexMap[lookupIndex] = index;
				if (!isAdjacent(whiteKingPos, blackKingPos)) {
					index++;
				} 
			}
		}
	}

	/**
	 * Computes the type of the mapping
	 */
	static uint32_t computePosMapType(Square pos, bool hasPawn) {
		uint32_t mapType = 0;
		if (getFile(pos) > File::D) {
			mapType |= 1;
		}
		if (!hasPawn) {
		}
		return mapType;
	}

	/**
	 * Maps a Square to the right symetric square. The Map type has two bits
	 * Bit 1 for file-mapping and Bit 2 for rank mapping
	 * 
	 */
	static Square mapPos(Square originalPos, uint32_t mapType) {
		File file = getFile(originalPos);
		Rank rank = getRank(originalPos);
		if ((mapType & MAP_FILE) != 0) {
			file = File::H - file;
		}
		if ((mapType & MAP_RANK) != 0) {
			rank = Rank::R8 - rank;
		}
		return computeSquare(file, rank);
	}

	/**
	 * Switch the board-side
	 */
	static Square mapPosToOtherColor(Square pos) {
		return H8 - pos;
	}

	/**
	 * Computes the right king index by first mapping the positions and then calculate the 
	 * kings index.
	 */
	static uint64_t computeKingIndex(Square whiteKingPos, Square blackKingPos, uint32_t mapType) {
		whiteKingPos = mapPos(whiteKingPos, mapType);
		blackKingPos = mapPos(blackKingPos, mapType);
		return kingIndexMap[whiteKingPos + blackKingPos * BOARD_SIZE];
	}

	/**
	 * Computes the pawn index by mapping to the right symetry and reducing it 
	 * by the minimal white pawn square (A2)
	 */
	static uint64_t computePawnIndex(Square whitePawnPos, uint32_t mapType) {
		return mapPos(whitePawnPos, mapType) - A2;
	}

	/**
	 * Initializes static lookup maps
	 */
	static struct InitStatic {
		InitStatic() {
			initKingIndexMap();
		}
	} _staticConstructor;


public:
	static const uint32_t MAP_FILE = 1;
	static const uint32_t MAP_RANK = 2;
	static const uint32_t MAP_DIAGONAL = 4;
	static const uint64_t AMOUNT_OF_TWO_KING_POSITIONS = 3612;
	static const uint64_t AMOUT_OF_PAWN_POSITIONS = H8 - 2 * A2;
	static const uint64_t COLORS = 2;

	static array<uint32_t, BOARD_SIZE* BOARD_SIZE> kingIndexMap;

};

