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
#include "quiescence.h"
#include "../bitbase/bitbasereader.h"

using namespace ChessSearch;

bool Search::hasBitbaseCutoff(const MoveGenerator& position, SearchVariables& node) {
	return false;
	// We only look into the bitbases, if we had a capture or a promote. This avoids "non-searching" on
	// positions of bitbases.
	// if (position.getPiecesSignature() == _rootSignature) return false;
	// if (curPly.alpha >= -MIN_MATE_VALUE && curPly.beta <= MIN_MATE_VALUE) return false;
	const QaplaBitbase::Result bitbaseValue = QaplaBitbase::BitbaseReader::getValueFromBitbase(position);
	if (bitbaseValue == QaplaBitbase::Result::Unknown) {
		return false;
	}
	_computingInfo._tbHits++;
	if (bitbaseValue == QaplaBitbase::Result::Win) { // && curPly.beta <= MIN_MATE_VALUE) {
		node.setCutoff(Cutoff::BITBASE, MIN_MATE_VALUE);
		return true;
	} 
	if (bitbaseValue == QaplaBitbase::Result::Loss) { // && curPly.alpha >= -MIN_MATE_VALUE) {
		node.setCutoff(Cutoff::BITBASE, -MIN_MATE_VALUE);
		return true;
	}
	if (bitbaseValue == QaplaBitbase::Result::Draw) {
		node.setCutoff(Cutoff::BITBASE, 1);
		return true;
	}
	return false;
}

/**
 * Check, if it is reasonable to do a nullmove search
 */
bool Search::isNullmoveReasonable(MoveGenerator& position, SearchVariables& node, ply_t ply) {
	bool result = true;
	if (!SearchParameter::DO_NULLMOVE) {
		result = false;
	}
	else if (node.remainingDepth <= SearchParameter::NULLMOVE_REMAINING_DEPTH) {
		result = false;
	}
	else if (node.noNullmove) {
		result = false;
	}
	else if (!position.sideToMoveHasQueenRookBishop(position.isWhiteToMove())) {
		result = false;
	}
	else if (position.isInCheck()) {
		result = false;
	}
	else if (node.beta >= MAX_VALUE - value_t(ply)) {
		result = false;
	}
	else if (node.beta <= -MAX_VALUE + value_t(ply)) {
		result = false;
	}
	else if (
		position.getMaterialValue(position.isWhiteToMove()).midgame() +
		MaterialBalance::PAWN_VALUE_MG < node.beta) {
		result = false;
	}
	else if (node.isTTValueBelowBeta(position, ply)) {
		result = false;
	}
	else if (ply + node.remainingDepth < 3) {
		result = false;
	}
	else if (node.isPVSearch()) {
		result = false;
	}

	return result;
}


/**
 * Check for a nullmove cutoff
 */
bool Search::isNullmoveCutoff(MoveGenerator& position, SearchStack& stack, uint32_t ply)
{
	SearchVariables& node = stack[ply];
	if (!isNullmoveReasonable(position, node, ply)) {
		return false;
	}
	assert(!position.isInCheck());
	
	auto depth = node.remainingDepth;
	depth -= SearchParameter::getNullmoveReduction(ply, depth);

	node.bestValue = depth > 2 ?
		-negaMax<SearchRegion::INNER>(position, Move::NULL_MOVE, stack, depth - 1, ply + 1):
		-negaMax<SearchRegion::NEAR_LEAF>(position, Move::NULL_MOVE, stack, depth - 1, ply + 1);

	WhatIf::whatIf.moveSearched(position, _computingInfo, stack, Move::NULL_MOVE, depth - 1, ply, "null");

	const bool isCutoff = node.bestValue >= node.beta;
	if (!isCutoff) {
		position.computeAttackMasksForBothColors();
	}
	// searchInfo.remainingDepth -= SearchParameter::getNullmoveVerificationDepthReduction(ply, searchInfo.remainingDepth);
	return isCutoff;
}

ply_t Search::computeLMR(SearchVariables& node, ply_t ply,  Move move)
{

	// Ability to disable history for a ply
	// if (node->mDisableHist) return 0;

	if (ply <= 2) return 0;
	if (move.isCapture()) return 0;
	if (node.moveNumber <= 3) return 0;
	if (node.moveNumber <= 7) return 1;
	if (node.isPVNode()) return 1;
	return 2;
}


/**
 * Negamax algorithm for plys 1..n-2
 */
template <Search::SearchRegion TYPE>
value_t Search::negaMax(MoveGenerator& position, Move previousPlyMove, SearchStack& stack, ply_t depth, ply_t ply) {
	
	if (ply >= SearchParameter::MAX_SEARCH_DEPTH) return Eval::eval(position, ply);
	if (TYPE == SearchRegion::INNER && stack[0].remainingDepth > 1 && _clockManager->mustAbortSearch(ply)) return -MAX_VALUE;

	SearchVariables& node = stack[ply];
	value_t result;
	bool cutoff = false;
	Move curMove;

	node.doMove(position, previousPlyMove);

	if (depth + node.sideToMoveIsInCheck < 0) {
		result = Quiescence::search(position, _computingInfo, node.previousMove,
			-stack[ply - 1].beta, -stack[ply - 1].alpha, ply);
		node.undoMove(position);
		return result;
	}

	node.setFromPreviousPly(position, stack[ply - 1], depth);
	_computingInfo._nodesSearched++;

	WhatIf::whatIf.moveSelected(position, _computingInfo, stack, node.previousMove, ply);

	// Cutoffs checks all kind of cutoffs including futility, nullmove, bitbase and others 
	cutoff = hasCutoff<TYPE>(position, stack, node, ply);

	if (cutoff) {
		node.undoMove(position);
		return node.bestValue;
	}

	node.computeMoves(position);
	depth = node.extendSearch(position);

	// The first move will be searched with PV window - the remaining with null window
	while (!(curMove = node.selectNextMove(position)).isEmpty()) {
		
		const auto lmr = depth > 0 ? computeLMR(node, ply, curMove) : 0;

		if (lmr > 0) {
			result = TYPE == SearchRegion::INNER && depth - lmr > 2 ?
				-negaMax<SearchRegion::INNER>(position, curMove, stack, depth - 1 - lmr, ply + 1) :
				-negaMax<SearchRegion::NEAR_LEAF>(position, curMove, stack, depth - 1 - lmr, ply + 1);
			WhatIf::whatIf.moveSearched(position, _computingInfo, stack, curMove, depth - 1 - lmr, ply, "LMR");
			if (result < node.alpha) continue;
		}
		
		result = TYPE == SearchRegion::INNER && depth > 2 ?
			-negaMax<SearchRegion::INNER>(position, curMove, stack, depth - 1, ply + 1) :
			-negaMax<SearchRegion::NEAR_LEAF>(position, curMove, stack, depth - 1, ply + 1);

		node.setSearchResult(result, stack[ply + 1], curMove);
		WhatIf::whatIf.moveSearched(position, _computingInfo, stack, curMove, depth - 1, ply);
		if (node.isFailHigh()) break;

		if (node.isNullWindowFailHigh()) {
			// If we fail high against the null-window beta but not against the beta at node start, we are in a PV Node with
			// null window search and the null window failed high. 
			node.setPVWindow();
			result = TYPE == SearchRegion::INNER && depth > 2 ?
				-negaMax<SearchRegion::INNER>(position, curMove, stack, depth - 1, ply + 1) :
				-negaMax<SearchRegion::NEAR_LEAF>(position, curMove, stack, depth - 1, ply + 1);

			node.setSearchResult(result, stack[ply + 1], curMove);
			WhatIf::whatIf.moveSearched(position, _computingInfo, stack, curMove, depth - 1, ply, "PV");
			if (node.isFailHigh()) break;
		}
		if (depth >= 2 && node.isPVSearch()) {
			node.setNullWindow();
		}
	} 
	// If search is aborted, bestValue and bestMove may be wrong, dont store them in the transposition table or killers  
	if (!_clockManager->isSearchStopped()) node.updateTTandKiller(position);
	node.undoMove(position);
	if (TYPE == SearchRegion::INNER) {
		_computingInfo.setHashFullInPermill(node.getHashFullInPermill());
		_computingInfo.printSearchInfo(_clockManager->isTimeToSendNextInfo());
	}
	return node.bestValue;
}

/**
 * Negamax algorithm for the first ply
 */
ComputingInfo Search::negaMaxRoot(MoveGenerator& position, SearchStack& stack, ClockManager& clockManager) {
	_clockManager = &clockManager;
	position.computeAttackMasksForBothColors();
	
	SearchVariables& node = stack[0];
	value_t result;
	ply_t depth = node.remainingDepth;
	RootMove* rootMove = &_rootMoves.getMove(0);
	stack.setPV(rootMove->getPV());

	node.computeMoves(position);
	_computingInfo.nextIteration(node);
	WhatIf::whatIf.moveSelected(position, _computingInfo, stack, Move::EMPTY_MOVE, 0);

	for (size_t triedMoves = 0; triedMoves < _rootMoves.getMoves().size();) {

		rootMove = &_rootMoves.getMove(triedMoves);
		triedMoves++;
	
		const Move curMove = rootMove->getMove();
		_computingInfo.setCurrentMove(curMove);

		while (true) {
			result = depth > 2 ?
				-negaMax<SearchRegion::INNER>(position, curMove, stack, depth - 1, 1):
				-negaMax<SearchRegion::NEAR_LEAF>(position, curMove, stack, depth - 1, 1);

			// AbortSearch must be checked first. If it is true, we do not have a valid search result
			if (node.remainingDepth > 1 && _clockManager->mustAbortSearch(0)) break;
			
			// We need to set the result to the root move before we update the node variables. 
			// The root move will check for a fail low and thus needs the alpha value not updated
			rootMove->set(result, stack);
			node.setSearchResult(result, stack[1], curMove);
			WhatIf::whatIf.moveSearched(position, _computingInfo, stack, curMove, depth - 1, 0);

			// If we failed high against the null window without failing high at the initial search window, we need to research 
			// with the full search window to get an accurate position value.
			if (!node.isFailHigh() && node.isNullWindowFailHigh()) {
				node.setPVWindow();
				continue;
			}
			if (depth >= 2) {
				node.setNullWindow();
			}
			break;
		};

		if (node.remainingDepth > 1 && _clockManager->mustAbortSearch(0)) break;
		_clockManager->setSearchedRootMove(node.isPVFailLow(), node.bestValue);

		_computingInfo.rootMoveSearched(stack); 
		if (node.isFailHigh()) break;
	}

	if (!_clockManager->isSearchStopped()) node.updateTTandKiller(position);
	_computingInfo.setHashFullInPermill(node.getHashFullInPermill());
	_computingInfo.printSearchInfo(_clockManager->isTimeToSendNextInfo());
	_rootMoves.bubbleSort(0);
	return _computingInfo;

}


