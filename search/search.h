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
 * Implements the main search functions (recursive search)
 */

#ifndef __SEARCH_H
#define __SEARCH_H

#include <thread>
#include "../movegenerator/movegenerator.h"
#include "computinginfo.h"
#include "clockmanager.h"
#include "SearchStack.h"
#include "../eval/eval.h"
#include "QuiescenceSearch.h"
#include "ClockManager.h"
#include "tt.h"
#include "whatif.h"
// #include "razoring.h"

using namespace std;
using namespace ChessMoveGenerator;
using namespace ChessInterface;

namespace ChessSearch {

	class Search {
	public:
		Search() {}

		template <bool WHATIF>
		value_t searchRec(MoveGenerator& board, SearchStack& stack, ComputingInfo& computingInfo, ClockManager& clockManager) {
			computingInfoPtr = &computingInfo;
			board.computeAttackMasksForBothColors();
			return negaMax<WHATIF>(board, stack, clockManager, Move::EMPTY_MOVE, 0);
		}

	private:


		void updateThinkingInfoPly0(SearchStack& stack) {
			computingInfoPtr->currentMoveNoSearched++;
			computingInfoPtr->positionValueInCentiPawn = stack[0].bestValue;
			computingInfoPtr->updatePV(stack[0].pvMovesStore);
		}

		/**
		 * Search the last plies - either negamax or quiescense
		 */
		template <bool WHATIF>
		value_t searchLastPlys(MoveGenerator& board, SearchVariables& searchInfo, SearchStack& stack, Move curMove, ply_t ply) {
			value_t searchResult;
			if (searchInfo.remainingDepth <= 0) {
				searchResult = -QuiescenceSearch::search(board, eval, *computingInfoPtr, curMove, 
					-searchInfo.beta, -searchInfo.alpha, ply + 1);
			}
			else {
				searchResult = -negaMaxLastPlys<WHATIF>(board, stack, curMove, ply + 1);
			}
			return searchResult;
		}

		/**
		 * Check for cutoffs 
		 */
		template <bool WHATIF>
		bool hasCutoff(MoveGenerator& board, SearchStack& stack, SearchVariables& curPly, ply_t ply) {
			if (curPly.alpha > MAX_VALUE - value_t(ply)) {
				curPly.setCutoff(Cutoff::FASTER_MATE_FOUND, curPly.alpha);
			}
			else if (curPly.beta < -MAX_VALUE + value_t(ply)) {
				curPly.setCutoff(Cutoff::FASTER_MATE_FOUND, curPly.beta);
			}
			else if (ply >= 3 && board.drawDueToMissingMaterial()) {
				curPly.setCutoff(Cutoff::NOT_ENOUGH_MATERIAL, 0);
			}
			else if (stack.isDrawByRepetitionInSearchTree(board, ply)) {
				curPly.setCutoff(Cutoff::DRAW_BY_REPETITION, 0);
			}
			else if (curPly.probeTT(board)) {
				curPly.setCutoff(Cutoff::HASH);
			}
			if (WHATIF && curPly.cutoff != Cutoff::NONE) { WhatIf::whatIf.cutoff(board, *computingInfoPtr, stack, ply, getCutoffString(curPly.cutoff)); }
			return curPly.cutoff != Cutoff::NONE;
		}

		/**
		 * Negamax algorithm for the last plies of the search
		 */
		template <bool WHATIF>
		value_t negaMaxLastPlys(MoveGenerator& board, SearchStack& stack, Move previousPlyMove, ply_t ply)
		{
			SearchVariables& searchInfo = stack[ply];
			value_t searchResult;
			Move curMove;

			computingInfoPtr->nodesSearched++;
			searchInfo.setFromPreviousPly(board, stack[ply - 1], previousPlyMove);
			if (WHATIF) { WhatIf::whatIf.moveSelected(board, *computingInfoPtr, stack, previousPlyMove, ply); }

			if (!hasCutoff<WHATIF>(board, stack, searchInfo, ply)) {

				searchInfo.generateMoves(board);
				searchInfo.extendSearch(board);

				while (!(curMove = searchInfo.selectNextMove(board)).isEmpty()) {
					//printf("%ld %lld %s %s\n", ply, computingInfo.nodesSearched, Move::getLAN(move).getCharBuffer(), searchInfo.inPV ? "in PV": "");
					
					if (searchInfo.remainingDepth > 0) {
						searchResult = -negaMaxLastPlys<WHATIF>(board, stack, curMove, ply + 1);
					}
					else {
						stack[ply + 1].previousMove = curMove;
						searchResult = -QuiescenceSearch::search(board, eval, *computingInfoPtr, 
							curMove, -searchInfo.beta, -searchInfo.alpha, ply + 1);
					}

					searchInfo.setSearchResult(searchResult, stack[ply + 1], curMove);
					if (WHATIF) { WhatIf::whatIf.moveSearched(board, *computingInfoPtr, stack, curMove, ply); }

				}
			}

			searchInfo.terminatePly(board);
			return searchInfo.bestValue;

		}

		/**
		 * Check, if it is reasonable to do a nullmove search
		 */
		bool isNullmoveReasonable(MoveGenerator& board, SearchVariables& searchInfo, ply_t ply) {
			bool result = true;
			if (!SearchParameter::DO_NULLMOVE) {
				result = false;
			} 
			else if (searchInfo.remainingDepth <= 1) {
				result = false;
			}
			else if (searchInfo.noNullmove) {
				result = false;
			}
			else if (!board.sideToMoveHasQueenRookBishop(board.isWhiteToMove())) {
				result = false;
			}
			else if (board.isInCheck()) {
				result = false;
			}
			else if (searchInfo.beta >= MAX_VALUE - value_t(ply)) {
				result = false;
			}
			else if (searchInfo.beta <= -MAX_VALUE + value_t(ply)) {
				result = false;
			}
			else if (board.getMaterialValue(board.isWhiteToMove()) + MaterialBalance::PAWN_VALUE < searchInfo.beta) {
				result = false;
			}
			else if (searchInfo.isTTValueBelowBeta(board)) {
				result = false;
			}
			else if (ply + searchInfo.remainingDepth < 3) {
				result = false;
			}
			else if (searchInfo.isInPV()) {
				result = false;
			}

			return result;
		}

		/** 
		 * Do a nullmove search
		 */
		template <bool WHATIF>
		void nullmove(MoveGenerator& board, SearchStack& stack, ClockManager& clockManager, uint32_t ply) 
		{
			SearchVariables& searchInfo = stack[ply];
			searchInfo.setNullmove();
			Move curMove(Move::NULL_MOVE);
			value_t searchResult;

			if (searchInfo.remainingDepth > 2) {
				searchResult = -negaMax<WHATIF>(board, stack, clockManager, curMove, ply + 1);
			}
			else {
				searchResult = searchLastPlys<WHATIF>(board, searchInfo, stack, curMove, ply);
			}
			if (WHATIF) { WhatIf::whatIf.moveSearched(board, *computingInfoPtr, stack, curMove, ply); }

			if (searchResult >= searchInfo.beta) {
				searchInfo.setCutoff(Cutoff::NULL_MOVE);
			}
			else {
				searchInfo.unsetNullmove();
			}
		}

		/**
		 * Do a full search using the negaMax algorithm
		 */
		template <bool WHATIF>
		value_t negaMax(MoveGenerator& board, SearchStack& stack, 
			ClockManager& clockManager, Move previousPlyMove, uint32_t ply) {

			SearchVariables& searchInfo = stack[ply];
			value_t searchResult;
			bool cutoff = false;
			Move curMove;

			if (ply > 0) {
				searchInfo.setFromPreviousPly(board, stack[ply - 1], previousPlyMove);
				computingInfoPtr->nodesSearched++;
				cutoff = hasCutoff<WHATIF>(board, stack, searchInfo, ply);
			}
			if (WHATIF) { WhatIf::whatIf.moveSelected(board, *computingInfoPtr, stack, previousPlyMove, ply); }

			// Nullmove
			if (isNullmoveReasonable(board, searchInfo, ply)) {
				nullmove<WHATIF>(board, stack, clockManager, ply);
			}

			if (!cutoff) {
				searchInfo.generateMoves(board);
				searchInfo.extendSearch(board);

				if (ply == 0) {
					computingInfoPtr->totalAmountOfMovesToConcider = stack[0].moveProvider.getTotalMoveAmount();
					computingInfoPtr->currentConcideredMove.setEmpty();
				}

				while (!(curMove = searchInfo.selectNextMove(board)).isEmpty()) {

					if (ply == 0) { computingInfoPtr->currentConcideredMove = curMove; }

					if (searchInfo.remainingDepth > 2) {
						searchResult = -negaMax<WHATIF>(board, stack, clockManager, curMove, ply + 1);
					}
					else {
						searchResult = searchLastPlys<WHATIF>(board, searchInfo, stack, curMove, ply);
					}

					if (stack[0].remainingDepth > 1 && clockManager.mustAbortCalculation(ply)) {
						break;
					}

					searchInfo.setSearchResult(searchResult, stack[ply + 1], curMove);
					
					if (WHATIF) { WhatIf::whatIf.moveSearched(board, *computingInfoPtr, stack, curMove, ply); }
					if (ply == 0) { updateThinkingInfoPly0(stack); }

				}
			}

			searchInfo.terminatePly(board);
			computingInfoPtr->printSearchInfoIfRequested();
			return searchInfo.bestValue;

		}

		Eval eval;
		ComputingInfo* computingInfoPtr;
	};
}



#endif // __SEARCH_H