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
	// Promotion is only Queen promotion, sub-promotes are not in the quiescence search.
	if (move.isPromote()) {
		return MAX_VALUE;
	}

	Piece capturedPiece = move.getCapture();

	// Tested 0.5.0-002: Removing it costs 7 elo, H0 accepted over 6532 games at 48.97 %.
	if (!position.doFutilityOnCapture(capturedPiece)) {
		return MAX_VALUE;
	}
	auto value = position.getMaterialValue().getValue(50);
	auto evalMargin = ((position.isWhiteToMove() ? value : -value)- standPatValue) * 30 / 100;
	evalMargin = std::max(0, evalMargin - 100);

	const value_t margin = tunable<SearchConfig::optimizeQS, "qsAlphaSafetyMargin", 50, 0, 100>();
	const value_t threshold = alpha - standPatValue - margin - evalMargin;
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
	// Tested 0.5.0-004: removing the two checks costs 3 elo, H0 accepted over 12540 games at
	// 49.50 %. 
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
	
	// Tested 0.5.0-007: guarding this cutoff and the one in SearchNode::probeTT with
	// std::abs(value) < MIN_MATE_VALUE costs 10 elo at 10+0.01, H0 accepted over 6272 games at
	// 48.57 %. 
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

	// Tested 0.5.0-005: node level delta pruning, H0 accepted at -7 elo over 6399 games, 48.94 %.
	// A queen plus 200 is not a safe bound for this eval: a capture also moves the positional
	// terms, and a capturing promotion gains more than a queen on its own. The item asked to
	// CLOP the margin only if the SPRT proved H1, so it ends here.
	// if (standPatValue > -WINNING_BONUS && standPatValue < WINNING_BONUS) {
	//	const value_t maxGain = position.getPieceValueForMoveSorting(WHITE_QUEEN)
	//		+ tunable<SearchConfig::optimizeQS, "qsDeltaMargin", 200, -200, 600>();
	//	if (standPatValue + maxGain < alpha) {
	//		return standPatValue + maxGain;
	//	}
	// }

	// Eval::assertSymetry(position, standPatValue);

	// 4. Correct alpha to be at least stand-pat value.
	alpha = std::max(alpha, standPatValue);

	// 5. Generate all moves (evades or captures) 
	// We set the ttMove we found before. It will then select the ttMove first.
	MoveProvider moveProvider;
	Move move;
	moveProvider.setTTMove(ttMove);
	moveProvider.computeCaptures(position, lastMove);

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


