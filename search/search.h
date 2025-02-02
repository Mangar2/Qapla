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
#include "clockmanager.h"
#include "tt.h"
#include "whatif.h"
#include "../nnue/engine.h"
 // #include "razoring.h"

using namespace std;
using namespace QaplaMoveGenerator;
using namespace QaplaInterface;

namespace QaplaSearch {

	class Search {
	public:
		Search() : _clockManager(0) {}

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

		ComputingInfo negaMaxRoot(MoveGenerator& position, SearchStack& stack, ClockManager& clockManager);

	private:

		enum class SearchRegion {
			INNER, NEAR_LEAF, QUIESCENCE
		};

		/**
		 * Check, if we have a bitbase value
		 */
		bool hasBitbaseCutoff(const MoveGenerator& position, SearchVariables& curPly);

		/**
		 * Check for cutoffs
		 */
		template <SearchRegion TYPE>
		bool hasCutoff(MoveGenerator& position, SearchStack& stack, SearchVariables& node, ply_t ply) {
			if (ply < 1) return false;
			if (node.alpha > MAX_VALUE - value_t(ply)) {
				node.setCutoff(Cutoff::FASTER_MATE_FOUND, MAX_VALUE - value_t(ply));
			}
			else if (node.beta < -MAX_VALUE + value_t(ply)) {
				node.setCutoff(Cutoff::FASTER_MATE_FOUND, -MAX_VALUE + value_t(ply));
			}
			else if (TYPE == SearchRegion::INNER && hasBitbaseCutoff(position, node)) {
				node.setCutoff(Cutoff::BITBASE);
			} else if (TYPE == SearchRegion::INNER && ply >= 3 && position.drawDueToMissingMaterial()) {
				// TODO: remove ply >= 3, kept for backward compatibility
				node.setCutoff(Cutoff::NOT_ENOUGH_MATERIAL, 0);
			}
			else if (stack.isDrawByRepetitionInSearchTree(position, ply)) {
				node.setCutoff(Cutoff::DRAW_BY_REPETITION, 0);
			}
			else if (node.probeTT(position, ply)) {
				node.setCutoff(Cutoff::HASH);
			}
			else if (position.isInCheck()) {
				return false;
			}
			else if (node.futility(position)) {
				// Must be after node.probeTT, because futility uses the information from TT
				node.setCutoff(Cutoff::FUTILITY);
			} 
			else if (TYPE == SearchRegion::INNER && isNullmoveCutoff(position, stack, ply)) {
				node.setCutoff(Cutoff::NULL_MOVE);
			}
			WhatIf::whatIf.cutoff(position, _computingInfo, stack, ply, node.cutoff);
			return node.cutoff != Cutoff::NONE;
		}

		/**
		 * Returns true, if we should do internal iterative deepening
		 */
		inline bool isIIDReasonable(MoveGenerator& position, SearchVariables& searchInfo, ply_t depth, ply_t ply) {

			if (!SearchParameter::DO_IID) return false;

			bool isPV = searchInfo.isPVNode();
			if (depth <= SearchParameter::getIIDMinDepth(isPV)) return false;

			if (!searchInfo.getTTMove().isEmpty()) return false;
			if (!isPV && (!SearchParameter::DO_IID_FOR_CUT_NODES || !searchInfo.isCutNode())) return false;
			if (!isPV && position.isInCheck()) return false;

			return true;
		}

		/**
		 * Compute internal iterative deepening
		 * IID modifies variables from stack[ply] (like move counter, search depth, ...)
		 * Thus it must be called before setting the stack in negamax (setFromPreviousPly). 
		 */
		void iid(MoveGenerator& position, SearchStack& stack, ply_t depth, ply_t ply) {
			SearchVariables& searchInfo = stack[ply];
			if (!isIIDReasonable(position, searchInfo, depth, ply)) {
				return;
			}

			ply_t iidR = SearchParameter::getIIDReduction(depth, searchInfo.isPVNode());
			negaMax<SearchRegion::INNER>(position, stack, depth - iidR, ply);
			WhatIf::whatIf.moveSearched(position, _computingInfo, stack, stack[ply].previousMove, depth - iidR, ply - 1, "IID");
			position.computeAttackMasksForBothColors();
			if (!searchInfo.bestMove.isEmpty()) {
				searchInfo.moveProvider.setTTMove(searchInfo.bestMove);
			}

		}

		ply_t se(MoveGenerator& position, SearchStack& stack, ply_t depth, ply_t ply);

		ply_t computeLMR(SearchVariables& node, MoveGenerator& position, ply_t depth, ply_t ply, Move move);

		/**
		 * Check, if it is reasonable to do a nullmove search
		 */
		bool isNullmoveReasonable(MoveGenerator& position, SearchVariables& searchInfo, ply_t ply);

		/**
		 * Check for a nullmove cutoff
		 */
		bool isNullmoveCutoff(MoveGenerator& position, SearchStack& stack, uint32_t ply);

		/**
		 * Do a full search using the negaMax algorithm
		 */
		template <SearchRegion TYPE>
		value_t negaMax(MoveGenerator& position, SearchStack& stack, ply_t depth, ply_t ply);

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