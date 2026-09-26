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
 * Evaluates a position with the net, computing both accumulators from scratch.
 *
 * Only the full computation, no incremental update yet. It is what a position set
 * by fen needs anyway, and it is the reference the incremental update will have to
 * agree with to the bit.
 *
 * Two paths, and they must return the same value: evaluate uses the vector
 * instructions of the processor where there are any, evaluateReference is plain
 * C++. The arithmetic is integer throughout, so "the same" means equal and not
 * close - which makes the comparison of the two a real test.
 */

#pragma once

#include "nnue-arch.h"
#include "../../basics/board.h"
#include "../../basics/evalvalue.h"

namespace QaplaNnue {

	/**
	 * Builds the accumulator of a perspective from nothing: the bias plus the
	 * weight column of every active feature.
	 */
	template <QaplaBasics::Piece PERSPECTIVE>
	void refreshAccumulator(const Network& network, const QaplaBasics::Board& board,
		int16_t* accumulator);

	/** Adds the weight column of a feature to an accumulator. */
	void addFeature(const Network& network, int16_t* accumulator, uint32_t feature);

	/** Takes it away again. */
	void removeFeature(const Network& network, int16_t* accumulator, uint32_t feature);

	/**
	 * The dense layers on two accumulators, the one of the side to move first.
	 * Returns the value in the unit of the engine.
	 */
	QaplaBasics::value_t forward(const Network& network, const int16_t* own,
		const int16_t* opponent);

	/** The same without vector instructions, for the test of the one above. */
	QaplaBasics::value_t forwardReference(const Network& network, const int16_t* own,
		const int16_t* opponent);

	class Evaluator {
	public:
		explicit Evaluator(const Network& network) : _network(network) {}

		/**
		 * The value of the position from the view of the side to move, in the
		 * value unit of the engine.
		 */
		QaplaBasics::value_t evaluate(const QaplaBasics::Board& board) const;

		/**
		 * The same, computed without vector instructions.
		 */
		QaplaBasics::value_t evaluateReference(const QaplaBasics::Board& board) const;

		/** True if evaluate uses vector instructions in this build. */
		static bool usesVectorInstructions();

	private:
		const Network& _network;
	};
}
