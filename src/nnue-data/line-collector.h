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
 * Watches a search and puts the lines it walks into the position book.
 *
 * A line is taken when the node it ends in has a given number of plies left
 * below it - the positions the evaluation is called for most, and therefore the
 * positions the net has to be good at. Of those, only the ones whose value is
 * within a margin of the root value are kept, so that the library holds
 * plausible positions instead of lines in which one side has already thrown a
 * piece away deep in the tree.
 *
 * Filtered on the value the search returned, not on the static evaluation, for
 * two reasons. The search of Qapla is fail soft: a node keeps the best value its
 * children actually returned instead of clamping it to the window, so a line
 * that is a rook down comes back at about minus a rook and is thrown out - by
 * the pruning, or by the static exchange evaluation of the quiescence search.
 * The static evaluation would let exactly that line through, being blind to a
 * piece that is hanging. And it would throw out the opposite case: captures are
 * searched first, so a node between a capture and its recapture is common, its
 * static evaluation is a piece off, and at a remaining depth of two the
 * recapture is searched and the value is right again.
 *
 * Only nodes that ran their move loop are reported, so the value is never one of
 * the synthetic values a pruning cutoff leaves behind.
 */

#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "../../search/search-observer.h"
#include "../book/packed-move.h"
#include "nnue-book.h"

namespace QaplaNnueData {

	class LineCollector : public QaplaSearch::ISearchObserver {
	public:
		struct Settings {
			/**
			 * The remaining depth of the root of the searches to collect from.
			 * It is one less than the depth the engine reports, so a depth 8
			 * search - "info depth 8" - has a root remaining depth of 7.
			 */
			QaplaSearch::ply_t rootRemainingDepth = 7;

			/** Plies that have to be left below a node for its line to be taken. */
			QaplaSearch::ply_t collectRemainingDepth = 2;

			/**
			 * How far the value of a position may be from the root value, in the
			 * internal unit of the engine where a pawn is 80 to 95.
			 */
			QaplaBasics::value_t margin = 100;

			/** Longest line, counted from the start position of the game. */
			QaplaSearch::ply_t maxPly = 120;

			/** Stops collecting after that many new leaves, 0 for no limit. */
			uint64_t maxNewLeaves = 0;
		};

		/**
		 * What the collector did, for the report at the end of a run.
		 */
		struct Counters {
			/** Nodes at the wanted remaining depth, before any filter. */
			uint64_t candidates = 0;
			uint64_t rejectedInCheck = 0;
			uint64_t rejectedMate = 0;
			uint64_t rejectedOffMargin = 0;
			uint64_t rejectedNullMove = 0;
			uint64_t rejectedTooLong = 0;
			/** Lines accepted, duplicates of lines already in the book included. */
			uint64_t added = 0;
			/**
			 * Leaves the run has produced, taken from the book itself. It is far
			 * below the number of accepted lines: a line that is already in the
			 * book adds nothing - and a search of a leaf that was searched
			 * before yields only such lines, the search being deterministic -
			 * while a line that branches off a leaf turns that leaf into an
			 * inner node and leaves the count where it was.
			 */
			uint64_t newLeaves = 0;
		};

		LineCollector(PositionBook& book, const Settings& settings)
			: _book(book), _settings(settings), _leavesAtStart(book.leafCount()) {
		}

		/**
		 * The moves that lead from the start position of the game to the
		 * position the next search starts at. They are the first plies of every
		 * line the collector writes, so the book stays rooted at the start
		 * position.
		 */
		void setPrefix(std::span<const QaplaBook::BookMove> prefix);

		/**
		 * The root value the margin is measured against, taken from the
		 * iteration one ply shallower than the one that is collected from.
		 */
		bool hasRootValue() const {
			return _hasRootValue;
		}

		/** True once the wanted number of leaves has been produced. */
		bool isFull() const {
			return _settings.maxNewLeaves != 0 && _counters.newLeaves >= _settings.maxNewLeaves;
		}

		const Counters& counters() const {
			return _counters;
		}

		void resetCounters() {
			_counters = Counters{};
		}

		void nodeFinished(const QaplaSearch::SearchStack& stack,
			QaplaSearch::ply_t remainingDepth, QaplaSearch::ply_t ply) override;

		void iterationFinished(QaplaSearch::ply_t remainingDepth,
			QaplaBasics::value_t rootValue) override;

	private:
		PositionBook& _book;
		Settings _settings;
		Counters _counters;

		std::vector<QaplaBook::BookMove> _prefix;
		/** Reused between calls, a line is built thousands of times per second. */
		std::vector<QaplaBook::BookMove> _line;

		const size_t _leavesAtStart;
		QaplaBasics::value_t _rootValue = 0;
		bool _hasRootValue = false;
	};
}
