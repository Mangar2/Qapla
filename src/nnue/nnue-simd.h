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
 * @copyright Copyright (c) 2026 Volker Böhm
 * @Overview
 * The four operations the net needs of a processor, and nothing else. This is the only
 * file in the nnue code that knows an instruction set; everything above it is plain C++.
 *
 *   accumulatorAdd / accumulatorSubtract   a column of int16 onto an accumulator
 *   clippedReluBlock                       int16 accumulator to the int8 a layer reads
 *   dotProduct                             int8 times int8 into an int32
 *
 * Four implementations, picked by what the compiler says the target has:
 *
 *   AVX2      256 bit, 32 bytes at a time. Windows builds with -arch:AVX2.
 *   SSSE3     128 bit. What -march=x86-64-v2 gives, which is what the Linux and macOS
 *             release uses on an Intel machine, so this is the path that runs there.
 *   NEON      with the dot product instruction, Apple Silicon and ARMv8.4 upwards.
 *   plain     correct everywhere, slow, and the reference the others are tested against.
 *
 * On x86 there is no instruction that multiplies signed bytes pairwise, so the dot product
 * goes through maddubs, which takes one operand unsigned. The activations are what is fed
 * to it: they are a clipped relu and therefore between 0 and QA, never negative, so reading
 * them as unsigned bytes changes nothing. Two products of at most 127 * 127 add to 32258 and
 * stay inside the int16 maddubs returns, so nothing saturates.
 */

#pragma once

#include <cstdint>

#include "nnue-arch.h"

#if defined(__AVX2__)
#include <immintrin.h>
#define QAPLA_NNUE_SIMD_AVX2 1
#elif defined(__SSSE3__)
#include <tmmintrin.h>
#define QAPLA_NNUE_SIMD_SSSE3 1
#elif defined(__ARM_NEON) && defined(__ARM_FEATURE_DOTPROD)
#include <arm_neon.h>
#define QAPLA_NNUE_SIMD_NEON 1
#endif

namespace QaplaNnue {

	/** Names the path the build took, for the report of a test. */
	constexpr const char* vectorPathName() {
#if defined(QAPLA_NNUE_SIMD_AVX2)
		return "avx2";
#elif defined(QAPLA_NNUE_SIMD_SSSE3)
		return "ssse3";
#elif defined(QAPLA_NNUE_SIMD_NEON)
		return "neon";
#else
		return "plain";
#endif
	}

	constexpr bool hasVectorPath() {
#if defined(QAPLA_NNUE_SIMD_AVX2) || defined(QAPLA_NNUE_SIMD_SSSE3) || defined(QAPLA_NNUE_SIMD_NEON)
		return true;
#else
		return false;
#endif
	}

	/**
	 * Adds a weight column to an accumulator, ACCUMULATOR_SIZE values of int16.
	 */
	inline void accumulatorAdd(int16_t* accumulator, const int16_t* column) {
#if defined(QAPLA_NNUE_SIMD_AVX2)
		for (uint32_t index = 0; index < ACCUMULATOR_SIZE; index += 16) {
			_mm256_storeu_si256(reinterpret_cast<__m256i*>(accumulator + index),
				_mm256_add_epi16(
					_mm256_loadu_si256(reinterpret_cast<const __m256i*>(accumulator + index)),
					_mm256_loadu_si256(reinterpret_cast<const __m256i*>(column + index))));
		}
#elif defined(QAPLA_NNUE_SIMD_SSSE3)
		for (uint32_t index = 0; index < ACCUMULATOR_SIZE; index += 8) {
			_mm_storeu_si128(reinterpret_cast<__m128i*>(accumulator + index),
				_mm_add_epi16(
					_mm_loadu_si128(reinterpret_cast<const __m128i*>(accumulator + index)),
					_mm_loadu_si128(reinterpret_cast<const __m128i*>(column + index))));
		}
#elif defined(QAPLA_NNUE_SIMD_NEON)
		for (uint32_t index = 0; index < ACCUMULATOR_SIZE; index += 8) {
			vst1q_s16(accumulator + index,
				vaddq_s16(vld1q_s16(accumulator + index), vld1q_s16(column + index)));
		}
#else
		for (uint32_t index = 0; index < ACCUMULATOR_SIZE; index++) {
			accumulator[index] = int16_t(accumulator[index] + column[index]);
		}
#endif
	}

	/**
	 * Takes a weight column away again.
	 */
	inline void accumulatorSubtract(int16_t* accumulator, const int16_t* column) {
#if defined(QAPLA_NNUE_SIMD_AVX2)
		for (uint32_t index = 0; index < ACCUMULATOR_SIZE; index += 16) {
			_mm256_storeu_si256(reinterpret_cast<__m256i*>(accumulator + index),
				_mm256_sub_epi16(
					_mm256_loadu_si256(reinterpret_cast<const __m256i*>(accumulator + index)),
					_mm256_loadu_si256(reinterpret_cast<const __m256i*>(column + index))));
		}
#elif defined(QAPLA_NNUE_SIMD_SSSE3)
		for (uint32_t index = 0; index < ACCUMULATOR_SIZE; index += 8) {
			_mm_storeu_si128(reinterpret_cast<__m128i*>(accumulator + index),
				_mm_sub_epi16(
					_mm_loadu_si128(reinterpret_cast<const __m128i*>(accumulator + index)),
					_mm_loadu_si128(reinterpret_cast<const __m128i*>(column + index))));
		}
#elif defined(QAPLA_NNUE_SIMD_NEON)
		for (uint32_t index = 0; index < ACCUMULATOR_SIZE; index += 8) {
			vst1q_s16(accumulator + index,
				vsubq_s16(vld1q_s16(accumulator + index), vld1q_s16(column + index)));
		}
#else
		for (uint32_t index = 0; index < ACCUMULATOR_SIZE; index++) {
			accumulator[index] = int16_t(accumulator[index] - column[index]);
		}
#endif
	}

	/**
	 * The activation: count values of int16 become count bytes, each held between zero and
	 * QA. count is a multiple of 32.
	 *
	 * The packing instructions saturate at 127 by themselves, which is QA, so only the
	 * lower end needs a maximum against zero.
	 */
	inline void clippedReluBlock(const int16_t* input, int8_t* output, uint32_t count) {
		static_assert(QA == 127, "the packing instructions saturate at 127, which has to be QA");
#if defined(QAPLA_NNUE_SIMD_AVX2)
		const __m256i zero = _mm256_setzero_si256();
		for (uint32_t index = 0; index < count; index += 32) {
			const __m256i low = _mm256_max_epi16(zero,
				_mm256_loadu_si256(reinterpret_cast<const __m256i*>(input + index)));
			const __m256i high = _mm256_max_epi16(zero,
				_mm256_loadu_si256(reinterpret_cast<const __m256i*>(input + index + 16)));
			// packs works inside each 128 bit half, so the halves have to be put in order.
			const __m256i packed = _mm256_permute4x64_epi64(
				_mm256_packs_epi16(low, high), 0xD8);
			_mm256_storeu_si256(reinterpret_cast<__m256i*>(output + index), packed);
		}
#elif defined(QAPLA_NNUE_SIMD_SSSE3)
		const __m128i zero = _mm_setzero_si128();
		for (uint32_t index = 0; index < count; index += 16) {
			const __m128i low = _mm_max_epi16(zero,
				_mm_loadu_si128(reinterpret_cast<const __m128i*>(input + index)));
			const __m128i high = _mm_max_epi16(zero,
				_mm_loadu_si128(reinterpret_cast<const __m128i*>(input + index + 8)));
			_mm_storeu_si128(reinterpret_cast<__m128i*>(output + index),
				_mm_packs_epi16(low, high));
		}
#elif defined(QAPLA_NNUE_SIMD_NEON)
		const int16x8_t zero = vdupq_n_s16(0);
		const int16x8_t limit = vdupq_n_s16(QA);
		for (uint32_t index = 0; index < count; index += 16) {
			const int16x8_t low = vminq_s16(vmaxq_s16(vld1q_s16(input + index), zero), limit);
			const int16x8_t high = vminq_s16(vmaxq_s16(vld1q_s16(input + index + 8), zero), limit);
			vst1q_s8(output + index, vcombine_s8(vmovn_s16(low), vmovn_s16(high)));
		}
#else
		for (uint32_t index = 0; index < count; index++) {
			const int32_t value = input[index];
			output[index] = int8_t(value < 0 ? 0 : (value > QA ? QA : value));
		}
#endif
	}

	/**
	 * The dot product of count weights and count activations, count a multiple of 16.
	 */
	inline int32_t dotProduct(const int8_t* weights, const int8_t* input, uint32_t count) {
#if defined(QAPLA_NNUE_SIMD_AVX2)
		__m256i sum = _mm256_setzero_si256();
		const __m256i ones = _mm256_set1_epi16(1);
		for (uint32_t index = 0; index < count; index += 32) {
			const __m256i products = _mm256_maddubs_epi16(
				_mm256_loadu_si256(reinterpret_cast<const __m256i*>(input + index)),
				_mm256_loadu_si256(reinterpret_cast<const __m256i*>(weights + index)));
			sum = _mm256_add_epi32(sum, _mm256_madd_epi16(products, ones));
		}
		const __m128i folded = _mm_add_epi32(_mm256_castsi256_si128(sum),
			_mm256_extracti128_si256(sum, 1));
		const __m128i pairs = _mm_add_epi32(folded, _mm_shuffle_epi32(folded, 0x4E));
		return _mm_cvtsi128_si32(_mm_add_epi32(pairs, _mm_shuffle_epi32(pairs, 0xB1)));
#elif defined(QAPLA_NNUE_SIMD_SSSE3)
		__m128i sum = _mm_setzero_si128();
		const __m128i ones = _mm_set1_epi16(1);
		for (uint32_t index = 0; index < count; index += 16) {
			const __m128i products = _mm_maddubs_epi16(
				_mm_loadu_si128(reinterpret_cast<const __m128i*>(input + index)),
				_mm_loadu_si128(reinterpret_cast<const __m128i*>(weights + index)));
			sum = _mm_add_epi32(sum, _mm_madd_epi16(products, ones));
		}
		const __m128i pairs = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0x4E));
		return _mm_cvtsi128_si32(_mm_add_epi32(pairs, _mm_shuffle_epi32(pairs, 0xB1)));
#elif defined(QAPLA_NNUE_SIMD_NEON)
		int32x4_t sum = vdupq_n_s32(0);
		for (uint32_t index = 0; index < count; index += 16) {
			sum = vdotq_s32(sum, vld1q_s8(weights + index), vld1q_s8(input + index));
		}
		return vaddvq_s32(sum);
#else
		int32_t sum = 0;
		for (uint32_t index = 0; index < count; index++) {
			sum += int32_t(weights[index]) * int32_t(input[index]);
		}
		return sum;
#endif
	}
}
