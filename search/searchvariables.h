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

namespace ChessSearch {
	struct SearchVariables {

		typedef uint32_t pvIndex_t;
		typedef uint32_t cutoff_t;

		SearchVariables() {
			cutoff = CUTOFF_NONE;
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
			if (doIID()) {
				searchState = SearchType::IID;
				remainingDepth = 1;
			}
			else if (beta > alpha + 1) {
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
			bestMove = Move::EMPTY_MOVE;
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
			bestMove = Move::EMPTY_MOVE;
			bestValue = -MAX_VALUE;
			searchState = SearchType::PV;
			noNullmove = true;
			moveNo = 0;
			cutoff = CUTOFF_NONE;
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
		cutoff_t checkBasicCutoffs(MoveGenerator& board) {
			cutoff_t result = CUTOFF_NONE;
			if (alpha > MAX_VALUE - value_t(ply)) {
				result = CUTOFF_FASTER_MATE_FOUND;
				bestValue = alpha;
			}
			else if (beta < -MAX_VALUE + value_t(ply)) {
				result = CUTOFF_FASTER_MATE_FOUND;
				bestValue = beta;
			}
			else if (ply >= 3 && board.drawDueToMissingMaterial()) {
				cutoff = CUTOFF_NOT_ENOUGH_MATERIAL;
				bestValue = 0;
			}
			return result;
		}

		/**
		 * Gets an entry from the transposition table
		 */
		void getTTEntry(MoveGenerator& board) {

			if (cutoff == CUTOFF_NONE) {
				uint32_t ttIndex = ttPtr->getTTEntryIndex(positionHashSignature);

				if (ttIndex != TT::INVALID_INDEX) {
					TTEntry entry = ttPtr->getEntry(ttIndex);
					Move move = entry.getMove();
					moveProvider.setHashMove(move);
					// keeps the best move for a tt entry, if the search does stay <= alpha
					if (searchState != SearchType::PV) {
						bestMove = move;
					}
					if (entry.getValue(bestValue, alpha, beta, remainingDepth, ply)) {
						cutoff = CUTOFF_HASH;
					}
					if (bestValue >= beta) {
						cutoff = CUTOFF_HASH;
					}
					// Narrowing the window by the tt is problematic. The search will be made with a wrong window
					betaAtPlyStart = beta;
					alphaAtPlyStart = alpha;
				}
			}
		}

		inline void cutoffSearch(uint32_t cutoffType, value_t cutoffResult) {
			// Do not "overwrite" older cutoff informations
			if (cutoff == CUTOFF_NONE && cutoffType != CUTOFF_NONE) {
				cutoff = cutoffType;
				bestValue = cutoffResult;
			}
		}

		inline void cutoffSearch(uint32_t cutoffType) {
			if (cutoff == CUTOFF_NONE && cutoffType != CUTOFF_NONE) {
				cutoff = cutoffType;
			}
		}

		void addNullmove() {
			searchState = SearchType::SELECT_NULLMOVE;
		}

		void extendSearch(MoveGenerator& board) {
			assert(cutoff == CUTOFF_NONE);
			searchDepthExtension = Extension::calculateExtension(board, previousMove, remainingDepth);
			remainingDepth += searchDepthExtension;
		}

		void generateMoves(MoveGenerator& board) {
			assert(cutoff == CUTOFF_NONE);
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

		void setStateToNullmove() {
			searchState = SearchType::NULLMOVE;
			remainingDepth -= SearchParameter::getNullmoveReduction(ply, remainingDepth);
			alpha = beta - 1;
			if (remainingDepth < 1) {
				remainingDepth = 1;
			}
			keepBestMoveUnchanged = true;
		}


		Move setSearchState(MoveGenerator& board) {
			Move move = Move::EMPTY_MOVE;
			switch (searchState) {
			case SearchType::START: selectFirstSearchState(); break;
			case SearchType::IID:
				remainingDepth += 4;
				if (remainingDepth >= remainingDepthAtPlyStart - 3) {
					remainingDepth = remainingDepthAtPlyStart;
					searchState = SearchType::NORMAL;
				}
				break;
			case SearchType::PV:
				if (bestMove != Move::EMPTY_MOVE && remainingDepth >= 2) {
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
			case SearchType::SELECT_NULLMOVE:
				setStateToNullmove();
				move = Move::NULL_MOVE;
				break;
			case SearchType::NULLMOVE:
				if (bestValue < beta) {
					bestValue = -MAX_VALUE;
					alpha = alphaAtPlyStart;
					remainingDepth = remainingDepthAtPlyStart;
					keepBestMoveUnchanged = false;
					searchState = SearchType::NORMAL;
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
			assert(cutoff == CUTOFF_NONE);
			Move move = setSearchState(board);
			if (bestValue < betaAtPlyStart && move == Move::EMPTY_MOVE) {
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
			Move move = Move::EMPTY_MOVE;
			if (cutoff == CUTOFF_NONE) {
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
			if (searchResult >= beta && currentMove == Move::NULL_MOVE) {
				//remainingDepth -= SearchParameter::getNullmoveVerificationDepthReduction(ply, remainingDepth);
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
		void setHashEntry(hash_t hashKey) {
			bool drawByRepetetivePositionValue = (bestValue == 0);
			if (!drawByRepetetivePositionValue) {
				ttPtr->setHashEntry(hashKey, remainingDepthAtPlyStart, ply, bestMove, bestValue, alphaAtPlyStart, betaAtPlyStart, false);
				// WHATIF(WhatIf::whatIf.setHash(ttPtr, hashKey, remainingDepthAtPlyStart, ply, bestMove, bestValue, alphaAtPlyStart, betaAtPlyStart, false));
			}
		}

		/**
		 * terminates the search-ply
		 */
		void terminatePly(MoveGenerator& board) {

			if (cutoff == CUTOFF_NONE && bestMove != Move::NULL_MOVE) {
				moveProvider.setKillerMove(bestMove);
				setHashEntry(board.computeBoardHash());
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
			START, PV, NULL_WINDOW, PV_LMR, NORMAL, LMR, SELECT_NULLMOVE, NULLMOVE, VERIFY, IID
		};

		const string searchStateNames[8] = { "Start", "PV", "NULL", "Normal", "SelNull", "Null", "Verify", "IID" };

		enum cutoffTypes {
			CUTOFF_NONE, CUTOFF_DRAW_BY_REPETITION, CUTOFF_HASH, CUTOFF_FASTER_MATE_FOUND, CUTOFF_RAZORING, CUTOFF_NOT_ENOUGH_MATERIAL
		};

		static const cutoff_t CUTOFF_TYPE_AMOUNT = 5;

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

		cutoff_t cutoff;
		mutex mtxSearchResult;
		MoveProvider moveProvider;
		PV pvMovesStore;

	private:
		TT* ttPtr;
	};

}

#endif // __SEARCHVARIABLES_H