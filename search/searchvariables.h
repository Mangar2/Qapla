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
 * Implements all needed variables for search in one structure
 */

#ifndef __SEARCHVARIABLES_H
#define __SEARCHVARIABLES_H

#include <mutex>
#include <string>
#include "../basics/types.h"
#include "../basics/move.h"
#include "../movegenerator/movegenerator.h"
#include "killermove.h"
#include "pv.h"
#include "moveprovider.h"
#include "tt.h"
#include "searchparameter.h"
#include "extension.h"
#include "../eval/eval.h"
#include "../nnue/engine.h"

using namespace std;
using namespace QaplaMoveGenerator;
using namespace ChessEval;

enum class Cutoff {
	NONE, DRAW_BY_REPETITION, HASH, FASTER_MATE_FOUND, RAZORING, NOT_ENOUGH_MATERIAL, 
	NULL_MOVE, FUTILITY, BITBASE, LOST_WINNING_BONUS,
	COUNT
};

namespace QaplaSearch {
	struct SearchVariables {

		enum class SearchFinding {
			PV, NULL_WINDOW, PV_LMR, NORMAL, LMR, NULLMOVE, VERIFY, IID,
			AMOUNT
		};

		typedef uint32_t pvIndex_t;

		SearchVariables() {
			_searchState = SearchFinding::PV;

			cutoff = Cutoff::NONE;
		};

		/**
		 * Adds a move to the primary variant
		 */
		void setPVMove(Move pvMove) {
			moveProvider.setPVMove(pvMove);
		}

		/**
		 * @returns true, if the current search is a PV search
		 */
		inline bool isPVSearch() const { return _searchState == SearchFinding::PV; }
		inline bool isNullWindowSearch() const { return _searchState == SearchFinding::NULL_WINDOW; }
		inline auto getSearchState() const { return _searchState; }
		inline bool isWindowZero() const { return alpha + 1 == beta;  }
		inline bool isPVNode() const { return alphaAtPlyStart + 1 < betaAtPlyStart;  }

		/**
		 * Sets all variables from previous ply
		 */
		void setFromPreviousPly(MoveGenerator& position, const SearchVariables& previousPly, ply_t depth) {
			pvMovesStore.setEmpty(ply);
			pvMovesStore.setEmpty(ply + 1);
			bestMove.setEmpty();
			bestValue = -MAX_VALUE;
			cutoff = Cutoff::NONE;
			lateMoveReduction = 0;
			ttValueLessThanAlpha = false;
			eval = NO_VALUE;
			positionHashSignature = position.computeBoardHash();
			remainingDepth = depth;
			remainingDepthAtPlyStart = depth;
			beta = -previousPly.alpha;
			alpha = -previousPly.beta;
			alphaAtPlyStart = alpha;
			betaAtPlyStart = beta;
			moveNumber = 0;
			_nodeType = _nodeTypeMap[int(previousPly._nodeType)];
			_searchState = beta > alpha + 1 ? SearchFinding::PV : SearchFinding::NORMAL;
			noNullmove = previousPly.previousMove.isNullMove() || previousMove.isNullMove();
			if (previousMove.isNullMove()) {
				_searchState = SearchFinding::NORMAL;
				_nodeType = NodeType::CUT;
			}
			moveProvider.init();
		}

		/**
		 * Initializes all variables to start search
		 */
		void initSearchAtRoot(MoveGenerator& position, value_t initialAlpha, value_t initialBeta, ply_t searchDepth) {
			remainingDepth = searchDepth;
			moveNumber = 0;
			alpha = initialAlpha;
			beta = initialBeta;
			alphaAtPlyStart = alpha;
			betaAtPlyStart = beta;
			bestMove.setEmpty();
			bestValue = -MAX_VALUE;
			_searchState = SearchFinding::PV;
			noNullmove = true;
			cutoff = Cutoff::NONE;
			positionHashSignature = position.computeBoardHash();
			lateMoveReduction = 0;
			ttValueLessThanAlpha = false;
			eval = NO_VALUE;
			moveProvider.init();
		}

		/**
		 * Applies a move
		 */
		void doMove(MoveGenerator& position, Move previousPlyMove) {
			previousMove = previousPlyMove;
			boardState = position.getBoardState();
			position.doMove(previousMove);
			sideToMoveIsInCheck = position.isInCheck();
#ifdef USE_STOCKFISH_EVAL
			Stockfish::Engine::doMove(previousMove, si);
#endif
		}

		/**
		 * Take back the previously applied move
		 */
		void undoMove(MoveGenerator& position) {
			if (previousMove.isEmpty()) {
				return;
			}
			position.undoMove(previousMove, boardState);
			Stockfish::Engine::undoMove(previousMove);
		}

		/**
		 * Gets an entry from the transposition table
		 */
		bool probeTT(MoveGenerator& position, ply_t ply) {
			static const ply_t PLY = 8;

			uint32_t ttIndex = ttPtr->getTTEntryIndex(positionHashSignature);
			if (ttIndex == TT::INVALID_INDEX) return false;

			const TTEntry entry = ttPtr->getEntry(ttIndex);
			const Move move = entry.getMove();
			moveProvider.setTTMove(move);
			
			if (entry.alwaysUseValue()) {
				bestValue = entry.getPositionValue(ply);
				eval = bestValue;
				return true;
			}

			ttValueLessThanAlpha = entry.isValueGreaterOrEqual();
			if (entry.isValueExact()) {
				eval = entry.getPositionValue(ply);
			}

			value_t ttValue = entry.getValue(alpha, beta, remainingDepth, ply);
			bool isWinningValue = ttValue <= -WINNING_BONUS || ttValue >= WINNING_BONUS;
			if (!isPVSearch() || isWinningValue) {
				// keeps the best move for a tt entry, if the search does stay <= alpha
				bestMove = move;
				if (ttValue != NO_VALUE) {
					bestValue = ttValue;
					return true;
				}
			}

			return false;
		}

		/**
		 * Cutoff the current search
		 */
		inline void setCutoff(Cutoff cutoffType, value_t cutoffResult) {
			cutoff = cutoffType;
			bestValue = cutoffResult;
		}

		/**
		 * Cutoff the current search
		 */
		inline void setCutoff(Cutoff cutoffType) {
			cutoff = cutoffType;
		}

		inline bool isFailHigh() { 
			return bestValue >= betaAtPlyStart; 
		}

		inline bool isNullWindowFailHigh() {
			return bestValue >= beta;
		}

		/**
		 * Extend the current search
		 */
		auto extendSearch(MoveGenerator& position) {
			searchDepthExtension = Extension::calculateExtension(position, previousMove, remainingDepth);
			remainingDepth += searchDepthExtension;
			return remainingDepth;
		}

		/**
		 * Examine, if we can do a futility pruning based on an evaluation score
		 */
		inline bool futility(MoveGenerator& position) {
			if (SearchParameter::DO_FUTILITY_DEPTH <= remainingDepth) return false;
			if (isPVSearch()) return false;
			if (ttValueLessThanAlpha) return false;
			if (eval == NO_VALUE) {
				eval = Eval::eval(position);
			}

			if (eval > WINNING_BONUS) return false;
			bool doFutility = eval - SearchParameter::futilityMargin(remainingDepth) >= beta;
			if (doFutility) {
				bestValue = eval;
			}
			return doFutility;
		}

		/**
		 * Generates all moves in the current position
		 */
		void computeMoves(MoveGenerator& position) {
			moveProvider.computeMoves(position, previousMove);
			bestValue = moveProvider.checkForGameEnd(position, ply);
		}

		/**
		 * Set the search variables to a nullmove search
		 */
		void setNullmove() {
			_searchState = SearchFinding::NULLMOVE;
			setNodeType(NodeType::CUT);
			alpha = beta - 1;
		}

		/**
		 * Search depth remaining
		 */
		void setRemainingDepthAtPlyStart(ply_t newDepth) {
			remainingDepthAtPlyStart = newDepth;
			remainingDepth = newDepth;
		}

		/**
		 * Search depth remaining
		 */
		void setRemainingDepth(ply_t newDepth) {
			remainingDepth = newDepth;
		}

		ply_t getRemainingDepth() const {
			return remainingDepth;
		}

		/**
		 * Sets the search to a null window search
		 */
		void setNullWindow() {
			beta = alpha + 1;
			_searchState = SearchFinding::NULL_WINDOW;
			setNodeType(NodeType::ALL);
		}

		/**
		 * Sets the search to PV (open window) search
		 */
		void setPVWindow() {
			// Needed to ensure that the research will be > bestValue and set the PV
			bestValue = alpha;
			beta = betaAtPlyStart;
			_searchState = SearchFinding::PV;
			setNodeType(NodeType::PV);
		}



		/**
		 * Selects the next move to try
		 */
		inline Move selectNextMove(MoveGenerator& position) {
			Move result = moveProvider.selectNextMove(position);
			if (!isPVSearch() && moveProvider.isAllSearch()) {
				setNodeType(NodeType::ALL);
			}
			moveNumber++;
			return result;
		}

		/**
		 * Multi thread variant to select the next move
		 */
		Move selectNextMoveThreadSafe(MoveGenerator& position) {
			std::lock_guard<std::mutex> lockGuard(mtxSearchResult);
			return selectNextMove(position);
		}

		/**
		 * applies the search result to the status
		 */
		void setSearchResult(value_t searchResult, const SearchVariables& nextPlySearchInfo, Move currentMove) {
			currentValue = searchResult;
			if (searchResult > bestValue) {
				bestValue = searchResult;
				if (searchResult > alpha) {
					if (!isNullWindowSearch()) {
						bestMove = currentMove;
						if (isPVSearch()) {
							if (remainingDepth > 0) {
								pvMovesStore.copyFromPV(nextPlySearchInfo.pvMovesStore, ply + 1);
							}
							pvMovesStore.setMove(ply, bestMove);
						}
					}
					if (searchResult < beta) {
						// Never set alpha > beta, it will harm the PVS algorithm
						alpha = searchResult;
					}
				}
			}
		}

		/**
		 * Multi-Threading version to set the search result
		 */
		void setSearchResultThreadSafe(value_t searchResult, const SearchVariables& searchInfo, Move currentMove) {
			std::lock_guard<std::mutex> lockGuard(mtxSearchResult);
			setSearchResult(searchResult, searchInfo, currentMove);
		}

		/**
		 * Sets the hash entry
		 */
		void setTTEntry(hash_t hashKey) {
			bool drawByRepetetivePositionValue = (bestValue == 0);
			if (!drawByRepetetivePositionValue) {
				ply_t depth = max(remainingDepthAtPlyStart, 0);
				ttPtr->setEntry(hashKey, depth, ply, bestMove, bestValue, alphaAtPlyStart, betaAtPlyStart, false);
				// WhatIf::whatIf.setTT(ttPtr, hashKey, remainingDepthAtPlyStart, ply, bestMove, bestValue, alphaAtPlyStart, betaAtPlyStart, false);
			}
		}

		/**
		 * Indicates that the PV failed low
		 */
		bool isPVFailLow() {
			return isPVSearch() && bestValue <= alphaAtPlyStart;
		}

		/**
		 * terminates the search-ply
		 */
		void updateTTandKiller(MoveGenerator& position) {
			if (cutoff == Cutoff::NONE && bestValue != -MAX_VALUE && !bestMove.isNullMove()) {
				if (!bestMove.isEmpty()) {
					moveProvider.setKillerMove(bestMove);
				}
				if (!isPVFailLow()) {
					setTTEntry(position.computeBoardHash());
				}
			}
		}

		Move getMoveFromPVMovesStore(ply_t ply) const { return pvMovesStore.getMove(ply); }
		const KillerMove& getKillerMove() const { return moveProvider.getKillerMove(); }
		/**
		 * Gets the move proposed by the tt
		 */
		Move getTTMove() const { return moveProvider.getTTMove(); }
		void setPly(ply_t curPly) { ply = curPly; }

		string getSearchStateName(SearchFinding searchState) const {
			return searchStateNames[int32_t(searchState)];
		}
		
		string getSearchStateName() const {
			return getSearchStateName(_searchState);
		}
		
		/**
		 * Sets the transposition tables
		 */
		void setTT(TT* tt) {
			ttPtr = tt;
		}

		/**
		 * Gets a pointer to the transposition tables
		 */
		TT* getTT() {
			return ttPtr;
		}

		/**
		 * Gets the hash fill rate in permill
		 */
		inline uint32_t getHashFullInPermill() {
			return ttPtr->getHashFillRateInPermill();
		}

		/**
		 * prints the information 
		 */
		void print() {
			printf("[w:%6ld,%6ld]", alphaAtPlyStart, betaAtPlyStart);
			printf("[d:%ld]", remainingDepth);
			printf("[v:%6ld]", bestValue);
			printf("[hm:%5s]", getTTMove().getLAN().c_str());
			printf("[bm:%5s]", bestMove.getLAN().c_str());
			printf("[st:%8s]", getSearchStateName().c_str());
			printf("[nt:%4s]", getNodeTypeName().c_str());
			if (isPVSearch()) {
				printf(" [PV:");
				pvMovesStore.print(ply);
				printf(" ]");
			}
			printf("\n");
		}

		inline bool isTTValueBelowBeta(const Board& position, ply_t ply) {
			return ttPtr->isTTValueBelowBeta(positionHashSignature, beta, ply);
		}



		enum class NodeType {
			PV, CUT, ALL, COUNT
		};

		/**
		 * Sets the type of the node
		 */
		void setNodeType(NodeType type) {
			_nodeType = type;
		}

		/**
		 * Returns true, if the current node is a cut node
		 */
		inline bool isCutNode() { return _nodeType == NodeType::CUT; }

		/**
		 * Gets the name of the node type
		 */
		string getNodeTypeName() const {
			return nodeTypeName[int(_nodeType)];
		}

		static constexpr array<const char*, int(NodeType::COUNT)> nodeTypeName = { "PV", "CUT", "ALL" };

		static constexpr array<const char*, int(SearchFinding::AMOUNT)> searchStateNames =
			{ "PV", "NullW", "PV_LMR", "Normal", "LMR", "NullM", "Verify", "IID" };

		value_t alpha;
		value_t alphaAtPlyStart;
		value_t beta;
		value_t betaAtPlyStart;
		value_t bestValue;
		value_t currentValue;
		value_t eval;
		Move bestMove;
		Move previousMove;
		int32_t moveNumber;
		ply_t remainingDepth;
		ply_t remainingDepthAtPlyStart;
		ply_t ply;
		ply_t lateMoveReduction;
		ply_t searchDepthExtension;
		BoardState boardState;
		hash_t positionHashSignature;
		bool noNullmove;
		bool sideToMoveIsInCheck;
		bool ttValueLessThanAlpha;

		Cutoff cutoff;
		mutex mtxSearchResult;
		MoveProvider moveProvider;
		PV pvMovesStore;

	private:
		SearchFinding _searchState;
		NodeType _nodeType;
		TT* ttPtr;

		static constexpr array<NodeType, 3> _nodeTypeMap = { NodeType::PV, NodeType::ALL, NodeType::CUT };
#ifdef USE_STOCKFISH_EVAL
		Stockfish::StateInfo si;
#endif

	};

}

#endif // __SEARCHVARIABLES_H