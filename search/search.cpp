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
#include "search-thread.h"
#include "whatIf.h"
#include "quiescence.h"
#include "rootmoves.h"
#include "tunable.h"
#include "passedpawn.h"
#include "lmr-base-table.h"
#include "../basics/materialbalance.h"
#include "../movegenerator/movegenerator.h"
#include "../eval/tablebase-value.h"
#include "../src/syzygy/tbposition-builder.h"

using namespace QaplaSearch;

#ifdef __APPLE__
#include <pthread.h>
#endif
#include <chrono>
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
bool Search::checkEvalReleatedCutoffsAndSetEval(MoveGenerator& position, SearchStack& stack, SearchNode& node, ply_t depth, ply_t ply) {
	const auto evalBefore = ply > 1 ? stack[ply - 2].adjustedEval : NO_VALUE;
	if (position.isInCheck()) {
		node.adjustedEval = evalBefore;
		return false;
	}
	if (node.adjustedEval == NO_VALUE) {
		if (node.eval == NO_VALUE) {
			node.eval = Eval::eval(position, &_pawnTT);
		}
		node.adjustedEval = node.eval;
		node.isImproving = node.adjustedEval > evalBefore && evalBefore != NO_VALUE;
	}
	// Must be after node.probeTT, because futility uses the information from TT
	// Tested 0.5.0-028-3: fail low futility on the evaluation (eval + margin <= alpha), CLOP values 186/179/120/22:
	// H0 accepted, bounds -3/+2, 514 games, 32.49 %. Removed.
	if (node.forewardFutility(position)) {
		node.setCutoff(Cutoff::FUTILITY);
		return true;
	} 
	// Tested 0.5.0-028-3: fail low futility on a null window quiescence value (qvalue <= alpha - margin), CLOP values
	// 106/109/107/17: undecided, bounds -3/+2, 20000 games. Removed.
	if (TYPE == SearchRegion::INNER && isNullmoveCutoff(position, stack, depth, ply)) {
		node.setCutoff(Cutoff::NULL_MOVE);
		return true;
	}
	return false;
}

void Search::computeMoves(MoveGenerator& position, SearchNode& node) {
	node.moveProvider.computeMoves(position);
	node.selectStage = node.pvMove.isEmpty() && node.ttMove.isEmpty() ? MoveType::WEIGHT_CAPTURES : MoveType::PV;
	node.bestValue = node.moveProvider.checkForGameEnd(position, node.ply);
}

Move Search::selectNextMove(MoveGenerator& position, const SearchStack& stack, SearchNode& node) {
	node.movesTried++;
	MoveProvider& provider = node.moveProvider;
	Move move;
	// Every stage sets the next one first and returns its move if it has one, else it falls
	// through. Only GOOD_CAPTURES stays while it has captures left.
	switch (node.selectStage) {
	case MoveType::PV:
		node.selectStage = MoveType::WEIGHT_CAPTURES;
		move = provider.selectProposedMove(node.pvMove.isEmpty() ? node.ttMove : node.pvMove);
		node.pvMove.setEmpty();
		if (!move.isEmpty()) return move;
		[[fallthrough]];
	case MoveType::WEIGHT_CAPTURES:
		node.selectStage = MoveType::GOOD_CAPTURES;
		provider.weightCaptures(position, node.previousMove);
		[[fallthrough]];
	case MoveType::GOOD_CAPTURES:
		move = provider.selectGoodCapture(position);
		if (!move.isEmpty()) return move;
		node.selectStage = MoveType::KILLER1;
		[[fallthrough]];
	case MoveType::KILLER1:
		node.selectStage = MoveType::FOLLOW_UP_MOVE;
		move = provider.selectProposedMove(node.killerMove[0]);
		if (!move.isEmpty()) return move;
		[[fallthrough]];
	case MoveType::FOLLOW_UP_MOVE:
		// Tested 0.5.0-030: before the first killer: undecided, bounds -2/+3, 20000 games, 49.82 %, dead/followup-before-killer1
		// Tested 0.5.0-031: after the second killer: undecided, bounds -2/+3, 20000 games, 49.98 %, dead/followup-after-killer2
		// At tournament depth (19.5+0.1 on the M4) on 0.5.0-036: before the first killer against here: stopped undecided
		// after 10642 games, bounds -2/+3. The placement does not depend on the depth or on the history tables.
		node.selectStage = MoveType::KILLER2;
		move = provider.selectProposedMove(_continuationHistory->getFollowUpMove(stack, node.ply));
		if (!move.isEmpty()) return move;
		[[fallthrough]];
	case MoveType::KILLER2:
		node.selectStage = MoveType::SORT_MOVES;
		move = provider.selectProposedMove(node.killerMove[1]);
		if (!move.isEmpty()) return move;
		[[fallthrough]];
	case MoveType::SORT_MOVES:
		node.selectStage = MoveType::REMAINING_MOVES;
		provider.sortSilentMoves(*_butterflyBoard, node.continuationRows);
		[[fallthrough]];
	case MoveType::REMAINING_MOVES:
		return provider.selectRemainingMove();
	}
	return move;
}

/**
 * Asks the tablebases and cuts the node if the answer is usable.
 *
 * The stored entry is only the true value of the position when nothing zeroing is
 * available - where a capture, an en passant capture or a promotion exists, the
 * generator was free to store whatever compresses best. That is what the non silent
 * move count decides, so this can only run after the moves have been generated.
 *
 * The value is a bound, not a score: the tables say "won", not "mate in seven". A win
 * may therefore only cut when it reaches beta and a loss only when it falls to alpha,
 * which matters all the more because a cursed win is worth barely a pawn here. A draw
 * is different: it is the exact value of the position, so it cuts in any window.
 */
bool Search::hasTablebaseCutoff(MoveGenerator& position, SearchNode& node, ply_t depth) {

	// A root win found by the DTZ classification answers the fifty-move question differently
	// from this cutoff - running both would let a flat tablebase-win bound drown the actual
	// distance-to-mate signal the filtered root search is looking for (see rootmoves.h).
	if (_tbRootWin) return false;

	// Cheapest test first: it is the one that rejects nearly every node of a real game,
	// and with no tables loaded the cardinality is zero and nothing else is ever touched.
	const uint32_t pieceCount = uint32_t(popCount(position.getAllPiecesBB()));
	if (pieceCount > _tbCardinality) return false;

	// The depth limit only bites on the most expensive level; below it the tables are
	// small enough to be worth asking at any depth.
	if (pieceCount == _tbCardinality && depth < _tbProbeDepth) return false;

	if (node.getNonSilentMoveAmount() != 0) return false;
	if (position.getTotalHalfmovesWithoutPawnMoveOrCapture() != 0) return false;
	if (position.getBoardState().getCastlingRightsMask() != 0) return false;

	QaplaSyzygy::TbPosition tbPosition{};
	if (!QaplaSyzygy::buildTbPosition(position, tbPosition)) return false;

	const QaplaSyzygy::WdlEntry entry = QaplaSyzygy::probeWdlEntry(tbPosition);
	if (entry.status != QaplaSyzygy::Status::Ok) return false;

	_computingInfo._tbHits++;

	const value_t value = ChessEval::tablebaseWdlToValue(entry.value);

	// tb always returns a concrete value, there is no "unknown value" here.
	if (value > DRAW_VALUE && value < node.beta) {
		return false;
	}
	if (value < DRAW_VALUE && value > node.alpha) {
		return false;
	}

	// A draw is exact, not a bound: with best play there is nothing left to find here,
	// no win and no loss, so it cuts inside the window as well.
	node.setCutoff(Cutoff::TABLEBASE, value);

	// The answer does not depend on how deep the search is, so the entry gets a draft
	// well above the current one and survives long enough to spare the next probe.
	node.setTTEntry(node.positionHash, false, depth + SearchConfig::TABLEBASE_TT_DEPTH_BONUS,
		Move::EMPTY_MOVE, node.eval, value, node.alpha, node.beta);

	return true;
}

/**
 * Check, if it is reasonable to do a nullmove search
 */
bool Search::isNullmoveReasonable(MoveGenerator& position, SearchNode& node, ply_t depth, ply_t ply) {
	bool result = true;
	if (!SearchConfig::DO_NULLMOVE) {
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
	else if (node.remainingDepth <= SearchConfig::NULLMOVE_REMAINING_DEPTH) {
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
	SearchNode& node = stack[ply];
	if (!isNullmoveReasonable(position, node, depth, ply)) {
		return false;
	}
	assert(!position.isInCheck());
	SearchNode& childNode = stack[ply + 1];

	ply_t R = SearchConfig::getNullmoveReduction(ply, depth, node.betaAtPlyStart, node.adjustedEval, node.isImproving);

	childNode.doMove(position, Move::NULL_MOVE);
	// isNullmoveCutoff is only reached from an INNER node, see checkEvalReleatedCutoffsAndSetEval
	node.bestValue = -searchChild<SearchRegion::INNER>(
		position, stack, -node.beta, -node.beta + 1, depth - R - 1, ply + 1);

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
	// searchInfo.remainingDepth -= SearchConfig::getNullmoveVerificationDepthReduction(ply, searchInfo.remainingDepth);
	return isCutoff;
}

namespace {
	// Values of the move number curve of the reduction at LmrBaseTable::MOVE_POINTS (moves 2, 5, 9, 16, 30)
	// 0.5.0-054 with the untuned start values 0 / 25 / 48 / 55 / 55 and 7 / 13 / 21 / 35: H0 accepted against 038 at
	// 19.5+0.1, bounds -2/+3, 6285 games. 0.5.0-060 (Volker): read from the CLOP still running on the M4 at about 2000
	// of 5000 samples (-6.8 / 11.8 / 43.1 / 73.5 / 68.8 and 9.1 / 19.3 / 21.2 / 32.5) and made monotone: the move
	// curve no longer falls at its end (move 30 = move 16 = 70), move 5 lower (10), move 2 at -2.
	// SPRT 0.5.0-060 against 038 at 13.5+0.07 (Studio, tournament depth): H1 accepted, bounds -2/+3, 11791 games.
	// 0.5.0-061 (on 0.5.0-055): the CLOP at 3375 of 5000 samples, rounded, no correction needed (both curves rise):
	// moves -7.1 / 11.8 / 45.2 / 71.0 / 77.2, depths 9.4 / 17.8 / 19.9 / 31.3. Stopped undecided against 055 after 7231.
	// 0.5.0-063 (Volker): the values of 060 (Volker's) on 055, to be proven against 055 - a proof against 038 cannot be
	// carried over. SPRT 0.5.0-063 against 055 at 13.5+0.07 (Studio, tournament depth): H1 accepted, bounds -2/+3,
	// 3170 games.
	template <bool OPT>
	std::array<int32_t, LmrBaseTable::POINTS> lmrMoveValues() {
		return { tunable<OPT, "lmrMove2", -2, -30, 30>(), tunable<OPT, "lmrMove5", 10, 0, 50>(),
			tunable<OPT, "lmrMove9", 40, 0, 96>(), tunable<OPT, "lmrMove16", 70, 0, 110>(),
			tunable<OPT, "lmrMove30", 70, 0, 110>() };
	}

	// Values of the depth curve of the reduction at LmrBaseTable::DEPTH_POINTS (depths 1, 4, 8, 15, 30)
	template <bool OPT>
	std::array<int32_t, LmrBaseTable::POINTS> lmrDepthValues() {
		return { tunable<OPT, "lmrDepth1", 9, 0, 14>(), tunable<OPT, "lmrDepth4", 19, 0, 26>(),
			tunable<OPT, "lmrDepth8", 21, 0, 42>(), tunable<OPT, "lmrDepth15", 32, 0, 70>(),
			LmrBaseTable::DEPTH_LAST_VALUE };
	}

	const LmrBaseTable LMR_BASE_TABLE = [] {
		LmrBaseTable table;
		table.build(lmrMoveValues<false>(), lmrDepthValues<false>());
		return table;
	}();

	template <bool OPT>
	const LmrBaseTable& lmrBaseTable() {
		if constexpr (!OPT) {
			return LMR_BASE_TABLE;
		}
		else {
			// The values can be changed by UCI between searches. Every thread keeps its own table and rebuilds it
			// when they changed.
			thread_local LmrBaseTable table;
			thread_local std::array<int32_t, LmrBaseTable::POINTS> builtMoveValues{};
			thread_local std::array<int32_t, LmrBaseTable::POINTS> builtDepthValues{};
			thread_local bool built = false;
			const auto moveValues = lmrMoveValues<true>();
			const auto depthValues = lmrDepthValues<true>();
			if (!built || moveValues != builtMoveValues || depthValues != builtDepthValues) {
				table.build(moveValues, depthValues);
				builtMoveValues = moveValues;
				builtDepthValues = depthValues;
				built = true;
			}
			return table;
		}
	}
}

int32_t Search::computeLMR(SearchNode& node, MoveGenerator& position, ply_t depth, ply_t ply, Move move, bool losingCapture)
{

	// Ability to disable history for a ply
	// if (node->mDisableHist) return 0;
	constexpr bool OPT = SearchConfig::optimizeLMR;

	constexpr ply_t MIN_PLY = 1;

	const int32_t moveNo = static_cast<int32_t>(node.movesTried);

	if (ply <= MIN_PLY) return 0;

	// Captures that SEE does not rate as loosing are never reduced. A loosing capture is reduced by 1 ply, whatever it
	// takes. Tested 0.5.0-050 against 0.5.0-038: at 2+0.004 undecided after 20000 games, at tournament depth
	// (13.5+0.07, 12 engines) H1 accepted after 7336 games, bounds -2/+3.
	// Tested 0.5.0-047: reduced like a quiet move less one ply per 100 cp of the captured piece: H0 accepted,
	// bounds -2/+3, 8391 games. Tested 0.5.0-048: pawn 2, knight and bishop 1, rook and queen 0: undecided,
	// bounds -2/+3, 20000 games. Reducing the loosing ones by a tuned amount was tried in 0.4.0-049 and did not pay.
	// Only from the fourth move on, as tested: the LMR of quiet moves starts at the second move since the spline base.
	if (move.isCapture()) return losingCapture && moveNo > 3 ? LMR_UNIT : 0;

	// The base reduction is the product of a curve over the move number and one over the remaining depth, each a
	// monotone spline through five points (LmrBaseTable). Shared by all node types; only the divisor tells PV nodes
	// apart. It replaces two clamped linear ramps whose move ramp jumped from 23 to 51 between moves 6 and 7.
	int32_t numerator = lmrBaseTable<OPT>().get(depth, moveNo);

	// One divisor per node type: PV, a node that ever was on the PV (tt pv bit), cut and all.
	// CLOP (Linux, 4000 samples at 29+0.15, node types as in Stockfish; start values PV 512, tt pv 512, cut 261,
	// all 261, each in the middle of its range): PV 619.8, tt pv 432.0, cut 206.9, all 279.6, rounded, no correction
	// needed (PV reduced least, then tt pv, all, cut most, as in Stockfish).
	// With the IIR in cut nodes only (0.5.0-075), CLOP of cut and all (Studio, 4000 samples at 13.5+0.07, centred on
	// the values above): cut divisor 203.7, all divisor 263.9 - and in lmrSearchReduction cut term 114.9, all term
	// 52.2 - rounded, no correction needed (cut reduced most, all a bit more than before to take over the IIR).
	constexpr bool OPT_DIVISOR = SearchConfig::optimizeLmrDivisor;
	int32_t divisor = node.isPVNode() ? tunable<OPT_DIVISOR, "lmrPvDivisor", 620, 256, 768>()
		: node.ttPv ? tunable<OPT_DIVISOR, "lmrTtPvDivisor", 432, 133, 768>()
		: node.isCutNode() ? tunable<OPT_DIVISOR, "lmrCutDivisor", 204, 64, 389>()
		: tunable<OPT_DIVISOR, "lmrAllDivisor", 264, 133, 389>();
	// A pawn no opponent pawn can stop is reduced less than another quiet move, and by the same
	// value it is also skipped later, as the move count pruning reads the reduction. A promotion
	// counts as such a push.
	// Tested 0.4.0-051: +6.0 Elo, 8883 games
	if (PassedPawn::isPassedPawnPush(position, move)) {
		divisor += tunable<OPT, "lmrPassedPawnDivisorAdd", 112, 0, 224>();
	}
	// Extra reduction where a reduction already happens and the position is not improving.
	if (!node.isImproving && numerator >= divisor) {
		numerator += tunable<OPT, "lmrNotImprovingAdd", 133, 0, 266>();
	}
	// A move with a good history is reduced less, one with a bad history more, as Stockfish does with
	// its statScore. The butterfly values saturate at about +-historyGravity (14000), so at a factor
	// of 19 the strongest history shifts the reduction by about one ply of a non PV node (divisor 261).
	// The factor belongs to the history group: its scale is the history's scale.
	// CLOP (M4, 2000 samples at 3.3+0.007, range -64..64 around 0): 39.78.
	// Tested on d225a32 (0.5.0-034 plus the futility cleanup): 40 against 0: H1 accepted, bounds -2/+3, 4257 games, 51.68 %
	// At tournament depth (CLAUDE.md), on 0.5.0-036: CLOP (Studio, 2000 samples at 13.5+0.07, range 0..80 around 40): 51.92.
	// 52 against 40 at 13.5+0.07: H1 accepted, bounds -2/+3, 10714 games.
	constexpr bool OPT_HISTORY = SearchConfig::optimizeHistory;
	numerator -= _butterflyBoard->getValue(move) * tunable<OPT_HISTORY, "lmrHistoryFactor", 52, 0, 104>() / 1024;

	return std::max(numerator, 0) * LMR_UNIT / divisor;
}

/**
 * The terms sit between the move count pruning and the late move reduction search, as in Stockfish: the pruning
 * reads the reduction of computeLMR, the reduced search adds these terms, each in 1/LMR_UNIT = 1/128 ply (Volker
 * 2026-11-24). Quiet moves only - captures keep the tested rule of computeLMR. The reduction never drops below 0, and
 * the terms never take the search below depth 1 (they do not lower the depth further than computeLMR already does).
 * All at 0: identical to the version without them. CLOP (Studio, 5000 samples at 13.5+0.07, base 069, start values
 * ttPv -128, PV -64, cut 128, all 32, tt capture 128 in the middle of their ranges): ttPv -55.5, PV -69.0, cut 117.0,
 * all 28.6, tt capture 123.0, rounded, no correction needed (PV and tt pv reduced less, cut and tt capture more).
 */
ply_t Search::lmrSearchReduction(const SearchNode& node, Move move, int32_t lmrFine, ply_t moveDepth) {
	const ply_t lmr = static_cast<ply_t>(lmrFine / LMR_UNIT);
	if (move.isCapture()) return lmr;
	constexpr bool OPT = SearchConfig::optimizeLmrTerms;
	int32_t terms = 0;
	if (node.ttPv) terms += tunable<OPT, "lmrTermTtPv", -56, -256, 256>();
	if (node.isPVNode()) terms += tunable<OPT, "lmrTermPv", -69, -256, 256>();
	else if (node.isCutNode()) terms += tunable<OPT, "lmrTermCut", 115, -256, 256>();
	else terms += tunable<OPT, "lmrTermAll", 52, -256, 256>();
	const Move ttMove = node.getTTMove();
	if (!ttMove.isEmpty() && ttMove.isCapture()) terms += tunable<OPT, "lmrTermTtCapture", 123, -256, 256>();
	const ply_t reduction = static_cast<ply_t>(std::max(0, lmrFine + terms) / LMR_UNIT);
	if (reduction <= lmr) return reduction;
	// A larger reduction stops at depth 1, unless computeLMR already went below
	return std::min(reduction, std::max(lmr, static_cast<ply_t>(moveDepth - 2)));
}

value_t Search::negaMaxPreSearch(MoveGenerator& position, SearchStack& stack, value_t alpha, value_t beta, ply_t depth, ply_t ply) {
	SearchNode& node = stack[ply];
	SearchNode& childNode = stack[ply + 1];
	node.setFromParentNode(position, stack[ply - 1], alpha, beta, depth, false);
	// Must be after setFromParentNode
	node.probeTT(false, alpha, beta, depth, ply);
	computeMoves(position, node);
	Move curMove;
	while (!(curMove = selectNextMove(position, stack, node)).isEmpty()) {

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
template <Search::SearchRegion TYPE>
ply_t Search::iir(const SearchStack& stack, ply_t depth, ply_t ply) {
	// A near leaf node never reaches the minimal depth below
	if constexpr (TYPE == SearchRegion::NEAR_LEAF) return 0;
	else {
		const SearchNode& node = stack[ply];

		if (!SearchConfig::DO_IIR) return 0;
		if (depth <= SearchConfig::IIR_MIN_DEPTH) return 0;
		// Any move worth trying first is enough, from the hash or from the previous iteration.
		if (!node.getTTMove().isEmpty()) return 0;
		if (node.hasPVMove()) return 0;
		// PV and cut nodes are reduced, all nodes are not, as in Stockfish (!allNode). Up to 0.5.0-066 the node types
		// were swapped (see childNodeType) and the condition read "cut" - so the IIR reduced the real all nodes only;
		// 0.5.0-067 kept that with corrected types. Cut nodes only lost on its own (0.5.0-070, H0 after 2503 games,
		// node count +57 %): in Qapla the reduction in all nodes saved most. Cut and all nodes won (0.5.0-071 H1,
		// 0.5.0-072 H1). Here the all nodes get their saving from the LMR instead - the all node term between move
		// count pruning and LMR, tuned together with this (Volker 2026-11-25).
		// Tested 0.4.0-060: PV nodes only, (real) all nodes left out: -8.4 Elo, 5597 games
		// Tested 0.4.0-053: the same idea as a pre search (IID) in (real) all nodes: -7.0 Elo, 7049 games
		if constexpr (TYPE != SearchRegion::PV) {
			if (SearchNode::childNodeType(stack[ply - 1].getNodeType(), false)
				!= SearchNode::NodeType::CUT) return 0;
		}

		return SearchConfig::IIR_REDUCTION;
	}
}

template <Search::SearchRegion TYPE>
ply_t Search::se(MoveGenerator& position, SearchStack& stack, value_t alpha, value_t beta, ply_t depth, ply_t ply) {
	// Near leaf nodes are searched with a depth below the minimal depth required below
	if constexpr (TYPE == SearchRegion::NEAR_LEAF) return 0;
	constexpr bool IS_PV = TYPE == SearchRegion::PV;
	// The extension in non pv nodes is switchable as a whole, see search-config.h
	if constexpr (!IS_PV && !SearchConfig::DO_SE_IN_NON_PV) return 0;
	if (!SearchConfig::DO_SE_EXTENSION) return 0;
	SearchNode& node = stack[ply];
	SearchNode& childNode = stack[ply + 1];
	SearchNode& parentNode = stack[ply - 1];

	// Do not double extend check moves
	if (SearchConfig::DO_CHECK_EXTENSIONS && node.sideToMoveIsInCheck) return 0;

	// We need a certain search depth left to efficiently calculate a singular extension
	if (depth < 4) return 0;

	// Limit maximal extension depth
	if (ply + depth > std::min(stack[0].remainingDepth * 2, int(SearchConfig::MAX_SEARCH_DEPTH))) return 0;

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
		depth - tunable<SearchConfig::optimizeSE, "seTTMinDepthReduction", 6, 0, 12>();
	// Depth used to search the remaining moves against the singular margin
	const ply_t seDepth =
		depth * 100 / tunable<SearchConfig::optimizeSE, "seDepthDivisor", 200, 100, 300>();
	if (ttMove.isEmpty()) return 0;
	// We require a certain search depth for the tt move to be considered for a singular extension
	if (node.ttDepth < ttMinDepth) return 0;
	// No se, if the tt already shows a mate or equivalent value
	if (node.ttValue != NO_VALUE && (node.ttValue < -MIN_MATE_VALUE || node.ttValue > MIN_MATE_VALUE)) return 0;

	// The margin the remaining moves must fail below to make the tt move singular. PV and non
	// PV nodes get their own values, the tt value is a much weaker information in a non PV node
	node.setSE(IS_PV
		? tunable<SearchConfig::optimizeSE, "sePvMarginConst", 1, -100, 300>()
			+ tunable<SearchConfig::optimizeSE, "sePvMarginFactor", 4, 0, 100>() * depth
		: tunable<SearchConfig::optimizeSE, "seNonPvMarginConst", 0, -100, 300>()
			+ tunable<SearchConfig::optimizeSE, "seNonPvMarginFactor", 4, 0, 100>() * depth);
	_computingInfo._nodesSearched++;

	// Cutoffs checks all kind of cutoffs including futility, nullmove, bitbase and others 
	if (checkEvalReleatedCutoffsAndSetEval<SearchRegion::NEAR_LEAF>(position, stack, node, seDepth, ply)) return 0;

	// A pv node is never cut. Its value is needed exactly, and the reduced se search is no
	// basis to drop the pv move without having searched it
	constexpr bool doMultiCut = SearchConfig::DO_MULTI_CUT && !IS_PV;

	computeMoves(position, node);
	Move curMove;
	while (!(curMove = selectNextMove(position, stack, node)).isEmpty()) {
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
bool Search::nonSearchingCutoff(MoveGenerator& position, SearchStack& stack, SearchNode& node, value_t alpha, value_t beta, ply_t depth, ply_t ply) {
	assert(ply >= 1);

	node.cutoff = Cutoff::NONE;
	node.setHashSignature(position);

	// The bound itself is unreachable as well - the best value reachable at ply q is
	// MAX_VALUE - q - 1 - so it belongs inside the test, as in Quiescence::search.
	// Tested 0.5.0-018, SPRT h0 = -10, h1 = 0: H0 accepted once, H1 accepted in both repeats.
	if (TYPE != SearchRegion::PV && alpha >= MAX_VALUE - value_t(ply)) {
		node.setCutoff(Cutoff::FASTER_MATE_FOUND, MAX_VALUE - value_t(ply));
	}
	else if (TYPE != SearchRegion::PV && beta <= -MAX_VALUE + value_t(ply)) {
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
	else if (ply >= SearchConfig::MAX_SEARCH_DEPTH) {
		node.setCutoff(Cutoff::MAX_SEARCH_DEPTH, Eval::eval(position, &_pawnTT, ply));
	}
	else if (TYPE != SearchRegion::NEAR_LEAF && stack[0].remainingDepth > 1 && _isMaster && _clockManager->emergencyAbort()) {
		node.setCutoff(Cutoff::ABORT, -MAX_VALUE);
	}
	else if (_isMaster ? _clockManager->stopOnNodeTarget(_computingInfo.getTotalNodes()) : isSearchStopped()) {
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

	SearchNode& node = stack[ply];
	node.pv.setEmpty(ply);

	// 1. Detect direct cutoffs without requiring search or eval
	// This includes checking the hash and setting the hash information like ttMove
	if (nonSearchingCutoff<TYPE>(position, stack, node, alpha, beta, depth, ply)) return node.bestValue;
	SearchNode& childNode = stack[ply + 1];
	SearchNode& parentNode = stack[ply - 1];

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
	// result unreliable, so it is searched shallower. Must be before node.setFromParentNode,
	// which hands the depth on to the node.
	depth -= iir<TYPE>(stack, depth, ply);

	// From here on the node orders its quiet moves: in the singular extension search, the null move
	// verification and the move loop. They read the rows of the continuation history for the moves
	// played after the move into this node and after the moves before it.
	_continuationHistory->setRows(stack, ply, node.continuationRows);

	// 5. Singular extension for the tt move, computed for PV and non PV nodes, see se()
	const auto seExtension = se<TYPE>(position, stack, alpha, beta, depth, ply);

	// se() cuts the node, if it finds a second move above beta, see multi cut in se().
	// Must be before setFromParentNode, as that resets the cutoff
	if (node.cutoff == Cutoff::MULTI_CUT) {
		WhatIf::whatIf.cutoff(position, _computingInfo, stack, ply, node.cutoff);
		return node.bestValue;
	}

	// 6. Setting node values from parent node, must be after seExtensions
	// We need only stable information from parent node; the node type and the previous move.
	node.setFromParentNode(position, parentNode, alpha, beta, depth, TYPE == SearchRegion::PV);
	

	// 7. Check all kind of early cutoffs including futility, nullmove and others
	// Additionally set eval. This is done as late as possible, as it is very time consuming. Some cutoff checks needs eval.
	if (checkEvalReleatedCutoffsAndSetEval<TYPE>(position, stack, node, depth, ply)) {
		WhatIf::whatIf.cutoff(position, _computingInfo, stack, ply, node.cutoff);
		return node.bestValue;
	}

	computeMoves(position, node);

	// 7b. Ask the tablebases. It has to be here and not earlier: the move list decides whether
	// the stored entry is exact, the hash probe above keeps a repeated position cheap, and the
	// eval the value builds on has just been set.
	if (hasTablebaseCutoff(position, node, depth)) {
		WhatIf::whatIf.cutoff(position, _computingInfo, stack, ply, node.cutoff);
		return node.bestValue;
	}

	node.computeCheckGivingSquares(position);
	// 8. Calculate additional node wide search extensions
	if (TYPE == SearchRegion::PV) depth = node.extendSearch(position, stack[0].remainingDepth);

	moveLoop<TYPE, false>(position, stack, node, depth, seExtension, ply, Move::EMPTY_MOVE, false);

	// 6. Update tt and killer, but not if search is aborted as then bestValue and bestMove may be wrong.
	// Every cutoff has returned above and the first move is always searched (pruning needs a
	// bestValue), so the move loop leaves a real value. bestMove stays empty on a fail low.
	if (!isSearchStopped()) {
		assert(node.cutoff == Cutoff::NONE && node.bestValue != -MAX_VALUE);
		// The node owns the hash of its own position: it was stored on entry, and by the
		// time the ply terminates every move tried below has been taken back.
		assert(node.positionHash == position.computeBoardHash());
		node.setTTEntry(node.positionHash, TYPE == SearchRegion::PV);
		if (!node.bestMove.isEmpty()) {
			node.killerMove.setKiller(node.bestMove);
			// The best move of a node in check is an evasion: it says little about the quiet moves of the
			// positions without check, so it does not enter the history tables (Volker 2026-11-19, 0.5.0-046)
			// SPRT 0.5.0-046 against 038 at 29+0.15 (Linux, tournament depth): H1 accepted, bounds -2/+3, 13967 games
			if (!node.sideToMoveIsInCheck) {
				_butterflyBoard->newBestMove(node.bestMove, depth,
					node.moveProvider.getTriedMoves(), node.moveProvider.getTriedMovesAmount());
				_continuationHistory->newBestMove(node.continuationRows, node.bestMove, depth,
					node.moveProvider.getTriedMoves(), node.moveProvider.getTriedMovesAmount());
			}
			// Quiet moves only: a capture would only take the place of a quiet answer, the
			// capture stages order captures
			if (!node.bestMove.isCaptureOrPromote()) {
				_continuationHistory->setMove(stack, ply, node.bestMove);
			}
		}
	}
	// Inform the user about advances in search
	if (TYPE != SearchRegion::NEAR_LEAF && _isMaster) {
		_computingInfo.setHashFullInPermill(node.getHashFillRateInPermill());
		collectThreadTotals();
		_computingInfo.printSearchInfo(_clockManager->isTimeToSendNextInfo());
	}
	return node.bestValue;
}

template <Search::SearchRegion TYPE, bool SPLIT>
void Search::moveLoop(MoveGenerator& position, SearchStack& stack, SearchNode& splitNode,
	ply_t depth, ply_t seExtension, ply_t ply, Move firstMove, bool firstLosingCapture)
{
	SearchNode& node = stack[ply];
	SearchNode& childNode = stack[ply + 1];
	value_t result;
	Move curMove = firstMove;
	bool curLosingCapture = firstLosingCapture;

	// Loop through all moves
	while (true) {
		// Take the next move from the node that owns the list. At a split point the list, the
		// window and the best value are shared, so they are read under the lock together.
		if (curMove.isEmpty()) {
			SplitLock<SPLIT> lock(splitNode.splitMutex);
			if constexpr (SPLIT) {
				if (splitNode.isFailHigh()) break;
			}
			curMove = selectNextMove(position, stack, splitNode);
			if (curMove.isEmpty()) break;
			// A capture handed out in the last stage is one that SEE rated as loosing; read with the move,
			// under the same lock
			curLosingCapture = curMove.isCapture() && splitNode.selectStage == MoveType::REMAINING_MOVES;
			if constexpr (SPLIT) {
				node.alpha = splitNode.alpha;
				node.bestValue = splitNode.bestValue;
				node.movesTried = splitNode.movesTried;
			}
		}

		// A node with enough depth left opens itself as split point when a thread waits for
		// work: the helper joins this loop and takes moves from the same list, see
		// helpAtSplitPoint. Never on the first move - its result sets the window the others
		// are searched with - and never on the last, this thread would only wait for it. The
		// rest of the loop runs in the split instantiation, this one is done then. Near leaf
		// nodes carry none of this.
		if constexpr (TYPE != SearchRegion::NEAR_LEAF && !SPLIT) {
			if (_threads && depth >= 3 && node.movesTried > 1 && !node.isLastMove()
				&& _threads->hasWaitingThread()
				&& openSplitPoint(stack, node, depth, seExtension, ply, TYPE == SearchRegion::PV)) {
				moveLoop<TYPE, true>(position, stack, node, depth, seExtension, ply, curMove, curLosingCapture);
				closeSplitPoint(position, stack, node, ply);
				return;
			}
		}

		// The singular extension belongs to the move se() proved to be singular, not to the node.
		// It cannot add on top of the check extension, as se() returns 0 for a node in check.
		const ply_t moveExtension = curMove == node.getTTMove() ? seExtension : 0;
		const ply_t moveDepth = moveExtension > 0 ?
			std::min(depth + moveExtension, stack[0].remainingDepth * 2) : depth;

		// Late move reduction from the second move on, futility and move count pruning from the fourth
		const bool doLmr = node.movesTried > 1 && !node.isCheckMove(position, curMove);
		const bool doMovePrunings = doLmr && node.movesTried > 3;
		// lmr is needed for move count pruning and late move reduction search
		const int32_t lmrFine = doLmr ? computeLMR(node, position, depth, ply, curMove, curLosingCapture) : 0;
		const ply_t lmr = static_cast<ply_t>(lmrFine / LMR_UNIT);
		// Never skip moves when escaping from mate and in positions with pawns only.
		if (doMovePrunings && node.bestValue > -MIN_MATE_VALUE && position.hasMoreThanPawns()) {
			
			// 1. Futility pruning: skip quiet moves in late move loop when position is too bad
			if (node.canPruneFutility(position, curMove)) {
				curMove.setEmpty();
				continue;
			}

			// 2. Move count pruning, for quiet moves only: a loosing capture is reduced, never skipped
			if (lmr > 0 && depth - lmr < 0 && !curMove.isCapture()) {
				curMove.setEmpty();
				continue;
			}
		}

		childNode.doMove(position, curMove);

		// 3. Late move reduction search
		// We continue with the next move, if the lmr search returns a value less than alpha
		const ply_t lmrSearch = doLmr ? lmrSearchReduction(node, curMove, lmrFine, moveDepth) : 0;
		if (lmrSearch > 0) {
			result = -searchChild<TYPE>(
				position, stack, -node.alpha - 1, -node.alpha, moveDepth - 1 - lmrSearch, ply + 1);
			WhatIf::whatIf.moveSearched(position, _computingInfo, stack, curMove, moveDepth - 1 - lmrSearch, ply, result, "LMR");
			if (result <= node.alpha) {
				childNode.undoMove(position);
				// A stopped search returns no value worth keeping, at a split point it must not
				// reach the owner's node
				if constexpr (SPLIT) {
					if (isSearchStopped()) break;
				}
				// We improve value on lmr result. Especially important to not get false mate values due to skipped escape moves
				SplitLock<SPLIT> lock(splitNode.splitMutex);
				if (result > splitNode.bestValue) {
					splitNode.bestValue = result;
				}
				curMove.setEmpty();
				continue;
			}
			// searching modifies the attack masks. But they are required for the next move generation
			position.computeAttackMasksForBothColors();
			// Another thread may have raised the window meanwhile
			if constexpr (SPLIT) node.alpha = splitNode.alpha;
		}
		// 4. Searching with null window either because of non pv search or because it is not the first move in pv.
		// Additionally we do not go to null window search on PV, if depth is 1 or 0
		// We do not return fail high from a null window search in PV node
		bool isDirectPVWindowSearch = TYPE == SearchRegion::PV && (node.movesTried == 1 || depth <= 1);
		if (!isDirectPVWindowSearch) {
			result = -searchChild<TYPE>(
				position, stack, -node.alpha - 1, -node.alpha, moveDepth - 1, ply + 1);
			WhatIf::whatIf.moveSearched(position, _computingInfo, stack, curMove, moveDepth - 1, ply, result, TYPE == SearchRegion::PV ? "ZeroW" : "Std.");
			if constexpr (SPLIT) node.alpha = splitNode.alpha;
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

		if constexpr (SPLIT) {
			if (isSearchStopped()) {
				childNode.undoMove(position);
				break;
			}
		}
		{
			SplitLock<SPLIT> lock(splitNode.splitMutex);
			splitNode.setSearchResult(result, childNode, curMove);
		}

		childNode.undoMove(position);
		if (splitNode.isFailHigh()) break;
		curMove.setEmpty();
	}
}

uint64_t Search::helperNodes() const {
	uint64_t nodes = 0;
	for (uint32_t index = 1; index < _threads->size(); index++) {
		nodes += (*_threads)[index].search.getNodesSearched();
	}
	return nodes;
}

uint64_t Search::helperTbHits() const {
	uint64_t tbHits = 0;
	for (uint32_t index = 1; index < _threads->size(); index++) {
		tbHits += (*_threads)[index].search.getTbHits();
	}
	return tbHits;
}

void Search::collectThreadTotals() {
	if (_threads) {
		_computingInfo.setHelperNodes(helperNodes());
		_computingInfo.setHelperTbHits(helperTbHits());
	}
	if (_extraSearchTotals) {
		_computingInfo.setExtraTotals(_extraSearchTotals->getTotalNodes(),
			_extraSearchTotals->getTotalTbHits());
	}
}

bool Search::isSearchStopped() const {
	return _clockManager->isSearchStopped() || (_self && _self->hasFailedHighSplitPoint());
}

bool Search::openSplitPoint(SearchStack& stack, SearchNode& node, ply_t depth, ply_t seExtension, ply_t ply, bool pvNode) {
	node.owner = _self;
	node.split.node = &node;
	node.split.stack = &stack;
	node.split.ply = ply;
	node.split.depth = depth;
	node.split.seExtension = seExtension;
	node.split.pvNode = pvNode;
	node.helpers = 0;
	node.isSplitPoint = true;
	// Every booked thread counts from now on, so that the node is not left before it has
	// fetched what it reads here.
	const int32_t booked = _threads->bookAll(*_self, node.split);
	if (booked == 0) {
		node.isSplitPoint = false;
		node.owner = nullptr;
		return false;
	}
	node.helpers.fetch_add(booked, std::memory_order_acq_rel);
	_self->addSplit(&node, ply);
	return true;
}

void Search::closeSplitPoint(MoveGenerator& position, SearchStack& stack, SearchNode& node, ply_t ply) {
	{
		std::lock_guard<std::mutex> lock(node.splitMutex);
		node.isSplitPoint = false;
	}
	waitForHelpers(*_self, node, position, stack, ply);
	_self->removeSplitsAbove(ply - 1);
	node.owner = nullptr;
}

void Search::waitForHelpers(SearchThread& self, SearchNode& node, MoveGenerator& position, SearchStack& stack, ply_t ply) {
	// A helper that owns a split point inside its job waits here as well: it becomes
	// bookable for the time being and is its job's again afterwards
	SearchNode* const outerWaitingAt = self.waitingAt;
	self.waitingAt = &node;
	std::unique_lock<std::mutex> lock(self.mutex);
	while (node.helpers.load(std::memory_order_acquire) > 0) {
		self.waiting = true;
		_threads->enterWaiting();
		const auto start = std::chrono::steady_clock::now();
		self.cv.wait(lock, [&] { return self.hasJob || node.helpers.load(std::memory_order_acquire) == 0; });
		_waitMicroseconds += std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - start).count();
		self.waiting = false;
		_threads->leaveWaiting();
		if (self.hasJob) {
			self.hasJob = false;
			lock.unlock();
			runJob(self, position, stack, ply);
			lock.lock();
		}
	}
	self.waitingAt = outerWaitingAt;
}

void Search::helperLoop(SearchThread& self) {
	std::unique_lock<std::mutex> lock(self.mutex);
	for (;;) {
		self.waiting = true;
		_threads->enterWaiting();
		// Tells waitUntilIdle
		self.cv.notify_all();
		self.cv.wait(lock, [&self] { return self.hasJob || self.stop; });
		self.waiting = false;
		_threads->leaveWaiting();
		if (self.stop) break;
		self.hasJob = false;
		lock.unlock();
		runJob(self, self.position, self.stack, 0);
		lock.lock();
	}
}

void Search::runJob(SearchThread& self, MoveGenerator& position, SearchStack& stack, ply_t fromPly) {
	const auto start = std::chrono::steady_clock::now();
	if (self.job.pvNode) {
		helpAtSplitPoint<SearchRegion::PV>(self, position, stack, fromPly);
	}
	else {
		helpAtSplitPoint<SearchRegion::INNER>(self, position, stack, fromPly);
	}
	_helpMicroseconds += std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - start).count();
}

template <Search::SearchRegion TYPE>
void Search::helpAtSplitPoint(SearchThread& self, MoveGenerator& position, SearchStack& stack, ply_t fromPly) {
	const SplitJob job = self.job;
	SearchNode& splitNode = *job.node;
	SearchThread& owner = *splitNode.owner;
	const ply_t ply = job.ply;
	SearchNode& node = stack[ply];

	// The split points the owner works under are this thread's now as well: a thread waiting
	// at one of them may be booked from here, and a fail high at one of them stops this
	// search too
	self.takeSplitsFrom(owner, ply);
	self.addSplit(&splitNode, ply);

	// Fetch the line and the node's constants from the owner's stack, then reach the node on
	// the own board. No lock: the owner stays in the node while helpers work there.
	Move line[SearchConfig::MAX_SEARCH_DEPTH + 1];
	stack.fetchForHandover(*job.stack, fromPly, ply, line);
	stack.replayLine(position, line, fromPly, ply);
	node.computeCheckGivingSquares(position);

	moveLoop<TYPE, true>(position, stack, splitNode, job.depth, job.seExtension, ply, Move::EMPTY_MOVE, false);

	stack.undoLine(position, fromPly, ply);
	self.removeSplitsAbove(fromPly);

	// The last one to leave wakes the owner
	if (splitNode.helpers.fetch_sub(1, std::memory_order_acq_rel) == 1) {
		owner.wake();
	}
}

void Search::storePVToTT(MoveGenerator& position, SearchStack& stack, const RootMove& rootMove, ply_t ply) {
	if (ply >= stack.size()) {
		return; // No stack for this ply
	}
	SearchNode& node = stack[ply];
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
	SearchNode& node = stack[0];
	value_t result;

	ply_t depth = node.remainingDepth;

	// we use the movelist from rootmoves. computeMoves is only to initialize other variables
	computeMoves(position, node);
	_computingInfo.nextIteration(node);
	WhatIf::whatIf.moveSelected(position, _computingInfo, stack, Move::EMPTY_MOVE, depth, 0);
    //printStackInfo("stack size: ");
#ifdef USE_STOCKFISH_EVAL
	Stockfish::Engine::set_position(position.getFen());
#endif
	// Every move in the tablebase win bucket gets searched, plus - only if MultiPV asks for more
	// lines than the win bucket has moves - as many more (next-best bucket first, per the sort
	// order) as needed to reach it. Without a root win this is every legal move, as before.
	for (uint32_t triedMoves = 0; triedMoves < _tbSearchableMoves; ++triedMoves) {

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

	// No move searched (all skipped, or an abort before the first result) leaves -MAX_VALUE: nothing to store
	if (!_clockManager->isSearchStopped() && node.bestValue != -MAX_VALUE) {
		assert(node.positionHash == position.computeBoardHash());
		node.setTTEntry(node.positionHash, true);
		// No killer here: the root takes its moves from the root move list, its move provider
		// never selects a move and would never read it.
		// Known flaw, left as it is: for the same reason the tried moves of the root are empty,
		// so newBestMove only raises the best move and never lowers the root moves tried
		// before it. One update per iteration cannot have a measurable effect on playing strength.
		if (!node.bestMove.isEmpty()) {
			_butterflyBoard->newBestMove(node.bestMove, depth,
				node.moveProvider.getTriedMoves(), node.moveProvider.getTriedMovesAmount());
		}
	}
	_computingInfo.getRootMoves().bubbleSort(0);
	_computingInfo.setHashFullInPermill(node.getHashFillRateInPermill());
	collectThreadTotals();
	_computingInfo.printSearchResult();
	// Only the master reports: an extra search writes to the same stream from its own thread
	if (_isMaster && _threads && _threads->size() > 1) {
		// Test output while the parallel search is being built
		const auto elapsed = std::max<int64_t>(1, _computingInfo.getTimeSpentInMilliseconds());
		std::cout << "info string master waited " << (_waitMicroseconds / 10 / elapsed) << "%"
			<< " helped " << (_helpMicroseconds / 10 / elapsed) << "%";
		for (uint32_t index = 1; index < _threads->size(); index++) {
			const Search& helper = (*_threads)[index].search;
			std::cout << " | helper " << index << " nodes " << helper.getNodesSearched()
				<< " busy " << (helper.getHelpMicroseconds() / 10 / elapsed) << "%";
		}
		std::cout << std::endl;
	}
}


