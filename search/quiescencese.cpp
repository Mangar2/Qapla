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

/**
 * Computes the value a capture move can gain + safety margin
 * If this value is not enough to make it a valuable move, the move is skipped
 * The gain is the exchange value of the capture. It falls back to the value of the captured
 * piece on its own, that is the upper bound the exchange starts from.
 */
value_t Quiescence::computePruneForewardValue(MoveGenerator& position, value_t standPatValue, value_t alpha, Move move) {
	// A winning bonus can be fully destroyed by capturing the piece so don´t prune on winning bonus eval.
	if (standPatValue < -WINNING_BONUS || standPatValue > WINNING_BONUS) {
		return MAX_VALUE;
	}
	// Only queen promotions arrive here, the generator files sub promotions as silent moves.
	if (move.isPromote()) {
		return MAX_VALUE;
	}

	Piece capturedPiece = move.getCapture();

	// Tested 0.5.0-002, removing it: SPRT h0 = -2, h1 = 3, H0 accepted.
	if (!position.doFutilityOnCapture(capturedPiece)) {
		return MAX_VALUE;
	}
	// Both terms must use the same margin, else a tuning run moves two halves against each other.
	// Tested 0.5.0-014, margin 35 instead of 50: SPRT h0 = -2, h1 = 3, H0 accepted.
	// Tested 0.5.0-009, -010, -015, -017, an eval dependent addition to this margin:
	// SPRT h0 = -2, h1 = 3, H0 accepted. 0.5.0-012 and -013 undecided.
	const value_t margin = tunable<SearchConfig::optimizeQS, "qsAlphaSafetyMargin", 50, 0, 100>();
	const value_t threshold = alpha - standPatValue - margin;
	return standPatValue + margin + _see.computeExchangeValue(position, move, threshold);
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

	// 5. Generate all moves (evades or captures) 
	// We set the ttMove we found before. It will then select the ttMove first.
	MoveProvider moveProvider;
	Move move;
	moveProvider.computeCaptures(position, lastMove, ttMove);

	// 6. Move Loop
	while (!(move = moveProvider.selectNextCapture()).isEmpty()) {

		// 7. Move foreward pruning. 
		// We compute a possible gain of the move (already including margin) and prune this move search,
		// if it does not reach alpha.
		auto valueOfNextPlySearch = computePruneForewardValue(position, standPatValue, alpha, move);
		if (valueOfNextPlySearch < alpha) {
			bestValue = std::max(valueOfNextPlySearch, bestValue);
			continue;
		}

		// 8. Recursive quiescence search
		// We store the state we do not want to recompute on undoMove. This is a performance optimization.
		const PositionSnapshot snapshot = position.getSnapshot();
		position.doMove(move);
		_tt->prefetch(position.computeBoardHash());

#ifdef USE_STOCKFISH_EVAL
		Stockfish::Engine::doMove(move, si);
#endif
		valueOfNextPlySearch = -search(isPvNode, position, computingInfo, move, -beta, -alpha, ply + 1);
		position.undoMove(move, snapshot);
#ifdef USE_STOCKFISH_EVAL
		Stockfish::Engine::undoMove(move);
#endif

		// 9. Use the search - value to update the search window, including beta-cutoff.
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


