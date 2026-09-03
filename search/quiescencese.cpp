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

#include "quiescence.h"
#include "../eval/eval.h"
#include "search-config.h"
#include "tunable.h"
#include "moveprovider.h"
#include "whatIf.h"
#include "see.h"


using namespace QaplaInterface;
using namespace ChessEval;
using namespace QaplaSearch;
using QaplaBasics::MoveList;

/**
 * Computes the weight of all captures
 */
void Quiescence::computeAllCaptureWeight(const MoveGenerator& board, MoveList& moveList, Move previousMove) {
	for (uint32_t moveNo = 0; moveNo < moveList.getNonSilentMoveAmount(); moveNo++) {
		Move move = moveList[moveNo];
		if (move != Move::EMPTY_MOVE) {
			value_t weight = board.getAbsolutePieceValue(move.getCapture());
			// Tested 0.5.0-024, removing this bonus: SPRT h0 = -4, h1 = 1, undecided at 20000 games.
			// Tested 0.4.0-039, the stronger form with real piece values that puts every recapture
			// ahead of every other capture: SPRT rejected it over 10018 games.
			if (previousMove.isCapture() && (previousMove.getDestination() == move.getDestination())) {
				// order recaptures to the front
				weight += 10;
			}
			moveList.setWeight(moveNo, weight);
		}
	}
}

/**
 * Generates the non silent moves of the position and weights them for the ordering
 */
// Tested 0.4.0-057, quiescence trying the tt move first, in this list and in the evades:
// SPRT rejected it over 9826 games.
// Tested 0.5.0-021, the tt move weighted to the front of this list instead: SPRT h0 = -2,
// h1 = 3 against 0.5.0-020, H0 accepted after 11587 games.
void Quiescence::computeCaptures(MoveGenerator& board, MoveList& moveList, Move previousMove) {
	board.genNonSilentMovesOfMovingColor(moveList);
	computeAllCaptureWeight(board, moveList, previousMove);
}

/**
 * Gets the index of the highest weighted capture from curMoveNo on, -1 if none is left
 */
int32_t Quiescence::findNextBestCaptureMove(MoveList& moveList, uint32_t curMoveNo) {
	int32_t bestMoveNo = -1;
	value_t maxWeight = -MAX_VALUE;
	value_t curWeight;
	Move move;
	for (uint32_t moveNo = curMoveNo; moveNo < moveList.getNonSilentMoveAmount(); moveNo++) {
		move = moveList[moveNo];
		if (move != Move::EMPTY_MOVE) {
			curWeight = moveList.getWeight(moveNo);
			if (curWeight > maxWeight) {
				maxWeight = curWeight;
				bestMoveNo = static_cast<int32_t>(moveNo);
			}
		}
	}
	return bestMoveNo;
}

/**
 * Provides the next capture, highest weight first
 */
Move Quiescence::selectNextCapture(MoveList& moveList, uint32_t& curMoveNo) {
	// Search the move list for the next best capture move beginning with curMoveNo.
	const int32_t bestMoveNo = findNextBestCaptureMove(moveList, curMoveNo);
	if (bestMoveNo == -1) {
		return Move::EMPTY_MOVE;
	}
	// Move the best capture to the front of the list, shifting everything else one step back.
	// This preserves the order of the remaining moves, which decides the ties: findNextBestCaptureMove
	// takes the first move of equal weight.
	// Tested 0.5.0-025, swapping instead of shifting, which is constant instead of linear work but
	// reorders the rest: SPRT h0 = -2, h1 = 3, undecided at 20000 games.
	moveList.dragMoveToTheBack(curMoveNo, static_cast<uint32_t>(bestMoveNo));
	const Move move = moveList[curMoveNo];
	curMoveNo++;
	return move;
}


/**
 * Gets an entry from the transposition table
 * @returns hash value and hash move or -MAX_VALUE, if no value found
 */
std::tuple<value_t, value_t, uint32_t, Move> Quiescence::probeTT(MoveGenerator& position, value_t alpha, value_t beta, ply_t ply) {
	uint32_t ttIndex = _tt->getEntryIndex(position.computeBoardHash());

	if (ttIndex != TT::INVALID_INDEX) {
		TTEntry entry = _tt->getEntry(ttIndex);
		const auto eval = entry.getEval();
		const auto bestValue = entry.getTTCutoffValue(alpha, beta, 0, ply);
		const auto move = entry.getMove();
		const auto precision = entry.getComputedPrecision();
		return std::make_tuple(eval, bestValue, precision, move);
	}
	return std::make_tuple(NO_VALUE, NO_VALUE, TTEntry::INVALID, Move::EMPTY_MOVE);
}


/**
 * Performs the quiescense search
 */
value_t Quiescence::search(bool isPvNode,
	MoveGenerator& position, ComputingInfo& computingInfo, Move lastMove,
	value_t alpha, value_t beta, ply_t ply)
{
	// Unclear, we need MAX_SEARCH_DEPTH in normal depth to reduce stack size, but maybe not in quiescence search.
	if (ply >= SearchConfig::MAX_SEARCH_DEPTH) {
		return position.isInCheck() ? DRAW_VALUE : Eval::eval(position, _tt->getPawnTT(), ply);
	}
	// Tested 0.5.0-004, removing the two checks: SPRT h0 = -2, h1 = 3, H0 accepted.
	// Cut, if distance to mate is too high to reach the search window
	if (alpha >= MAX_VALUE - ply) {
		return MAX_VALUE - ply;
	}	
	if (beta <= -MAX_VALUE + ply) {
		return -MAX_VALUE + ply;
	}
	// Test Stockfish NNUE evaluation
#ifdef USE_STOCKFISH_EVAL
	Stockfish::StateInfo si;
#endif

	// 1. Access the tt and return the ttValue, if available
	// Note: the ttValue is ensured to be valide, Exact/Lower/Upper bound is handled already.
	computingInfo._nodesSearched++;
	WhatIf::whatIf.moveSelected(position, computingInfo, lastMove, ply, true);
	auto [ttEval, ttValue, ttPrecision, ttMove] = probeTT(position, alpha, beta, ply);
	
	// Tested 0.5.0-007, guarding this and the cutoff in SearchNode::probeTT with
	// abs(value) < MIN_MATE_VALUE: SPRT h0 = -6, h1 = -1, H0 accepted.
	if (ttValue != NO_VALUE) {
		return ttValue;
	}

	// 2. Handle evade case
	const auto evadesCheck = SearchConfig::EVADES_CHECK_IN_QUIESCENSE && position.isInCheck();
	if (evadesCheck) {
		// Stand pat is not an option in check. The minimal value is both the start value and the
		// mate value the node returns when the evade list stays empty.
		auto bestValue = -MAX_VALUE + ply;

		MoveProvider moveProvider;
		Move move;
		moveProvider.setTTMove(ttMove);
		moveProvider.computeEvades(position, lastMove);

		while (!(move = moveProvider.selectNextMove(position)).isEmpty()) {

			const PositionSnapshot snapshot = position.getSnapshot();
			position.doMove(move);
			_tt->prefetch(position.computeBoardHash());

		#ifdef USE_STOCKFISH_EVAL
			Stockfish::Engine::doMove(move, si);
		#endif
			auto valueOfNextPlySearch = -search(isPvNode, position, computingInfo, move, -beta, -alpha, ply + 1);
			position.undoMove(move, snapshot);
		#ifdef USE_STOCKFISH_EVAL
			Stockfish::Engine::undoMove(move);
		#endif

			if (valueOfNextPlySearch > bestValue) {
				bestValue = valueOfNextPlySearch;
				if (bestValue >= beta) {
					break;
				}
				alpha = std::max(alpha, bestValue);
			}
		}
		WhatIf::whatIf.moveSearched(position, computingInfo, lastMove, alpha, beta, bestValue, bestValue, ply);
		return bestValue;
	}


	// 2. Compute stand-pat value
	value_t bestValue, standPatValue;
	bestValue = standPatValue = ttEval != NO_VALUE ? ttEval : Eval::eval(position, _tt->getPawnTT(), ply, alpha);

	// 3. Beta cut-off: If the stand-pat value is already above beta, we can return it immediately.
	if (standPatValue >= beta) {
		WhatIf::whatIf.moveSearched(position, computingInfo, lastMove, alpha, beta, bestValue, standPatValue, ply);
		return standPatValue;
	}

	// Tested 0.5.0-005 and 0.5.0-016, a node level delta cutoff here:
	// SPRT h0 = -2, h1 = 3, H0 accepted.

	// Eval::assertSymetry(position, standPatValue);

	// 4. Correct alpha to be at least stand-pat value.
	alpha = std::max(alpha, standPatValue);

	// 5. Compute futility pruing treshold
	// A winning bonus can be fully destroyed by capturing the piece so don´t prune on winning bonus eval.
	// Tested 0.5.0-014, margin 35 instead of 50: SPRT h0 = -2, h1 = 3, H0 accepted.
	// Tested 0.5.0-009, -010, -015, -017, an eval dependent addition to this margin:
	// SPRT h0 = -2, h1 = 3, H0 accepted. 0.5.0-012 and -013 undecided.
	auto safeValue = 
		(standPatValue < -WINNING_BONUS || standPatValue > WINNING_BONUS || 
			!position.doFutilityOnCapture(position.isWhiteToMove() ? BLACK: WHITE)) ? 
		MAX_VALUE : 
		standPatValue + tunable<SearchConfig::optimizeQS, "qsAlphaSafetyMargin", 50, 0, 100>();

	// 6. Generate the captures and weight them for the ordering
	MoveList moveList;
	uint32_t curMoveNo = 0;
	Move move;
	// Tested 0.5.0-026, dropping the captures that will certainly be pruned before the weights are
	// computed: SPRT h0 = -3, h1 = 2, undecided at 20000 games. Node count identical and the same
	// speed, so there was nothing for the run to find. The threshold is alpha - safeValue and alpha
	// sits near the stand pat value in most nodes, so it is usually negative and nothing can be
	// filtered at all. Two traps if it is ever picked up again: the loop folds the futility value of
	// a pruned move into bestValue, so a filter has to carry that out with it, and the removal has
	// to be stable or the ties above change.
	computeCaptures(position, moveList, lastMove);

	// 7. Move Loop
	while (!(move = selectNextCapture(moveList, curMoveNo)).isEmpty()) {

		// 8. Move foreward pruning. 
		// We compute a possible gain of the move (already including margin) and prune this move search,
		// if it does not reach alpha.
		// Tested 0.5.0-002, removing it: SPRT h0 = -2, h1 = 3, H0 accepted.
		if (safeValue != MAX_VALUE && !move.isPromote()) {
			const value_t threshold = alpha - safeValue;
			auto futilityValue = safeValue + _see.computeExchangeValue(position, move, threshold);
			if (futilityValue < alpha) {
				bestValue = std::max(futilityValue, bestValue);
				continue;
			}
		}

		// 9. Recursive quiescence search
		// We store the state we do not want to recompute on undoMove. This is a performance optimization.
		const PositionSnapshot snapshot = position.getSnapshot();
		position.doMove(move);
		_tt->prefetch(position.computeBoardHash());

#ifdef USE_STOCKFISH_EVAL
		Stockfish::Engine::doMove(move, si);
#endif
		auto valueOfNextPlySearch = -search(isPvNode, position, computingInfo, move, -beta, -alpha, ply + 1);
		position.undoMove(move, snapshot);
#ifdef USE_STOCKFISH_EVAL
		Stockfish::Engine::undoMove(move);
#endif

		// 10. Use the search - value to update the search window, including beta-cutoff.
		if (valueOfNextPlySearch > bestValue) {
			bestValue = valueOfNextPlySearch;
			if (bestValue >= beta) {
				break;
			}
			alpha = std::max(alpha, bestValue);
		}
	}

	WhatIf::whatIf.moveSearched(position, computingInfo, lastMove, alpha, beta, bestValue, standPatValue, ply);
	return bestValue;
}


