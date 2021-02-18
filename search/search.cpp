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
value_t Search::negaMaxLastPlys(MoveGenerator& position, SearchStack& stack, Move previousPlyMove, ply_t ply)
{
	if (ply >= SearchParameter::MAX_SEARCH_DEPTH) return Eval::eval(position);
	SearchVariables& searchInfo = stack[ply];
	value_t searchResult;
	Move curMove;

	if (stack[ply - 1].remainingDepth == 0) {
		searchInfo.previousMove = previousPlyMove;
		return QuiescenceSearch::search(position, *_computingInfo,
			previousPlyMove, -stack[ply - 1].beta, -stack[ply - 1].alpha, ply);
	}

	_computingInfo->_nodesSearched++;
	searchInfo.setFromPreviousPly(position, stack[ply - 1], previousPlyMove);
	WhatIf::whatIf.moveSelected(position, *_computingInfo, stack, previousPlyMove, ply);

	if (!hasCutoff<SearchType::LAST_PLY>(position, stack, searchInfo, ply)) {

		searchInfo.computeMoves(position);
		searchInfo.extendSearch(position);

		while (!(curMove = searchInfo.selectNextMove(position)).isEmpty()) {

			searchResult = -negaMaxLastPlys(position, stack, curMove, ply + 1);
			searchInfo.setSearchResult(searchResult, stack[ply + 1], curMove);
			WhatIf::whatIf.moveSearched(position, *_computingInfo, stack, curMove, ply);
			if (searchInfo.isFailHigh()) break;

		}
	}

	searchInfo.terminatePly(position);
	return searchInfo.bestValue;
}


value_t Search::negaMax(MoveGenerator& position, SearchStack& stack, Move previousPlyMove, ply_t ply) {
	
	if (ply >= SearchParameter::MAX_SEARCH_DEPTH) return Eval::eval(position);

	SearchVariables& searchInfo = stack[ply];
	value_t searchResult;
	bool cutoff = false;
	Move curMove;

	searchInfo.setFromPreviousPly(position, stack[ply - 1], previousPlyMove);
	_computingInfo->_nodesSearched++;

	WhatIf::whatIf.moveSelected(position, *_computingInfo, stack, previousPlyMove, ply);
	cutoff = hasCutoff<SearchType::NORMAL>(position, stack, searchInfo, ply);

	if (!cutoff) {
		searchInfo.computeMoves(position);
		searchInfo.extendSearch(position);

		while (!(curMove = searchInfo.selectNextMove(position)).isEmpty()) {

			if (searchInfo.remainingDepth > 2) {
				searchResult = -negaMax(position, stack, curMove, ply + 1);
			}
			else {
				searchResult = -negaMaxLastPlys(position, stack, curMove, ply + 1);
			}

			if (stack[0].remainingDepth > 1 && _clockManager->mustAbortCalculation(ply)) {
				break;
			}

			searchInfo.setSearchResult(searchResult, stack[ply + 1], curMove);
			WhatIf::whatIf.moveSearched(position, *_computingInfo, stack, curMove, ply);
			if (searchInfo.isFailHigh()) break;
		}
	}

	// if (stack[ply - 1].isPVSearch() && position.isWhiteToMove() == (searchInfo.bestValue <= 0)) { printf("info string ");  stack.printMoves(searchInfo.bestMove, ply); searchInfo.print(); }

	searchInfo.terminatePly(position);
	_computingInfo->setHashFullInPermill(searchInfo.getHashFullInPermill());
	_computingInfo->printSearchInfo(_clockManager->isTimeToSendNextInfo());
	return searchInfo.bestValue;

}

value_t Search::searchRoot(MoveGenerator& position, SearchStack& stack, ComputingInfo& computingInfo, ClockManager& clockManager) {
	_computingInfo = &computingInfo;
	_clockManager = &clockManager;
	position.computeAttackMasksForBothColors();

	SearchVariables& searchInfo = stack[0];
	value_t searchResult;
	RootMove* rootMove = &_rootMoves.getMove(0);

	searchInfo.computeMoves(position);

	_computingInfo->nextIteration(stack[0].moveProvider.getTotalMoveAmount());
	WhatIf::whatIf.moveSelected(position, *_computingInfo, stack, Move::EMPTY_MOVE, 0);

	for (size_t triedMoves = 0;;) {
		bool research = searchInfo.updateSearchType(uint32_t(triedMoves));
		if (!research) {
			if (triedMoves >= _rootMoves.getMoves().size()) break;
			rootMove = &_rootMoves.getMove(triedMoves);
			triedMoves++;
		}
		const Move curMove = _computingInfo->_currentConcideredMove = rootMove->getMove();

		if (searchInfo.remainingDepth > 2) {
			searchResult = -negaMax(position, stack, curMove, 1);
		}
		else {
			searchResult = -negaMaxLastPlys(position, stack, curMove, 1);
		}

		if (searchInfo.remainingDepth > 1 && _clockManager->mustAbortCalculation(0)) {
			break;
		}

		rootMove->set(searchResult, stack);
		searchInfo.setSearchResult(searchResult, stack[1], curMove);

		WhatIf::whatIf.moveSearched(position, *_computingInfo, stack, curMove, 0);
		updateThinkingInfoPly0(stack); 
		if (searchInfo.isFailHigh()) break;
	}

	searchInfo.terminatePly(position);
	_computingInfo->setHashFullInPermill(searchInfo.getHashFullInPermill());
	_computingInfo->printSearchInfo(_clockManager->isTimeToSendNextInfo());
	_rootMoves.bubbleSort(0);
	return searchInfo.bestValue;

}


