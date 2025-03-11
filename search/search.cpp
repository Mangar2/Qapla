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
#include "whatIf.h"
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
bool Search::isNullmoveReasonable(MoveGenerator& position, SearchVariables& node, ply_t depth, ply_t ply) {
	bool result = true;
	if (!SearchParameter::DO_NULLMOVE) {
		result = false;
	}
	/*
	else if (node.eval - 100 + 10 * depth < node.beta) {
		result = false;
	}
	*/
	else if (node.eval < node.beta) {
		return false;
	} 
	else if (position.getMaterialValue(position.isWhiteToMove()).midgame() + MaterialBalance::PAWN_VALUE_MG < node.beta) {
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
	else if (node.ttValue != NO_VALUE && node.isTTValueBelowBeta(position, ply)) {
		result = false;
	}
	else if (ply + depth < 3) {
		result = false;
	}
	else if (node.isPVNode()) {
		result = false;
	}

	return result;
}


/**
 * Check for a nullmove cutoff
 */
bool Search::isNullmoveCutoff(MoveGenerator& position, SearchStack& stack, ply_t depth, ply_t ply)
{
	SearchVariables& node = stack[ply];
	if (!isNullmoveReasonable(position, node, depth, ply)) {
		return false;
	}
	assert(!position.isInCheck());
	SearchVariables& childNode = stack[ply + 1];

	ply_t R = SearchParameter::getNullmoveReduction(ply, depth, node.betaAtPlyStart, node.eval);

	childNode.doMove(position, Move::NULL_MOVE);
	node.bestValue = depth - R > 2 ?
		-negaMax<SearchRegion::INNER>(position, stack, -node.alpha - 1, -node.alpha, depth - R - 1, ply + 1) :
		-negaMax<SearchRegion::NEAR_LEAF>(position, stack, -node.alpha - 1, -node.alpha, depth - R - 1, ply + 1);

	WhatIf::whatIf.moveSearched(position, _computingInfo, stack, Move::NULL_MOVE, depth - R - 1, ply, node.bestValue, "null");
	childNode.undoMove(position);
	bool isCutoff = node.bestValue >= node.beta;
	
	if (isCutoff && depth - R - 1 >= 0) {
		position.computeAttackMasksForBothColors();
		node.isVerifyingNullmove = true;
		const auto verify = negaMaxPreSearch(position, stack, node.alpha, node.beta, depth - R - 1, ply);
		node.isVerifyingNullmove = false;
		isCutoff = verify >= node.beta;
	}
	
	if (!isCutoff) {
		position.computeAttackMasksForBothColors();
		node.setToPlyStart();
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

value_t Search::negaMaxPreSearch(MoveGenerator& position, SearchStack& stack, value_t alpha, value_t beta, ply_t depth, ply_t ply) {
	SearchVariables& node = stack[ply];
	SearchVariables& childNode = stack[ply + 1];
	node.setFromParentNode(position, stack[ply - 1], alpha, beta, depth, false);
	// Must be after setFromParentNode
	node.probeTT(false, alpha, beta, depth, ply);
	node.computeMoves(position, _butterflyBoard);
	Move curMove;
	while (!(curMove = node.selectNextMove(position)).isEmpty()) {

		childNode.doMove(position, curMove);
		const auto result = -negaMax<SearchRegion::INNER>(position, stack, -node.alpha - 1, -node.alpha, depth - 1, ply + 1);
		node.setSearchResult(result, childNode, curMove);
		WhatIf::whatIf.moveSearched(position, _computingInfo, stack, curMove, depth - 1, ply, result, "PRE");
		childNode.undoMove(position);

		if (node.isFailHigh()) break;
	}

	// Attack masks are lazily computed. Dont forget to recreate them, if you search again
	return node.bestValue;
}

/**
 * Compute internal iterative deepening
 * IID modifies variables from stack[ply] (like move counter, search depth, ...)
 * Thus it must be called before setting the stack in negamax (setFromPreviousPly).
 */
void Search::iid(MoveGenerator& position, SearchStack& stack, value_t alpha, value_t beta, ply_t depth, ply_t ply) {
	SearchVariables& node = stack[ply];

	if (!SearchParameter::DO_IID) return;
	if (depth <= SearchParameter::getIIDMinDepth()) return;
	if (!node.getTTMove().isEmpty()) return;

	ply_t iidR = SearchParameter::getIIDReduction(depth);
	const value_t curValue = negaMax<SearchRegion::PV>(position, stack, alpha, beta, depth - iidR, ply);
	WhatIf::whatIf.moveSearched(position, _computingInfo, stack, stack[ply].previousMove, depth - iidR, ply - 1, curValue, "IID");
	position.computeAttackMasksForBothColors();
	if (!node.bestMove.isEmpty()) {
		node.setTTMove(node.bestMove);
	}

}

ply_t Search::se(MoveGenerator& position, SearchStack& stack, value_t alpha, value_t beta, ply_t depth, ply_t ply) {
	if (!SearchParameter::DO_SE_EXTENSION) return 0;
	SearchVariables& node = stack[ply];
	SearchVariables& childNode = stack[ply + 1];
	SearchVariables& parentNode = stack[ply - 1];

	// Do not double extend check moves
	if (SearchParameter::DO_CHECK_EXTENSIONS && node.sideToMoveIsInCheck) return 0;

	// We need a certain search depth left to efficiently calculate a singular extension
	if (depth < 4) return 0;

	// Limit maximal extension depth
	if (ply + depth > std::min(stack[0].remainingDepth * 2, int(SearchParameter::MAX_SEARCH_DEPTH))) return 0;

	node.setFromParentNode(position, parentNode, alpha, beta, depth, false);
	
	// Must be after setFromParentNode
	node.probeTT(false, alpha, beta, depth, ply);

	// No se, if tt does not have a good move value (> alpha)
	if (node.ttValueIsUpperBound) return 0;
	// We need a ttValue to have something to search for
	if (node.ttValue == NO_VALUE) return 0;

	// Singular extension based on tt move. Only, if the search found a value > alpha it found a "best move" in the position and is able to store it to
	// the transposition table
	auto ttMove = node.getTTMove();
	const auto seDepth = depth / 2;
	if (ttMove.isEmpty()) return 0;
	// We require a certain search depth for the tt move to be considered for a singular extension
	if (node.ttDepth < seDepth) return 0;
	// No se, if the tt already shows a mate or equivalent value
	if (node.ttValue != NO_VALUE && (node.ttValue < -MIN_MATE_VALUE || node.ttValue > MIN_MATE_VALUE)) return 0;

	node.setSE(SearchParameter::singularExtensionMargin(depth));
	_computingInfo._nodesSearched++;

	// Cutoffs checks all kind of cutoffs including futility, nullmove, bitbase and others 
	if (checkCutoffAndSetEval<SearchRegion::NEAR_LEAF>(position, stack, node, seDepth, ply)) return 0;

	node.computeMoves(position, _butterflyBoard);
	Move curMove;
	while (!(curMove = node.selectNextMove(position)).isEmpty()) {
		if (curMove == ttMove) continue;

		childNode.doMove(position, curMove);
		const auto result = -negaMax<SearchRegion::INNER>(position, stack, -node.alpha - 1, -node.alpha, seDepth - 1, ply + 1);
		node.setSearchResult(result, childNode, curMove);
		WhatIf::whatIf.moveSearched(position, _computingInfo, stack, curMove, seDepth - 1, ply, result, "SE");
		childNode.undoMove(position);

		if (node.isFailHigh()) break;
	}

	// Attack masks are lazily computed. We need to make sure to recompute them, if we like to search twice in the same position
	position.computeAttackMasksForBothColors();
	
	return node.isFailHigh() ? 0: 1;
}

/**
 * Checks for a cutoff not requiering search or eval
 */
template <Search::SearchRegion TYPE>
bool Search::nonSearchingCutoff(MoveGenerator& position, SearchStack& stack, SearchVariables& node, value_t alpha, value_t beta, ply_t depth, ply_t ply) {
	assert(ply >= 1);

	node.cutoff = Cutoff::NONE;
	node.setHashSignature(position);

	if (alpha > MAX_VALUE - value_t(ply)) {
		node.setCutoff(Cutoff::FASTER_MATE_FOUND, MAX_VALUE - value_t(ply));
	}
	else if (beta < -MAX_VALUE + value_t(ply)) {
		node.setCutoff(Cutoff::FASTER_MATE_FOUND, -MAX_VALUE + value_t(ply));
	}
	else if (position.drawDueToMissingMaterial()) {
		node.setCutoff(Cutoff::NOT_ENOUGH_MATERIAL, 0);
	}
	else if (stack.isDrawByRepetitionInSearchTree(position, ply)) {
		node.setCutoff(Cutoff::DRAW_BY_REPETITION, 0);
	}
	else if (ply >= SearchParameter::MAX_SEARCH_DEPTH) {
		node.setCutoff(Cutoff::MAX_SEARCH_DEPTH, Eval::eval(position, ply));
	}
	else if (TYPE != SearchRegion::NEAR_LEAF && hasBitbaseCutoff(position, node)) {
		node.setCutoff(Cutoff::BITBASE);
	}
	else if (TYPE != SearchRegion::NEAR_LEAF && stack[0].remainingDepth > 1 && _clockManager->emergencyAbort()) {
		node.setCutoff(Cutoff::ABORT, -MAX_VALUE);
	}

	WhatIf::whatIf.cutoff(position, _computingInfo, stack, ply, node.cutoff);
	return node.cutoff != Cutoff::NONE;
}

/**
 * Negamax algorithm for plys 1..n
 */
template <Search::SearchRegion TYPE>
value_t Search::negaMax(MoveGenerator& position, SearchStack& stack, value_t alpha, value_t beta, ply_t depth, ply_t ply) {

	SearchVariables& node = stack[ply];
	node.pvMovesStore[ply] = Move::EMPTY_MOVE;

	// 1. Detect direct cutoffs without requiring search or eval
	// This includes checking the hash and setting the hash information like ttMove
	if (nonSearchingCutoff<TYPE>(position, stack, node, alpha, beta, depth, ply)) return node.bestValue;
	SearchVariables& childNode = stack[ply + 1];

	// 2. Quiescense search
	if (depth < 0) {
		return Quiescence::search(TYPE == SearchRegion::PV, position, _computingInfo, node.previousMove, alpha, beta, ply);
	}

	const auto nodesSearched = _computingInfo._nodesSearched;
	
	if (nodesSearched == 3925) {
		position.print();
		for (int i = 0; i < ply; i++) {
			stack[i].printTTEntry();
		}
	}
	
	_computingInfo._nodesSearched++;


	// 3. Probe the hash table. This will set the hash information to the node.
	// The required hash signature is set in nonSearchingCutoff
	if (node.probeTT(TYPE == SearchRegion::PV, alpha, beta, depth, ply)) {
		node.setCutoff(Cutoff::HASH);
		return node.bestValue;
	}

	// 4. IID recursive for pv move. Must be before node.setFromParentNode, as it modifies node values
	if (TYPE == SearchRegion::PV) iid(position, stack, alpha, beta, depth, ply);

	// 5. Singular extension
	const auto seExtension = se(position, stack, alpha, beta, depth, ply);

	value_t result;
	Move curMove;

	node.setFromParentNode(position, stack[ply - 1], alpha, beta, depth, TYPE == SearchRegion::PV);
	
	WhatIf::whatIf.moveSelected(position, _computingInfo, stack, node.previousMove, ply);

	// Check all kind of early cutoffs including futility, nullmove, bitbase and others 
	// Additionally set eval. This is done as late as possible, as it is very time consuming. Some cutoff checks needs eval.
	if (checkCutoffAndSetEval<TYPE>(position, stack, node, depth, ply)) {
		WhatIf::whatIf.cutoff(position, _computingInfo, stack, ply, node.cutoff);
		return node.bestValue;
	}

	node.computeMoves(position, _butterflyBoard);
	depth = node.extendSearch(position, stack[0].remainingDepth, seExtension);

	// Loop through all moves
	while (!(curMove = node.selectNextMove(position)).isEmpty()) {

		const auto lmr = computeLMR(node, position, depth, ply, curMove);

		// 1. Move count pruning
		if (lmr > 0 && depth - lmr < 0 && node.bestValue > -MIN_MATE_VALUE) continue;

		childNode.doMove(position, curMove);

		// 2. Late move reduction search
		// We continue with the next move, if the lmr search returns a value less than alpha
		if (lmr > 0) {
			result = TYPE != SearchRegion::NEAR_LEAF && depth - lmr > 2 ?
				-negaMax<SearchRegion::INNER>(position, stack, -node.alpha - 1, -node.alpha, depth - 1 - lmr, ply + 1) :
				-negaMax<SearchRegion::NEAR_LEAF>(position, stack, -node.alpha - 1, -node.alpha, depth - 1 - lmr, ply + 1);
			WhatIf::whatIf.moveSearched(position, _computingInfo, stack, curMove, depth - 1 - lmr, ply, result, "LMR");
			if (result <= node.alpha) {
				childNode.undoMove(position);
				// We improve value on lmr result. Especially important to not get false mate values due to skipped escape moves
				if (result > node.bestValue) {
					node.bestValue = result;
				}
				continue;
			}
			// searching modifies the attack masks. But they are required for the next move generation
			position.computeAttackMasksForBothColors();
		}
		// 3. Searching with null window either because of non pv search or because it is not the first move in pv.
		// Additionally we do not go to null window search on PV, if depth is 1 or 0
		// We do not return fail high from a null window search in PV node
		bool isDirectPVWindowSearch = TYPE == SearchRegion::PV && (node.moveNumber == 1 || depth <= 1);
		if (!isDirectPVWindowSearch) {
			result = TYPE != SearchRegion::NEAR_LEAF && depth > 2 ?
				-negaMax<SearchRegion::INNER>(position, stack, -node.alpha - 1, -node.alpha, depth - 1, ply + 1) :
				-negaMax<SearchRegion::NEAR_LEAF>(position, stack, -node.alpha - 1, -node.alpha, depth - 1, ply + 1);
			WhatIf::whatIf.moveSearched(position, _computingInfo, stack, curMove, depth - 1, ply, result, TYPE == SearchRegion::PV ? "ZeroW" : "Std.");
		}
		// 4. Full window PV search or research the move with full window, if result is better than alpha
		if (TYPE == SearchRegion::PV && (isDirectPVWindowSearch || result > node.alpha)) {
			const ply_t adjustedDepth = depth <= 0 && curMove == node.getTTMove() && ply < stack[0].remainingDepth * 2 ? 1 : depth;
			if (!isDirectPVWindowSearch) {
				position.computeAttackMasksForBothColors();
			}
			result = -negaMax<SearchRegion::PV>(position, stack, -node.beta, -node.alpha, adjustedDepth - 1, ply + 1);
			WhatIf::whatIf.moveSearched(position, _computingInfo, stack, curMove, adjustedDepth - 1, ply, result, "PV");
		}

		node.setSearchResult(result, childNode, curMove);

		childNode.undoMove(position);
		if (node.isFailHigh()) break;
	}
	// 5. Update tt and killer, but not if search is aborted as then bestValue and bestMove may be wrong  
	if (!_clockManager->isSearchStopped()) node.updateTTandKiller(position, _butterflyBoard, TYPE == SearchRegion::PV, depth);
	// Inform the user about advances in search
	if (TYPE != SearchRegion::NEAR_LEAF) {
		_computingInfo.setHashFullInPermill(node.getHashFullInPermill());
		_computingInfo.printSearchInfo(_clockManager->isTimeToSendNextInfo());
	}
	return node.bestValue;
}

/**
 * Negamax algorithm for the first ply
 */
void Search::negaMaxRoot(MoveGenerator& position, SearchStack& stack, uint32_t skipMoves, ClockManager& clockManager) {
	if (skipMoves >= _computingInfo.getMovesAmount()) return;

	_clockManager = &clockManager;
	position.computeAttackMasksForBothColors();
	SearchVariables& node = stack[0];
	value_t result;

	ply_t depth = node.remainingDepth;

	// we use the movelist from rootmoves. node.computeMoves is only to initialize other variables
	node.computeMoves(position, _butterflyBoard);
	_computingInfo.nextIteration(node);
	WhatIf::whatIf.moveSelected(position, _computingInfo, stack, Move::EMPTY_MOVE, 0);
#ifdef USE_STOCKFISH_EVAL
	Stockfish::Engine::set_position(position.getFen());
#endif

	for (uint32_t triedMoves = 0; triedMoves < _computingInfo.getMovesAmount(); ++triedMoves) {

		RootMove& rootMove = _computingInfo.getRootMoves().getMove(triedMoves);
		if (rootMove.isPVSearchedInWindow(depth) && triedMoves < skipMoves) {
			continue;
		}
		stack.setPV(rootMove.getPV());
	
		const Move curMove = rootMove.getMove();
		_computingInfo.setCurrentMove(triedMoves, curMove);

		stack[1].doMove(position, curMove);
		auto pvSearch = depth <= 1 || triedMoves <= skipMoves;
		result = pvSearch ?
			-negaMax<SearchRegion::PV>(position, stack, -node.beta, -node.alpha, depth - 1, 1):
			-negaMax<SearchRegion::INNER>(position, stack, -node.alpha - 1, -node.alpha, depth - 1, 1);
		stack[1].undoMove(position);

		// AbortSearch must be checked first. If it is true, we do not have a valid search result
		if (_clockManager->isSearchStopped()) break;

		if (result > node.alpha && !pvSearch) {
			WhatIf::whatIf.moveSearched(position, _computingInfo, stack, curMove, depth - 1, 0, result);
			pvSearch = true;
			node.setPVWindow();
			stack[1].doMove(position, curMove);
			result = -negaMax<SearchRegion::PV>(position, stack, -node.beta, -node.alpha, depth - 1, 1);
			stack[1].undoMove(position);
		}

		// AbortSearch must be checked first. If it is true, we do not have a valid search result
		if (_clockManager->isSearchStopped()) break;
			
		// We need to set the result to the root move before we update the node variables. 
		// The root move will check for a fail low and thus needs the alpha value not updated
		rootMove.set(result, stack, pvSearch);
		node.setSearchResult(result, stack[1], curMove);
		WhatIf::whatIf.moveSearched(position, _computingInfo, stack, curMove, depth - 1, 0, result);

		if (depth >= 2) {
			node.setNullWindow();
		}

		_clockManager->setSearchedRootMove(node.isPVFailLow(), node.bestValue);
		if (_clockManager->shouldAbort()) break;
		_computingInfo.printNewPV(triedMoves);
		if (node.isFailHigh()) break;
	}

	if (!_clockManager->isSearchStopped()) node.updateTTandKiller(position, _butterflyBoard, true, depth);
	_computingInfo.getRootMoves().bubbleSort(0);
	_computingInfo.setHashFullInPermill(node.getHashFullInPermill());
	_computingInfo.printSearchResult();
}


