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

#include <atomic>
#include "../movegenerator/movegenerator.h"
#include "computinginfo.h"
#include "clockmanager.h"
#include "searchstack.h"
#include "rootmoves.h"
#include "clockmanager.h"
#include "tt.h"
#include "butterfly-boards.h"
#include "../eval/pawntt.h"
#include "quiescence.h"
#include "../src/syzygy/tablebase.h"
#ifdef USE_STOCKFISH_EVAL
#include "../nnue/engine.h"
#endif

using namespace std;
using namespace QaplaMoveGenerator;
using namespace QaplaInterface;

namespace QaplaSearch {

	struct SearchThread;
	class SearchThreads;

	class Search {
	public:
		Search() : _clockManager(0) {
			_pawnTT.setSizeInKilobytes(1024);
			_quiescence.setPawnTT(&_pawnTT);
		}

		/**
		 * Starts a new game or sets a new position e.g. by fen
		 */
		void startNewGame() {
			_butterflyBoard->clear();
		}

		void clearMemories() {
			_butterflyBoard->clear();
			_pawnTT.clear();
		}

		/**
		 * The pawn hash of this thread. Every thread has its own: pawn structures repeat within
		 * one subtree far more than across the threads' different subtrees.
		 */
		ChessEval::PawnTT* getPawnTT() {
			return &_pawnTT;
		}

		/**
		 * Starts a new search
		 */
		void startNewSearch(MoveGenerator& position, const std::vector<Move>& searchMoves,
			bool hasRepeatedPosition) {
			_computingInfo.initNewSearch(position, searchMoves, *_butterflyBoard);
			_waitMicroseconds = 0;
			_helpMicroseconds = 0;
			_butterflyBoard->newSearch();
			// The tablebase settings cannot change during a search, and reading them at
			// every node costs more than the probes save.
			_tbCardinality = QaplaSyzygy::Tablebase::cardinality();
			_tbProbeDepth = QaplaSyzygy::Tablebase::probeDepth();

			// Root position does not change during the search either, so the classification and
			// the counts that follow from it are computed once here, not per iteration.
			_tbRootWin = _computingInfo.getRootMoves().computeTablebaseInfo(position, hasRepeatedPosition);
			// Every classified root move is a position answered by the tables, counted the same
			// way hasTablebaseCutoff counts its own probes - which matters because that one stops
			// counting for the rest of the search as soon as _tbRootWin is set.
			_computingInfo._tbHits += _computingInfo.getRootMoves().getTablebaseInfoCount();
			// Only the best bucket needs real search effort - a move from a worse one can never turn
			// out better. MultiPV asks for more lines than that, so it raises the count.
			const uint32_t bestBucket = _computingInfo.getRootMoves().getTablebaseBestBucketCount();
			_tbSearchableMoves = bestBucket > 0
				? std::max(bestBucket, _computingInfo.getMultiPV())
				: uint32_t(_computingInfo.getMovesAmount());
			_tbSearchableMoves = std::min(_tbSearchableMoves, uint32_t(_computingInfo.getMovesAmount()));
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

		/**
		 * Gives the search its thread and the threads it may book helpers from
		 */
		void setThreads(SearchThreads* threads, SearchThread* self) {
			_threads = threads;
			_self = self;
		}

		/**
		 * Makes this the search of a helper thread. It takes the settings of the search from
		 * the master and searches with the master's history: the history is shared between the
		 * threads by decision, it lives on more information. A helper never checks the clock
		 * and never prints; it only follows the stop flag.
		 */
		void initAsHelper(Search& master, ClockManager* clockManager, TT* tt) {
			_isMaster = false;
			_clockManager = clockManager;
			_computingInfo.clear();
			_quiescence.setTT(tt);
			_tbCardinality = master._tbCardinality;
			_tbProbeDepth = master._tbProbeDepth;
			_tbRootWin = master._tbRootWin;
			_tbSearchableMoves = master._tbSearchableMoves;
			_butterflyBoard = master._butterflyBoard;
		}

		/**
		 * Makes this the master search of an extra search, see ExtraSearch: it runs on its
		 * own clock, never prints and keeps a history of its own.
		 */
		void initAsExtraSearch(ClockManager* clockManager, TT* tt) {
			_isMaster = false;
			_clockManager = clockManager;
			_quiescence.setTT(tt);
		}

		/**
		 * The thread body of a helper: waits for jobs and runs them, see helpAtSplitPoint
		 */
		void helperLoop(SearchThread& self);

		/**
		 * Nodes this thread searched so far. Read by the master for the info lines, so it may
		 * lag behind.
		 */
		uint64_t getNodesSearched() const {
			return _computingInfo._nodesSearched;
		}
		uint64_t getHelpMicroseconds() const { return _helpMicroseconds; }

		/**
		 * Nodes the helper threads searched, for the master's reports
		 */
		uint64_t helperNodes() const;
		uint64_t getWaitMicroseconds() const { return _waitMicroseconds; }

	private:

		enum class SearchRegion {
			INNER, NEAR_LEAF, PV
		};

		/**
		 * Asks the tablebases and cuts the node if the answer is usable. Must be called
		 * after the moves have been generated - the move list decides whether the stored
		 * entry is exact.
		 */
		bool hasTablebaseCutoff(MoveGenerator& position, SearchNode& node, ply_t depth);

		template <SearchRegion TYPE>
		bool nonSearchingCutoff(MoveGenerator& position, SearchStack& stack, SearchNode& node, value_t alpha, value_t beta, ply_t depth, ply_t ply);

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
		bool checkEvalReleatedCutoffsAndSetEval(MoveGenerator& position, SearchStack& stack, SearchNode& node, ply_t depth, ply_t ply);

		/**
		 * Internal iterative reduction: the plies a node is searched shallower by because it has
		 * no move to try first, neither from the hash nor from the previous iteration. Runs in PV
		 * and in cut nodes, never in all nodes.
		 */
		template <SearchRegion TYPE>
		ply_t iir(const SearchStack& stack, ply_t depth, ply_t ply);

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
		ply_t computeLMR(SearchNode& node, MoveGenerator& position, ply_t depth, ply_t ply, Move move);

		/**
		 * Check, if it is reasonable to do a nullmove search
		 */
		bool isNullmoveReasonable(MoveGenerator& position, SearchNode& node, ply_t depth, ply_t ply);

		/**
		 * Check for a nullmove cutoff
		 */
		bool isNullmoveCutoff(MoveGenerator& position, SearchStack& stack, ply_t depth, ply_t ply);


		/**
		 * Do a full search using the negaMax algorithm
		 */
		template <SearchRegion TYPE>
		value_t negaMax(MoveGenerator& position, SearchStack& stack, value_t alpha, value_t beta, ply_t depth, ply_t ply);

		/**
		 * Searches a child node, picking the region it belongs to: a child with more than one
		 * ply left below it is still an inner node, everything else is near the leaves. A child
		 * of a NEAR_LEAF node is never an inner node again.
		 * The caller negates the result and passes the window already from the child's view.
		 */
		template <SearchRegion TYPE>
		inline value_t searchChild(MoveGenerator& position, SearchStack& stack,
			value_t alpha, value_t beta, ply_t childDepth, ply_t childPly)
		{
			return TYPE != SearchRegion::NEAR_LEAF && childDepth > 1 ?
				negaMax<SearchRegion::INNER>(position, stack, alpha, beta, childDepth, childPly) :
				negaMax<SearchRegion::NEAR_LEAF>(position, stack, alpha, beta, childDepth, childPly);
		}

		value_t negaMaxPreSearch(MoveGenerator& position, SearchStack& stack, value_t alpha, value_t beta, ply_t depth, ply_t ply);

		/**
		 * True, if the search is to stop: the clock says so, or a split point this thread
		 * works under has failed high - nothing below it is worth searching then.
		 */
		bool isSearchStopped() const;

		/**
		 * The move loop of negaMax. SPLIT: the node is a split point, splitNode is the node of
		 * the thread that owns it - its move list, window and best value are shared between
		 * the threads and touched under its lock only. Without SPLIT, splitNode is the node
		 * itself and no lock exists. A node opens itself as split point from the move loop, the
		 * helper joins with helpAtSplitPoint.
		 * @param firstMove a move already taken from the list, to be searched first
		 */
		template <SearchRegion TYPE, bool SPLIT>
		void moveLoop(MoveGenerator& position, SearchStack& stack, SearchNode& splitNode,
			ply_t depth, ply_t seExtension, ply_t ply, Move firstMove);

		/**
		 * Opens the node at ply as split point and books a waiting thread for it. This thread
		 * copies nothing: the helper fetches what it reads from the stack itself and replays
		 * the line to the node on its own board.
		 * @returns false, if no thread could be booked - the node stays as it is
		 */
		bool openSplitPoint(SearchStack& stack, SearchNode& node, ply_t depth, ply_t seExtension, ply_t ply, bool pvNode);

		/**
		 * Closes the split point for joining and waits until the helpers have left it.
		 * Meanwhile this thread can be booked for a split point below its own, see
		 * waitForHelpers.
		 */
		void closeSplitPoint(MoveGenerator& position, SearchStack& stack, SearchNode& node, ply_t ply);

		/**
		 * Waits until the helpers have left the split point node and helps at split points
		 * below it meanwhile.
		 */
		void waitForHelpers(SearchThread& self, SearchNode& node, MoveGenerator& position, SearchStack& stack, ply_t ply);

		/**
		 * Runs the job of the thread: joins the split point, from where the thread stands
		 * @param fromPly ply the thread stands at, 0 for a helper without a node of its own
		 */
		void runJob(SearchThread& self, MoveGenerator& position, SearchStack& stack, ply_t fromPly);

		/**
		 * The helper's part: reaches the split point on its own board and joins its move loop.
		 * Leaves the split point when the list is empty or the node failed high, and wakes the
		 * owner as the last one to leave.
		 */
		template <SearchRegion TYPE>
		void helpAtSplitPoint(SearchThread& self, MoveGenerator& position, SearchStack& stack, ply_t fromPly);

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
		ChessEval::PawnTT _pawnTT;
		ComputingInfo _computingInfo;
		ClockManager* _clockManager;

		// Tablebase settings, read once per search - see startNewSearch
		uint32_t _tbCardinality = 0;
		int32_t  _tbProbeDepth = 1;

		// Root tablebase classification, computed once per search - see startNewSearch. A root
		// win found this way answers the fifty-move question differently from the in-search WDL
		// cutoff, so the two must not run together: hasTablebaseCutoff skips itself for the rest
		// of the search while _tbRootWin is set.
		bool     _tbRootWin = false;
		uint32_t _tbSearchableMoves = 0;

		// Master or helper thread, see initAsHelper. Only the master checks the clock, prints
		// and hands moves to the helper.
		bool _isMaster = true;
		SearchThreads* _threads = nullptr;
		SearchThread* _self = nullptr;
		// Test output while the parallel search is being built
		uint64_t _waitMicroseconds = 0;
		uint64_t _helpMicroseconds = 0;

		// The history the move ordering reads. Shared: a helper points to the master's, see
		// initAsHelper. Lost updates between threads are accepted, it holds ordering weights only.
		ButterflyBoard _ownButterflyBoard;
	public:
		ButterflyBoard* _butterflyBoard = &_ownButterflyBoard;
	};
}



#endif // __SEARCH_H
