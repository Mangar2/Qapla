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
 * Check, if it is reasonable to do a nullmove search
 */
bool Search::isNullmoveReasonable(MoveGenerator& position, SearchVariables& searchInfo, ply_t ply) {
	bool result = true;
	if (!SearchParameter::DO_NULLMOVE) {
		result = false;
	}
	else if (searchInfo.remainingDepth <= 2) {
		result = false;
	}
	else if (searchInfo.noNullmove) {
		result = false;
	}
	else if (!position.sideToMoveHasQueenRookBishop(position.isWhiteToMove())) {
		result = false;
	}
	else if (position.isInCheck()) {
		result = false;
	}
	else if (searchInfo.beta >= MAX_VALUE - value_t(ply)) {
		result = false;
	}
	else if (searchInfo.beta <= -MAX_VALUE + value_t(ply)) {
		result = false;
	}
	else if (
		position.getMaterialValue(position.isWhiteToMove()).midgame() +
		MaterialBalance::PAWN_VALUE_MG < searchInfo.beta) {
		result = false;
	}
	else if (searchInfo.isTTValueBelowBeta(position, ply)) {
		result = false;
	}
	else if (ply + searchInfo.remainingDepth < 3) {
		result = false;
	}
	else if (searchInfo.isPVSearch()) {
		result = false;
	}

	return result;
}


/**
 * Check for a nullmove cutoff
 */
bool Search::isNullmoveCutoff(MoveGenerator& position, SearchStack& stack, uint32_t ply)
{
	SearchVariables& searchInfo = stack[ply];
	if (!isNullmoveReasonable(position, searchInfo, ply)) {
		return false;
	}
	assert(!position.isInCheck());
	
	stack[ply + 1].doMove(position, searchInfo, Move::NULL_MOVE);
	stack[ply + 1].setNullmove();
	
	if (searchInfo.remainingDepth > 2) {
		searchInfo.bestValue = -negaMax(position, stack, ply + 1);
	}
	else {
		searchInfo.bestValue = -negaMaxLastPlys(position, stack, ply + 1);
	}
	stack[ply + 1].undoMove(position);
	WhatIf::whatIf.moveSearched(position, _computingInfo, stack, Move::NULL_MOVE, ply);

	const bool isCutoff = searchInfo.bestValue >= searchInfo.beta;
	if (!isCutoff) {
		position.computeAttackMasksForBothColors();
	}
	// searchInfo.remainingDepth -= SearchParameter::getNullmoveVerificationDepthReduction(ply, searchInfo.remainingDepth);
	return isCutoff;
}

/**
 * Negamax algorithm for the last plies of the search
 */
value_t Search::negaMaxLastPlys(MoveGenerator& position, SearchStack& stack, ply_t ply)
{
	if (ply >= SearchParameter::MAX_SEARCH_DEPTH) return Eval::eval(position);
	SearchVariables& searchInfo = stack[ply];
	value_t searchResult;
	Move curMove;

	if (stack[ply - 1].remainingDepth == 0) {
		return QuiescenceSearch::search(position, _computingInfo, searchInfo.previousMove,
			-stack[ply - 1].beta, -stack[ply - 1].alpha, ply);
	}

	_computingInfo._nodesSearched++;
	searchInfo.setFromPreviousPly(position, stack[ply - 1]);
	WhatIf::whatIf.moveSelected(position, _computingInfo, stack, searchInfo.previousMove, ply);

	if (!hasCutoff<SearchFinding::LAST_PLY>(position, stack, searchInfo, ply)) {

		searchInfo.computeMoves(position);
		searchInfo.extendSearch(position);

		while (!(curMove = searchInfo.selectNextMove(position)).isEmpty()) {
			
			stack[ply + 1].doMove(position, searchInfo, curMove);
			searchResult = -negaMaxLastPlys(position, stack, ply + 1);
			stack[ply + 1].undoMove(position);

			searchInfo.setSearchResult(searchResult, stack[ply + 1], curMove);
			WhatIf::whatIf.moveSearched(position, _computingInfo, stack, curMove, ply);
			if (searchInfo.isFailHigh()) break;

		}
	}

	searchInfo.terminatePly(position);
	return searchInfo.bestValue;
}


value_t Search::negaMax(MoveGenerator& position, SearchStack& stack, ply_t ply) {
	
	if (ply >= SearchParameter::MAX_SEARCH_DEPTH) return Eval::eval(position);

	SearchVariables& searchInfo = stack[ply];
	value_t searchResult;
	bool cutoff = false;
	Move curMove;

	searchInfo.setFromPreviousPly(position, stack[ply - 1]);
	_computingInfo._nodesSearched++;

	WhatIf::whatIf.moveSelected(position, _computingInfo, stack, searchInfo.previousMove, ply);
	cutoff = hasCutoff<SearchFinding::NORMAL>(position, stack, searchInfo, ply);

	if (!cutoff) {
		iid(position, stack, ply);
		searchInfo.computeMoves(position);
		searchInfo.extendSearch(position);

		while (!(curMove = searchInfo.selectNextMove(position)).isEmpty()) {

			stack[ply + 1].doMove(position, searchInfo, curMove);
			if (searchInfo.remainingDepth > 2) {
				searchResult = -negaMax(position, stack, ply + 1);
			}
			else {
				searchResult = -negaMaxLastPlys(position, stack, ply + 1);
			}
			stack[ply + 1].undoMove(position);

			if (stack[0].remainingDepth > 1 && _clockManager->mustAbortSearch(ply)) {
				break;
			}

			searchInfo.setSearchResult(searchResult, stack[ply + 1], curMove);
			WhatIf::whatIf.moveSearched(position, _computingInfo, stack, curMove, ply);
			if (searchInfo.isFailHigh()) break;
		}
	}

	searchInfo.terminatePly(position);
	_computingInfo.setHashFullInPermill(searchInfo.getHashFullInPermill());
	_computingInfo.printSearchInfo(_clockManager->isTimeToSendNextInfo());
	return searchInfo.bestValue;

}

ComputingInfo Search::searchRoot(MoveGenerator& position, SearchStack& stack, ClockManager& clockManager) {
	_clockManager = &clockManager;
	position.computeAttackMasksForBothColors();
	
	SearchVariables& searchInfo = stack[0];
	value_t searchResult;
	RootMove* rootMove = &_rootMoves.getMove(0);
	stack.setPV(rootMove->getPV());

	searchInfo.computeMoves(position);
	_computingInfo.nextIteration(searchInfo);
	WhatIf::whatIf.moveSelected(position, _computingInfo, stack, Move::EMPTY_MOVE, 0);

	for (size_t triedMoves = 0;;) {
		bool research = searchInfo.updateSearchType(uint32_t(triedMoves));
		if (!research) {
			if (triedMoves >= _rootMoves.getMoves().size()) break;
			rootMove = &_rootMoves.getMove(triedMoves);
			triedMoves++;
		}
		const Move curMove = rootMove->getMove();
		_computingInfo.setCurrentMove(curMove);

		stack[1].doMove(position, searchInfo, curMove);
		if (searchInfo.remainingDepth > 2) {
			searchResult = -negaMax(position, stack, 1);
		}
		else {
			searchResult = -negaMaxLastPlys(position, stack, 1);
		}
		stack[1].undoMove(position);

		if (searchInfo.remainingDepth > 1 && _clockManager->mustAbortSearch(0)) {
			break;
		}

		rootMove->set(searchResult, stack);
		searchInfo.setSearchResult(searchResult, stack[1], curMove);
		_clockManager->setSearchedRootMove(searchInfo.isPVFailLow(), searchInfo.bestValue);

		WhatIf::whatIf.moveSearched(position, _computingInfo, stack, curMove, 0);
		_computingInfo.rootMoveSearched(stack); 
		if (searchInfo.isFailHigh()) break;
	}

	searchInfo.terminatePly(position);
	_computingInfo.setHashFullInPermill(searchInfo.getHashFullInPermill());
	_computingInfo.printSearchInfo(_clockManager->isTimeToSendNextInfo());
	_rootMoves.bubbleSort(0);
	return _computingInfo;

}


