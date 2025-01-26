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

#include "quiescence.h"
#include "../eval/eval.h"
#include "searchparameter.h"
#include "moveprovider.h"
#include "whatif.h"
#include "see.h"


using namespace QaplaInterface;
using namespace ChessEval;
using namespace QaplaSearch;

TT* Quiescence::_tt;


/**
 * Computes the maximal value a capture move can gain + safety margin
 * If this value is not enough to make it a valuable move, the move is skipped
 */
value_t Quiescence::computePruneForewardValue(MoveGenerator& position, value_t standPatValue, Move move) {
	value_t result = MAX_VALUE;
	// A winning bonus can be fully destroyed by capturing the piece
	bool hasWinningBonus = standPatValue < -WINNING_BONUS || standPatValue > WINNING_BONUS;
	if (hasWinningBonus) return result;
	if (move.isPromote()) return result;

	Piece capturedPiece = move.getCapture();
	if (position.doFutilityOnCapture(capturedPiece)) {
		value_t maxGain = position.getAbsolutePieceValue(capturedPiece);
		result = standPatValue + SearchParameter::PRUING_SAFETY_MARGIN_IN_CP + maxGain;
	}
	return result;
}

/**
 * Gets an entry from the transposition table
 * @returns hash value and hash move or -MAX_VALUE, if no value found
 */
std::tuple<value_t, Move> Quiescence::probeTT(MoveGenerator& position, value_t alpha, value_t beta, ply_t ply) {
	bool cutoff = false;
	uint32_t ttIndex = _tt->getTTEntryIndex(position.computeBoardHash());

	if (ttIndex != TT::INVALID_INDEX) {
		TTEntry entry = _tt->getEntry(ttIndex);
		const auto bestValue = entry.getValue(alpha, beta, 0, ply);
		const auto move = entry.getMove();
		return std::make_tuple(bestValue, move);
	}
	return std::make_tuple(NO_VALUE, Move::EMPTY_MOVE);
}


/**
 * Performs the quiescense search
 */
value_t Quiescence::search(
	MoveGenerator& position, ComputingInfo& computingInfo, Move lastMove,
	value_t alpha, value_t beta, ply_t ply)
{
	if (ply >= SearchParameter::MAX_SEARCH_DEPTH) {
		return position.isInCheck() ? DRAW_VALUE : Eval::eval(position, ply);
	}
	if (alpha > MAX_VALUE - ply) {
		return MAX_VALUE - ply;
	}	
	if (beta < -MAX_VALUE + ply) {
		return -MAX_VALUE + ply;
	}
#ifdef USE_STOCKFISH_EVAL
	Stockfish::StateInfo si;
#endif
	MoveProvider moveProvider;
	Move move;
	computingInfo._nodesSearched++;
	WhatIf::whatIf.moveSelected(position, computingInfo, lastMove, ply, true);
	if (SearchParameter::USE_HASH_IN_QUIESCENSE) {
		auto [ttValue, ttMove] = probeTT(position, alpha, beta, ply);
		moveProvider.setTTMove(ttMove);
		if (ttValue != NO_VALUE) return ttValue;
	}
	//if (position.computeBoardHash() == -6935370772522267216ULL) {
		// position.print();
		// std::cout << Stockfish::Engine::trace();
	//}
	const auto evadesCheck = SearchParameter::EVADES_CHECK_IN_QUIESCENSE && position.isInCheck();
	value_t bestValue, standPatValue;
	bestValue = standPatValue = evadesCheck ? -MAX_VALUE + ply : Eval::eval(position, ply, alpha);

	// Eval::assertSymetry(position, standPatValue);
	value_t valueOfNextPlySearch;

	if (standPatValue < beta) {
		if (standPatValue > alpha) {
			alpha = standPatValue;
		}
		if (evadesCheck) {
			moveProvider.computeEvades(position, lastMove);
		} else {
			moveProvider.computeCaptures(position, lastMove);
		}
		while (!(move = moveProvider.selectNextCaptureOrEvade(position, evadesCheck)).isEmpty()) {
			valueOfNextPlySearch = computePruneForewardValue(position, standPatValue, move);
			if (valueOfNextPlySearch < alpha) {
				if (valueOfNextPlySearch > bestValue) {
					bestValue = valueOfNextPlySearch;
				}
				break;
			}
			if (SearchParameter::QUIESCENSE_USE_SEE_PRUNINT && SEE::isLoosingCaptureLight(position, move)) {
				continue;
			}

			BoardState positionState = position.getBoardState();
			position.doMove(move);
#ifdef USE_STOCKFISH_EVAL
			Stockfish::Engine::doMove(move, si);
#endif
			valueOfNextPlySearch = -search(position, computingInfo, move, -beta, -alpha, ply + 1);
			position.undoMove(move, positionState);
#ifdef USE_STOCKFISH_EVAL
			Stockfish::Engine::undoMove(move);
#endif

			if (valueOfNextPlySearch > bestValue) {
				bestValue = valueOfNextPlySearch;
				if (bestValue >= beta) {
					break;
				}
				if (bestValue > alpha) {
					alpha = bestValue;
				}
			}
		}
	}

	WhatIf::whatIf.moveSearched(position, computingInfo, lastMove, alpha, beta, bestValue, standPatValue, ply);
	return bestValue;
}


