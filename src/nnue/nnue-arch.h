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
 * The shape of the net and the numbers it is quantized with. Everything that has
 * to agree between the trainer and the engine is here and nowhere else.
 *
 *   features -> accumulator 256 per perspective -> 32 -> 32 -> 1
 *
 * Quantization. A real activation lies in [0,1] after the clipped relu and is
 * stored as an integer of scale QA, a real weight as one of scale QB, a real
 * bias as one of scale QA*QB. An affine layer therefore computes a sum of scale
 * QA*QB and shifts it right by log2(QB) to hand the next layer the scale QA it
 * expects.
 *
 * QA is 127 and not 255 on purpose: the activations stay inside a signed byte,
 * which is what the dot product instruction of the processor takes. The accuracy
 * that costs is a single step of the activation range.
 *
 * The accumulator cannot overflow its 16 bits: at most 32 pieces stand on the
 * board, and a weight of scale 127 that is twice the natural range is 254, so a
 * column sums to at most 32 * 254 = 8128.
 *
 * The value the last layer produces has the scale QA*QB. NET_VALUE_SCALE turns it
 * into the value unit of the engine, where a pawn is 80 to 95 - it is the number
 * the trainer scales its target with, and the one place where the two have to
 * mean the same thing.
 */

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace QaplaNnue {

	enum architecture : uint32_t {
		SQUARE_COUNT = 64,
		/** Piece planes of a perspective: twelve pieces less the own king. */
		PIECE_PLANES = 11,
		/** Features of one perspective: own king square, plane, piece square. */
		FEATURE_COUNT = PIECE_PLANES * SQUARE_COUNT * SQUARE_COUNT,
		/** Pieces on a board, and with it the largest number of active features. */
		MAX_ACTIVE_FEATURES = 32,

		ACCUMULATOR_SIZE = 256,
		/** The first dense layer sees both perspectives. */
		L1_INPUT_SIZE = 2 * ACCUMULATOR_SIZE,
		L1_SIZE = 32,
		L2_SIZE = 32
	};

	enum quantization : int32_t {
		/** Scale of an activation, and with it its largest value. */
		QA = 127,
		/** Scale of a weight of a dense layer. */
		QB = 64,
		/** log2(QB), the shift that takes a layer's sum back to the scale QA. */
		QB_SHIFT = 6,
		/**
		 * Turns the output of the last layer into the value unit of the engine.
		 * Has to be the same number the trainer scales with.
		 */
		NET_VALUE_SCALE = 400
	};

	/** Alignment the vector instructions want. */
	inline constexpr size_t NNUE_ALIGNMENT = 64;

	/**
	 * The weights of the net. 23 MB, so it lives on the heap and is loaded once.
	 *
	 * The feature weights are stored feature by feature, the 256 values of one
	 * feature next to each other: a refresh adds whole columns, and that is the
	 * order in which it wants to read them.
	 */
	struct alignas(NNUE_ALIGNMENT) Network {
		std::array<int16_t, ACCUMULATOR_SIZE> featureBias{};
		std::array<int16_t, size_t(FEATURE_COUNT)* size_t(ACCUMULATOR_SIZE)> featureWeight{};

		std::array<int32_t, L1_SIZE> l1Bias{};
		std::array<int8_t, size_t(L1_SIZE)* size_t(L1_INPUT_SIZE)> l1Weight{};

		std::array<int32_t, L2_SIZE> l2Bias{};
		std::array<int8_t, size_t(L2_SIZE)* size_t(L1_SIZE)> l2Weight{};

		int32_t outputBias = 0;
		std::array<int8_t, L2_SIZE> outputWeight{};
	};

	/**
	 * The file a net is stored in: a magic, a number that describes the shape, and
	 * the arrays above in their order, little endian.
	 *
	 * The shape number is checked on loading. A net of a different shape is
	 * refused instead of read as noise, which is the one mistake that costs days.
	 */
	inline constexpr char NNUE_MAGIC[8] = { 'Q', 'A', 'P', 'L', 'A', 'N', 'N', '1' };

	constexpr uint32_t architectureId() {
		return uint32_t(FEATURE_COUNT) * 31u + uint32_t(ACCUMULATOR_SIZE) * 7u
			+ uint32_t(L1_SIZE) * 3u + uint32_t(L2_SIZE) + uint32_t(QA) * 131u
			+ uint32_t(QB) * 17u;
	}

	/**
	 * Reads a net. Returns nothing if the file is missing, too short or of
	 * another shape, and says why on the console.
	 */
	std::unique_ptr<Network> readNetwork(const std::string& path);

	/**
	 * Writes a net, which is what the exporter of the trainer has to produce.
	 * Used by the tests to write one with known weights.
	 */
	bool writeNetwork(const std::string& path, const Network& network);

	/**
	 * Fills a net with pseudo random weights, so that the inference can be tested
	 * before a net has been trained.
	 */
	std::unique_ptr<Network> randomNetwork(uint64_t seed);
}
