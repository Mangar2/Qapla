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
 * @author Volker B�hm
 * @copyright Copyright (c) 2021 Volker B�hm
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
#include "aspirationwindow.h"
#include "tt.h"
#include "butterfly-boards.h"
#include "whatIf.h"
#ifdef USE_STOCKFISH_EVAL
#include "../nnue/engine.h"
#endif
 // #include "razoring.h"

using namespace std;
using namespace QaplaMoveGenerator;
using namespace QaplaInterface;

namespace QaplaSearch {

	class Search {
	public:
		Search() : _clockManager(0) {}

		/**
		 * Starts a new game or sets a new position e.g. by fen
		 */
		void startNewGame() {
			_butterflyBoard.clear();
		}

		void clearMemories() {
			_butterflyBoard.clear();
		}

		/**
		 * Starts a new search
		 */
		void startNewSearch(MoveGenerator& position) {
			_computingInfo.initNewSearch(position, _butterflyBoard);
			_butterflyBoard.newSearch();
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

		void negaMaxRoot(MoveGenerator& position, SearchStack& stack, uint32_t skipMoves, ClockManager& clockManager);

		const ComputingInfo& getComputingInfo() const {
			return _computingInfo;
		}

		uint32_t getMultiPV() const {
			return _computingInfo.getMultiPV();
		}

		void setMultiPV(uint32_t multiPV) {
			_computingInfo.setMultiPV(multiPV);
		}

	private:

		enum class SearchRegion {
			INNER, NEAR_LEAF, PV
		};

		/**
		 * Check, if we have a bitbase value
		 */
		bool hasBitbaseCutoff(const MoveGenerator& position, SearchVariables& curPly);

		template <SearchRegion TYPE>
		bool nonSearchingCutoff(MoveGenerator& position, SearchStack& stack, SearchVariables& node, value_t alpha, value_t beta, ply_t depth, ply_t ply);

		/**
		 * Check for cutoffs
		 */
		template <SearchRegion TYPE>
		bool checkCutoffAndSetEval(MoveGenerator& position, SearchStack& stack, SearchVariables& node, ply_t depth, ply_t ply) {
			const auto evalBefore = ply > 1 ? stack[ply - 2].eval : NO_VALUE;
			if (position.isInCheck()) {
				node.eval = evalBefore;
				return false;
			}
			if (node.eval == NO_VALUE) {
				node.eval = Eval::eval(position);
				node.isImproving = node.eval > evalBefore && evalBefore != NO_VALUE;
			}
			// Must be after node.probeTT, because futility uses the information from TT
			if (node.futility(position)) {
				node.setCutoff(Cutoff::FUTILITY);
				return true;
			} 
			if (TYPE == SearchRegion::INNER && isNullmoveCutoff(position, stack, depth, ply)) {
				node.setCutoff(Cutoff::NULL_MOVE);
				return true;
			}
			return false;
		}


		void iid(MoveGenerator& position, SearchStack& stack, value_t alpha, value_t beta, ply_t depth, ply_t ply);

		ply_t se(MoveGenerator& position, SearchStack& stack, value_t alpha, value_t beta, ply_t depth, ply_t ply);

		ply_t computeLMR(SearchVariables& node, MoveGenerator& position, ply_t depth, ply_t ply, Move move);

		/**
		 * Check, if it is reasonable to do a nullmove search
		 */
		bool isNullmoveReasonable(MoveGenerator& position, SearchVariables& node, ply_t depth, ply_t ply);

		/**
		 * Check for a nullmove cutoff
		 */
		bool isNullmoveCutoff(MoveGenerator& position, SearchStack& stack, ply_t depth, ply_t ply);

		/**
		 * Do a full search using the negaMax algorithm
		 */
		template <SearchRegion TYPE>
		value_t negaMax(MoveGenerator& position, SearchStack& stack, value_t alpha, value_t beta, ply_t depth, ply_t ply);

		value_t negaMaxPreSearch(MoveGenerator& position, SearchStack& stack, ply_t depth, ply_t ply);

		/**
		 * Returns the information about the root moves
		 */
		const RootMoves& getRootMoves() const {
			return _computingInfo.getRootMoves();
		}



	private:
		Eval eval;
		ComputingInfo _computingInfo;
		ClockManager* _clockManager;

		// RootMoves _rootMoves;
	public:
		ButterflyBoard _butterflyBoard;
	};
}



#endif // __SEARCH_H