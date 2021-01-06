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
#include "Whatif.h"
// #include "razoring.h"

using namespace std;
using namespace ChessMoveGenerator;
using namespace ChessInterface;

namespace ChessSearch {

	class Search {
	public:
		Search() {}

		value_t searchRec(MoveGenerator& board, SearchStack& stack, ChessSearch::ComputingInfo& computingInfo, ClockManager& clockManager) {
			negaMax(board, stack, computingInfo, clockManager, Move::EMPTY_MOVE, 0);
		}

	private:


		void updateThinkingInfo(SearchStack& stack) {
			if (ply <= 0) {
				computingInfoPtr->currentMoveNoSearched++;
				computingInfoPtr->positionValueInCentiPawn = stack[0].bestValue;
				computingInfoPtr->updatePV(stack[0].pvMovesStore);
			}
		}

		void searchLastPlys(MoveGenerator& board, SearchVariables& currentPly, SearchStack& stack, Move currentMove) {
			value_t searchResult;
			if (currentPly.remainingDepth <= 0) {
				stack[ply + 1].previousMove = currentMove;
				searchResult = -QuiescenceSearch::search(board, eval, *computingInfoPtr, currentMove, -currentPly.beta, -currentPly.alpha, ply + 1);
			}
			else {
				searchResult = -negaMaxLastPlys(board, stack, currentMove, ply + 1);
			}
			currentPly.setSearchResult(searchResult, stack[ply + 1], currentMove);
			
			// WhatIf::whatIf.moveSearched(board, *computingInfoPtr, stack, currentMove, ply);
		}

		void handleCutoffs(MoveGenerator& board, SearchStack& stack, SearchVariables& curPly, ply_t ply) {
			SearchVariables::cutoff_t cutoff = stack.isDrawByRepetitionInSearchTree(board, ply) ? SearchVariables::CUTOFF_DRAW_BY_REPETITION : SearchVariables::CUTOFF_NONE;
			curPly.cutoffSearch(cutoff, 0);
			
			// WHATIF(if (cutoff == SearchVariables::CUTOFF_DRAW_BY_REPETITION) { WhatIf::whatIf.cutoff(board, *computingInfoPtr, stack, ply, "RPET"); })
			curPly.getTTEntry(board);
			// WHATIF(if (curPly.cutoff == SearchVariables::CUTOFF_HASH) { WhatIf::whatIf.cutoff(board, *computingInfoPtr, stack, ply, "HASH"); })
			if (!curPly.cutoff) {
				//cutoff = Razoring::razoring(board, eval, *computingInfoPtr, curPly) ? SearchVariables::CUTOFF_RAZORING : SearchVariables::CUTOFF_NONE;
				//curPly.cutoffSearch(cutoff);
			}
		}

		/**
		 * Negamax algorithm for the last plies of the search
		 */
		value_t negaMaxLastPlys(MoveGenerator& board, SearchStack& stack, Move previousPlyMove, ply_t ply)
		{
			SearchVariables& currentPly = stack[ply];
			value_t searchResult;
			Move currentMove;

			computingInfoPtr->nodesSearched++;
			currentPly.setFromPreviousPly(board, stack[ply - 1], previousPlyMove);
			// WHATIF(WhatIf::whatIf.moveSelected(board, *computingInfoPtr, stack, previousPlyMove, ply);)

			handleCutoffs(board, stack, currentPly, ply);

			if (currentPly.cutoff == SearchVariables::CUTOFF_NONE) {

				currentPly.generateMoves(board);
				currentPly.extendSearch(board);

				while (!(currentMove = currentPly.selectNextMove(board)).isEmpty()) {
					//printf("%ld %lld %s %s\n", ply, computingInfo.nodesSearched, Move::getLAN(move).getCharBuffer(), searchInfo.inPV ? "in PV": "");
					
					if (currentPly.remainingDepth > 0) {
						searchResult = -negaMaxLastPlys(board, stack, currentMove, ply + 1);
					}
					else {
						stack[ply + 1].previousMove = currentMove;
						searchResult = -QuiescenceSearch::search(board, eval, *computingInfoPtr, 
							currentMove, -currentPly.beta, -currentPly.alpha, ply + 1);
					}

					currentPly.setSearchResult(searchResult, stack[ply + 1], currentMove);
					// WHATIF(WhatIf::whatIf.moveSearched(board, *computingInfoPtr, stack, currentMove, ply);)

				}
			}

			currentPly.terminatePly(board);
			return currentPly.bestValue;

		}

		/**
		 * Check, if it is reasonable to do a nullmove search
		 */
		bool isNullmoveReasonable(MoveGenerator& board, bool isLastMoveNullmove, SearchVariables& currentPly) {
			bool result = true;

			if (currentPly.remainingDepth <= 1) {
				result = false;
			}
			else if (currentPly.noNullmove) {
				result = false;
			}
			else if (!board.sideToMoveHasQueenRookBishop(board.isWhiteToMove())) {
				result = false;
			}
			else if (board.isInCheck()) {
				result = false;
			}
			else if (currentPly.beta >= MAX_VALUE - value_t(ply)) {
				result = false;
			}
			else if (currentPly.beta <= -MAX_VALUE + value_t(ply)) {
				result = false;
			}
			else if (board.getMaterialValue(board.isWhiteToMove()) + MaterialBalance::PAWN_VALUE < currentPly.beta) {
				result = false;
			}
			else if (currentPly.isTTValueBelowBeta(board)) {
				result = false;
			}
			else if (ply + currentPly.remainingDepth < 3) {
				result = false;
			}
			else if (isLastMoveNullmove) {
				result = false;
			}
			else if (currentPly.isInPV()) {
				result = false;
			}

			return result;
		}

		/**
		 * Do a full search using the negaMax algorithm
		 */
		value_t negaMax(MoveGenerator& board, SearchStack& stack, ComputingInfo& computingInfo, 
			ClockManager& clockManager, Move previousPlyMove, uint32_t ply) {

			SearchVariables& searchInfo = stack[ply];
			value_t searchResult;
			Move currentMove;

			if (ply > 0) {
				searchInfo.setFromPreviousPly(board, stack[ply - 1], previousPlyMove);
				computingInfo.nodesSearched++;
			}
			searchInfo.generateMoves(board);

			if (ply == 0) {
				computingInfo.totalAmountOfMovesToConcider = stack[0].moveProvider.getTotalMoveAmount();
				computingInfo.currentConcideredMove.setEmpty();
			}

			while (!(currentMove = searchInfo.selectNextMove(board)).isEmpty()) {

				if (ply == 0) {
					computingInfo.currentConcideredMove = currentMove;
				}

				if (searchInfo.remainingDepth > 2) {
					searchResult = -negaMax(board, stack, computingInfo, clockManager, currentMove, ply + 1);
				}
				else if (searchInfo.remainingDepth > 0) {
					searchResult = -negaMaxLastPlys(board, stack, currentMove, ply + 1);
				}
				else {
					searchResult = -QuiescenceSearch::search(board, eval, computingInfo, currentMove, -searchInfo.beta, -searchInfo.alpha, ply + 1);
				}

				if (clockManager.mustAbortCalculation(ply)) {
					break;
				}

				searchInfo.setSearchResult(searchResult, stack[ply + 1], currentMove);

				if (ply == 0) {
					computingInfo.currentMoveNoSearched++;
					computingInfo.positionValueInCentiPawn = stack[0].bestValue;
					computingInfo.updatePV(stack[0].pvMovesStore);
				}

			}

			searchInfo.terminatePly(board);
			computingInfo.printSearchInfoIfRequested();
			if (ply > 0) {
				searchInfo.undoMove(board);
			}
			return searchInfo.bestValue;

		}

		ply_t ply;
		Eval eval;
		ComputingInfo* computingInfoPtr;
	};
}



#endif // __SEARCH_H