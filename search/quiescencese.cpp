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


using namespace ChessInterface;
using namespace ChessEval;
using namespace ChessSearch;

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
 * @returns hash value or -MAX_VALUE, if no value found
 */
value_t Quiescence::probeTT(MoveGenerator& position, value_t alpha, value_t beta, ply_t ply) {
	bool cutoff = false;
	value_t bestValue = NO_VALUE;
	uint32_t ttIndex = _tt->getTTEntryIndex(position.computeBoardHash());
	static uint64_t count, found, good = 0;
	count++;

	if (ttIndex != TT::INVALID_INDEX) {
		found++;
		TTEntry entry = _tt->getEntry(ttIndex);
		bestValue = entry.getValue(alpha, beta, 0, ply);
		if (bestValue != NO_VALUE) good++;
	}
	// if (count % 10000 == 0) { cout << "count: " << count << " found: " << found << " good: " << good << endl; }
	return bestValue;
}

/**
 * Performs the quiescense search
 */
value_t Quiescence::search(
	MoveGenerator& position, ComputingInfo& computingInfo, Move lastMove,
	value_t alpha, value_t beta, ply_t ply)
{

	MoveProvider moveProvider;
	Move move;
	computingInfo._nodesSearched++;
	WhatIf::whatIf.moveSelected(position, computingInfo, lastMove, ply, true);
	if (SearchParameter::USE_HASH_IN_QUIESCENSE) {
		value_t ttValue = probeTT(position, alpha, beta, ply);
		if (ttValue != NO_VALUE) return ttValue;
	}
	value_t standPatValue = Eval::eval(position, alpha);
	// Eval::assertSymetry(position, standPatValue);
	value_t bestValue;
	value_t valueOfNextPlySearch;

	bestValue = standPatValue;
	if (standPatValue < beta) {
		if (standPatValue > alpha) {
			alpha = standPatValue;
		}
		moveProvider.computeCaptures(position, lastMove);
		while (!(move = moveProvider.selectNextCapture(position)).isEmpty()) {
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
			valueOfNextPlySearch = -search(position, computingInfo, move, -beta, -alpha, ply + 1);
			position.undoMove(move, positionState);

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

	WhatIf::whatIf.moveSearched(position, computingInfo, lastMove, alpha, beta, bestValue, ply);
	return bestValue;
}


