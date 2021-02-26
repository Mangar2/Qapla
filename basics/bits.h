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
 * Implements bit handling routines
 */

#ifndef __BITS_H
#define __BITS_H

#include <assert.h>
#include "types.h"

#if (defined(__INTEL_COMPILER) || defined(_MSC_VER))
#  include <nmmintrin.h> // Intel and Microsoft header for _mm_popcnt_u64()
#endif

#if defined(_WIN64) && defined(_MSC_VER) // No Makefile used
#  include <intrin.h> // Microsoft header for _BitScanForward64()
#  define IS_64BIT
#endif

namespace ChessBasics {

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
	 * Counts the amout of set bits in a 64 bit variables - only performant for
	 * sparcely populated bitboards. (1-3 bits set).
	 */
	static int32_t popCountBrianKernighan(bitBoard_t bitBoard) {
		int32_t popCount = 0;
		for (; bitBoard != 0; bitBoard &= bitBoard - 1) {
			popCount++;
		}
		return popCount;
	}

	/**
	 * Counts the amount of bits in a bitboard
	 */
	static int32_t popCountLookup(bitBoard_t bitBoard) {
		/*
		return popCountLookup[bitBoard & 0xff] +
			popCountLookup[(bitBoard >> 8) & 0xff] +
			popCountLookup[(bitBoard >> 16) & 0xff] +
			popCountLookup[(bitBoard >> 24) & 0xff] +
			popCountLookup[(bitBoard >> 32) & 0xff] +
			popCountLookup[(bitBoard >> 40) & 0xff] +
			popCountLookup[(bitBoard >> 48) & 0xff] +
			popCountLookup[bitBoard >> 56];
		*/
		return 0;
	}


	/**
	 * Removes the least significant bit
	 */
	inline static uint32_t popLSB(bitBoard_t& bitBoard)
	{
		uint32_t res = lsb(bitBoard);
		bitBoard &= bitBoard - 1;
		return res;
	}

	/**
	 * Counts the amount of bits in a bitboard
	 */

#if defined (__OLD_HARDWARE__)
	inline static uint8_t popCount(bitBoard_t bitBoard) {
		return popCountBrianKernighan(bitBoard);
	}
#elif defined(_MSC_VER) || defined(__INTEL_COMPILER)
	inline static int32_t popCount(bitBoard_t bitBoard) {
		return (int)_mm_popcnt_u64(bitBoard);
	}
#else // Assumed gcc or compatible compiler
	inline static int32_t popCount(bitBoard_t bitBoard) {
		return __builtin_popcountll(bitBoard);
	}
#endif

	/**
	 * Counts the amout of set bits in a 64 bit variables - only performant for
	 * sparcely populated bitboards. (1-3 bits set).
	 */
#if defined (__OLD_HARDWARE__)
	inline static uint8_t popCountForSparcelyPopulatedBitBoards(bitBoard_t bitBoard) {
		return popCountBrianKernighan(bitBoard);
	}
#else
	inline static uint8_t popCountForSparcelyPopulatedBitBoards(bitBoard_t bitBoard) {
		return popCount(bitBoard);
	}
#endif

}

#endif  // __BITS_H
