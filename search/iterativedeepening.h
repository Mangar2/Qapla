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
 * Iteratively deepens the search ply by ply
 */

#ifndef __ITERATIVEDEEPENING_H
#define __ITERATIVEDEEPENING_H


#include "movehistory.h"
#include "search.h"
#include "search-thread.h"
#include "extra-search.h"
#include "../interface/clocksetting.h"
#include "computinginfo.h"
#include "clockmanager.h"
#include "../interface/isendsearchinfo.h"
#include "tt.h"
#include "aspirationwindow.h"

#include <algorithm>
#include <memory>

namespace QaplaSearch {

	class IterativeDeepening {

	public:
		IterativeDeepening() { 
			_tt.setSizeInKilobytes(32736); 
			setUpThreads();
			clearMemories();
		}

		static const uint64_t ESTIMATED_TIME_FACTOR_FOR_NEXT_DEPTH = 4;

		static const uint32_t MAX_SEARCH_DEPTH = 128;

		/**
		 * Starts a new game or sets a new position e.g. by fen
		 */
		void startNewGame() {
			clearMemories();
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
			for (uint32_t index = 0; index < _threads.size(); index++) {
				_threads[index].search.clearMemories();
			}
		}

		/**
		 * Sets the size of the transposition table in kilobytes
		 */
		void setTTSizeInKilobytes(int32_t size) {
			_tt.setSizeInKilobytes(size);
		}

		void setMultiPV(int32_t count) {
			_multiPV = count;
			_search->setMultiPV(count);
		}

		void setThreads(int32_t threads) {
			_threadCount = threads;
		}

		/**
		 * Sets the number of searches that run beside the master's, see ExtraSearch. Each of
		 * them gets as many threads as the master's search.
		 */
		void setSearches(int32_t searches) {
			const size_t extra = size_t(std::max(0, searches - 1));
			while (_extraSearches.size() > extra) _extraSearches.pop_back();
			while (_extraSearches.size() < extra) _extraSearches.push_back(std::make_unique<ExtraSearch>());
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
		ComputingInfo searchByIterativeDeepening(
			const MoveGenerator& position, const std::vector<Move>& searchMoves, MoveHistory& moveHistory)
		{

			MoveGenerator searchBoard = position;
			if (_clockManager.isAnalyzeMode()) {
				clearMemories();
				/*
				auto fen = position.getFen();
				std::replace(fen.begin(), fen.end(), '/', '_');
				_tt.read("tt_in_" + fen + ".bin");
				*/
			}
			else {
				_tt.newSearch();
				/*
				auto fen = position.getFen();
				std::replace(fen.begin(), fen.end(), '/', '_');
				_tt.write("tt_out_" + fen + ".bin");
				*/
			}
			for (auto& window : _window) {
				window.initSearch();
			}	
			setUpThreads();
			_search->startNewSearch(searchBoard, searchMoves,
				moveHistory.hasRepeatedPosition(searchBoard));
			setUpHelpers(searchBoard);
			_clockManager.setNewMove();
			if (_search->getComputingInfo().getMovesAmount() == 0) {
				return _search->getComputingInfo();
			}
			ply_t maxDepth = SearchConfig::MAX_SEARCH_DEPTH - 28;
			const ply_t depthLimit = _clockSetting.getSearchDepthLimit();
			if (depthLimit > 0 && depthLimit < maxDepth) {
				maxDepth = depthLimit;
			}

			SearchStack& stack = _threads.master().stack;
			
			// tt.readFromFile("C:\\Programming\\chess\\Qapla\\Qapla\\tt.bin");
			moveHistory.setDrawPositionsToHash(position, _tt);

			// The extra searches start now and run until the master's search is done
			for (auto& extra : _extraSearches) {
				extra->start(searchBoard, searchMoves, moveHistory.hasRepeatedPosition(searchBoard), _threadCount, &_tt);
			}

			for (ply_t curDepth = 0; curDepth < maxDepth; curDepth++) {
				stack.clear();
				searchOneIteration(searchBoard, stack, curDepth);
				_clockManager.setSearchResult(curDepth, _search->getComputingInfo().getPVMoveValueInCentiPawn(0));
				if (!_clockManager.mayComputeNextDepth(curDepth)) {
					break;
				}
				if (hasMateFound(_search->getComputingInfo()) && _clockManager.stopSearchOnMateFound()) {
					break;
				}
			}

			// The next search hands the helpers new settings, so they must be idle before
			// this one returns
			_threads.waitUntilIdle();
			for (auto& extra : _extraSearches) {
				extra->stop();
			}
			if (!_extraSearches.empty() && _verbose) {
				uint64_t extraNodes = 0;
				for (auto& extra : _extraSearches) extraNodes += extra->getNodesSearched();
				std::cout << "info string extra searches " << _extraSearches.size() << " nodes " << extraNodes << std::endl;
			}

			// tt.writeToFile("tt.bin");
			// Ensures that all draw positions are removed and not used after undo or new game
			moveHistory.removeDrawPositionsFromHash(_tt);
			//static int i = 0;
			// tt.writeToFile("tt" + to_string(i) + ".bin"); i++;
			return _search->getComputingInfo();
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
			_sendSearchInfo = sendSearchInfo;
			_search->setSendSearchInfoInterface(sendSearchInfo);
		}

		/**
		 * Stores the requests to print search information.
		 * Next time the search calls print search info, it will be printed and the
		 * request flag will be set to false again
		 */
		void requestPrintSearchInfo() {
			_search->requestPrintSearchInfo();
		}


	private:

		/**
		 * Creates the threads the option asks for. Helper threads are started once and kept;
		 * the master is the first one and runs on the calling thread.
		 */
		void setUpThreads() {
			if (_threads.size() == uint32_t(_threadCount)) return;
			_threads.resize(_threadCount, &_tt);
			_search = &_threads.master().search;
			_search->setSendSearchInfoInterface(_sendSearchInfo, _verbose);
			_search->setMultiPV(_multiPV);
		}

		/**
		 * Hands every helper the settings of the new search. A helper's board stands at the
		 * root between jobs, it replays the line to every node it helps at from there.
		 */
		void setUpHelpers(const MoveGenerator& root) {
			for (uint32_t index = 1; index < _threads.size(); index++) {
				SearchThread& helper = _threads[index];
				helper.search.initAsHelper(*_search, &_clockManager, &_tt);
				helper.position = root;
			}
		}

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
		void searchOneIteration(MoveGenerator& position, SearchStack& stack, uint32_t searchDepth)
		{
			const auto multiPV = _search->getMultiPV();
			for (uint32_t i = 0; i < multiPV; ++i) {
				_window[i].newDepth(searchDepth);
			}
			uint32_t numberOfPVSearchedMoves = 0;
			//uint32_t iterations = 0;
			do {
				const auto alphaRed = std::max(0, int32_t(multiPV) - int32_t(numberOfPVSearchedMoves) - 1) * 5;
				stack.initSearchAtRoot(position, _window[numberOfPVSearchedMoves].getAlpha() - alphaRed, _window[numberOfPVSearchedMoves].getBeta(), searchDepth, _search->getPawnTT());
				_clockManager.setCalculationDepth(searchDepth);
				_search->negaMaxRoot(position, stack, multiPV - 1, _clockManager);
				const auto& computingInfo = _search->getComputingInfo();
				numberOfPVSearchedMoves = computingInfo.countPVSearchedMovesInWindow(searchDepth);
				const auto multiPVPos = std::min(numberOfPVSearchedMoves, multiPV - 1);
				const value_t positionValue = computingInfo.getPVMoveValueInCentiPawn(multiPVPos);
				_clockManager.setIterationResult(_window[multiPVPos].getAlpha(), _window[multiPVPos].getBeta(), positionValue);
				_window[multiPVPos].setSearchResult(positionValue);
				/*
				iterations++;
				if (iterations > 10) {
					cout << numberOfPVSearchedMoves << " " << multiPVPos << " " << positionValue << endl;
					_window[multiPVPos].print();
					position.print();
				}
				*/
			} while (!_clockManager.shouldAbort() && numberOfPVSearchedMoves < multiPV);

		}

		static const uint32_t MAX_PV = 40;
		ClockSetting _clockSetting;
		ClockManager _clockManager;
		TT _tt;
		// The master's search, owned by the first of the threads
		Search* _search = nullptr;
		int32_t _threadCount = 1;
		int32_t _multiPV = 1;
		SearchThreads _threads;
		// Searches beside the master's, contributing through the transposition table only
		std::vector<std::unique_ptr<ExtraSearch>> _extraSearches;
		ISendSearchInfo* _sendSearchInfo = nullptr;
		bool _verbose = true;
		array<AspirationWindow, MAX_PV> _window;
	};

}


#endif // __ITERATIVEDEEPENING_H
