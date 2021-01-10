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
 * @copyright Copyright (c) 2021 Volker Böhm
 */

#include "search.h"
#include "whatif.h"

using namespace ChessSearch;

/**
 * Negamax algorithm for the last plies of the search
 */
template <bool WHATIF>
value_t Search::negaMaxLastPlys(MoveGenerator& board, SearchStack& stack, Move previousPlyMove, ply_t ply)
{
	SearchVariables& searchInfo = stack[ply];
	value_t searchResult;
	Move curMove;

	_computingInfo->nodesSearched++;
	searchInfo.setFromPreviousPly(board, stack[ply - 1], previousPlyMove);
	if (WHATIF) { WhatIf::whatIf.moveSelected(board, *_computingInfo, stack, previousPlyMove, ply); }

	if (!hasCutoff<WHATIF>(board, stack, searchInfo, ply)) {

		searchInfo.generateMoves(board);
		searchInfo.extendSearch(board);

		while (!(curMove = searchInfo.selectNextMove(board)).isEmpty()) {

			if (searchInfo.remainingDepth > 0) {
				searchResult = -negaMaxLastPlys<WHATIF>(board, stack, curMove, ply + 1);
			}
			else {
				stack[ply + 1].previousMove = curMove;
				searchResult = -QuiescenceSearch::search(board, eval, *_computingInfo,
					curMove, -searchInfo.beta, -searchInfo.alpha, ply + 1);
			}

			searchInfo.setSearchResult(searchResult, stack[ply + 1], curMove);
			if (WHATIF) { WhatIf::whatIf.moveSearched(board, *_computingInfo, stack, curMove, ply); }

		}
	}

	searchInfo.terminatePly(board);
	return searchInfo.bestValue;
}

/**
 * Search the last plies - either negamax or quiescense
 */
template <bool WHATIF>
value_t Search::searchLastPlys(MoveGenerator& board, SearchVariables& searchInfo, SearchStack& stack, Move curMove, ply_t ply) {
	value_t searchResult;
	if (searchInfo.remainingDepth <= 0) {
		searchResult = -QuiescenceSearch::search(board, eval, *_computingInfo, curMove,
			-searchInfo.beta, -searchInfo.alpha, ply + 1);
	}
	else {
		searchResult = -negaMaxLastPlys<WHATIF>(board, stack, curMove, ply + 1);
	}
	return searchResult;
}


template <bool WHATIF>
value_t Search::negaMax(MoveGenerator& board, SearchStack& stack, Move previousPlyMove, ply_t ply) {

	SearchVariables& searchInfo = stack[ply];
	value_t searchResult;
	bool cutoff = false;
	Move curMove;

	if (ply > 0) {
		searchInfo.setFromPreviousPly(board, stack[ply - 1], previousPlyMove);
		_computingInfo->nodesSearched++;
		cutoff = hasCutoff<WHATIF>(board, stack, searchInfo, ply);
	}
	if (WHATIF) { WhatIf::whatIf.moveSelected(board, *_computingInfo, stack, previousPlyMove, ply); }

	// Nullmove
	if (isNullmoveReasonable(board, searchInfo, ply)) {
		nullmove<WHATIF>(board, stack, ply);
	}

	if (!cutoff) {
		searchInfo.generateMoves(board);
		searchInfo.extendSearch(board);

		if (ply == 0) {
			_computingInfo->totalAmountOfMovesToConcider = stack[0].moveProvider.getTotalMoveAmount();
			_computingInfo->currentConcideredMove.setEmpty();
		}

		while (!(curMove = searchInfo.selectNextMove(board)).isEmpty()) {

			if (ply == 0) { _computingInfo->currentConcideredMove = curMove; }

			if (searchInfo.remainingDepth > 2) {
				searchResult = -negaMax<WHATIF>(board, stack, curMove, ply + 1);
			}
			else {
				searchResult = searchLastPlys<WHATIF>(board, searchInfo, stack, curMove, ply);
			}

			if (stack[0].remainingDepth > 1 && _clockManager->mustAbortCalculation(ply)) {
				break;
			}

			searchInfo.setSearchResult(searchResult, stack[ply + 1], curMove);

			if (WHATIF) { WhatIf::whatIf.moveSearched(board, *_computingInfo, stack, curMove, ply); }
			if (ply == 0) { updateThinkingInfoPly0(stack); }

		}
	}

	searchInfo.terminatePly(board);
	_computingInfo->printSearchInfoIfRequested();
	return searchInfo.bestValue;

}

value_t Search::searchRec(MoveGenerator& board, SearchStack& stack, ComputingInfo& computingInfo, ClockManager& clockManager) {
	_computingInfo = &computingInfo;
	_clockManager = &clockManager;
	board.computeAttackMasksForBothColors();
	return negaMax<DOWHATIF>(board, stack, Move::EMPTY_MOVE, 0);
}


