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
 * @Overview
 * Implements the quiescense search algorithm for the chess engine
 * Quiescense search searches captures and some checking moves until 
 * a quiet position is reached.
 */

#ifndef __QUIESCENCESEARCH_H
#define __QUIESCENCESEARCH_H

#include "../eval/eval.h"
#include "searchparameter.h"
#include "moveprovider.h"
#include "../movegenerator/movegenerator.h"
#include "computinginfo.h"
#include "whatif.h"
#include "see.h"

using namespace ChessInterface;
using namespace ChessEval;

namespace ChessSearch {

	class QuiescenceSearch {

	private:
		QuiescenceSearch() {}

	public:

		/**
		 * Performs the quiescense search or evade search, if in check
		 */
		static value_t search(
			MoveGenerator& board, ComputingInfo& computingInfo, Move lastMove, 
			value_t alpha, value_t beta, ply_t ply) 
		{
			value_t result;
			BoardState boardState = board.getBoardState();

			board.doMove(lastMove);

			if (board.isInCheck())
			{
				result = searchEvades(board, computingInfo, lastMove, alpha, beta, ply);
			}
			else {
				result = quiescenseSearch(board, computingInfo, lastMove, alpha, beta, ply);
			}

			board.undoMove(lastMove, boardState);
			return result;
		}

		static void setTT(TT* tt) { _tt = tt; }

	private:

		/**
		 * Computes the maximal value a capture move can gain + safety margin
		 * If this value is not enough to make it a valuable move, the move is skipped
		 */
		static value_t computePruneForewardValue(MoveGenerator& board, value_t standPatValue, Move move) {
			value_t result = MAX_VALUE;
			// A winning bonus can be fully destroyed by capturing the piece
			bool hasWinningBonus = standPatValue < -WINNING_BONUS || standPatValue > WINNING_BONUS;
			if (hasWinningBonus) return result;
			if (move.isPromote()) return result;

			Piece capturedPiece = move.getCapture();
			if (board.doFutilityOnCapture(capturedPiece)) {
				value_t maxGain = board.getAbsolutePieceValue(capturedPiece);
				result = standPatValue + SearchParameter::PRUING_SAFETY_MARGIN_IN_CP + maxGain;
			}
			return result;
		}

		/**
		 * Gets an entry from the transposition table
		 * @returns hash value or -MAX_VALUE, if no value found
		 */
		static value_t probeTT(MoveGenerator& board, value_t alpha, value_t beta, ply_t ply) {
			bool cutoff = false;
			value_t bestValue = -MAX_VALUE;
			uint32_t ttIndex = _tt->getTTEntryIndex(board.computeBoardHash());

			if (ttIndex != TT::INVALID_INDEX) {
				TTEntry entry = _tt->getEntry(ttIndex);
				static uint32_t amount = 0;
				amount++;
				if (amount % 100000 == 0) { cout << "Hits: " << amount << " fill: " << _tt->getHashFillRateInPercent() << endl; }
				entry.getValue(bestValue, alpha, beta, 0, ply);
			}
			else {
				static uint32_t amount = 0;
				amount++;
				if (amount % 1000000 == 0) { cout << "Non-Hit: " << amount << endl; }
			}
			return bestValue;
		}

		/**
		 * Performs a full one-ply search for a check position (only evades are possible)
		 */
		static value_t searchEvades(
			MoveGenerator& board, ComputingInfo& computingInfo, Move lastMove, 
			value_t alpha, value_t beta, ply_t ply) 
		{
			value_t valueOfNextPlySearch;
			Move move;
			MoveProvider moveProvider;
			computingInfo._nodesSearched++;
			WhatIf::whatIf.moveSelected(board, computingInfo, lastMove, ply, true);

			moveProvider.computeEvades(board, lastMove);

			value_t bestValue = moveProvider.checkForGameEnd(board, ply);

			while (!(move = moveProvider.selectNextMove(board)).isEmpty()) {

				BoardState boardState = board.getBoardState();
				board.doMove(move);
				valueOfNextPlySearch = -quiescenseSearch(board, computingInfo, move, -beta, -alpha, ply + 1);
				board.undoMove(move, boardState);

				if (valueOfNextPlySearch > bestValue) {
					bestValue = valueOfNextPlySearch;
					if (bestValue > alpha) {
						alpha = bestValue;
						if (bestValue >= beta) {
							break;
						}

					}
				}
			}
			WhatIf::whatIf.moveSearched(board, computingInfo, lastMove, alpha, beta, bestValue, ply);
			return bestValue;
		}

		/**
		 * Performs the quiescense search
		 */
		static value_t quiescenseSearch(
			MoveGenerator& board, ComputingInfo& computingInfo, Move lastMove, 
			value_t alpha, value_t beta, ply_t ply)
		{

			MoveProvider moveProvider;
			Move move;
			computingInfo._nodesSearched++;
			WhatIf::whatIf.moveSelected(board, computingInfo, lastMove, ply, true);

			value_t standPatValue = Eval::evaluateBoardPosition(board, alpha);
			// Eval::assertSymetry(board, standPatValue);
			value_t bestValue;
			value_t valueOfNextPlySearch;

			if (!board.isWhiteToMove()) {
				standPatValue = -standPatValue;
			}

			bestValue = standPatValue;
			if (standPatValue < beta) {
				if (standPatValue > alpha) {
					alpha = standPatValue;
				}
				moveProvider.computeCaptures(board, lastMove);
				while (!(move = moveProvider.selectNextCapture(board)).isEmpty()) {
					valueOfNextPlySearch = computePruneForewardValue(board, standPatValue, move);
					if (valueOfNextPlySearch < alpha) {
						if (valueOfNextPlySearch > bestValue) {
							bestValue = valueOfNextPlySearch;
						}
						break;
					}
					if (SearchParameter::QUIESCENSE_USE_SEE_PRUNINT && SEE::isLoosingCaptureLight(board, move)) {
						continue; 
					}

					BoardState boardState = board.getBoardState();
					board.doMove(move);
					valueOfNextPlySearch = -quiescenseSearch(board, computingInfo, move, -beta, -alpha, ply + 1);
					board.undoMove(move, boardState);

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

			WhatIf::whatIf.moveSearched(board, computingInfo, lastMove, alpha, beta, bestValue, ply);
			return bestValue;
		}

		static TT* _tt;

	};

}

#endif // __QUIESCENCESEARCH_H