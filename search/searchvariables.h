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
// #include "whatif.h"
#include "../basics/move.h"
#include "../movegenerator/movegenerator.h"
#include "killermove.h"
#include "pv.h"
#include "moveprovider.h"
#include "tt.h"
#include "searchparameter.h"
#include "extension.h"

using namespace std;
using namespace ChessMoveGenerator;

enum class Cutoff {
	NONE, DRAW_BY_REPETITION, HASH, FASTER_MATE_FOUND, RAZORING, NOT_ENOUGH_MATERIAL, NULL_MOVE,
	COUNT
};

namespace ChessSearch {
	struct SearchVariables {

		typedef uint32_t pvIndex_t;

		SearchVariables() {
			cutoff = Cutoff::NONE;
		};

		SearchVariables(const SearchVariables& searchInfo) {
			operator=(searchInfo);
		}

		SearchVariables& operator=(const SearchVariables& searchInfo) {
			alpha = searchInfo.alpha;
			beta = searchInfo.beta;
			remainingDepth = searchInfo.remainingDepth;
			bestMove = searchInfo.bestMove;
			bestValue = searchInfo.bestValue;
			moveNo = searchInfo.moveNo;
			ply = searchInfo.ply;
			moveProvider = searchInfo.moveProvider;
			searchState = searchInfo.searchState;
			noNullmove = searchInfo.noNullmove;
			return *this;
		}

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
			moveNo = 0;
			doMove(board, previousPlyMove);
			cutoff = checkBasicCutoffs(board);
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
			moveNo = 0;
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
			if (previousPlyMove == Move::NULL_MOVE) {
				board.doNullmove();
			}
			else {
				board.doMove(previousMove);
			}
			positionHashSignature = board.computeBoardHash();
		}

		/**
		 * Take back the previously applied move
		 */
		void undoMove(MoveGenerator& board) {
			if (previousMove.isEmpty()) {
				return;
			}
			if (previousMove == Move::NULL_MOVE) {
				board.undoNullmove(boardState);
			}
			else {
				board.undoMove(previousMove, boardState);
			}
		}

		/**
		 * Check basic cutoff rules
		 */
		Cutoff checkBasicCutoffs(MoveGenerator& board) {
			Cutoff result = Cutoff::NONE;
			if (alpha > MAX_VALUE - value_t(ply)) {
				result = Cutoff::FASTER_MATE_FOUND;
				bestValue = alpha;
			}
			else if (beta < -MAX_VALUE + value_t(ply)) {
				result = Cutoff::FASTER_MATE_FOUND;
				bestValue = beta;
			}
			else if (ply >= 3 && board.drawDueToMissingMaterial()) {
				cutoff = Cutoff::NOT_ENOUGH_MATERIAL;
				bestValue = 0;
			}
			return result;
		}

		/**
		 * Gets an entry from the transposition table
		 */
		void getTTEntry(MoveGenerator& board) {

			if (cutoff == Cutoff::NONE) {
				uint32_t ttIndex = ttPtr->getTTEntryIndex(positionHashSignature);

				if (ttIndex != TT::INVALID_INDEX) {
					TTEntry entry = ttPtr->getEntry(ttIndex);
					Move move = entry.getMove();
					moveProvider.setHashMove(move);
					// keeps the best move for a tt entry, if the search does stay <= alpha
					if (searchState != SearchType::PV) {
						bestMove = move;
						if (entry.getValue(bestValue, alpha, beta, remainingDepth, ply)) {
							cutoff = Cutoff::HASH;
						}
						if (bestValue >= beta) {
							cutoff = Cutoff::HASH;
						}
					}
					// Narrowing the window by the tt is problematic. The search will be made with a wrong window
					betaAtPlyStart = beta;
					alphaAtPlyStart = alpha;
				}
			}
		}

		inline void cutoffSearch(Cutoff cutoffType, value_t cutoffResult) {
			// Do not "overwrite" older cutoff informations
			if (cutoff == Cutoff::NONE && cutoffType != Cutoff::NONE) {
				cutoff = cutoffType;
				bestValue = cutoffResult;
			}
		}

		/**
		 * Cutoff the current search
		 */
		inline void cutoffSearch(Cutoff cutoffType) {
			if (cutoff == Cutoff::NONE && cutoffType != Cutoff::NONE) {
				cutoff = cutoffType;
			}
		}


		void extendSearch(MoveGenerator& board) {
			assert(cutoff == Cutoff::NONE);
			searchDepthExtension = Extension::calculateExtension(board, previousMove, remainingDepth);
			remainingDepth += searchDepthExtension;
		}

		/**
		 * Generates all moves in the current position
		 */
		void generateMoves(MoveGenerator& board) {
			assert(cutoff == Cutoff::NONE);
			moveProvider.computeMoves(board, previousMove, remainingDepth);
			bestValue = moveProvider.checkForGameEnd(board, ply);
			sideToMoveIsInCheck = board.isInCheck();
		}

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
		}

		/**
		 * Sets a state used in the search-move-loop
		 */
		Move setSearchState(MoveGenerator& board) {
			Move move;
			switch (searchState) {
			case SearchType::PV:
				if (!bestMove.isEmpty() && remainingDepth >= 2) {
					beta = alpha + 1;
					searchState = SearchType::NULL_WINDOW;
					keepBestMoveUnchanged = true;
				}
				break;
			case SearchType::NULL_WINDOW:
				if (currentValue >= beta) {
					// Needed to ensure that the research will be > bestValue and set the PV
					bestValue = alpha;
					beta = betaAtPlyStart;
					searchState = SearchType::PV;
					move = moveProvider.getCurrentMove();
					keepBestMoveUnchanged = false;
				}
				break;
			case SearchType::NORMAL:
				lateMoveReduction = SearchParameter::getLateMoveReduction(false, ply, moveProvider.getTriedMovesAmount());
				if (lateMoveReduction > 0) {
					searchState = SearchType::LMR;
					keepBestMoveUnchanged = true;
					remainingDepth -= lateMoveReduction;
				}

				break;
			case SearchType::LMR:
				if (currentValue >= beta) {
					// Research
					searchState = SearchType::NORMAL;
					remainingDepth = remainingDepthAtPlyStart;
					move = moveProvider.getCurrentMove();
					keepBestMoveUnchanged = false;
					lateMoveReduction = 0;
				}
				break;
			}
			return move;
		}

		/**
		 * Selects the next move to try
		 */
		Move selectNextMove(MoveGenerator& board) {
			assert(cutoff == Cutoff::NONE);
			Move move = setSearchState(board);
			if (bestValue < betaAtPlyStart && move.isEmpty()) {
				moveNo++;
				move = moveProvider.selectNextMove(board);
			}
			return move;

		}

		/**
		 * Multi thread variant to select the next move
		 */
		Move selectNextMoveThreadSafe(MoveGenerator& board) {
			std::lock_guard<std::mutex> lockGuard(mtxSearchResult);
			Move move;
			if (cutoff == Cutoff::NONE) {
				move = selectNextMove(board);
			}
			return move;
		}

		/**
		 * applies the search result to the status
		 */
		void setSearchResult(value_t searchResult, const SearchVariables& nextPlySearchInfo, Move currentMove) {
			currentValue = searchResult;
			if (searchResult > bestValue) {
				bestValue = searchResult;
				if (searchResult > alpha && currentMove != Move::NULL_MOVE) {
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

					else if (remainingDepth > 1) {
						// HistoryTable::setSearchResult(remainingDepth, bestMove, moveProvider);
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
				// WHATIF(WhatIf::whatIf.setHash(ttPtr, hashKey, remainingDepthAtPlyStart, ply, bestMove, bestValue, alphaAtPlyStart, betaAtPlyStart, false));
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

		inline bool isTTValueBelowBeta(const Board& board) {
			return ttPtr->isTTValueBelowBeta(board.computeBoardHash(), beta);
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
		Move bestMove;
		Move previousMove;
		ply_t remainingDepth;
		ply_t remainingDepthAtPlyStart;
		ply_t ply;
		ply_t lateMoveReduction;
		ply_t searchDepthExtension;
		BoardState boardState;
		uint32_t moveNo;
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