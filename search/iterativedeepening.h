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
 * Iteratively deepens the search ply by ply
 */

#ifndef __ITERATIVEDEEPENING_H
#define __ITERATIVEDEEPENING_H

#include <algorithm>
#include "../movegenerator/movegenerator.h"
#include "movehistory.h"
#include "quiescence.h"
#include "search.h"
#include "../interface/clocksetting.h"
#include "computinginfo.h"
#include "clockmanager.h"
#include "../interface/isendsearchinfo.h"
#include "tt.h"
#include "aspirationwindow.h"
#include "searchstate.h"

namespace QaplaSearch {

	class IterativeDeepening {

	public:
		IterativeDeepening() { 
			_tt.setSizeInKilobytes(32736); 
		}

		static const uint64_t ESTIMATED_TIME_FACTOR_FOR_NEXT_DEPTH = 4;

		static const uint32_t MAX_SEARCH_DEPTH = 128;

		/**
		 * Starts a new game or sets a new position e.g. by fen
		 */
		void startNewGame() {
			_tt.clear();
			_search.startNewGame();
		}

		/**
		 * Clears the hash for example on a new game
		 */
		void clearTT() {
			_tt.clear();
		}

		/**
		 * Clears all memories like cache, butterflyboards, ...
		 */
		void clearMemories() {
			_tt.clear();
			_search.clearMemories();
		}

		/**
		 * Sets the size of the transposition table in kilobytes
		 */
		void setTTSizeInKilobytes(int32_t size) {
			_tt.setSizeInKilobytes(size);
		}

		void setMultiPV(int32_t count) {
			_search.setMultiPV(count);
		}

		/**
		 * true, if the search found a mate
		 */
		bool hasMateFound(const ComputingInfo& computingInfo) {
			const value_t SECURITY_BUFFER = 2;
			bool result = false;
			if (abs(computingInfo.getPVMoveValueInCentiPawn(0)) > MAX_VALUE - (value_t)computingInfo.getSearchDepht() + SECURITY_BUFFER) {
				result = true;
			}
			return result;
		}

		/**
		 * Searches the best move by iteratively deepening the search depth
		 */
		ComputingInfo searchByIterativeDeepening(const MoveGenerator& position, MoveHistory& moveHistory)
		{

			MoveGenerator searchBoard = position;
			if (_clockManager.isAnalyzeMode()) {
				clearMemories();
			}
			else {
				_tt.setNextSearch();
			}
			for (auto& window : _window) {
				window.initSearch();
			}	
			_search.startNewSearch(searchBoard);
			_clockManager.setNewMove();

			ply_t maxDepth = SearchParameter::MAX_SEARCH_DEPTH - 28;
			const ply_t depthLimit = _clockSetting.getSearchDepthLimit();
			if (depthLimit > 0 && depthLimit < maxDepth) {
				maxDepth = depthLimit;
			}

			Move result;

			// tt.readFromFile("C:\\Programming\\chess\\Qapla\\Qapla\\tt.bin");
			moveHistory.setDrawPositionsToHash(position, _tt);

			static const uint8_t DEPTH_BUFFER = 0;
			for (ply_t curDepth = 0; curDepth < maxDepth; curDepth++) {
				searchOneIteration(searchBoard, curDepth);
				_clockManager.setSearchResult(curDepth, _search.getComputingInfo().getPVMoveValueInCentiPawn(0));
				if (!_clockManager.mayComputeNextDepth(curDepth)) {
					break;
				}
				if (hasMateFound(_search.getComputingInfo()) && _clockManager.stopSearchOnMateFound()) {
					break;
				}
			}

			// tt.writeToFile("tt.bin");
			// Ensures that all draw positions are removed and not used after undo or new game
			moveHistory.removeDrawPositionsFromHash(_tt);
			//static int i = 0;
			// tt.writeToFile("tt" + to_string(i) + ".bin"); i++;
			return _search.getComputingInfo();
		}

		/**
		 * Stops the serach
		 */
		void stopSearch() {
			_clockManager.stopSearch();
		}

		/**
		 * Signals a ponder hit
		 */
		void ponderHit() {
			_clockManager.setSearchMode();
		}

		/**
		 * Sets the clock for the next search
		 */
		void setClockForNextSearch(const ClockSetting& clockSetting) {
			_clockSetting = clockSetting;
			_clockManager.startCalculatingMove(60, clockSetting);
		}
		
		/**
		 * Sets the interface printing search information
		 */
		void setSendSearchInfoInterface(ISendSearchInfo* sendSearchInfo) {
			_search.setSendSearchInfoInterface(sendSearchInfo);
		}

		/**
		 * Stores the requests to print search information.
		 * Next time the search calls print search info, it will be printed and the
		 * request flag will be set to false again
		 */
		void requestPrintSearchInfo() {
			_search.requestPrintSearchInfo();
		}


	private:

		/**
		 * Computes the available time to search the next move
		 */
		uint64_t computeSearchTime(const ClockSetting& clockSetting) {
			uint32_t movesToSearchInTime = clockSetting.getMoveAmountForClock();
			if (movesToSearchInTime == 0) {
				movesToSearchInTime = 80;
			}

			uint64_t timeToSearchForNextMove =
				clockSetting.getTimeToThinkForAllMovesInMilliseconds() / movesToSearchInTime +
				clockSetting.getTimeIncrementPerMoveInMilliseconds();

			return timeToSearchForNextMove;
		}

		/**
		 * Searches one iteration - at constant search depth using an aspiration window
		 */
		void searchOneIteration(MoveGenerator& position, uint32_t searchDepth)
		{
			SearchStack stack(&_tt);
			bool isInWindow = false;
			const auto multiPV = _search.getMultiPV();
			for (uint32_t i = 0; i < multiPV; ++i) {
				_window[i].newDepth(searchDepth);
			}
			uint32_t numberOfPVSearchedMoves = 0;
			do {
				const auto alphaRed = std::max(0, int32_t(multiPV) - int32_t(numberOfPVSearchedMoves) - 1) * 5;
				stack.initSearchAtRoot(position, _window[numberOfPVSearchedMoves].getAlpha() - alphaRed, _window[numberOfPVSearchedMoves].getBeta(), searchDepth);
				_clockManager.setCalculationDepth(searchDepth);
				_search.negaMaxRoot(position, stack, multiPV - 1, _clockManager);
				const auto& computingInfo = _search.getComputingInfo();
				numberOfPVSearchedMoves = computingInfo.countPVSearchedMovesInWindow(searchDepth);
				const auto multiPVPos = std::min(numberOfPVSearchedMoves, multiPV - 1);
				const value_t positionValue = computingInfo.getPVMoveValueInCentiPawn(multiPVPos);
				_clockManager.setIterationResult(_window[multiPVPos].getAlpha(), _window[multiPVPos].getBeta(), positionValue);
				isInWindow = _window[multiPVPos].isInside(positionValue);
				_window[multiPVPos].setSearchResult(positionValue);

			} while (!_clockManager.shouldAbort() && numberOfPVSearchedMoves < multiPV);

		}

		static const uint32_t MAX_PV = 40;
		ClockSetting _clockSetting;
		ClockManager _clockManager;
		TT _tt;
		Search _search;
		array<AspirationWindow, MAX_PV> _window;
	};

}


#endif // __ITERATIVEDEEPENING_H