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
#include <array>
#include <climits>
#include <cmath>
#include <cstdint>
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
	 * The square of an output of the first layer, at the scale QA: (QA x)^2 / 128 is QA * x^2 *
	 * 127/128, Stockfish's SqrClippedReLU, cut at QA. A value beyond 128 gives QA anyway, so it is
	 * cut there first and the product stays small.
	 */
	constexpr int8_t squaredRelu(int32_t value) {
		const int32_t bounded = std::min(value < 0 ? -value : value, 128);
		return int8_t(std::min((bounded * bounded) >> 7, int32_t(QA)));
	}

	/**
	 * The first layer of a net of the fourth format: every output activated twice, the squares in
	 * the first L1_SIZE values of output, the clipped relu in the next L1_SIZE.
	 */
	template <uint32_t INPUT_SIZE, uint32_t OUTPUT_SIZE>
	void affineSqrRelu(const int8_t* input, const int8_t* weight, const int32_t* bias,
		int8_t* output) {
		static_assert(INPUT_SIZE % 16 == 0, "the dot product takes a multiple of sixteen");
		static_assert(OUTPUT_SIZE % 4 == 0, "four output rows are computed at a time");
		for (uint32_t out = 0; out < OUTPUT_SIZE; out += 4) {
			int32_t sums[4];
			dotProduct4(weight + size_t(out) * INPUT_SIZE, INPUT_SIZE, input, INPUT_SIZE, sums);
			for (uint32_t row = 0; row < 4; row++) {
				const int32_t value = (bias[out + row] + sums[row]) >> QB_SHIFT;
				output[out + row] = squaredRelu(value);
				output[OUTPUT_SIZE + out + row] = clippedRelu(value);
			}
		}
	}

	/** The same layer without any instruction of the processor. */
	template <uint32_t INPUT_SIZE, uint32_t OUTPUT_SIZE>
	void affineSqrReluPlain(const int8_t* input, const int8_t* weight, const int32_t* bias,
		int8_t* output) {
		for (uint32_t out = 0; out < OUTPUT_SIZE; out++) {
			int32_t sum = bias[out];
			const int8_t* row = weight + size_t(out) * INPUT_SIZE;
			for (uint32_t in = 0; in < INPUT_SIZE; in++) sum += int32_t(row[in]) * int32_t(input[in]);
			const int32_t value = sum >> QB_SHIFT;
			output[out] = squaredRelu(value);
			output[OUTPUT_SIZE + out] = clippedRelu(value);
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

	/**
	 * The MOE_TOP experts the router of a mixture-of-experts net picks: the highest logits, the lower
	 * index on a tie, as netfile.moe_experts_of(). The router reads the MOE_ROUTER values behind the
	 * transformed ones, own perspective first, unclipped.
	 */
	inline void chooseExperts(const MoeLayers& moe, const int16_t* own, const int16_t* opponent,
		uint32_t experts[MOE_TOP]) {
		static_assert(MOE_TOP == 2, "the choice below keeps the best two");
		int64_t best[2] = { INT64_MIN, INT64_MIN };
		experts[0] = experts[1] = 0;
		for (uint32_t expert = 0; expert < MOE_EXPERTS; expert++) {
			const int16_t* weight = moe.routerWeight.data() + size_t(expert) * 2 * MOE_ROUTER;
			int64_t logit = moe.routerBias[expert];
			for (uint32_t index = 0; index < MOE_ROUTER; index++) {
				logit += int64_t(weight[index]) * own[TRANSFORMED_SIZE + index]
					+ int64_t(weight[MOE_ROUTER + index]) * opponent[TRANSFORMED_SIZE + index];
			}
			if (logit > best[0]) {
				best[1] = best[0]; experts[1] = experts[0];
				best[0] = logit; experts[0] = expert;
			}
			else if (logit > best[1]) {
				best[1] = logit; experts[1] = expert;
			}
		}
	}

	/**
	 * A mixture-of-experts net: the two chosen experts' neurons of the first layer, their columns of
	 * the second summed with its bias, then the output. VECTOR selects the dot products of the
	 * processor; the plain path is the reference for the test of the other.
	 */
	template <bool VECTOR>
	value_t forwardMoe(const Network& network, const int16_t* own, const int16_t* opponent) {
		const MoeLayers& moe = network.moe;
		alignas(NNUE_ALIGNMENT) int8_t input[L1_INPUT_SIZE];
		for (uint32_t index = 0; index < TRANSFORMED_SIZE; index++) {
			input[index] = clippedRelu(own[index]);
			input[index + TRANSFORMED_SIZE] = clippedRelu(opponent[index]);
		}
		uint32_t experts[MOE_TOP];
		chooseExperts(moe, own, opponent, experts);

		alignas(NNUE_ALIGNMENT) int8_t hidden[MOE_TOP * MOE_EXPERT_SIZE];
		alignas(NNUE_ALIGNMENT) int32_t sums[L2_SIZE];
		for (uint32_t out = 0; out < L2_SIZE; out++) sums[out] = moe.l2Bias[out];
		for (uint32_t chosen = 0; chosen < MOE_TOP; chosen++) {
			const uint32_t expert = experts[chosen];
			int8_t* expertHidden = hidden + chosen * MOE_EXPERT_SIZE;
			const int8_t* l1 = moe.l1Weight.data() + size_t(expert) * MOE_EXPERT_SIZE * L1_INPUT_SIZE;
			const int32_t* l1Bias = moe.l1Bias.data() + size_t(expert) * MOE_EXPERT_SIZE;
			const int8_t* l2 = moe.l2Weight.data() + size_t(expert) * L2_SIZE * MOE_EXPERT_SIZE;
			if (VECTOR) {
				affineRelu<L1_INPUT_SIZE, MOE_EXPERT_SIZE>(input, l1, l1Bias, expertHidden);
				for (uint32_t out = 0; out < L2_SIZE; out += 4) {
					int32_t four[4];
					dotProduct4(l2 + size_t(out) * MOE_EXPERT_SIZE, MOE_EXPERT_SIZE, expertHidden,
						MOE_EXPERT_SIZE, four);
					for (uint32_t row = 0; row < 4; row++) sums[out + row] += four[row];
				}
			}
			else {
				affineReluPlain<L1_INPUT_SIZE, MOE_EXPERT_SIZE>(input, l1, l1Bias, expertHidden);
				for (uint32_t out = 0; out < L2_SIZE; out++) {
					for (uint32_t index = 0; index < MOE_EXPERT_SIZE; index++) {
						sums[out] += int32_t(l2[out * MOE_EXPERT_SIZE + index]) * int32_t(expertHidden[index]);
					}
				}
			}
		}
		alignas(NNUE_ALIGNMENT) int8_t second[L2_SIZE];
		for (uint32_t out = 0; out < L2_SIZE; out++) second[out] = clippedRelu(sums[out] >> QB_SHIFT);
		int32_t output = moe.outputBias;
		for (uint32_t index = 0; index < L2_SIZE; index++) {
			output += int32_t(moe.outputWeight[index]) * int32_t(second[index]);
		}
		return toEngineValue(output);
	}

	/** The better chosen expert's weight over the logit difference, as netfile.moe_gate_of(). */
	struct MoeGateTable {
		std::array<int32_t, MOE_GATE_TABLE> weight{};
		MoeGateTable() {
			for (uint32_t step = 0; step < MOE_GATE_TABLE; step++) {
				weight[step] = int32_t(std::lround(double(MOE_GATE_SCALE)
					/ (1.0 + std::exp(-double(step) / double(MOE_GATE_STEPS)))));
			}
		}
	};

	inline int32_t moeGateOf(int32_t difference) {
		static const MoeGateTable table;
		const int32_t step = std::min<int32_t>(int32_t(MOE_GATE_TABLE) - 1,
			difference * int32_t(MOE_GATE_STEPS) / int32_t(QA));
		return table.weight[size_t(step)];
	}

	/**
	 * The two experts of a layer of a three-layer MoE net and the better one's weight: logit k is the own plus the
	 * opponent value k of the layer's 16 behind the transformed values; highest first, the lower index on a tie.
	 */
	inline int32_t chooseLayerExperts(const int16_t* own, const int16_t* opponent, uint32_t layer,
		uint32_t& best, uint32_t& second) {
		const uint32_t start = TRANSFORMED_SIZE + layer * MOE_LAYER_EXPERTS;
		int32_t first = INT32_MIN, next = INT32_MIN;
		best = second = 0;
		for (uint32_t k = 0; k < MOE_LAYER_EXPERTS; k++) {
			const int32_t logit = int32_t(own[start + k]) + int32_t(opponent[start + k]);
			if (logit > first) { next = first; second = best; first = logit; best = k; }
			else if (logit > next) { next = logit; second = k; }
		}
		return moeGateOf(first - next);
	}

	/** Signed throughout: the constants live in an enum of uint32_t, and an output value can be negative. */
	inline int32_t mixOf(int32_t gate, int32_t a, int32_t b) {
		return (gate * a + (int32_t(MOE_GATE_SCALE) - gate) * b) >> int32_t(MOE_GATE_SHIFT);
	}

	/** A net with experts in all three layers: 512 -> 16, 16 -> 32, 32 -> 1, two of 16 in each, mixed. */
	template <bool VECTOR>
	value_t forwardMoeLayers(const Network& network, const int16_t* own, const int16_t* opponent) {
		const MoeLayerSet& set = network.moeLayers;
		alignas(NNUE_ALIGNMENT) int8_t input[L1_INPUT_SIZE];
		for (uint32_t index = 0; index < TRANSFORMED_SIZE; index++) {
			input[index] = clippedRelu(own[index]);
			input[index + TRANSFORMED_SIZE] = clippedRelu(opponent[index]);
		}
		uint32_t experts[2];
		alignas(NNUE_ALIGNMENT) int8_t out1[2][MOE_EXPERT_SIZE];
		int32_t gate = chooseLayerExperts(own, opponent, 0, experts[0], experts[1]);
		for (uint32_t k = 0; k < 2; k++) {
			const int8_t* w = set.l1Weight.data() + size_t(experts[k]) * MOE_EXPERT_SIZE * L1_INPUT_SIZE;
			const int32_t* b = set.l1Bias.data() + size_t(experts[k]) * MOE_EXPERT_SIZE;
			if (VECTOR) affineRelu<L1_INPUT_SIZE, MOE_EXPERT_SIZE>(input, w, b, out1[k]);
			else affineReluPlain<L1_INPUT_SIZE, MOE_EXPERT_SIZE>(input, w, b, out1[k]);
		}
		alignas(NNUE_ALIGNMENT) int8_t first[MOE_EXPERT_SIZE];
		for (uint32_t i = 0; i < MOE_EXPERT_SIZE; i++) first[i] = int8_t(mixOf(gate, out1[0][i], out1[1][i]));

		alignas(NNUE_ALIGNMENT) int8_t out2[2][L2_SIZE];
		gate = chooseLayerExperts(own, opponent, 1, experts[0], experts[1]);
		for (uint32_t k = 0; k < 2; k++) {
			const int8_t* w = set.l2Weight.data() + size_t(experts[k]) * L2_SIZE * MOE_EXPERT_SIZE;
			const int32_t* b = set.l2Bias.data() + size_t(experts[k]) * L2_SIZE;
			if (VECTOR) affineRelu<MOE_EXPERT_SIZE, L2_SIZE>(first, w, b, out2[k]);
			else affineReluPlain<MOE_EXPERT_SIZE, L2_SIZE>(first, w, b, out2[k]);
		}
		alignas(NNUE_ALIGNMENT) int8_t second[L2_SIZE];
		for (uint32_t i = 0; i < L2_SIZE; i++) second[i] = int8_t(mixOf(gate, out2[0][i], out2[1][i]));

		gate = chooseLayerExperts(own, opponent, 2, experts[0], experts[1]);
		int32_t value[2];
		for (uint32_t k = 0; k < 2; k++) {
			const int8_t* w = set.outputWeight.data() + size_t(experts[k]) * L2_SIZE;
			int32_t total = set.outputBias[experts[k]];
			for (uint32_t i = 0; i < L2_SIZE; i++) total += int32_t(w[i]) * int32_t(second[i]);
			value[k] = total;
		}
		return toEngineValue(mixOf(gate, value[0], value[1]));
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
	if (network.isMoeLayers) return forwardMoeLayers<true>(network, own, opponent);
	if (network.isMoe) return forwardMoe<true>(network, own, opponent);
	const Head& head = network.heads[stack];
	alignas(NNUE_ALIGNMENT) int8_t input[L1_INPUT_SIZE];
	clippedReluBlock(own, input, TRANSFORMED_SIZE);
	clippedReluBlock(opponent, input + TRANSFORMED_SIZE, TRANSFORMED_SIZE);

	alignas(NNUE_ALIGNMENT) int8_t hidden1[L2_INPUT_SIZE_SQR];
	alignas(NNUE_ALIGNMENT) int8_t hidden2[L2_SIZE];
	if (network.hasSqr) {
		affineSqrRelu<L1_INPUT_SIZE, L1_SIZE>(input, head.l1Weight.data(), head.l1Bias.data(), hidden1);
		affineRelu<L2_INPUT_SIZE_SQR, L2_SIZE>(hidden1, head.l2Weight.data(), head.l2Bias.data(), hidden2);
	}
	else {
		affineRelu<L1_INPUT_SIZE, L1_SIZE>(input, head.l1Weight.data(), head.l1Bias.data(), hidden1);
		affineRelu<L1_SIZE, L2_SIZE>(hidden1, head.l2Weight.data(), head.l2Bias.data(), hidden2);
	}

	const int32_t output = head.outputBias + dotProduct(head.outputWeight.data(), hidden2, L2_SIZE);
	return network.hasPsqt ? toEngineValueWithPsqt(output, ownPsqt[stack], opponentPsqt[stack])
		: toEngineValue(output);
}

value_t QaplaNnue::forwardReference(const Network& network, const int16_t* own,
	const int16_t* opponent, const int32_t* ownPsqt, const int32_t* opponentPsqt, uint32_t stack) {
	if (network.isMoeLayers) return forwardMoeLayers<false>(network, own, opponent);
	if (network.isMoe) return forwardMoe<false>(network, own, opponent);
	const Head& head = network.heads[stack];
	alignas(NNUE_ALIGNMENT) int8_t input[L1_INPUT_SIZE];
	for (uint32_t index = 0; index < TRANSFORMED_SIZE; index++) {
		input[index] = clippedRelu(own[index]);
		input[index + TRANSFORMED_SIZE] = clippedRelu(opponent[index]);
	}

	alignas(NNUE_ALIGNMENT) int8_t hidden1[L2_INPUT_SIZE_SQR];
	alignas(NNUE_ALIGNMENT) int8_t hidden2[L2_SIZE];
	if (network.hasSqr) {
		affineSqrReluPlain<L1_INPUT_SIZE, L1_SIZE>(input, head.l1Weight.data(), head.l1Bias.data(), hidden1);
		affineReluPlain<L2_INPUT_SIZE_SQR, L2_SIZE>(hidden1, head.l2Weight.data(), head.l2Bias.data(), hidden2);
	}
	else {
		affineReluPlain<L1_INPUT_SIZE, L1_SIZE>(input, head.l1Weight.data(), head.l1Bias.data(), hidden1);
		affineReluPlain<L1_SIZE, L2_SIZE>(hidden1, head.l2Weight.data(), head.l2Bias.data(), hidden2);
	}

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
