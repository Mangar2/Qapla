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
#include "../../basics/bits.h"
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
		static_assert(OUTPUT_SIZE % 4 == 0, "four output rows are computed at a time");
		for (uint32_t out = 0; out < OUTPUT_SIZE; out += 4) {
			int32_t sums[4];
			dotProduct4(weight + size_t(out) * INPUT_SIZE, INPUT_SIZE, input, INPUT_SIZE, sums);
			for (uint32_t row = 0; row < 4; row++) {
				output[out + row] = clippedRelu((bias[out + row] + sums[row]) >> QB_SHIFT);
			}
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
	 *
	 * In 32 bits and not in 64: the output of the last layer is at most its bias plus
	 * L2_SIZE products of two bytes, so under 600000, and times NET_VALUE_SCALE that is
	 * under 240 million - a quarter of what an int32 holds. The result is the same number
	 * as before, the division is simply the cheaper one.
	 */
	constexpr value_t toEngineValue(int32_t netOutput) {
		static_assert(int64_t(L2_SIZE) * QA * 127 * NET_VALUE_SCALE < (int64_t(1) << 31),
			"the product has to stay inside an int32");
		return value_t(netOutput * int32_t(NET_VALUE_SCALE) / (int32_t(QA) * int32_t(QB)));
	}

	/**
	 * The value of a net with a piece-square part: the output of the head plus half the difference
	 * of the two psqt sums. Those carry the material and grow far beyond what toEngineValue's int32
	 * product may hold, so this one computes in 64 bits. A net without the part never gets here and
	 * keeps the cheaper path above.
	 */
	inline value_t toEngineValueWithPsqt(int32_t netOutput, int32_t ownPsqt, int32_t opponentPsqt) {
		const int64_t total = int64_t(netOutput) + (int64_t(ownPsqt) - int64_t(opponentPsqt)) / 2;
		return value_t(total * int64_t(NET_VALUE_SCALE) / (int64_t(QA) * int64_t(QB)));
	}

	inline void addPsqt(const Network& network, int32_t* psqt, uint32_t feature) {
		const int32_t* weight = network.psqtWeight.data() + size_t(feature) * PSQT_BUCKETS;
		for (uint32_t bucket = 0; bucket < PSQT_BUCKETS; bucket++) psqt[bucket] += weight[bucket];
	}

	inline void subtractPsqt(const Network& network, int32_t* psqt, uint32_t feature) {
		const int32_t* weight = network.psqtWeight.data() + size_t(feature) * PSQT_BUCKETS;
		for (uint32_t bucket = 0; bucket < PSQT_BUCKETS; bucket++) psqt[bucket] -= weight[bucket];
	}
}

bool Evaluator::usesVectorInstructions() {
	return hasVectorPath();
}

const char* Evaluator::vectorPath() {
	return vectorPathName();
}

void QaplaNnue::addFeature(const Network& network, int16_t* accumulator, int32_t* psqt,
	uint32_t feature) {
	accumulatorAdd(accumulator,
		network.featureWeight.data() + size_t(feature) * ACCUMULATOR_SIZE);
	if (network.hasPsqt) addPsqt(network, psqt, feature);
}

void QaplaNnue::removeFeature(const Network& network, int16_t* accumulator, int32_t* psqt,
	uint32_t feature) {
	accumulatorSubtract(accumulator,
		network.featureWeight.data() + size_t(feature) * ACCUMULATOR_SIZE);
	if (network.hasPsqt) subtractPsqt(network, psqt, feature);
}

template <Piece PERSPECTIVE>
void QaplaNnue::refreshAccumulator(const Network& network, const Board& board,
	int16_t* accumulator, int32_t* psqt) {
	uint32_t features[MAX_ACTIVE_FEATURES];
	const uint32_t count = computeActiveFeatures<PERSPECTIVE>(board, features);
	std::memcpy(accumulator, network.featureBias.data(), ACCUMULATOR_SIZE * sizeof(int16_t));
	std::fill(psqt, psqt + PSQT_BUCKETS, 0);
	for (uint32_t index = 0; index < count; index++) {
		addFeature(network, accumulator, psqt, features[index]);
	}
}

template void QaplaNnue::refreshAccumulator<QaplaBasics::WHITE>(const Network&, const Board&,
	int16_t*, int32_t*);
template void QaplaNnue::refreshAccumulator<QaplaBasics::BLACK>(const Network&, const Board&,
	int16_t*, int32_t*);

value_t QaplaNnue::forward(const Network& network, const int16_t* own, const int16_t* opponent,
	const int32_t* ownPsqt, const int32_t* opponentPsqt, uint32_t stack) {
	const Head& head = network.heads[stack];
	alignas(NNUE_ALIGNMENT) int8_t input[L1_INPUT_SIZE];
	clippedReluBlock(own, input, ACCUMULATOR_SIZE);
	clippedReluBlock(opponent, input + ACCUMULATOR_SIZE, ACCUMULATOR_SIZE);

	alignas(NNUE_ALIGNMENT) int8_t hidden1[L1_SIZE];
	alignas(NNUE_ALIGNMENT) int8_t hidden2[L2_SIZE];
	affineRelu<L1_INPUT_SIZE, L1_SIZE>(input, head.l1Weight.data(), head.l1Bias.data(), hidden1);
	affineRelu<L1_SIZE, L2_SIZE>(hidden1, head.l2Weight.data(), head.l2Bias.data(), hidden2);

	const int32_t output = head.outputBias + dotProduct(head.outputWeight.data(), hidden2, L2_SIZE);
	return network.hasPsqt ? toEngineValueWithPsqt(output, ownPsqt[stack], opponentPsqt[stack])
		: toEngineValue(output);
}

value_t QaplaNnue::forwardReference(const Network& network, const int16_t* own,
	const int16_t* opponent, const int32_t* ownPsqt, const int32_t* opponentPsqt, uint32_t stack) {
	const Head& head = network.heads[stack];
	alignas(NNUE_ALIGNMENT) int8_t input[L1_INPUT_SIZE];
	for (uint32_t index = 0; index < ACCUMULATOR_SIZE; index++) {
		input[index] = clippedRelu(own[index]);
		input[index + ACCUMULATOR_SIZE] = clippedRelu(opponent[index]);
	}

	alignas(NNUE_ALIGNMENT) int8_t hidden1[L1_SIZE];
	alignas(NNUE_ALIGNMENT) int8_t hidden2[L2_SIZE];
	affineReluPlain<L1_INPUT_SIZE, L1_SIZE>(input, head.l1Weight.data(), head.l1Bias.data(), hidden1);
	affineReluPlain<L1_SIZE, L2_SIZE>(hidden1, head.l2Weight.data(), head.l2Bias.data(), hidden2);

	int32_t output = head.outputBias;
	for (uint32_t index = 0; index < L2_SIZE; index++) {
		output += int32_t(head.outputWeight[index]) * int32_t(hidden2[index]);
	}
	return network.hasPsqt ? toEngineValueWithPsqt(output, ownPsqt[stack], opponentPsqt[stack])
		: toEngineValue(output);
}

value_t Evaluator::evaluate(const Board& board) const {
	alignas(NNUE_ALIGNMENT) int16_t white[ACCUMULATOR_SIZE];
	alignas(NNUE_ALIGNMENT) int16_t black[ACCUMULATOR_SIZE];
	int32_t whitePsqt[PSQT_BUCKETS];
	int32_t blackPsqt[PSQT_BUCKETS];
	refreshAccumulator<QaplaBasics::WHITE>(_network, board, white, whitePsqt);
	refreshAccumulator<QaplaBasics::BLACK>(_network, board, black, blackPsqt);
	const uint32_t stack = layerStackOf(QaplaBasics::popCount(board.getAllPiecesBB()));
	return board.isWhiteToMove() ? forward(_network, white, black, whitePsqt, blackPsqt, stack)
		: forward(_network, black, white, blackPsqt, whitePsqt, stack);
}

value_t Evaluator::evaluateReference(const Board& board) const {
	alignas(NNUE_ALIGNMENT) int16_t white[ACCUMULATOR_SIZE];
	alignas(NNUE_ALIGNMENT) int16_t black[ACCUMULATOR_SIZE];
	int32_t whitePsqt[PSQT_BUCKETS];
	int32_t blackPsqt[PSQT_BUCKETS];
	refreshAccumulator<QaplaBasics::WHITE>(_network, board, white, whitePsqt);
	refreshAccumulator<QaplaBasics::BLACK>(_network, board, black, blackPsqt);
	const uint32_t stack = layerStackOf(QaplaBasics::popCount(board.getAllPiecesBB()));
	return board.isWhiteToMove()
		? forwardReference(_network, white, black, whitePsqt, blackPsqt, stack)
		: forwardReference(_network, black, white, blackPsqt, whitePsqt, stack);
}
