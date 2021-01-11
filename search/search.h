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

		value_t searchRec(MoveGenerator& board, SearchStack& stack, ComputingInfo& computingInfo, ClockManager& clockManager);

	private:

		/**
		 * Update status information on ply 0
		 */
		void updateThinkingInfoPly0(SearchStack& stack) {
			_computingInfo->currentMoveNoSearched++;
			_computingInfo->positionValueInCentiPawn = stack[0].bestValue;
			_computingInfo->updatePV(stack[0].pvMovesStore);
		}

		/**
		 * Check for cutoffs
		 */
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
			else if (isNullmoveCutoff(board, stack, ply)) {
				curPly.setCutoff(Cutoff::NULL_MOVE);
			}
			WhatIf::whatIf.cutoff(board, *_computingInfo, stack, ply, curPly.cutoff);
			return curPly.cutoff != Cutoff::NONE;
		}


		/**
		 * Check, if it is reasonable to do a nullmove search
		 */
		bool isNullmoveReasonable(MoveGenerator& board, SearchVariables& searchInfo, ply_t ply) {
			bool result = true;
			if (!SearchParameter::DO_NULLMOVE) {
				result = false;
			}
			else if (searchInfo.remainingDepth <= 2) {
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
		 * Check for a nullmove cutoff
		 */
		bool isNullmoveCutoff(MoveGenerator& board, SearchStack& stack, uint32_t ply)
		{
			SearchVariables& searchInfo = stack[ply];
			if (!isNullmoveReasonable(board, searchInfo, ply)) {
				return false;
			}
			assert(!board.isInCheck());
			searchInfo.setNullmove();
			Move curMove(Move::NULL_MOVE);

			if (searchInfo.remainingDepth > 2) {
				searchInfo.bestValue = -negaMax(board, stack, curMove, ply + 1);
			}
			else {
				searchInfo.bestValue = searchLastPlys(board, searchInfo, stack, curMove, ply + 1);
			}
			WhatIf::whatIf.moveSearched(board, *_computingInfo, stack, curMove, ply);
			searchInfo.unsetNullmove();
			const bool isCutoff = searchInfo.bestValue >= searchInfo.beta;
			if (!isCutoff) {
				board.computeAttackMasksForBothColors();
			}
			// searchInfo.remainingDepth -= SearchParameter::getNullmoveVerificationDepthReduction(ply, searchInfo.remainingDepth);
			return isCutoff;
		}


		/**
		 * Search the last plies - either negamax or quiescense
		 */
		value_t searchLastPlys(MoveGenerator& board, SearchVariables& searchInfo, SearchStack& stack, Move curMove, ply_t ply);

		/**
		 * Negamax algorithm for the last plies of the search
		 */
		value_t negaMaxLastPlys(MoveGenerator& board, SearchStack& stack, Move previousPlyMove, ply_t ply);

		/**
		 * Do a full search using the negaMax algorithm
		 */
		value_t negaMax(MoveGenerator& board, SearchStack& stack, Move previousPlyMove, ply_t ply);

		Eval eval;
		ComputingInfo* _computingInfo;
		ClockManager* _clockManager;
	};
}



#endif // __SEARCH_H