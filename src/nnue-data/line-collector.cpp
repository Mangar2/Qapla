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
 * Collects the lines of a search into the position book, see line-collector.h
 */

#include "line-collector.h"
#include "../../search/searchstack.h"

using namespace QaplaNnueData;
using namespace QaplaBook;
using QaplaBasics::Move;
using QaplaBasics::NO_PIECE;
using QaplaBasics::NO_VALUE;
using QaplaBasics::value_t;
using QaplaSearch::ply_t;
using QaplaSearch::SearchStack;

namespace {

	/**
	 * The two squares and the promotion piece of a move, which is all a book
	 * stores of it.
	 */
	BookMove toBookMove(Move move) {
		return BookMove{ .from = move.getDeparture(), .to = move.getDestination(),
			.promotion = move.isPromote() ? move.getPromotion() : NO_PIECE };
	}
}

void LineCollector::setPrefix(std::span<const BookMove> prefix) {
	_prefix.assign(prefix.begin(), prefix.end());
}

void LineCollector::iterationFinished(ply_t remainingDepth, value_t rootValue) {
	// The value of the iteration below the collected one. Taking it from the
	// collected iteration itself would not work: it is not finished while its
	// nodes are being reported.
	if (remainingDepth == _settings.rootRemainingDepth - 1) {
		_rootValue = rootValue;
		_hasRootValue = true;
	}
}

void LineCollector::nodeFinished(const SearchStack& stack, ply_t remainingDepth, ply_t ply) {
	if (remainingDepth != _settings.collectRemainingDepth) return;
	if (stack[0].remainingDepth != _settings.rootRemainingDepth) return;
	if (ply < 1 || !_hasRootValue || isFull()) return;
	_counters.candidates++;

	if (ply + ply_t(_prefix.size()) > _settings.maxPly) {
		_counters.rejectedTooLong++;
		return;
	}

	const auto& node = stack[ply];
	if (node.sideToMoveIsInCheck || node.eval == NO_VALUE) {
		// Qapla does not evaluate a position the side to move is in check in, so
		// such a position is not one the net is ever asked about.
		_counters.rejectedInCheck++;
		return;
	}
	if (node.bestValue >= QaplaBasics::MIN_MATE_VALUE
		|| node.bestValue <= -QaplaBasics::MIN_MATE_VALUE) {
		_counters.rejectedMate++;
		return;
	}
	// The value of a node is seen from the side to move there, the root value
	// from the side to move at the root. On odd plies those are opponents.
	const value_t valueFromRoot = (ply % 2 == 0) ? node.bestValue : -node.bestValue;
	if (valueFromRoot - _rootValue > _settings.margin
		|| _rootValue - valueFromRoot > _settings.margin) {
		_counters.rejectedOffMargin++;
		return;
	}

	_line = _prefix;
	for (ply_t index = 1; index <= ply; index++) {
		const Move move = stack[index].previousMove;
		if (move.isNullMove() || move.isEmpty()) {
			// Null move pruning puts a move into the stack that cannot be
			// played, the line is not a line of the game.
			_counters.rejectedNullMove++;
			return;
		}
		_line.push_back(toBookMove(move));
	}

	_book.addLine(_line);
	_counters.added++;
	_counters.newLeaves = _book.leafCount() - _leavesAtStart;
}
