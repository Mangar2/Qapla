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
 * Implements a manager for computing time of a chess search
 */

#ifndef _CLOCKMANAGER_H
#define _CLOCKMANAGER_H

#include <time.h>
#include <algorithm>
#include <sys/timeb.h>
#include "../basics/types.h"
#include "../interface/clocksetting.h"
#include "searchstate.h"

using namespace std;
using namespace QaplaInterface;

namespace QaplaSearch {

	class ClockManager {
	public:
		ClockManager() {
			_verbose = false;
			_startTime = 0;
		}

		/**
		 * Computes the time spent so far
		 */
		int64_t computeTimeSpentInMilliseconds() const {
			return getSystemTimeInMilliseconds() - _startTime;
		}

		/**
		 * Sets the current time as start time
		 */
		void setStartTime() {
			_startTime = getSystemTimeInMilliseconds();
		}

		/**
		 * Starts calculation of next move
		 * @param movesToGo amount of moves to play in the time provided by clockSetting
		 * @param clockSetting the time available to play the chess game
		 */
		void startCalculatingMove(int32_t movesToGo, const ClockSetting& clockSetting)
		{
			setStartTime();
			_clockSetting = clockSetting;
			_maxTimePerMove = computeMaxTime();
			_mode = clockSetting.getMode();
			// must be last!
			setNewMove();
		}

		/**
		 * Checks, if calculation must be aborded due to time constrains
		 */
		bool mustAbortSearch(int32_t ply) {
			int64_t timeSpent = computeTimeSpentInMilliseconds();
			bool abort = false;
			if (_mode == ClockMode::stopped) {
				abort = true;
			}
			else if (_mode != ClockMode::search) {
				abort = false;
			}
			else if (timeSpent > _maxTimePerMove) {
				abort = true;
				stopSearch();
			}
			else if (ply == 0) {
				if (timeSpent > (_averageTimePerMove / 10) * 8) {
					abort = true;
					stopSearch();
				}
			}
			// Ensure that the information to stop the search is stable
			return abort;
		}

		/**
		 * Checks if there is suitable to start the calculation of the next depth
		 * @returns true, if calculation of next depth is ok
		 */
		bool mayComputeNextDepth() const {
			bool result;
			if (_mode == ClockMode::stopped) {
				result = false;
			}
			else if (_mode != ClockMode::search) {
				result = true;
			}
			else {
				int64_t time = min(_maxTimePerMove, (_averageTimePerMove / 10) * 7);
				result = computeTimeSpentInMilliseconds() < time;
			}
			return result;
		}

		/**
		 * Checks, if it is time to send the next information to the gui
		 */
		bool isTimeToSendNextInfo() {
			if (isSearchStopped()) return false;
			int64_t timeBetweenInfoInMilliseconds = _clockSetting.getTimeBetweenInfoInMilliseconds();
			bool sendInfo = timeBetweenInfoInMilliseconds > 0 && 
				getSystemTimeInMilliseconds() > _nextInfoTime;

			if (sendInfo) {
				_nextInfoTime = getSystemTimeInMilliseconds() + timeBetweenInfoInMilliseconds;
			}
			return sendInfo;
		}

		/**
		 * Call to stop the search immediately
		 */
		void stopSearch() { 
			_mode = ClockMode::stopped; 
		}

		/**
		 * Checks, if search has been stopped
		 */
		bool isSearchStopped() const {
			return _mode == ClockMode::stopped;
		}

		/**
		 * Sets the mode to normal search
		 */
		void setSearchMode() {
			_mode = ClockMode::search;
		}

		/**
		 * Check, if search must be stopped on mate found
		 */
		bool stopSearchOnMateFound() const {
			return _mode == ClockMode::search;
		}

		/**
		 * @returns true, if search is in analyze (never stopping) mode
		 */
		bool isAnalyzeMode() const { 
			return _mode == ClockMode::analyze; 
		}

		/**
		 * Sets the search related finding about the position value change
		 */
		void setNewMove() {
			_searchState.setNewMove();
			_averageTimePerMove = computeAverageTime();
			if (_verbose && _mode == ClockMode::search) {
				cout << "info string, average time after new Move " << _averageTimePerMove << endl;
			}
		}

		/**
		 * Stores a new search result that is inside the aspiration window
		 */
		void setSearchResult(ply_t depth, value_t positionValue)
		{
			_searchState.setSearchResult(depth, positionValue);
			_averageTimePerMove = computeAverageTime();
			if (_verbose && _mode == ClockMode::search) {
				cout << "info string, average time after search result; [d: "
					<< depth << "][v:" << positionValue << "] " << _averageTimePerMove << endl;
			}
		}

		/**
		 * Adjust the state accoring an iteration result that might be in or outside the
		 * aspiration window
		 */
		void setIterationResult(value_t alpha, value_t beta, value_t positionValue)
		{
			_searchState.setIterationResult(alpha, beta, positionValue);
			_averageTimePerMove = computeAverageTime();
			if (_verbose && _mode == ClockMode::search) {
				cout << "info string, average time after iteration result; [w: "
					<< alpha << ", " << beta << "][v:" << positionValue << "] " << _averageTimePerMove << endl;
			}
		}

		/**
		 * Adjust the state according having a new move in the root search
		 */
		void setSearchedRootMove(bool failLow, value_t positionValue) {
			_searchState.setSearchedRootMove(failLow, positionValue);
			_averageTimePerMove = computeAverageTime();
			if (_verbose && _mode == ClockMode::search) {
				cout << "info string, average time after root move ; "
					<< (failLow ? "[fail low]" : "") << "[v:" << positionValue << "] " << _averageTimePerMove << endl;
			}
		}

	private:


		/**
		 * Gets a predicted amount of moves to play until next time control
		 */
		int32_t computeMovesToGo()
		{
			int32_t movesToGo = _clockSetting.getMoveAmountForClock();
			const int32_t movesPlayed = _clockSetting.getPlayedMovesInGame();
			if (movesToGo == 0) {
				movesToGo = max(AVERAGE_MOVE_COUNT_PER_GAME - (movesPlayed / 2), KEEP_TIME_FOR_MOVES);
			}
			movesToGo = max(1, movesToGo);
			return movesToGo;
		}

		/**
		 * Returns true, if the search is infinite
		 */
		bool isInfiniteSearch() {
			return _clockSetting.isAnalyseMode() || 
				_clockSetting.isPonderMode() ||
				_clockSetting.getSearchDepthLimit() > 0 || 
				_clockSetting.getNodeCount() > 0;
		}

		/**
		 * Computes the avarage move time
		 */
		int64_t computeAverageTime()
		{
			int64_t averageTime = 0;

			if (isInfiniteSearch())
			{
				averageTime = numeric_limits<int64_t>::max();
			} else 	if (_clockSetting.getExactTimePerMoveInMilliseconds() > 0) {
				averageTime = _clockSetting.getExactTimePerMoveInMilliseconds();
			} else {
				const int64_t timeLeft = _clockSetting.getTimeToThinkForAllMovesInMilliseconds();
				const int32_t movesToGo = computeMovesToGo();
				
				// use movesToGo + 2 to not loose on time
				averageTime = timeLeft / (movesToGo + 2);

				// Infinite amount of moves:
				if (_clockSetting.getMoveAmountForClock() == 0)
				{
					if ((timeLeft < 10000) && (_clockSetting.getTimeIncrementPerMoveInMilliseconds() <= 1))
						averageTime /= 2;
					averageTime *= min(2000LL, max(1000LL, int64_t((6810000 + timeLeft) / (6810 + 300))));
					averageTime /= 1000;
				}
				averageTime = _searchState.modifyTimeBySearchFinding(averageTime);
				averageTime += _clockSetting.getTimeIncrementPerMoveInMilliseconds();
			}
			return averageTime;
		}

		/**
		 * Computes the maximum move time
		 */
		int64_t computeMaxTime()
		{
			int64_t maxTime = 0;

			if (isInfiniteSearch())
			{
				maxTime = numeric_limits<int64_t>::max();
			}
			else 	if (_clockSetting.getExactTimePerMoveInMilliseconds() > 0) {
				maxTime = _clockSetting.getExactTimePerMoveInMilliseconds();
			}
			else {
				const int64_t timeLeft = _clockSetting.getTimeToThinkForAllMovesInMilliseconds();
				const int64_t timeIncrement = _clockSetting.getTimeIncrementPerMoveInMilliseconds();
				const int32_t movesToGo = computeMovesToGo();
				maxTime = timeLeft / 3;

				// Keep at least two seconds
				maxTime = min(maxTime, timeLeft - 2000);
				// Not less than the fair share
				maxTime = max(maxTime, timeLeft / (movesToGo + 1));
				// Safety, not less than zero
				maxTime = max(maxTime, 0LL);
			}

			return maxTime;
		}


		/**
		 * 
		 * Returns the system time in milliseconds
		 */
		int64_t getSystemTimeInMilliseconds() const
		{
			timeb aCurrentTime;
			ftime(&aCurrentTime);
			return ((int64_t)(aCurrentTime.time) * 1000 +
				(int64_t)(aCurrentTime.millitm));
		}

		int64_t _startTime;
		int64_t _averageTimePerMove;
		int64_t _maxTimePerMove;
		int64_t _nextInfoTime;

		ClockMode _mode;
		
		ClockSetting _clockSetting;
		SearchState _searchState;
		bool _verbose;

		static const int32_t KEEP_TIME_FOR_MOVES = 35;
		static const int32_t AVERAGE_MOVE_COUNT_PER_GAME = 60;
	};
}

#endif // _CLOCKMANAGER_H