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
 * @copyright Copyright (c) 2025 Volker Böhm
 */

#include "search.h"
#include "whatIf.h"
#include "quiescence.h"
#include "rootmoves.h"
#include "search-param.h"
#include "passedpawn.h"
#include "../basics/materialbalance.h"
#include "../bitbase/bitbase-reader.h"
#include "../movegenerator/movegenerator.h"

using namespace QaplaSearch;

#ifdef __APPLE__
#include <pthread.h>
#endif
#include <iostream>

void printStackInfo(const char* msg) {
#ifdef __APPLE__
    pthread_t self = pthread_self();

    void* base   = pthread_get_stackaddr_np(self);   // oberes Ende des Stacks
    size_t size  = pthread_get_stacksize_np(self);   // maximale Größe

    void* sp     = __builtin_frame_address(0);       // aktueller Stackpointer
    std::ptrdiff_t used = (char*)base - (char*)sp;   // Stack wächst abwärts

    std::cout << msg
              << " stack base=" << base
              << " size=" << size/1024 << " KB"
              << " used≈" << used/1024 << " KB\n";
#else
    // Stack info not implemented on this platform
    (void)msg;
#endif
}

template <Search::SearchRegion TYPE>
bool Search::checkEvalReleatedCutoffsAndSetEval(MoveGenerator& position, SearchStack& stack, SearchVariables& node, ply_t depth, ply_t ply) {
	const auto evalBefore = ply > 1 ? stack[ply - 2].adjustedEval : NO_VALUE;
	if (position.isInCheck()) {
		node.adjustedEval = evalBefore;
		return false;
	}
	if (node.adjustedEval == NO_VALUE) {
		if (node.eval == NO_VALUE) {
			node.eval = Eval::eval(position, node.getTT()->getPawnTT());
		}
		node.adjustedEval = node.eval;
		node.isImproving = node.adjustedEval > evalBefore && evalBefore != NO_VALUE;
	}
	// Must be after node.probeTT, because futility uses the information from TT
	if (node.forewardFutility(position)) {
		node.setCutoff(Cutoff::FUTILITY);
		return true;
	} 
	if (TYPE == SearchRegion::INNER && isNullmoveCutoff(position, stack, depth, ply)) {
		node.setCutoff(Cutoff::NULL_MOVE);
		return true;
	}
	return false;
}

bool Search::hasBitbaseCutoff(const MoveGenerator& position, SearchVariables& node) {
	return false;
	// We only look into the bitbases, if we had a capture or a promote. This avoids "non-searching" on
	// positions of bitbases.
	// if (position.getPiecesSignature() == _rootSignature) return false;
	// if (curPly.alpha >= -MIN_MATE_VALUE && curPly.beta <= MIN_MATE_VALUE) return false;
	const QaplaBitbase::BitbaseResult bitbaseValue = QaplaBitbase::BitbaseReader::getValueFromBitbase(position);
	if (bitbaseValue == QaplaBitbase::BitbaseResult::Unknown) {
		return false;
	}
	_computingInfo._tbHits++;
	if (bitbaseValue == QaplaBitbase::BitbaseResult::Win) { // && curPly.beta <= MIN_MATE_VALUE) {
		node.setCutoff(Cutoff::BITBASE, MIN_MATE_VALUE);
		return true;
	} 
	if (bitbaseValue == QaplaBitbase::BitbaseResult::Loss) { // && curPly.alpha >= -MIN_MATE_VALUE) {
		node.setCutoff(Cutoff::BITBASE, -MIN_MATE_VALUE);
		return true;
	}
	if (bitbaseValue == QaplaBitbase::BitbaseResult::Draw) {
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
	else if (node.adjustedEval < node.beta) {
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

	ply_t R = SearchParameter::getNullmoveReduction(ply, depth, node.betaAtPlyStart, node.adjustedEval, node.isImproving);

	childNode.doMove(position, Move::NULL_MOVE);
	node.bestValue = depth - R > 2 ?
		-negaMax<SearchRegion::INNER>(position, stack, -node.beta, -node.beta + 1, depth - R - 1, ply + 1) :
		-negaMax<SearchRegion::NEAR_LEAF>(position, stack, -node.beta, -node.beta + 1, depth - R - 1, ply + 1);

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
	constexpr bool OPT = SearchParameter::optimizeLMR;

	// Values that can only take a handful of integers. A tuning run gets no signal out of such a
	// range, so they are compile time constants and not UCI parameters.
	constexpr ply_t MIN_PLY = 1;
	constexpr int32_t MOVE_OFFSET = 3;
	constexpr int32_t MOVE_SLOPE = 4;
	constexpr int32_t MOVE_HIGH_DIV = 2;
	constexpr int32_t DEPTH_OFFSET = 3;
	constexpr int32_t DEPTH_SLOPE = 2;

	const int32_t moveNo = static_cast<int32_t>(node.moveNumber);

	if (ply <= MIN_PLY) return 0;

	// Captures are never reduced. Reducing the loosing ones by a tuned amount was tried in
	// 0.4.0-049 and did not pay, see plan/version-log.md.
	if (move.isCapture()) return 0;

	// The reduction is the product of two ramps, one over the move number and one over the
	// remaining depth. The move number ramp is steep up to a break point and flat after it.
	// Both ramps are shared by all node types; only the divisor tells PV nodes apart.
	const int32_t moveBreak = param<OPT, "lmrMoveBreak", 6, 2, 10>();
	const int32_t moveRamp = moveNo <= moveBreak
		? param<OPT, "lmrMoveBase", 11, 0, 22>() + (moveNo - MOVE_OFFSET) * MOVE_SLOPE
		: param<OPT, "lmrMoveHighBase", 51, 0, 102>() + (moveNo - moveBreak) / MOVE_HIGH_DIV;
	const int32_t depthRamp = param<OPT, "lmrDepthBase", 11, 0, 22>() + (depth - DEPTH_OFFSET) * DEPTH_SLOPE;

	const int32_t rampMin = param<OPT, "lmrRampMin", 15, 0, 30>();
	const int32_t rampMax = param<OPT, "lmrRampMax", 55, 30, 80>();
	int32_t numerator = std::clamp(moveRamp, rampMin, rampMax) * std::clamp(depthRamp, rampMin, rampMax);

	int32_t divisor = node.isPVNode()
		? param<OPT, "lmrPvDivisor", 512, 256, 768>()
		: param<OPT, "lmrDivisor", 261, 133, 389>();
	// A pawn no opponent pawn can stop is reduced less than another quiet move, and by the same
	// value it is also skipped later, as the move count pruning reads the reduction. A promotion
	// counts as such a push.
	if (PassedPawn::isPassedPawnPush(position, move)) {
		divisor += param<OPT, "lmrPassedPawnDivisorAdd", 112, 0, 224>();
	}
	// Extra reduction where a reduction already happens and the position is not improving.
	if (!node.isImproving && numerator >= divisor) {
		numerator += param<OPT, "lmrNotImprovingAdd", 133, 0, 266>();
	}

	return static_cast<ply_t>(numerator / divisor);
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
ply_t Search::iir(const SearchVariables& node, ply_t depth) {
	if (!SearchParameter::DO_IIR) return 0;
	if (depth <= SearchParameter::IIR_MIN_DEPTH) return 0;
	// Any move worth trying first is enough, from the hash or from the previous iteration.
	if (!node.getTTMove().isEmpty()) return 0;
	if (node.hasPVMove()) return 0;

	return SearchParameter::IIR_PV_REDUCTION;
}

template <Search::SearchRegion TYPE>
ply_t Search::se(MoveGenerator& position, SearchStack& stack, value_t alpha, value_t beta, ply_t depth, ply_t ply) {
	// Near leaf nodes are searched with a depth below the minimal depth required below
	if constexpr (TYPE == SearchRegion::NEAR_LEAF) return 0;
	constexpr bool IS_PV = TYPE == SearchRegion::PV;
	// The extension in non pv nodes is switchable as a whole, see searchparameter.h
	if constexpr (!IS_PV && !SearchParameter::DO_SE_IN_NON_PV) return 0;
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

	// No se, if tt does not have a good move value (> alpha). With a tt value <= alpha the
	// value is an upper bound only, thus other moves failing below it prove nothing.
	if (node.ttValueIsLessOrEqualAlpha) return 0;
	// We need a ttValue to have something to search for
	if (node.ttValue == NO_VALUE) return 0;

	// Singular extension based on tt move. Only, if the search found a value > alpha it found a "best move" in the position and is able to store it to
	// the transposition table
	auto ttMove = node.getTTMove();
	// Minimal depth the tt move must have been searched with to be tested at all,
	// a constant distance to the current depth instead of a share of it
	const ply_t ttMinDepth =
		depth - param<SearchParameter::optimizeSE, "seTTMinDepthReduction", 6, 0, 12>();
	// Depth used to search the remaining moves against the singular margin
	const ply_t seDepth =
		depth * 100 / param<SearchParameter::optimizeSE, "seDepthDivisor", 200, 100, 300>();
	if (ttMove.isEmpty()) return 0;
	// We require a certain search depth for the tt move to be considered for a singular extension
	if (node.ttDepth < ttMinDepth) return 0;
	// No se, if the tt already shows a mate or equivalent value
	if (node.ttValue != NO_VALUE && (node.ttValue < -MIN_MATE_VALUE || node.ttValue > MIN_MATE_VALUE)) return 0;

	// The margin the remaining moves must fail below to make the tt move singular. PV and non
	// PV nodes get their own values, the tt value is a much weaker information in a non PV node
	node.setSE(IS_PV
		? param<SearchParameter::optimizeSE, "sePvMarginConst", 1, -100, 300>()
			+ param<SearchParameter::optimizeSE, "sePvMarginFactor", 4, 0, 100>() * depth
		: param<SearchParameter::optimizeSE, "seNonPvMarginConst", 0, -100, 300>()
			+ param<SearchParameter::optimizeSE, "seNonPvMarginFactor", 4, 0, 100>() * depth);
	_computingInfo._nodesSearched++;

	// Cutoffs checks all kind of cutoffs including futility, nullmove, bitbase and others 
	if (checkEvalReleatedCutoffsAndSetEval<SearchRegion::NEAR_LEAF>(position, stack, node, seDepth, ply)) return 0;

	// A pv node is never cut. Its value is needed exactly, and the reduced se search is no
	// basis to drop the pv move without having searched it
	constexpr bool doMultiCut = SearchParameter::DO_MULTI_CUT && !IS_PV;

	node.computeMoves(position, _butterflyBoard);
	Move curMove;
	while (!(curMove = node.selectNextMove(position)).isEmpty()) {
		if (curMove == ttMove) continue;

		childNode.doMove(position, curMove);
		const auto result = -negaMax<SearchRegion::INNER>(position, stack, -node.alpha - 1, -node.alpha, seDepth - 1, ply + 1);
		node.setSearchResult(result, childNode, curMove);
		WhatIf::whatIf.moveSearched(position, _computingInfo, stack, curMove, seDepth - 1, ply, result, "SE");
		childNode.undoMove(position);

		// A move reaching beta ends the search, the tt move is not singular either way
		if (doMultiCut && result >= beta) break;

		if (node.isFailHigh()) break;
	}

	// Attack masks are lazily computed. We need to make sure to recompute them, if we like to search twice in the same position
	position.computeAttackMasksForBothColors();

	// Multi cut: the tt move is expected to fail high and one more move reached beta as well.
	// Two moves above beta, thus the node is not singular but expected to fail high itself and
	// the whole subtree is cut. The search is already done, this costs nothing extra. Mate
	// values are left out, the reduced search is far too shallow to claim a mate.
	// The caller returns this value instead of searching the node, see negaMax step 5
	if (doMultiCut && node.bestValue >= beta && node.bestValue < MIN_MATE_VALUE) {
		node.setCutoff(Cutoff::MULTI_CUT, node.bestValue);
		return 0;
	}

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

	if (TYPE != SearchRegion::PV && alpha > MAX_VALUE - value_t(ply)) {
		node.setCutoff(Cutoff::FASTER_MATE_FOUND, MAX_VALUE - value_t(ply));
	}
	else if (TYPE != SearchRegion::PV && beta < -MAX_VALUE + value_t(ply)) {
		node.setCutoff(Cutoff::FASTER_MATE_FOUND, -MAX_VALUE + value_t(ply));
	}
	else if (position.drawDueToMissingMaterial()) {
		node.setCutoff(Cutoff::NOT_ENOUGH_MATERIAL, 0);
	}
	else if (stack.isDrawByRepetitionInSearchTree(position, ply)) {
		node.setCutoff(Cutoff::DRAW_BY_REPETITION, 0);
	}
	else if (position.getTotalHalfmovesWithoutPawnMoveOrCapture() >= 100) {
		node.setCutoff(Cutoff::DRAW_BY_50_MOVES_RULE, 0);
	}
	else if (ply >= SearchParameter::MAX_SEARCH_DEPTH) {
		node.setCutoff(Cutoff::MAX_SEARCH_DEPTH, Eval::eval(position, node.getTT()->getPawnTT(), ply));
	}
	else if (TYPE != SearchRegion::NEAR_LEAF && hasBitbaseCutoff(position, node)) {
		node.setCutoff(Cutoff::BITBASE);
	}
	else if (TYPE != SearchRegion::NEAR_LEAF && stack[0].remainingDepth > 1 && _clockManager->emergencyAbort()) {
		node.setCutoff(Cutoff::ABORT, -MAX_VALUE);
	}
	else if (_clockManager->stopOnNodeTarget(_computingInfo._nodesSearched)) {
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
	node.pvMovesStore.setEmpty(ply);

	// 1. Detect direct cutoffs without requiring search or eval
	// This includes checking the hash and setting the hash information like ttMove
	if (nonSearchingCutoff<TYPE>(position, stack, node, alpha, beta, depth, ply)) return node.bestValue;
	SearchVariables& childNode = stack[ply + 1];
	SearchVariables& parentNode = stack[ply - 1];

	// 2. Quiescense search
	if (depth < 0) {
		return _quiescence.search(TYPE == SearchRegion::PV, position, _computingInfo, node.previousMove, alpha, beta, ply);
	}

	[[maybe_unused]] const auto nodesSearched = _computingInfo._nodesSearched;
	/*
	if (nodesSearched == 161) {
		position.print();
		for (int i = 0; i < ply; i++) {
			stack[i].printTTEntry();
		}
	}
	*/
	_computingInfo._nodesSearched++;

	// 3. Probe the hash table. This will set the hash information to the node.
	// The required hash signature is set in nonSearchingCutoff
	if (node.probeTT(TYPE == SearchRegion::PV, alpha, beta, depth, ply)) {
		node.setCutoff(Cutoff::HASH);
		return node.bestValue;
	}
	WhatIf::whatIf.moveSelected(position, _computingInfo, stack, node.previousMove, depth, ply);

	// 4. Internal iterative reduction. A node with no move to try first is expensive and its
	// result unreliable, so it is searched shallower instead of being pre-searched. Must be
	// before node.setFromParentNode, which hands the depth on to the node.
	if (TYPE == SearchRegion::PV) depth -= iir(node, depth);

	// 5. Singular extension for the tt move, computed for PV and non PV nodes, see se()
	const auto seExtension = se<TYPE>(position, stack, alpha, beta, depth, ply);

	// se() cuts the node, if it finds a second move above beta, see multi cut in se().
	// Must be before setFromParentNode, as that resets the cutoff
	if (node.cutoff == Cutoff::MULTI_CUT) {
		WhatIf::whatIf.cutoff(position, _computingInfo, stack, ply, node.cutoff);
		return node.bestValue;
	}

	value_t result;
	Move curMove;

	// 6. Setting node values from parent node, must be after seExtensions
	// We need only stable information from parent node; the node type and the previous move.
	node.setFromParentNode(position, parentNode, alpha, beta, depth, TYPE == SearchRegion::PV);
	

	// 7. Check all kind of early cutoffs including futility, nullmove, bitbase and others 
	// Additionally set eval. This is done as late as possible, as it is very time consuming. Some cutoff checks needs eval.
	if (checkEvalReleatedCutoffsAndSetEval<TYPE>(position, stack, node, depth, ply)) {
		WhatIf::whatIf.cutoff(position, _computingInfo, stack, ply, node.cutoff);
		return node.bestValue;
	}

	node.computeMoves(position, _butterflyBoard);
	// 8. Calculate additional node wide search extensions
	if (TYPE == SearchRegion::PV) depth = node.extendSearch(position, stack[0].remainingDepth);

	// Loop through all moves
	while (!(curMove = node.selectNextMove(position)).isEmpty()) {

		// The singular extension belongs to the move se() proved to be singular, not to the node.
		// It cannot add on top of the check extension, as se() returns 0 for a node in check.
		const ply_t moveExtension = curMove == node.getTTMove() ? seExtension : 0;
		const ply_t moveDepth = moveExtension > 0 ?
			std::min(depth + moveExtension, stack[0].remainingDepth * 2) : depth;

		bool doMovePrunings = node.moveNumber > 3 && !node.isCheckMove(position, curMove);
		// lmr is needed for move count pruning and late move reduction search
		const auto lmr = doMovePrunings ? computeLMR(node, position, depth, ply, curMove) : 0;
		// Never skip moves when escaping from mate and in positions with pawns only.
		if (doMovePrunings && node.bestValue > -MIN_MATE_VALUE && position.hasMoreThanPawns()) {
			
			// 1. Futility pruning: skip quiet moves in late move loop when position is too bad
			if (node.canPruneFutility(position, curMove)) {
				continue;
			}

			// 2. Move count pruning
			if (lmr > 0 && depth - lmr < 0) {
				continue;
			}
		}

		childNode.doMove(position, curMove);

		// 3. Late move reduction search
		// We continue with the next move, if the lmr search returns a value less than alpha
		if (lmr > 0) {
			result = TYPE != SearchRegion::NEAR_LEAF && moveDepth - lmr > 2 ?
				-negaMax<SearchRegion::INNER>(position, stack, -node.alpha - 1, -node.alpha, moveDepth - 1 - lmr, ply + 1) :
				-negaMax<SearchRegion::NEAR_LEAF>(position, stack, -node.alpha - 1, -node.alpha, moveDepth - 1 - lmr, ply + 1);
			WhatIf::whatIf.moveSearched(position, _computingInfo, stack, curMove, moveDepth - 1 - lmr, ply, result, "LMR");
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
		// 4. Searching with null window either because of non pv search or because it is not the first move in pv.
		// Additionally we do not go to null window search on PV, if depth is 1 or 0
		// We do not return fail high from a null window search in PV node
		bool isDirectPVWindowSearch = TYPE == SearchRegion::PV && (node.moveNumber == 1 || depth <= 1);
		if (!isDirectPVWindowSearch) {
			result = TYPE != SearchRegion::NEAR_LEAF && moveDepth > 2 ?
				-negaMax<SearchRegion::INNER>(position, stack, -node.alpha - 1, -node.alpha, moveDepth - 1, ply + 1) :
				-negaMax<SearchRegion::NEAR_LEAF>(position, stack, -node.alpha - 1, -node.alpha, moveDepth - 1, ply + 1);
			WhatIf::whatIf.moveSearched(position, _computingInfo, stack, curMove, moveDepth - 1, ply, result, TYPE == SearchRegion::PV ? "ZeroW" : "Std.");
		}
		// 5. Full window PV search or research the move with full window, if result is better than alpha
		if (TYPE == SearchRegion::PV && (isDirectPVWindowSearch || result > node.alpha)) {
			const ply_t adjustedDepth = moveDepth <= 0 && curMove == node.getTTMove() && ply < stack[0].remainingDepth * 2 ? 1 : moveDepth;
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
	// 6. Update tt and killer, but not if search is aborted as then bestValue and bestMove may be wrong  
	if (!_clockManager->isSearchStopped()) node.updateTTandKiller(position, _butterflyBoard, TYPE == SearchRegion::PV, depth);
	// Inform the user about advances in search
	if (TYPE != SearchRegion::NEAR_LEAF) {
		_computingInfo.setHashFullInPermill(node.getHashFillRateInPermill());
		_computingInfo.printSearchInfo(_clockManager->isTimeToSendNextInfo());
	}
	return node.bestValue;
}

void Search::storePVToTT(MoveGenerator& position, SearchStack& stack, const RootMove& rootMove, ply_t ply) {
	if (ply >= stack.size()) {
		return; // No stack for this ply
	}
	SearchVariables& node = stack[ply];
	auto move = rootMove.getPV()[ply];
	if (move.isEmpty()) return;
	auto alpha = ply % 2 == 0 ? rootMove.getAlpha() : -rootMove.getBeta();
	auto beta = ply % 2 == 0 ? rootMove.getBeta() : -rootMove.getAlpha();
	auto value = ply % 2 == 0 ? rootMove.getValue() : -rootMove.getValue();
	auto depth = rootMove.getDepth() - ply;
	auto hashKey = position.computeBoardHash();
	node.setTTEntry(hashKey, true, depth, move, NO_VALUE, value, alpha, beta);
	stack[ply + 1].doMove(position, move);
	storePVToTT(position, stack, rootMove, ply + 1);
	stack[ply + 1].undoMove(position);
}

/**
 * Negamax algorithm for the first ply
 */
void Search::negaMaxRoot(MoveGenerator& position, SearchStack& stack, uint32_t skipMoves, ClockManager& clockManager) {
	if (skipMoves >= _computingInfo.getMovesAmount()) return;

	_quiescence.setTT(stack[0].getTT());
	_clockManager = &clockManager;
	position.computeAttackMasksForBothColors();
	SearchVariables& node = stack[0];
	value_t result;

	ply_t depth = node.remainingDepth;

	// we use the movelist from rootmoves. node.computeMoves is only to initialize other variables
	node.computeMoves(position, _butterflyBoard);
	_computingInfo.nextIteration(node);
	WhatIf::whatIf.moveSelected(position, _computingInfo, stack, Move::EMPTY_MOVE, depth, 0);
    //printStackInfo("stack size: ");
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

		_clockManager->setSearchedRootMove(node.isFailLow(), node.bestValue);
		if (_clockManager->shouldAbort()) break;
		_computingInfo.printNewPV(triedMoves);
		if (node.isFailHigh()) break;
	}

	_computingInfo.setDebug(-1);

	if (!_clockManager->isSearchStopped()) node.updateTTandKiller(position, _butterflyBoard, true, depth);
	_computingInfo.getRootMoves().bubbleSort(0);
	_computingInfo.setHashFullInPermill(node.getHashFillRateInPermill());
	_computingInfo.printSearchResult();
}


