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
#include "nnue-simd.h"

using namespace QaplaNnue;
using QaplaBasics::Board;
using QaplaBasics::Piece;
using QaplaBasics::value_t;

namespace {

	/** The clipped relu of the reference path. */
	constexpr int8_t clippedRelu(int32_t value) {
		return int8_t(value < 0 ? 0 : (value > QA ? QA : value));
	}

	/**
	 * An affine layer plus its activation. The dot product is the one operation of the
	 * processor, everything around it is plain.
	 */
	template <uint32_t INPUT_SIZE, uint32_t OUTPUT_SIZE>
	void affineRelu(const int8_t* input, const int8_t* weight, const int32_t* bias,
		int8_t* output) {
		static_assert(INPUT_SIZE % 16 == 0, "the dot product takes a multiple of sixteen");
		for (uint32_t out = 0; out < OUTPUT_SIZE; out++) {
			const int32_t sum = bias[out]
				+ dotProduct(weight + size_t(out) * INPUT_SIZE, input, INPUT_SIZE);
			output[out] = clippedRelu(sum >> QB_SHIFT);
		}
	}

	/** The same layer without any instruction of the processor, for the test of the one above. */
	template <uint32_t INPUT_SIZE, uint32_t OUTPUT_SIZE>
	void affineReluPlain(const int8_t* input, const int8_t* weight, const int32_t* bias,
		int8_t* output) {
		for (uint32_t out = 0; out < OUTPUT_SIZE; out++) {
			int32_t sum = bias[out];
			const int8_t* row = weight + size_t(out) * INPUT_SIZE;
			for (uint32_t in = 0; in < INPUT_SIZE; in++) sum += int32_t(row[in]) * int32_t(input[in]);
			output[out] = clippedRelu(sum >> QB_SHIFT);
		}
	}

	/**
	 * The value the net produces, brought into the value unit of the engine.
	 */
	constexpr value_t toEngineValue(int32_t netOutput) {
		return value_t(int64_t(netOutput) * NET_VALUE_SCALE / (int64_t(QA) * int64_t(QB)));
	}
}

bool Evaluator::usesVectorInstructions() {
	return hasVectorPath();
}

const char* Evaluator::vectorPath() {
	return vectorPathName();
}

void QaplaNnue::addFeature(const Network& network, int16_t* accumulator, uint32_t feature) {
	accumulatorAdd(accumulator,
		network.featureWeight.data() + size_t(feature) * ACCUMULATOR_SIZE);
}

void QaplaNnue::removeFeature(const Network& network, int16_t* accumulator, uint32_t feature) {
	accumulatorSubtract(accumulator,
		network.featureWeight.data() + size_t(feature) * ACCUMULATOR_SIZE);
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
	alignas(NNUE_ALIGNMENT) int8_t input[L1_INPUT_SIZE];
	clippedReluBlock(own, input, ACCUMULATOR_SIZE);
	clippedReluBlock(opponent, input + ACCUMULATOR_SIZE, ACCUMULATOR_SIZE);

	alignas(NNUE_ALIGNMENT) int8_t hidden1[L1_SIZE];
	alignas(NNUE_ALIGNMENT) int8_t hidden2[L2_SIZE];
	affineRelu<L1_INPUT_SIZE, L1_SIZE>(input, network.l1Weight.data(),
		network.l1Bias.data(), hidden1);
	affineRelu<L1_SIZE, L2_SIZE>(hidden1, network.l2Weight.data(),
		network.l2Bias.data(), hidden2);

	return toEngineValue(network.outputBias
		+ dotProduct(network.outputWeight.data(), hidden2, L2_SIZE));
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
	affineReluPlain<L1_INPUT_SIZE, L1_SIZE>(input, network.l1Weight.data(),
		network.l1Bias.data(), hidden1);
	affineReluPlain<L1_SIZE, L2_SIZE>(hidden1, network.l2Weight.data(),
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
