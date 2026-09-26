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
 * The accumulators of a search, one pair per ply, kept up to date move by move.
 *
 * A full computation reads 31 weight columns of 512 bytes out of a table of 23 MB,
 * which is bound by memory and costs about two microseconds - ten times what the
 * hand written evaluation costs. An ordinary move changes at most three features,
 * so building the child accumulator out of its parent's costs three columns
 * instead of 31, and that is the whole reason the net can be used in a search at
 * all.
 *
 * A king move is the exception: the king square is the first part of every feature
 * index of its own perspective, so all of them change at once and that perspective
 * is computed from nothing. Castling is treated the same way, the rook moving
 * along with the king. Both perspectives are refreshed there, which is more than
 * necessary - the opponent's side only loses and gains the king feature - and is
 * left that way until the cheap cases are measured.
 *
 * The stack is one per thread, and the search must push and pop it in step with
 * the board. Where that is not guaranteed - an evaluation asked for outside a
 * search - the stack says so and the value is computed from nothing. A build with
 * QAPLA_VERIFY_NNUE_INCREMENTAL, which a debug build sets, compares the
 * incremental accumulators against a fresh computation at every evaluation and
 * stops on the first difference. The arithmetic is integer, so a difference of one
 * is a difference.
 */

#pragma once

#include <string>
#include <vector>

#include "nnue-arch.h"
#include "../../basics/board.h"
#include "../../basics/move.h"

#if defined(_DEBUG) && !defined(QAPLA_VERIFY_NNUE_INCREMENTAL)
#define QAPLA_VERIFY_NNUE_INCREMENTAL
#endif

namespace QaplaNnue {

	/**
	 * Loads the net the engine plays with. Returns false and says why if it cannot
	 * be read.
	 */
	bool loadNetwork(const std::string& path);

	/** The net, or nullptr while none is loaded. */
	const Network* network();

	/**
	 * Says once that the engine was built to play with a net and has none. It keeps
	 * playing with the hand written evaluation, and the difference must not be taken
	 * for a result of the net.
	 */
	void reportMissingNetwork();

	class AccumulatorStack {
	public:
		/** Plies the stack holds. Beyond that it gives up and says so. */
		static constexpr uint32_t MAX_PLIES = 512;

		/**
		 * Computes both accumulators of the position from nothing and makes it the
		 * bottom of the stack. Called wherever a search starts.
		 */
		void reset(const QaplaBasics::Board& board);

		/**
		 * Follows a move that has just been applied to the board.
		 */
		void push(const QaplaBasics::Board& board, QaplaBasics::Move move);

		/** Follows the move being taken back. */
		void pop();

		/**
		 * The value of the position from the view of the side to move. Computes
		 * everything from nothing if the stack does not stand at the position.
		 */
		QaplaBasics::value_t evaluate(const QaplaBasics::Board& board);

		bool isValid() const {
			return _valid;
		}

	private:
		struct Entry {
			alignas(NNUE_ALIGNMENT) int16_t accumulator[2][ACCUMULATOR_SIZE];
		};

		Entry& top() {
			return _entries[_top];
		}

		void computeBoth(const QaplaBasics::Board& board, Entry& entry);

		std::vector<Entry> _entries;
		uint32_t _top = 0;
		bool _valid = false;
	};

	/** One stack per thread: a search runs on several of them. */
	extern thread_local AccumulatorStack accumulators;
}
