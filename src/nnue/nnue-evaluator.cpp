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
 * Evaluates a position with the net, see nnue-evaluator.h
 */

#include <algorithm>
#include <cstring>

#include "nnue-evaluator.h"
#include "nnue-features.h"

#if defined(__ARM_NEON) && defined(__ARM_FEATURE_DOTPROD)
#include <arm_neon.h>
#define NNUE_NEON_DOTPROD 1
#else
#define NNUE_NEON_DOTPROD 0
#endif

using namespace QaplaNnue;
using QaplaBasics::Board;
using QaplaBasics::Piece;
using QaplaBasics::value_t;

namespace {

	/** The clipped relu: the activation of every layer. */
	constexpr int8_t clippedRelu(int32_t value) {
		return int8_t(value < 0 ? 0 : (value > QA ? QA : value));
	}

	/**
	 * An affine layer plus its activation, plainly.
	 */
	template <uint32_t INPUT_SIZE, uint32_t OUTPUT_SIZE>
	void affineRelu(const int8_t* input, const int8_t* weight, const int32_t* bias,
		int8_t* output) {
		for (uint32_t out = 0; out < OUTPUT_SIZE; out++) {
			int32_t sum = bias[out];
			const int8_t* row = weight + size_t(out) * INPUT_SIZE;
			for (uint32_t in = 0; in < INPUT_SIZE; in++) sum += int32_t(row[in]) * int32_t(input[in]);
			output[out] = clippedRelu(sum >> QB_SHIFT);
		}
	}

#if NNUE_NEON_DOTPROD
	/**
	 * The same layer with the dot product instruction, which multiplies and adds
	 * sixteen bytes at a time. INPUT_SIZE is a multiple of sixteen for both layers
	 * of this net.
	 */
	template <uint32_t INPUT_SIZE, uint32_t OUTPUT_SIZE>
	void affineReluNeon(const int8_t* input, const int8_t* weight, const int32_t* bias,
		int8_t* output) {
		static_assert(INPUT_SIZE % 16 == 0);
		for (uint32_t out = 0; out < OUTPUT_SIZE; out++) {
			const int8_t* row = weight + size_t(out) * INPUT_SIZE;
			int32x4_t sum = vdupq_n_s32(0);
			for (uint32_t in = 0; in < INPUT_SIZE; in += 16) {
				sum = vdotq_s32(sum, vld1q_s8(row + in), vld1q_s8(input + in));
			}
			output[out] = clippedRelu((bias[out] + vaddvq_s32(sum)) >> QB_SHIFT);
		}
	}

	/**
	 * Turns an accumulator into the bytes the first dense layer reads.
	 */
	void clippedReluNeon(const int16_t* accumulator, int8_t* output) {
		const int16x8_t zero = vdupq_n_s16(0);
		const int16x8_t limit = vdupq_n_s16(QA);
		for (uint32_t index = 0; index < ACCUMULATOR_SIZE; index += 16) {
			const int16x8_t low = vminq_s16(vmaxq_s16(vld1q_s16(accumulator + index), zero), limit);
			const int16x8_t high = vminq_s16(vmaxq_s16(vld1q_s16(accumulator + index + 8), zero), limit);
			vst1q_s8(output + index, vcombine_s8(vmovn_s16(low), vmovn_s16(high)));
		}
	}
#endif

	/**
	 * The value the net produces, brought into the value unit of the engine.
	 */
	constexpr value_t toEngineValue(int32_t netOutput) {
		return value_t(int64_t(netOutput) * NET_VALUE_SCALE / (int64_t(QA) * int64_t(QB)));
	}
}

bool Evaluator::usesVectorInstructions() {
	return NNUE_NEON_DOTPROD != 0;
}

void QaplaNnue::addFeature(const Network& network, int16_t* accumulator, uint32_t feature) {
	const int16_t* column = network.featureWeight.data() + size_t(feature) * ACCUMULATOR_SIZE;
#if NNUE_NEON_DOTPROD
	for (uint32_t element = 0; element < ACCUMULATOR_SIZE; element += 8) {
		vst1q_s16(accumulator + element,
			vaddq_s16(vld1q_s16(accumulator + element), vld1q_s16(column + element)));
	}
#else
	for (uint32_t element = 0; element < ACCUMULATOR_SIZE; element++) {
		accumulator[element] = int16_t(accumulator[element] + column[element]);
	}
#endif
}

void QaplaNnue::removeFeature(const Network& network, int16_t* accumulator, uint32_t feature) {
	const int16_t* column = network.featureWeight.data() + size_t(feature) * ACCUMULATOR_SIZE;
#if NNUE_NEON_DOTPROD
	for (uint32_t element = 0; element < ACCUMULATOR_SIZE; element += 8) {
		vst1q_s16(accumulator + element,
			vsubq_s16(vld1q_s16(accumulator + element), vld1q_s16(column + element)));
	}
#else
	for (uint32_t element = 0; element < ACCUMULATOR_SIZE; element++) {
		accumulator[element] = int16_t(accumulator[element] - column[element]);
	}
#endif
}

template <Piece PERSPECTIVE>
void QaplaNnue::refreshAccumulator(const Network& network, const Board& board,
	int16_t* accumulator) {
	uint32_t features[MAX_ACTIVE_FEATURES];
	const uint32_t count = computeActiveFeatures<PERSPECTIVE>(board, features);
	std::memcpy(accumulator, network.featureBias.data(), ACCUMULATOR_SIZE * sizeof(int16_t));
	for (uint32_t index = 0; index < count; index++) {
		addFeature(network, accumulator, features[index]);
	}
}

template void QaplaNnue::refreshAccumulator<QaplaBasics::WHITE>(const Network&, const Board&, int16_t*);
template void QaplaNnue::refreshAccumulator<QaplaBasics::BLACK>(const Network&, const Board&, int16_t*);

value_t QaplaNnue::forward(const Network& network, const int16_t* own, const int16_t* opponent) {
#if NNUE_NEON_DOTPROD
	alignas(NNUE_ALIGNMENT) int8_t input[L1_INPUT_SIZE];
	clippedReluNeon(own, input);
	clippedReluNeon(opponent, input + ACCUMULATOR_SIZE);

	alignas(NNUE_ALIGNMENT) int8_t hidden1[L1_SIZE];
	alignas(NNUE_ALIGNMENT) int8_t hidden2[L2_SIZE];
	affineReluNeon<L1_INPUT_SIZE, L1_SIZE>(input, network.l1Weight.data(),
		network.l1Bias.data(), hidden1);
	affineReluNeon<L1_SIZE, L2_SIZE>(hidden1, network.l2Weight.data(),
		network.l2Bias.data(), hidden2);

	int32_t output = network.outputBias;
	for (uint32_t index = 0; index < L2_SIZE; index++) {
		output += int32_t(network.outputWeight[index]) * int32_t(hidden2[index]);
	}
	return toEngineValue(output);
#else
	return forwardReference(network, own, opponent);
#endif
}

value_t QaplaNnue::forwardReference(const Network& network, const int16_t* own,
	const int16_t* opponent) {
	alignas(NNUE_ALIGNMENT) int8_t input[L1_INPUT_SIZE];
	for (uint32_t index = 0; index < ACCUMULATOR_SIZE; index++) {
		input[index] = clippedRelu(own[index]);
		input[index + ACCUMULATOR_SIZE] = clippedRelu(opponent[index]);
	}

	alignas(NNUE_ALIGNMENT) int8_t hidden1[L1_SIZE];
	alignas(NNUE_ALIGNMENT) int8_t hidden2[L2_SIZE];
	affineRelu<L1_INPUT_SIZE, L1_SIZE>(input, network.l1Weight.data(),
		network.l1Bias.data(), hidden1);
	affineRelu<L1_SIZE, L2_SIZE>(hidden1, network.l2Weight.data(),
		network.l2Bias.data(), hidden2);

	int32_t output = network.outputBias;
	for (uint32_t index = 0; index < L2_SIZE; index++) {
		output += int32_t(network.outputWeight[index]) * int32_t(hidden2[index]);
	}
	return toEngineValue(output);
}

value_t Evaluator::evaluate(const Board& board) const {
	alignas(NNUE_ALIGNMENT) int16_t white[ACCUMULATOR_SIZE];
	alignas(NNUE_ALIGNMENT) int16_t black[ACCUMULATOR_SIZE];
	refreshAccumulator<QaplaBasics::WHITE>(_network, board, white);
	refreshAccumulator<QaplaBasics::BLACK>(_network, board, black);
	return board.isWhiteToMove() ? forward(_network, white, black)
		: forward(_network, black, white);
}

value_t Evaluator::evaluateReference(const Board& board) const {
	alignas(NNUE_ALIGNMENT) int16_t white[ACCUMULATOR_SIZE];
	alignas(NNUE_ALIGNMENT) int16_t black[ACCUMULATOR_SIZE];
	refreshAccumulator<QaplaBasics::WHITE>(_network, board, white);
	refreshAccumulator<QaplaBasics::BLACK>(_network, board, black);
	return board.isWhiteToMove() ? forwardReference(_network, white, black)
		: forwardReference(_network, black, white);
}
