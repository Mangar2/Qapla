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
 * @author Volker B�hm
 * @copyright Copyright (c) 2021 Volker B�hm
 * @Overview
 * Implements bit handling routines
 */

#ifndef __BITS_H
#define __BITS_H

// #include <bit>
#include <assert.h>
#include "types.h"



#if (defined(__INTEL_COMPILER) || defined(_MSC_VER))
#  include <nmmintrin.h> // Intel and Microsoft header for _mm_popcnt_u64()
#endif

#if defined(_WIN64) && defined(_MSC_VER) // No Makefile used
#include <__msvc_bit_utils.hpp>
#include <intrin.h> // Microsoft header for _BitScanForward64()
#define IS_64BIT
#endif

namespace QaplaBasics {

	const int32_t index64[64] = {
		0, 47,  1, 56, 48, 27,  2, 60,
	   57, 49, 41, 37, 28, 16,  3, 61,
	   54, 58, 35, 52, 50, 42, 21, 44,
	   38, 32, 29, 23, 17, 11,  4, 62,
	   46, 55, 26, 59, 40, 36, 15, 53,
	   34, 51, 20, 43, 31, 22, 10, 45,
	   25, 39, 14, 33, 19, 30,  9, 24,
	   13, 18,  8, 12,  7,  6,  5, 63
	};

	/**
	 * bitScanForward
	 * @author Kim Walisch (2012)
	 * @param bb bitboard to scan
	 * @precondition bb != 0
	 * @return index (0..63) of least significant one bit
	 */
	[[nodiscard]] constexpr int32_t bitScanForward(bitBoard_t bb) noexcept {
		const bitBoard_t debruijn64 = bitBoard_t(0x03f79d71b4cb0a89);
		assert(bb != 0);
		return index64[((bb ^ (bb - 1)) * debruijn64) >> 58];
	}

#if defined(__GNUC__) && !defined(__OLD_HW__)

	constexpr Square lsb(bitBoard_t bitBoard) {
		assert(bitBoard);
		return Square(__builtin_ctzll(bitBoard));
	}

#elif defined(_WIN64) && defined(_MSC_VER) && !defined(__OLD_HW__)
	
	static inline Square lsb(bitBoard_t bitBoard) {
		assert(bitBoard);
		unsigned long pos;
		_BitScanForward64(&pos, bitBoard);
		return (Square)pos;
	}

#else 
	inline static Square lsb(bitBoard_t bitBoard) {
		return (Square)bitScanForward(bitBoard);
	}
#endif


	/**
	 * Removes the least significant bit
	 */
	inline static Square popLSB(bitBoard_t& bitBoard)
	{
		auto res = lsb(bitBoard);
		bitBoard &= bitBoard - 1;
		return res;
	}

	/**
	 * Counts the amount of bits in a bitboard - different implementations depending on the hardware support
	 * If popcount is supported by hardware -> use it. If not use the algorithm from BrianKernighan for 
	 * sparcely populated bitboards (0 - 3 bits set) or the SWAR-Popcount 
	 */

	 /**
	  * Counts the amout of set bits in a 64 bit variables - only performant for
	  * sparcely populated bitboards. (1-3 bits set).
	  */
	constexpr uint32_t popCountBrianKernighan(bitBoard_t bitBoard) {
		uint32_t popCount = 0;
		for (; bitBoard != 0; bitBoard &= bitBoard - 1) {
			popCount++;
		}
		return popCount;
	}


	 /**
	  * SWAR Mask version of popcount by Donald Knuth
	  */
	[[nodiscard]] constexpr int32_t SWARPopcount(bitBoard_t bitBoard) noexcept {
		const bitBoard_t k1 = 0x5555555555555555ull;
		const bitBoard_t k2 = 0x3333333333333333ull;
		const bitBoard_t k4 = 0x0F0F0F0F0F0F0F0Full;
		const bitBoard_t kf = 0x0101010101010101ull;

		bitBoard = bitBoard - ((bitBoard >> 1) & k1);
		bitBoard = (bitBoard & k2) + ((bitBoard >> 2) & k2);
		bitBoard = (bitBoard + (bitBoard >> 4)) & k4;
		bitBoard = (bitBoard * kf) >> 56;
		return int32_t(bitBoard);
	}

#if (defined(_WIN64) && defined(_MSC_VER) && !defined(__OLD_HW__)) || defined(__INTEL_COMPILER)

	inline uint32_t popCount(bitBoard_t bitBoard) {
		//const bool _Definitely_have_popcnt = __isa_available >= __ISA_AVAILABLE_SSE42;
		return (int)_mm_popcnt_u64(bitBoard);
	}

	inline uint32_t popCountForSparcelyPopulatedBitBoards(bitBoard_t bitBoard) {
		//const bool _Definitely_have_popcnt = __isa_available >= __ISA_AVAILABLE_SSE42;
		return popCount(bitBoard);
	}

#elif defined(__GNUC__) && !defined(__OLD_HW__)
	constexpr int32_t popCount(bitBoard_t bitBoard) {
		return __builtin_popcountll(bitBoard);
	}

	constexpr uint8_t popCountForSparcelyPopulatedBitBoards(bitBoard_t bitBoard) {
		return popCount(bitBoard);
	}

#else
	constexpr uint32_t popCount(bitBoard_t bitBoard) {
		return SWARPopcount(bitBoard);
	}
	
	constexpr uint32_t popCountForSparcelyPopulatedBitBoards(bitBoard_t bitBoard) {
		return popCountBrianKernighan(bitBoard);
	}
#endif


}

#endif  // __BITS_H
