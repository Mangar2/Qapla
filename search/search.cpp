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
 * @author Volker B�hm
 * @copyright Copyright (c) 2021 Volker B�hm
 */

#include "search.h"
#include "whatif.h"

using namespace ChessSearch;

/**
 * Negamax algorithm for the last plies of the search
 */
value_t Search::negaMaxLastPlys(MoveGenerator& board, SearchStack& stack, Move previousPlyMove, ply_t ply)
{
	SearchVariables& searchInfo = stack[ply];
	value_t searchResult;
	Move curMove;

	if (stack[ply - 1].remainingDepth == 0) {
		searchInfo.previousMove = previousPlyMove;
		return QuiescenceSearch::search(board, *_computingInfo,
			previousPlyMove, -stack[ply - 1].beta, -stack[ply - 1].alpha, ply);
	}

	_computingInfo->_nodesSearched++;
	searchInfo.setFromPreviousPly(board, stack[ply - 1], previousPlyMove);
	WhatIf::whatIf.moveSelected(board, *_computingInfo, stack, previousPlyMove, ply);

	if (!hasCutoff<SearchType::LAST_PLY>(board, stack, searchInfo, ply)) {

		searchInfo.computeMoves(board);
		searchInfo.extendSearch(board);

		while (!(curMove = searchInfo.selectNextMove(board)).isEmpty()) {

			searchResult = -negaMaxLastPlys(board, stack, curMove, ply + 1);
			searchInfo.setSearchResult(searchResult, stack[ply + 1], curMove);
			WhatIf::whatIf.moveSearched(board, *_computingInfo, stack, curMove, ply);
			if (searchInfo.isFailHigh()) break;

		}
	}

	searchInfo.terminatePly(board);
	return searchInfo.bestValue;
}


value_t Search::negaMax(MoveGenerator& board, SearchStack& stack, Move previousPlyMove, ply_t ply) {

	SearchVariables& searchInfo = stack[ply];
	value_t searchResult;
	bool cutoff = false;
	Move curMove;

	searchInfo.setFromPreviousPly(board, stack[ply - 1], previousPlyMove);
	_computingInfo->_nodesSearched++;

	WhatIf::whatIf.moveSelected(board, *_computingInfo, stack, previousPlyMove, ply);
	cutoff = hasCutoff<SearchType::NORMAL>(board, stack, searchInfo, ply);

	if (!cutoff) {
		searchInfo.computeMoves(board);
		searchInfo.extendSearch(board);

		while (!(curMove = searchInfo.selectNextMove(board)).isEmpty()) {

			if (searchInfo.remainingDepth > 2) {
				searchResult = -negaMax(board, stack, curMove, ply + 1);
			}
			else {
				searchResult = -negaMaxLastPlys(board, stack, curMove, ply + 1);
			}

			if (stack[0].remainingDepth > 1 && _clockManager->mustAbortCalculation(ply)) {
				break;
			}

			searchInfo.setSearchResult(searchResult, stack[ply + 1], curMove);
			WhatIf::whatIf.moveSearched(board, *_computingInfo, stack, curMove, ply);
			if (searchInfo.isFailHigh()) break;
		}
	}

	searchInfo.terminatePly(board);
	_computingInfo->printSearchInfo(_clockManager->isTimeToSendNextInfo());
	return searchInfo.bestValue;

}

value_t Search::searchRoot(MoveGenerator& board, SearchStack& stack, ComputingInfo& computingInfo, ClockManager& clockManager) {
	_computingInfo = &computingInfo;
	_clockManager = &clockManager;
	board.computeAttackMasksForBothColors();

	SearchVariables& searchInfo = stack[0];
	value_t searchResult;
	RootMove rootMove;

	searchInfo.computeMoves(board);

	_computingInfo->_totalAmountOfMovesToConcider = stack[0].moveProvider.getTotalMoveAmount();
	_computingInfo->_currentConcideredMove.setEmpty();
	_rootMoves.print();

	for (size_t triedMoves = 0;;) {
		bool research = searchInfo.updateSearchType(uint32_t(triedMoves));
		if (!research) {
			if (triedMoves >= _rootMoves.getMoves().size()) break;
			rootMove = _rootMoves.getMoves()[triedMoves];
			triedMoves++;
		}
		const Move curMove = _computingInfo->_currentConcideredMove = rootMove.getMove();

		if (searchInfo.remainingDepth > 2) {
			searchResult = -negaMax(board, stack, curMove, 1);
		}
		else {
			searchResult = -negaMaxLastPlys(board, stack, curMove, 1);
		}

		if (stack[0].remainingDepth > 1 && _clockManager->mustAbortCalculation(0)) {
			break;
		}

		rootMove.set(searchResult, stack);
		searchInfo.setSearchResult(searchResult, stack[1], curMove);

		WhatIf::whatIf.moveSearched(board, *_computingInfo, stack, curMove, 0);
		updateThinkingInfoPly0(stack); 
		if (searchInfo.isFailHigh()) break;
	}

	searchInfo.terminatePly(board);
	_computingInfo->printSearchInfo(_clockManager->isTimeToSendNextInfo());
	_rootMoves.bubbleSort(0);
	return searchInfo.bestValue;

}


