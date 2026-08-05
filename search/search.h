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
#include "quiescence.h"
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
		void startNewSearch(MoveGenerator& position, const std::vector<Move>& searchMoves) {
			_computingInfo.initNewSearch(position, searchMoves, _butterflyBoard);
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
		 * @brief Checks evaluation-related cutoffs and initializes node evaluation values
		 * 
		 * Evaluates the current position and tests for pruning opportunities based on static evaluation.
		 * Handles special cases like check positions and computes the "improving" flag by comparing
		 * to the evaluation two plies earlier.
		 * Cutoff tests:
		 * - futility pruning for non-check positions
		 * - null move pruning (if applicable)
		 * 
		 * @template TYPE The search region (INNER, NEAR_LEAF, PV) - null move pruning only in INNER
		 * @param position Current board position to evaluate
		 * @param stack Search stack containing evaluation history
		 * @param node Current search node variables (eval, adjustedEval, isImproving)
		 * @param depth Remaining search depth
		 * @param ply Current distance from root (0 at root)
		 * 
		 * @return true if a cutoff occurred (futility pruning or null move), false otherwise
		 * 
		 * @note In check positions, evaluation is set to previous ply's eval and no cutoffs occur
		 * @note Forward futility pruning is attempted after TT probe (requires TT information)
		 * @note Null move cutoff is only attempted when TYPE == SearchRegion::INNER
		 * @note Sets node.adjustedEval, node.eval, and node.isImproving as side effects
		 */
		template <SearchRegion TYPE>
		bool checkEvalReleatedCutoffsAndSetEval(MoveGenerator& position, SearchStack& stack, SearchVariables& node, ply_t depth, ply_t ply);

		void iid(MoveGenerator& position, SearchStack& stack, value_t alpha, value_t beta, ply_t depth, ply_t ply);

		/**
		 * Computes the singular extension for the tt move, in PV as well as in non PV nodes.
		 * Near leaf nodes never reach the minimal depth needed, thus it returns immediately
		 * for that search region. The margin is tuned separately for PV and non PV nodes.
		 *
		 * The same search also provides the multi cut: if one of the searched moves reaches
		 * beta, a second move besides the tt move is expected to fail high, the node is thus
		 * not singular and gets cut. In that case the node holds Cutoff::MULTI_CUT and the
		 * caller must return its bestValue instead of searching the node.
		 */
		template <SearchRegion TYPE>
		ply_t se(MoveGenerator& position, SearchStack& stack, value_t alpha, value_t beta, ply_t depth, ply_t ply);

		/**
	 	 * @brief   
		 */
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

		value_t negaMaxPreSearch(MoveGenerator& position, SearchStack& stack, value_t alpha, value_t beta, ply_t depth, ply_t ply);

		/**
		 * Returns the information about the root moves
		 */
		const RootMoves& getRootMoves() const {
			return _computingInfo.getRootMoves();
		}

		void setTT(TT* tt) {
			_quiescence.setTT(tt);
		}

		void storePVToTT(MoveGenerator& position, SearchStack& stack, const RootMove& rootMove, ply_t ply);

	private:
		Quiescence _quiescence;
		ComputingInfo _computingInfo;
		ClockManager* _clockManager;
		// RootMoves _rootMoves;
	public:
		ButterflyBoard _butterflyBoard;
	};
}



#endif // __SEARCH_H
