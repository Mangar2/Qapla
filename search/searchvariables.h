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

using namespace std;
using namespace ChessMoveGenerator;
using namespace ChessEval;

enum class Cutoff {
	NONE, DRAW_BY_REPETITION, HASH, FASTER_MATE_FOUND, RAZORING, NOT_ENOUGH_MATERIAL, NULL_MOVE, FUTILITY,
	COUNT
};

namespace ChessSearch {
	struct SearchVariables {

		typedef uint32_t pvIndex_t;

		SearchVariables() {
			cutoff = Cutoff::NONE;
		};

		/**
		 * Adds a move to the primary variant
		 */
		void setPVMove(Move pvMove) {
			moveProvider.setPVMove(pvMove);
		}

		/**
		 * Selectes the first search state (IID, PV, NORMAL)
		 */
		void selectFirstSearchState() {
			if (beta > alpha + 1) {
				searchState = SearchType::PV;
			}
			else {
				searchState = SearchType::NORMAL;
			}
		}

		/**
		 * Sets all variables from previous ply
		 */
		void setFromPreviousPly(MoveGenerator& board, const SearchVariables& previousPlySearchInfo, Move previousPlyMove) {
			beta = -previousPlySearchInfo.alpha;
			alpha = -previousPlySearchInfo.beta;
			alphaAtPlyStart = alpha;
			betaAtPlyStart = beta;
			remainingDepth = previousPlySearchInfo.remainingDepth - 1;
			remainingDepthAtPlyStart = remainingDepth;
			pvMovesStore.setEmpty(ply);
			pvMovesStore.setEmpty(ply + 1);
			bestMove.setEmpty();
			bestValue = -MAX_VALUE;
			selectFirstSearchState();
			noNullmove = previousPlySearchInfo.previousMove.isNullMove() || previousPlyMove.isNullMove();
			moveProvider.init();
			doMove(board, previousPlyMove);
			cutoff = Cutoff::NONE;
			keepBestMoveUnchanged = false;
			lateMoveReduction = 0;
		}

		/**
		 * Initializes all variables to start search
		 */
		void init(MoveGenerator& board, value_t initialAlpha, value_t initialBeta, int32_t searchDepth) {
			remainingDepth = searchDepth;
			alpha = initialAlpha;
			beta = initialBeta;
			alphaAtPlyStart = alpha;
			betaAtPlyStart = beta;
			bestMove.setEmpty();
			bestValue = -MAX_VALUE;
			searchState = SearchType::PV;
			noNullmove = true;
			cutoff = Cutoff::NONE;
			positionHashSignature = board.computeBoardHash();
			moveProvider.init();
			keepBestMoveUnchanged = false;
			lateMoveReduction = 0;
		}

		/**
		 * Applies a move
		 */
		void doMove(MoveGenerator& board, Move previousPlyMove) {
			previousMove = previousPlyMove;
			boardState = board.getBoardState();
			board.doMove(previousMove);
			sideToMoveIsInCheck = board.isInCheck();
			positionHashSignature = board.computeBoardHash();
		}

		/**
		 * Take back the previously applied move
		 */
		void undoMove(MoveGenerator& board) {
			if (previousMove.isEmpty()) {
				return;
			}
			board.undoMove(previousMove, boardState);
		}

		/**
		 * Gets an entry from the transposition table
		 */
		bool probeTT(MoveGenerator& board) {
			bool cutoff = false;
			uint32_t ttIndex = ttPtr->getTTEntryIndex(positionHashSignature);

			if (ttIndex != TT::INVALID_INDEX) {
				TTEntry entry = ttPtr->getEntry(ttIndex);
				Move move = entry.getMove();
				moveProvider.setTTMove(move);
				if (searchState != SearchType::PV) {
					// keeps the best move for a tt entry, if the search does stay <= alpha
					bestMove = move;
					// The current search cannot handle the search instability from tt entries
					// of older searches.
					// bool thisSearch = entry.getAgeIndicator() == ttPtr->getEntryAgeIndicator();
					if (entry.getValue(bestValue, alpha, beta, remainingDepth, ply)) {
						cutoff = true;
					}
				}
				else {
					if (entry.alwaysUseValue()) {
						bestValue = entry.getPositionValue(ply);
						cutoff = true;
					}
				}
			}
			return cutoff;
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


		/**
		 * Extend the current search
		 */
		void extendSearch(MoveGenerator& board) {
			searchDepthExtension = Extension::calculateExtension(board, previousMove, remainingDepth);
			remainingDepth += searchDepthExtension;
		}

		/**
		 * Examine, if we can do a futility pruning based on an evaluation score
		 */
		inline bool futility(MoveGenerator& board) {
			if (SearchParameter::DO_FUTILITY_DEPTH <= remainingDepth) return false;
			if (searchState == SearchType::PV) return false;

			eval = Eval::evaluateBoardPosition(board);
			if (!board.isWhiteToMove()) {
				eval = -eval;
			}
			bool doFutility = eval - SearchParameter::futilityMargin(remainingDepth) > beta;
			if (doFutility) {
				bestValue = eval;
			}
			return doFutility;
		}

		/**
		 * Generates all moves in the current position
		 */
		void computeMoves(MoveGenerator& board) {
			moveProvider.computeMoves(board, previousMove, remainingDepth);
			bestValue = moveProvider.checkForGameEnd(board, ply);
		}

		/**
		 * Internal interative deepening
		 */
		bool doIID() {
			return false;
			bool result = false;
			if (remainingDepth > 4 * ONE_PLY && moveProvider.getTTMove() == 0 && searchState == SearchType::PV) {
				result = true;
			}
		}

		/**
		 * Set the search variables to a nullmove search
		 */
		void setNullmove() {
			searchState = SearchType::NULLMOVE;
			remainingDepth -= SearchParameter::getNullmoveReduction(ply, remainingDepth);
			alpha = beta - 1;
		}

		/**
		 * Set the search variables to a nullmove search
		 */
		void unsetNullmove() {
			searchState = SearchType::NORMAL;
			remainingDepth = remainingDepthAtPlyStart;
			alpha = alphaAtPlyStart;
			// Nullmoves are never done in check
			sideToMoveIsInCheck = false;
		}

		/**
		 * Sets the search to a null window search
		 */
		void setNullWindowSearch() {
			beta = alpha + 1;
			searchState = SearchType::NULL_WINDOW;
			keepBestMoveUnchanged = true;
		}

		/**
		 * Sets the search to PV (open window) search
		 */
		void setResearchWithPVWindow() {
			// Needed to ensure that the research will be > bestValue and set the PV
			bestValue = alpha;
			beta = betaAtPlyStart;
			searchState = SearchType::PV;
			keepBestMoveUnchanged = false;
		}

		/**
		 * Selects the next move to try
		 */
		Move selectNextMove(MoveGenerator& board) {
			Move move;
			if (bestValue >= betaAtPlyStart) {
				return move;
			}
			if (searchState == SearchType::NULL_WINDOW && currentValue >= beta) {
				setResearchWithPVWindow();
				return moveProvider.getCurrentMove();
				assert(!move.isEmpty());
			}
			if (searchState == SearchType::PV && (moveProvider.getTriedMovesAmount() > 0) && remainingDepth >= 2) {
				setNullWindowSearch();
			}
			move = moveProvider.selectNextMove(board);
			return move;
		}

		/**
		 * Multi thread variant to select the next move
		 */
		Move selectNextMoveThreadSafe(MoveGenerator& board) {
			std::lock_guard<std::mutex> lockGuard(mtxSearchResult);
			return selectNextMove(board);
		}

		/**
		 * applies the search result to the status
		 */
		void setSearchResult(value_t searchResult, const SearchVariables& nextPlySearchInfo, Move currentMove) {
			currentValue = searchResult;
			if (searchState == SearchType::NULL_WINDOW) {
				return;
			}
			if (searchResult > bestValue) {
				bestValue = searchResult;
				if (searchResult > alpha) {
					if (!keepBestMoveUnchanged) {
						bestMove = currentMove;
						if (remainingDepth > 0 && searchState == SearchType::PV) {
							pvMovesStore.copyFromPV(nextPlySearchInfo.pvMovesStore, ply + 1);
						}
						pvMovesStore.setMove(ply, bestMove);
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
				ttPtr->setEntry(hashKey, remainingDepthAtPlyStart, ply, bestMove, bestValue, alphaAtPlyStart, betaAtPlyStart, false);
				// WhatIf::whatIf.setTT(ttPtr, hashKey, remainingDepthAtPlyStart, ply, bestMove, bestValue, alphaAtPlyStart, betaAtPlyStart, false);
			}
		}

		/**
		 * terminates the search-ply
		 */
		void terminatePly(MoveGenerator& board) {

			if (cutoff == Cutoff::NONE && bestValue != -MAX_VALUE && !bestMove.isEmpty() && !bestMove.isNullMove()) {
				moveProvider.setKillerMove(bestMove);
				setTTEntry(board.computeBoardHash());
			}
			undoMove(board);
		}

		bool isInPV() { return searchState == SearchType::PV; }

		Move getMoveFromPVMovesStore(ply_t ply) const { return pvMovesStore.getMove(ply); }
		const KillerMove& getKillerMove() const { return moveProvider.getKillerMove(); }
		/**
		 * Gets the move proposed by the tt
		 */
		Move getTTMove() const { return moveProvider.getTTMove(); }
		void setPly(ply_t curPly) { ply = curPly; }

		string getSearchStateName() const {
			return searchStateNames[int32_t(searchState)];
		}

		/**
		 * Sets the transposition tables
		 */
		void setTT(TT* tt) {
			ttPtr = tt;
		}

		inline bool isTTValueBelowBeta(const Board& board, ply_t ply) {
			return ttPtr->isTTValueBelowBeta(positionHashSignature, beta, ply);
		}

		enum class SearchType {
			START, PV, NULL_WINDOW, PV_LMR, NORMAL, LMR, NULLMOVE, VERIFY, IID,
			AMOUNT
		};

		const array<string, int(SearchType::AMOUNT)> searchStateNames
		{ "Start", "PV", "NullW", "PV_LMR", "Normal", "LMR", "NullM", "Verify", "IID" };

		value_t alpha;
		value_t alphaAtPlyStart;
		value_t beta;
		value_t betaAtPlyStart;
		value_t bestValue;
		value_t currentValue;
		value_t eval;
		Move bestMove;
		Move previousMove;
		ply_t remainingDepth;
		ply_t remainingDepthAtPlyStart;
		ply_t ply;
		ply_t lateMoveReduction;
		ply_t searchDepthExtension;
		BoardState boardState;
		SearchType searchState;
		hash_t positionHashSignature;
		bool noNullmove;
		bool sideToMoveIsInCheck;
		bool keepBestMoveUnchanged;

		Cutoff cutoff;
		mutex mtxSearchResult;
		MoveProvider moveProvider;
		PV pvMovesStore;

	private:
		TT* ttPtr;
	};

}

#endif // __SEARCHVARIABLES_H