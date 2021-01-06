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
 * Functions and Masks to work with bitboards for chess
 */

#ifndef __BITBOARDMASKS_H
#define __BITBOARDMASKS_H

#include <assert.h>
#include "../basics/types.h"
#include "../basics/move.h"

#if (defined(__INTEL_COMPILER) || defined(_MSC_VER))
#  include <nmmintrin.h> // Intel and Microsoft header for _mm_popcnt_u64()
#endif

#if defined(_WIN64) && defined(_MSC_VER) // No Makefile used
#  include <intrin.h> // Microsoft header for _BitScanForward64()
#  define IS_64BIT
#endif

using namespace ChessBasics;

namespace ChessMoveGenerator {


	class BitBoardMasks
	{
	public:
		// CTor 
		BitBoardMasks()
		{
		}
		// DTor
		~BitBoardMasks()
		{
		}


#if defined(__GNUC__)

		inline static Square lsb(bitBoard_t bitBoard) {
			assert(bitBoard);
			return Square(__builtin_ctzll(bitBoard));
		}

		inline Square msb(bitBoard_t bitBoard) {
			assert(bitBoard);
			return Square(63 - __builtin_clzll(bitBoard));
		}

#elif defined(_WIN64) && defined(_MSC_VER)

		inline static Square lsb(bitBoard_t bitBoard) {
			assert(bitBoard);
			unsigned long pos;
			_BitScanForward64(&pos, bitBoard);
			return (Square)pos;
		}

		inline static Square msb(bitBoard_t bitBoard) {
			assert(bitBoard);
			unsigned long pos;
			_BitScanReverse64(&pos, bitBoard);
			return (Square)pos;
		}
#endif

		/**
		 * Removes the least significant bit
		 */
		inline static uint32_t popLSB(bitBoard_t& bitBoard)
		{
			uint32_t res = lsb(bitBoard);
			bitBoard &= bitBoard - 1;
			return res;
		}

		static uint8_t popCountInFirstRank(bitBoard_t bitBoard) {
			return popCountLookup[bitBoard & 0xFF];
		}

		static value_t popCount(bitBoard_t bitBoard) {
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)

			return (int)_mm_popcnt_u64(bitBoard);

#else // Assumed gcc or compatible compiler

			return __builtin_popcountll(bitBoard);

#endif
		}

		/**
		 * Counts the amout of set bits in a 64 bit variables - only performant for
		 * sparcely populated bitboards. (1-3 bits set).
		 */
		static uint8_t popCountForSparcelyPopulatedBitBoards(bitBoard_t bitBoard) {
			uint8_t popCount = 0;
			for (; bitBoard != 0; bitBoard &= bitBoard - 1) {
				popCount++;
			}
			return popCount;
		}


		/**
		 * Computes the attack mask for pawns
		 */
		template <uint32_t COLOR>
		inline static bitBoard_t computePawnAttackMask(bitBoard_t pawns) {
			bitBoard_t attack = shiftPawnBitBoard<COLOR, NW>(pawns) | shiftPawnBitBoard<COLOR, NE>(pawns);
			return attack;
		}

		/**
		 * Shifts the pawn bitboard by one move
		 */
		template<uint32_t COLOR, Square DIRECTION>
		inline static bitBoard_t shiftPawnBitBoard(bitBoard_t bitboard) {
			if (COLOR == WHITE) {
				return shift<DIRECTION>(bitboard);
			}
			else {
				return shift<-DIRECTION>(bitboard);
			}
		}

		/**
		 * Calculate the axial reflection of a bitboard
		 */
		static bitBoard_t axialReflection(bitBoard_t bitBoard) {
			bitBoard_t reflectedBitBoard = 0;
			for (File file = File::A; file < File::H; ++file) {
				reflectedBitBoard |= bitBoard & 0xFF;
				reflectedBitBoard <<= 8;
				bitBoard >>= 8;
			}
			return reflectedBitBoard;
		}


	protected:

		static const int32_t LOOKUP_BITS = 8;
		static const int32_t LOOKUP_SIZE = 1 << LOOKUP_BITS;
		static const int32_t LOOKUP_MASK = LOOKUP_SIZE - 1;

		// ---------------------- Mapping tables ----------------------------------
		static int8_t mLeastSignificantBit[LOOKUP_SIZE];
		static const int32_t cLSB[64];

		static uint8_t popCountLookup[LOOKUP_SIZE];

	public:
		// map from position to knight move bits
		static bitBoard_t knightMoves[BOARD_SIZE];
		// map from position to king move bits
		static bitBoard_t kingMoves[BOARD_SIZE];
		// map from position to pawn attack bits
		static bitBoard_t pawnCaptures[2][BOARD_SIZE];
		//static bitBoard_t mBPawnCaptures[BOARD_SIZE];
		// map from pawn-target position to adjacent bits on ep file
		static bitBoard_t mEPMask[BOARD_SIZE];
		// map holding rays from a king position to a bishop, rook or queen
		// Index ist by KingPos + PiecePos * 64;
		static bitBoard_t mRay[BOARD_SIZE * BOARD_SIZE];
		// map holding a full size ray with two pieces on it
		// Index ist by KingPos + PiecePos * 64;
		static bitBoard_t mFullRay[BOARD_SIZE * BOARD_SIZE];

		// ---------------------- Helpers -----------------------------------------
		
		/**
		 * Generates all possible targets for a knight
		 */
		static bitBoard_t genKnightTargetBoard(Square square);

		/**
		 * Generates all possible targets for a king
		 */
		static bitBoard_t genKingTargetBoard(Square square);

		static const bitBoard_t RANK_1_BITMASK = 0x00000000000000FF;
		static const bitBoard_t RANK_2_BITMASK = 0x000000000000FF00;
		static const bitBoard_t RANK_3_BITMASK = 0x0000000000FF0000;
		static const bitBoard_t RANK_4_BITMASK = 0x00000000FF000000;
		static const bitBoard_t RANK_5_BITMASK = 0x000000FF00000000;
		static const bitBoard_t RANK_6_BITMASK = 0x0000FF0000000000;
		static const bitBoard_t RANK_7_BITMASK = 0x00FF000000000000;
		static const bitBoard_t RANK_8_BITMASK = 0xFF00000000000000;

		static const bitBoard_t FILE_A_BITMASK = 0x0101010101010101;
		static const bitBoard_t FILE_B_BITMASK = 0x0202020202020202;
		static const bitBoard_t FILE_C_BITMASK = 0x0404040404040404;
		static const bitBoard_t FILE_D_BITMASK = 0x0808080808080808;
		static const bitBoard_t FILE_E_BITMASK = 0x1010101010101010;
		static const bitBoard_t FILE_F_BITMASK = 0x2020202020202020;
		static const bitBoard_t FILE_G_BITMASK = 0x4040404040404040;
		static const bitBoard_t FILE_H_BITMASK = 0x8080808080808080;

		/**
		 * Shifts a board in a direction
		 * @param bitBoard board to shift
		 * @returns shifted board
		 */
		template<Square DIRECTION>
		constexpr static bitBoard_t shift(bitBoard_t bitBoard) {
			switch (DIRECTION) {
			case NORTH: return bitBoard << NORTH;
			case NORTH * 2: return bitBoard << (NORTH * 2);
			case SOUTH: return bitBoard >> -SOUTH;
			case SOUTH * 2: return bitBoard >> (-SOUTH * 2);
			case EAST: return (bitBoard & ~FILE_H_BITMASK) << EAST;
			case WEST: return (bitBoard & ~FILE_A_BITMASK) >> -WEST;
			case NW: return (bitBoard & ~FILE_A_BITMASK) << NW;
			case NE: return (bitBoard & ~FILE_H_BITMASK) << NE;
			case SW: return (bitBoard & ~FILE_A_BITMASK) >> -SW;
			case SE: return (bitBoard & ~FILE_H_BITMASK) >> -SE;
			default: return bitBoard;
			}
		}


		/**
		 * Logical or of a bitboard moved to all 4 directions
		 */
		inline static bitBoard_t moveInAllDirections(bitBoard_t board) {
			board |= shift<WEST>(board) | shift<EAST>(board);
			board |= shift<NORTH>(board) | shift<SOUTH>(board);
			return board;
		}

	private:
		static void initPopCount();
		static void initAttackRay();

		// Initializes masks
		static struct InitStatics {
			InitStatics();
		} _staticConstructor;

	};
}

#endif // BITBOARDMASKS_H
