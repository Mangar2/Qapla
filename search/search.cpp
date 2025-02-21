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

using namespace QaplaSearch;

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
	auto R = SearchParameter::getNullmoveReduction(ply, depth, node.betaAtPlyStart, node.eval);

	stack[ply + 1].doMove(position, Move::NULL_MOVE);
	node.bestValue = depth - R > 2 ?
		-negaMax<SearchRegion::INNER>(position, stack, depth - R - 1, ply + 1) :
		-negaMax<SearchRegion::NEAR_LEAF>(position, stack, depth - R - 1, ply + 1);

	WhatIf::whatIf.moveSearched(position, _computingInfo, stack, Move::NULL_MOVE, depth - R - 1, ply, "null");
	stack[ply + 1].undoMove(position);
	bool isCutoff = node.bestValue >= node.beta;
	
	if (isCutoff) {
		position.computeAttackMasksForBothColors();
		node.isVerifyingNullmove = true;
		const auto verify = negaMaxPreSearch(position, stack, depth - R - 1, ply);
		node.isVerifyingNullmove = false;
		isCutoff = verify >= node.beta;
	}
	
	if (!isCutoff) {
		position.computeAttackMasksForBothColors();
		node.setFromParentNode(position, stack[ply - 1], depth);
	}
	// searchInfo.remainingDepth -= SearchParameter::getNullmoveVerificationDepthReduction(ply, searchInfo.remainingDepth);
	return isCutoff;
}

ply_t Search::computeLMR(SearchVariables& node, MoveGenerator& position, ply_t depth, ply_t ply, Move move)
{

	// Ability to disable history for a ply
	// if (node->mDisableHist) return 0;
	const auto moveNo = node.moveNumber;

	if (ply <= 1) return 0;
	if (move.isCapture()) return 0;
	if (moveNo <= 3) return 0;
	if (node.isCheckMove(position, move)) return 0;
	ply_t moveCountLmr = std::clamp(moveNo <= 7 ? 16 + (moveNo - 3) * 16 / 4 : 32 + (moveNo - 7) / 2, 16, 3 * 16);
	ply_t moveCountDepth = std::clamp(16 + (depth - 3) * 2, 16, 3 * 16);
	ply_t lmr = moveCountLmr * moveCountDepth / 256;
	if (node.isPVNode()) lmr /= 2;
	return lmr;
}

value_t Search::negaMaxPreSearch(MoveGenerator& position, SearchStack& stack, ply_t depth, ply_t ply) {
	SearchVariables& node = stack[ply];
	SearchVariables& childNode = stack[ply + 1];
	node.setFromParentNode(position, stack[ply - 1], depth);
	// Must be after setFromParentNode
	node.probeTT(position, ply);
	node.computeMoves(position, _butterflyBoard);
	Move curMove;
	while (!(curMove = node.selectNextMove(position)).isEmpty()) {

		childNode.doMove(position, curMove);
		const auto result = -negaMax<SearchRegion::INNER>(position, stack, depth - 1, ply + 1);
		node.setSearchResult(result, childNode, curMove);
		WhatIf::whatIf.moveSearched(position, _computingInfo, stack, curMove, depth - 1, ply, "PRE");
		childNode.undoMove(position);

		if (node.isFailHigh()) break;
	}

	// Attack masks are lazily computed. Dont forget to recreate them, if you search again
	return node.bestValue;
}

ply_t Search::se(MoveGenerator& position, SearchStack& stack, ply_t depth, ply_t ply) {
	if (!SearchParameter::DO_SE_EXTENSION) return 0;
	SearchVariables& node = stack[ply];
	SearchVariables& childNode = stack[ply + 1];

	// Do not double extend check moves
	if (SearchParameter::DO_CHECK_EXTENSIONS && node.sideToMoveIsInCheck) return 0;

	// We need a certain search depth left to efficiently calculate a singular extension
	if (depth < 4) return 0;

	// Limit maximal extension depth
	if (ply + depth > std::min(stack[0].remainingDepth * 2, int(SearchParameter::MAX_SEARCH_DEPTH))) return 0;


	node.setFromParentNode(position, stack[ply - 1], depth);
	
	// Must be after setFromPreviousPly
	node.probeTT(position, ply);

	// No se, if tt does not have a good move value (> alpha)
	if (node.ttValueIsUpperBound) return 0;

	// Singular extension based on tt move. Only, if the search found a value > alpha it found a "best move" in the position and is able to store it to
	// the transposition table
	auto ttMove = node.getTTMove();
	const auto seDepth = depth / 2;
	if (ttMove.isEmpty()) return 0;
	// We require a certain search depth for the tt move to be considered for a singular extension
	if (node.ttDepth < seDepth) return 0;
	// No se, if the tt already shows a mate or equivalent value
	if (node.ttValue < -WINNING_BONUS || node.ttValue > WINNING_BONUS) return 0;

	const auto seBeta = node.ttValue - SearchParameter::singularExtensionMargin(depth);
	node.setWindowAtPlyStart(seBeta - 1, seBeta);

	_computingInfo._nodesSearched++;

	// Cutoffs checks all kind of cutoffs including futility, nullmove, bitbase and others 
	if (checkCutoffAndSetEval<SearchRegion::NEAR_LEAF>(position, stack, node, ply)) return 0;

	node.computeMoves(position, _butterflyBoard);
	Move curMove;
	while (!(curMove = node.selectNextMove(position)).isEmpty()) {
		if (curMove == ttMove) continue;

		childNode.doMove(position, curMove);
		const auto result = -negaMax<SearchRegion::INNER>(position, stack, seDepth - 1, ply + 1);
		node.setSearchResult(result, childNode, curMove);
		WhatIf::whatIf.moveSearched(position, _computingInfo, stack, curMove, seDepth - 1, ply, "SE");
		childNode.undoMove(position);

		if (node.isFailHigh()) break;
	}

	// Attack masks are lazily computed. We need to make sure to recompute them, if we like to search twice in the same position
	position.computeAttackMasksForBothColors();
	
	return node.isFailHigh() ? 0: 1;
}


/**
 * Negamax algorithm for plys 1..n
 */
template <Search::SearchRegion TYPE>
value_t Search::negaMax(MoveGenerator& position, SearchStack& stack, ply_t depth, ply_t ply) {
	
	if (ply >= SearchParameter::MAX_SEARCH_DEPTH) return Eval::eval(position, ply);
	if (TYPE == SearchRegion::INNER && stack[0].remainingDepth > 1 && _clockManager->mustAbortSearch(ply)) return -MAX_VALUE;
	if (TYPE == SearchRegion::INNER) iid(position, stack, depth, ply);
	const auto seExtension = se(position, stack, depth, ply);

	SearchVariables& node = stack[ply];
	value_t result;
	bool cutoff = false;
	Move curMove;

	if (depth + (node.sideToMoveIsInCheck && SearchParameter::DO_CHECK_EXTENSIONS) < 0) {
		result = Quiescence::search(position, _computingInfo, node.previousMove,
			-stack[ply - 1].beta, -stack[ply - 1].alpha, ply);
		return result;
	}

	node.setFromParentNode(position, stack[ply - 1], depth);
	const auto nodesSearched = _computingInfo._nodesSearched;
	if (nodesSearched == 3220762) {
		position.print();
	}
	_computingInfo._nodesSearched++;

	WhatIf::whatIf.moveSelected(position, _computingInfo, stack, node.previousMove, ply);

	// Cutoffs checks all kind of cutoffs including futility, nullmove, bitbase and others 
	cutoff = checkCutoffAndSetEval<TYPE>(position, stack, node, ply);
	if (cutoff) {
		WhatIf::whatIf.cutoff(position, _computingInfo, stack, ply, node.cutoff);
		return node.bestValue;
	}

	node.computeMoves(position, _butterflyBoard);
	depth = node.extendSearch(position, seExtension);

	// The first move will be searched with PV window - the remaining with null window
	while (!(curMove = node.selectNextMove(position)).isEmpty()) {
		
		const auto lmr = computeLMR(node, position, depth, ply, curMove);
		
		// Move count pruning
		if (lmr > 0 && depth - lmr < 0) continue;

		stack[ply + 1].doMove(position, curMove);

		if (lmr > 0) {
			result = TYPE == SearchRegion::INNER && depth - lmr > 2 ?
				-negaMax<SearchRegion::INNER>(position, stack, depth - 1 - lmr, ply + 1) :
				-negaMax<SearchRegion::NEAR_LEAF>(position, stack, depth - 1 - lmr, ply + 1);
			WhatIf::whatIf.moveSearched(position, _computingInfo, stack, curMove, depth - 1 - lmr, ply, "LMR");
			if (result < node.alpha) {
				stack[ply + 1].undoMove(position);
				continue;
			}
			// searching modifies the attack masks. But they are required for the next move generation
			position.computeAttackMasksForBothColors();
		}
		
		result = TYPE == SearchRegion::INNER && depth > 2 ?
			-negaMax<SearchRegion::INNER>(position, stack, depth - 1, ply + 1) :
			-negaMax<SearchRegion::NEAR_LEAF>(position, stack, depth - 1, ply + 1);

		node.setSearchResult(result, stack[ply + 1], curMove);
		WhatIf::whatIf.moveSearched(position, _computingInfo, stack, curMove, depth - 1, ply);

		if (node.isNullWindowFailHigh() && !node.isFailHigh()) {
			// If we fail high against the null-window beta but not against the beta at node start, we are in a PV Node with
			// null window search and the null window failed high. 
			position.computeAttackMasksForBothColors();
			node.setPVWindow();
			result = TYPE == SearchRegion::INNER && depth > 2 ?
				-negaMax<SearchRegion::INNER>(position, stack, depth - 1, ply + 1) :
				-negaMax<SearchRegion::NEAR_LEAF>(position, stack, depth - 1, ply + 1);

			node.setSearchResult(result, stack[ply + 1], curMove);
			WhatIf::whatIf.moveSearched(position, _computingInfo, stack, curMove, depth - 1, ply, "PV");
		}

		stack[ply + 1].undoMove(position);
		if (node.isFailHigh()) break;

		if (depth >= 2 && node.isPVSearch()) {
			node.setNullWindow();
		}
	} 
	// If search is aborted, bestValue and bestMove may be wrong, dont store them in the transposition table or killers  
	if (!_clockManager->isSearchStopped()) node.updateTTandKiller(position, _butterflyBoard, depth);
	if (TYPE == SearchRegion::INNER) {
		_computingInfo.setHashFullInPermill(node.getHashFullInPermill());
		_computingInfo.printSearchInfo(_clockManager->isTimeToSendNextInfo());
	}
	return node.bestValue;
}

/**
 * Negamax algorithm for the first ply
 */
void Search::negaMaxRoot(MoveGenerator& position, SearchStack& stack, uint32_t skipMoves, ClockManager& clockManager) {
	_clockManager = &clockManager;
	position.computeAttackMasksForBothColors();
	SearchVariables& node = stack[0];
	value_t result;

	ply_t depth = node.remainingDepth;
	RootMove* rootMove = &_computingInfo.getRootMoves().getMove(0);

	stack.setPV(rootMove->getPV());
	// we use the movelist from rootmoves. node.computeMoves is only to initialize other variables
	node.computeMoves(position, _butterflyBoard);
	_computingInfo.nextIteration(node);
	WhatIf::whatIf.moveSelected(position, _computingInfo, stack, Move::EMPTY_MOVE, 0);
	Stockfish::Engine::set_position(position.getFen());

	for (uint32_t triedMoves = skipMoves; triedMoves < _computingInfo.getRootMoves().getMoves().size(); ++triedMoves) {

		rootMove = &_computingInfo.getRootMoves().getMove(triedMoves);
	
		const Move curMove = rootMove->getMove();
		_computingInfo.setCurrentMove(curMove);

		// Move research loop, if we failed high against the null window
		while (true) {
			stack[1].doMove(position, curMove);
			result = depth > 2 ?
				-negaMax<SearchRegion::INNER>(position, stack, depth - 1, 1):
				-negaMax<SearchRegion::NEAR_LEAF>(position, stack, depth - 1, 1);

			// AbortSearch must be checked first. If it is true, we do not have a valid search result
			if (node.remainingDepth > 1 && _clockManager->mustAbortSearch(0)) break;
			
			// We need to set the result to the root move before we update the node variables. 
			// The root move will check for a fail low and thus needs the alpha value not updated
			rootMove->set(result, stack);
			node.setSearchResult(result, stack[1], curMove);
			WhatIf::whatIf.moveSearched(position, _computingInfo, stack, curMove, depth - 1, 0);
			stack[1].undoMove(position);

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

		_computingInfo.printNewPV(triedMoves, node);
		if (node.isFailHigh()) break;
	}

	if (!_clockManager->isSearchStopped()) node.updateTTandKiller(position, _butterflyBoard, depth);
	_computingInfo.getRootMoves().bubbleSort(0);
	_computingInfo.setHashFullInPermill(node.getHashFullInPermill());
	_computingInfo.printSearchInfo(_clockManager->isTimeToSendNextInfo());
	_computingInfo.printSearchResult();
}


