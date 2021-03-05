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
#include "searchstack.h"
#include "rootmoves.h"
#include "../eval/eval.h"
#include "quiescencesearch.h"
#include "clockmanager.h"
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

		/**
		 * Starts a new search
		 */
		void startNewSearch(MoveGenerator& position) {
			_computingInfo.initSearch();
			_rootMoves.setMoves(position);
		}

		/**
		 * Sets the interface printing search information
		 */
		void setSendSearchInfoInterface(ISendSearchInfo* sendSearchInfo, bool verbose = true) {
			_computingInfo.setSendSearchInfo(sendSearchInfo);
			_computingInfo.setVerbose(verbose);
		}

		/**
		 * Stores the requests to print search information. 
		 * Next time the search calls print search info, it will be printed and the 
		 * request flag will be set to false again
		 */
		void requestPrintSearchInfo() {
			_computingInfo.requestPrintSearchInfo();
		}

		ComputingInfo searchRoot(MoveGenerator& position, SearchStack& stack, ClockManager& clockManager);

	private:

		enum class SearchFinding {
			PV, NORMAL, PV_LAST_PLY, LAST_PLY
		};

		/**
		 * Check for cutoffs
		 */
		template <SearchFinding TYPE>
		bool hasCutoff(MoveGenerator& position, SearchStack& stack, SearchVariables& curPly, ply_t ply) {
			if (ply < 1) return false;
			if (curPly.alpha > MAX_VALUE - value_t(ply)) {
				curPly.setCutoff(Cutoff::FASTER_MATE_FOUND, MAX_VALUE - value_t(ply));
			}
			else if (curPly.beta < -MAX_VALUE + value_t(ply)) {
				curPly.setCutoff(Cutoff::FASTER_MATE_FOUND, -MAX_VALUE + value_t(ply));
			}
			else if (ply >= 3 && position.drawDueToMissingMaterial()) {
				curPly.setCutoff(Cutoff::NOT_ENOUGH_MATERIAL, 0);
			}
			else if (stack.isDrawByRepetitionInSearchTree(position, ply)) {
				curPly.setCutoff(Cutoff::DRAW_BY_REPETITION, 0);
			}
			else if (curPly.probeTT(position)) {
				curPly.setCutoff(Cutoff::HASH);
			}
			else if (position.isInCheck()) {
				return false;
			}
			else if (curPly.futility(position)) {
				curPly.setCutoff(Cutoff::FUTILITY);
			} 
			else if (TYPE == SearchFinding::NORMAL && isNullmoveCutoff(position, stack, ply)) {
				curPly.setCutoff(Cutoff::NULL_MOVE);
			}
			WhatIf::whatIf.cutoff(position, _computingInfo, stack, ply, curPly.cutoff);
			return curPly.cutoff != Cutoff::NONE;
		}

		/**
		 * Compute internal iterative deepening
		 */
		void iid(MoveGenerator& position, SearchStack& stack, ply_t ply) {
			SearchVariables& searchInfo = stack[ply];
			bool isPV = searchInfo.isPVSearch();

			if (!SearchParameter::DO_IID) return;
			if (searchInfo.remainingDepth <= SearchParameter::getIIDMinDepth(isPV)) return;
			if (!searchInfo.getTTMove().isEmpty()) return;
			if (!isPV && (!SearchParameter::DO_IID_FOR_CUT_NODES || !searchInfo.isCutNode())) return;
			if (!isPV && position.isInCheck()) return;

			static uint32_t iidCount[2] = { 0, 0 };
			static uint32_t iidSuccess[2] = { 0, 0 };
 			iidCount[isPV]++;

			ply_t formerDepth = searchInfo.getRemainingDepth();
			searchInfo.setRemainingDepthAtPlyStart(
				formerDepth - SearchParameter::getIIDReduction(formerDepth, isPV));

			value_t searchValue = negaMax(position, stack, ply);
			if (!searchInfo.bestMove.isEmpty()) {
				searchInfo.moveProvider.setTTMove(searchInfo.bestMove);
				iidSuccess[isPV]++;
			}
			
			if (false) cout << "IDD rate; PV count: " << iidCount[1] << " PV success: " << iidSuccess[1]
				<< " Non PV count: " << iidCount[0] << " Non PV success: " << iidSuccess[0]
				<< endl; 

			searchInfo.setForResearch(position, formerDepth);
		}

		/**
		 * Check, if it is reasonable to do a nullmove search
		 */
		bool isNullmoveReasonable(MoveGenerator& position, SearchVariables& searchInfo, ply_t ply);

		/**
		 * Check for a nullmove cutoff
		 */
		bool isNullmoveCutoff(MoveGenerator& position, SearchStack& stack, uint32_t ply);

		/**
		 * Negamax algorithm for the last plies of the search
		 */
		value_t negaMaxLastPlys(MoveGenerator& position, SearchStack& stack, ply_t ply);

		/**
		 * Do a full search using the negaMax algorithm
		 */
		value_t negaMax(MoveGenerator& position, SearchStack& stack, ply_t ply);

		/**
		 * Returns the information about the root moves
		 */
		const RootMoves& getRootMoves() const {
			return _rootMoves;
		}

	private:
		Eval eval;
		ComputingInfo _computingInfo;
		ClockManager* _clockManager;

		RootMoves _rootMoves;
	};
}



#endif // __SEARCH_H