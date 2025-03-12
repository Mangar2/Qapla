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
#include "butterfly-boards.h"
#include "searchparameter.h"
#include "extension.h"
#include "../eval/eval.h"
#ifdef USE_STOCKFISH_EVAL
#include "../nnue/engine.h"
#endif

using namespace std;
using namespace QaplaMoveGenerator;
using namespace ChessEval;

enum class Cutoff {
	NONE, DRAW_BY_REPETITION, HASH, FASTER_MATE_FOUND, RAZORING, NOT_ENOUGH_MATERIAL, 
	NULL_MOVE, FUTILITY, BITBASE, LOST_WINNING_BONUS, MAX_SEARCH_DEPTH, ABORT,
	COUNT
};

namespace QaplaSearch {
	struct SearchVariables {

		enum class SearchFinding {
			PV, NULL_WINDOW, PV_LMR, NORMAL, LMR, NULLMOVE, VERIFY, IID, SE,
			AMOUNT
		};

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
		 * @returns true, if the current search is a PV search
		 */
		inline bool isWindowZero() const { return alpha + 1 == beta;  }
		inline bool isPVNode() const { return _nodeType == NodeType::PV;  }
		inline bool isOldPVNode() const { return alphaAtPlyStart + 1 < betaAtPlyStart; }

		void setWindowAtPlyStart(value_t newAlpha, value_t newBeta) {
			alphaAtPlyStart = alpha = newAlpha;
			betaAtPlyStart = beta = newBeta;
		}	

		/**
		 * Sets all variables from previous ply
		 */
		void setFromParentNode(MoveGenerator& position, const SearchVariables& parentNode, value_t alpha, value_t beta, ply_t depth, bool isPVNode) {
			pvMovesStore.setEmpty(ply);
			pvMovesStore.setEmpty(ply + 1);
			bestMove.setEmpty();
			bestValue = -MAX_VALUE;
			cutoff = Cutoff::NONE;
			ttValueIsUpperBound = false;
			adjustedEval = NO_VALUE;
			isImproving = false;
			remainingDepth = depth;
			remainingDepthAtPlyStart = depth;
			setWindowAtPlyStart(alpha, beta);
			moveNumber = 0;
			_nodeType = isPVNode? NodeType::PV : parentNode._nodeType == NodeType::ALL ? NodeType::CUT : NodeType::ALL;
			isVerifyingNullmove = parentNode.isVerifyingNullmove;
			noNullmove = isVerifyingNullmove || parentNode.previousMove.isNullMove() || previousMove.isNullMove();
			moveProvider.init();
		}

		void setToPlyStart() {
			pvMovesStore.setEmpty(ply);
			pvMovesStore.setEmpty(ply + 1);
			bestValue = -MAX_VALUE;
			remainingDepth = remainingDepthAtPlyStart;
			alpha = alphaAtPlyStart;
			beta = betaAtPlyStart;
			moveNumber = 0;
		}

		/**
		 * Initializes all variables to start search
		 */
		void initSearchAtRoot(MoveGenerator& position, value_t initialAlpha, value_t initialBeta, ply_t searchDepth) {
			remainingDepthAtPlyStart = remainingDepth = searchDepth;
			moveNumber = 0;
			alpha = initialAlpha;
			beta = initialBeta;
			alphaAtPlyStart = alpha;
			betaAtPlyStart = beta;
			bestMove.setEmpty();
			bestValue = -MAX_VALUE;
			_nodeType = NodeType::PV;
			isVerifyingNullmove = false;
			noNullmove = true;
			cutoff = Cutoff::NONE;
			positionHashSignature = position.computeBoardHash();
			ttValueIsUpperBound = false;
			eval = adjustedEval = sideToMoveIsInCheck ? NO_VALUE : Eval::eval(position); 
			isImproving = false;
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
#ifdef USE_STOCKFISH_EVAL
			Stockfish::Engine::undoMove(previousMove);
#endif
		}

		/**
		 * Gets an entry from the transposition table
		 */
		bool probeTT(bool isPVNode, value_t alpha, value_t beta, ply_t depth, ply_t ply) {
			assert(positionHashSignature != 0);
			uint32_t ttIndex = ttPtr->getTTEntryIndex(positionHashSignature);
			ttMove = Move::EMPTY_MOVE;
			ttValue = NO_VALUE;
			eval = NO_VALUE;
			if (ttIndex == TT::INVALID_INDEX) return false;

			const TTEntry entry = ttPtr->getEntry(ttIndex);
			ttMove = entry.getMove();
			eval = entry.getEval();
			if (entry.alwaysUseValue()) {
				bestValue = entry.getPositionValue(ply);
				return true;
			}

			ttValueIsUpperBound = entry.isValueUpperBound();
			if (entry.isValueExact()) {
				adjustedEval = entry.getPositionValue(ply);
			}

			ttValue = entry.getValue(ply);
			ttDepth = entry.getComputedDepth();
			// We do not need to keep bestmove, as the tt will not overwrite a move with an empty move
			// bestMove = move;
			if (!isPVNode) {
				const auto cutoffValue = entry.getTTCutoffValue(alpha, beta, depth, ply);
				// We ignore ttValue of 0 indicating repetetive draw
				if (cutoffValue != NO_VALUE && cutoffValue != 0) {
					bestValue = cutoffValue;
					return true;
				}
			}

			return false;
		}

		void setHashSignature(const MoveGenerator& position) {
			positionHashSignature = position.computeBoardHash();
		}

		void printTTEntry() {
			ttPtr->printHash(positionHashSignature);
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
		auto extendSearch(MoveGenerator& position, ply_t depthAtRoot, ply_t seExtension) {
			searchDepthExtension = Extension::calculateExtension(position, previousMove, remainingDepth, seExtension);
			remainingDepth += searchDepthExtension;
			return std::min(remainingDepth, depthAtRoot * 2);
		}

		/**
		 * Examine, if we can do a futility pruning based on an evaluation score
		 */
		inline bool futility(MoveGenerator& position) {
			if (SearchParameter::DO_FUTILITY_DEPTH <= remainingDepth) return false;
			// We prune, if eval - margin is >= beta. This term prevents pruning below beta on negative futility margins.
			if (adjustedEval < beta) return false;
			// We do not prune in PV nodes. 
			if (isPVNode()) return false;
			// We do not prune, if we have a silent TT move, because silent TT moves are only available, if they have been in the search window before.
			if (!getTTMove().isEmpty() && !getTTMove().isCapture()) return false; 
			if (ttValueIsUpperBound) return false;
			// We do not prune on potentional mate values
			if (adjustedEval > WINNING_BONUS) return false;
			// Do not prune on window indicating mate values
			if (alpha > WINNING_BONUS || beta < -WINNING_BONUS) return false;

			const bool doFutility = adjustedEval - SearchParameter::futilityMargin(remainingDepth, isImproving) >= beta;
			if (doFutility) {
				bestValue = beta + (adjustedEval - beta) / 10;
			}
			return doFutility;
		}

		/**
		 * Generates all moves in the current position
		 */
		void computeMoves(MoveGenerator& position, ButterflyBoard& butterflyBoard) {
			position.isWhiteToMove();
			checkingBitmaps = position.computeCheckBitmapsForMovingColor();
			moveProvider.computeMoves(position, butterflyBoard, previousMove, ttMove);
			bestValue = moveProvider.checkForGameEnd(position, ply);
		}

		bool isCheckMove(MoveGenerator& position, Move move) {
			return position.isCheckMove(move, checkingBitmaps);
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
		}

		/**
		 * Sets the search to PV (open window) search
		 */
		void setPVWindow() {
			beta = betaAtPlyStart;
		}

		void setSE(value_t margin) {
			const auto seBeta = ttValue - margin;
			setWindowAtPlyStart(seBeta - 1, seBeta);
			// _searchState = SearchFinding::SE;
		}

		/**
		 * Selects the next move to try
		 */
		inline Move selectNextMove(MoveGenerator& position) {
			Move result = moveProvider.selectNextMove(position);
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
			assert(abs(searchResult) < MIN_MATE_VALUE || abs(searchResult) > MAX_VALUE - 50);
			currentValue = searchResult;
			if (searchResult > bestValue) {
				bestValue = searchResult;
				if (searchResult > alpha) {
					bestMove = currentMove;
					if (isPVNode()) {
						// PV line may be extended, thus always copy from pv
						pvMovesStore.copyFromPV(nextPlySearchInfo.pvMovesStore, ply + 1);
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
		void setTTEntry(hash_t hashKey, bool isPV) {
			ply_t depth = max(remainingDepthAtPlyStart, 0);
			ttPtr->setEntry(hashKey, isPV, depth, ply, bestMove, eval, bestValue, alphaAtPlyStart, betaAtPlyStart, false);
			// WhatIf::whatIf.setTT(ttPtr, hashKey, remainingDepthAtPlyStart, ply, bestMove, bestValue, alphaAtPlyStart, betaAtPlyStart, false);
		}

		/**
		 * Indicates that the PV failed low
		 */
		bool isFailLow() {
			return bestValue <= alphaAtPlyStart;
		}

		/**
		 * terminates the search-ply
		 */
		void updateTTandKiller(MoveGenerator& position, ButterflyBoard& butterflyBoard, bool isPV, ply_t depth) {
			if (cutoff == Cutoff::NONE && bestValue != -MAX_VALUE && !bestMove.isNullMove()) {
				if (!bestMove.isEmpty()) {
					moveProvider.setKillerMove(bestMove);
					butterflyBoard.newBestMove(bestMove, depth, moveProvider.getTriedMoves(), moveProvider.getTriedMovesAmount());
				}
				
				setTTEntry(position.computeBoardHash(), isPV);
			}
		}

		Move getMoveFromPVMovesStore(ply_t ply) const { return pvMovesStore.getMove(ply); }
		const KillerMove& getKillerMove() const { return moveProvider.getKillerMove(); }

		Move getTTMove() const { return ttMove; }
		void setTTMove(Move move) { ttMove = move; }

		void setPly(ply_t curPly) { ply = curPly; }

		
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
			std::cout << "[w:" << std::setw(6) << alphaAtPlyStart << "," << std::setw(6) << betaAtPlyStart << "]";
			std::cout << "[d:" << remainingDepth << "]";
			std::cout << "[v:" << std::setw(6) << bestValue << "]";
			std::cout << "[hm:" << std::setw(5) << getTTMove().getLAN() << "]";
			std::cout << "[bm:" << std::setw(5) << bestMove.getLAN() << "]";
			std::cout << "[nt:" << std::setw(4) << getNodeTypeName() << "]";
		
			if (isPVNode()) {
				std::cout << " [PV: ";
				pvMovesStore.print(ply);
				std::cout << " ]";
			}
		
			std::cout << std::endl;
		}

		inline bool isTTValueBelowBeta(const Board& position, ply_t ply) {
			return ttValue < beta;
		}


		enum class NodeType {
			PV, CUT, ALL, COUNT
		};


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
			{ "PV", "NullW", "PV_LMR", "Normal", "LMR", "NullM", "Verify", "IID", "SE"};

		value_t alpha;
		value_t alphaAtPlyStart;
		value_t beta;
		value_t betaAtPlyStart;
		value_t bestValue;
		value_t currentValue;
		value_t adjustedEval;
		value_t eval;
		Move bestMove;
		Move previousMove;
		int32_t moveNumber;
		ply_t remainingDepth;
		ply_t remainingDepthAtPlyStart;
		ply_t ply;
		ply_t searchDepthExtension;
		BoardState boardState;
		hash_t positionHashSignature;
		bool noNullmove;
		bool sideToMoveIsInCheck;
		bool ttValueIsUpperBound;
		bool isVerifyingNullmove;
		bool isImproving;
		value_t ttValue;
		ply_t ttDepth;
		Move ttMove;

		Cutoff cutoff;
		mutex mtxSearchResult;
		MoveProvider moveProvider;
		PV pvMovesStore;
		// Bitmaps to identify checking moves faster
		std::array<bitBoard_t, Piece::PIECE_AMOUNT / 2> checkingBitmaps;

	private:
		NodeType _nodeType;
		TT* ttPtr;

		static constexpr array<NodeType, 3> _nodeTypeMap = { NodeType::PV, NodeType::ALL, NodeType::CUT };
#ifdef USE_STOCKFISH_EVAL
		Stockfish::StateInfo si;
#endif

	};

}

#endif // __SEARCHVARIABLES_H