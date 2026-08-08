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
 * Implements a manager for computing time of a chess search
 */

#ifndef _CLOCKMANAGER_H
#define _CLOCKMANAGER_H

#include <algorithm>
#include <limits>
#include <sys/timeb.h>
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
			int64_t systemTime = getSystemTimeInMilliseconds();
			return systemTime - _startTime;
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
		void startCalculatingMove([[maybe_unused]]int32_t movesToGo, const ClockSetting& clockSetting)
		{
			setStartTime();
			_clockSetting = clockSetting;
			_nextInfoTime = getSystemTimeInMilliseconds() + clockSetting.getTimeBetweenInfoInMilliseconds();
			_maxTimePerMove = computeMaxTime();
			_nodeTarget = clockSetting.getNodeTarget();
			_mode = clockSetting.getMode();
			// must be last!
			setNewMove();
		}

		void setCalculationDepth(ply_t depth) {
			_depth = depth;
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
			if (_depth <= MIN_DEPTH_EXTERNAL_STOP) {
				return false;
			}
			return _mode == ClockMode::stopped;
		}

		bool stopOnNodeTarget(uint64_t nodeCount) {
			if (isSearchStopped()) return true;
			if (_nodeTarget == 0) return false;
			if (nodeCount > _nodeTarget) {
				stopSearch();
				return true;
			}
			return false;
		}

		/**
		 * Checks, if calculation must be aborded due to time constrains
		 */
		bool emergencyAbort() {
			if (isSearchStopped()) {
				return true;
			}
			if (_depth <= MIN_DEPTH) {
				return false;
			}
			if (_mode != ClockMode::search) {
				return false;
			}
			if (computeTimeSpentInMilliseconds() > _maxTimePerMove) {
				stopSearch();
				return true;
			}
			return false;
		}

		/**
		 * Checks, if calculation must be aborded due to time constrains
		 */
		bool shouldAbort() {
			if (isSearchStopped()) {
				return true;
			}
			if (_depth <= MIN_DEPTH) {
				return false;
			}
			if (_mode != ClockMode::search) {
				return false;
			}
			if (computeTimeSpentInMilliseconds() > (_averageTimePerMove / 10) * 8) {
				stopSearch();
				return true;
			}
			return false;
		}


		/**
		 * Checks if there is suitable to start the calculation of the next depth
		 * @returns true, if calculation of next depth is ok
		 */
		bool mayComputeNextDepth(ply_t depth) const {
			if (isSearchStopped()) {
				return false;
			}
			if (depth <= MIN_DEPTH) {
				return true;
			}
			if (_mode != ClockMode::search) {
				return true;
			}

			int64_t time = min(_maxTimePerMove, (_averageTimePerMove / 10) * 7);
			return computeTimeSpentInMilliseconds() < time;
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
				movesToGo = std::max(AVERAGE_MOVE_COUNT_PER_GAME - (movesPlayed / 2), KEEP_TIME_FOR_MOVES);
			}
			movesToGo = std::max(1, movesToGo);
			return movesToGo;
		}

		/**
		 * Returns true, if the search is infinite
		 * Note: ponder is not infinite. It will set search time but not use it as long as no ponder hit has been sent.
		 */
		bool isInfiniteSearch() {
			return _clockSetting.isAnalyseMode() || 
				_clockSetting.getSearchDepthLimit() > 0 || 
				_clockSetting.getNodeTarget() > 0;
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
					averageTime *= min(
						static_cast<int64_t>(2000LL), 
						max(static_cast<int64_t>(1000LL), int64_t((6810000 + timeLeft) / (6810 + 300))));
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
				// The share of the remaining time this move may spend runs on a ramp. While the
				// clock is low the move gets a fifth, once there is room it gets a third, and in
				// between the share grows steadily with no step anywhere. The ramp is expressed
				// through the divisor of the share, kept in milliseconds so that the clamp is
				// plain integer arithmetic; SHARE_DIVISOR_SCALE turns it into the real divisor.
				//
				// Until 0.4.0-042 this was max((timeLeft - 2000) / 3, timeLeft / 5) instead,
				// which held the share at a flat fifth all the way up to 10 seconds left.
				const int64_t SHARE_DIVISOR_SCALE = 1000;
				const int64_t RAMP_BEGIN_TIME = 1000;			// below this the share stays at a fifth
				const int64_t LOW_TIME_SHARE_DIVISOR = 5000;	// 5.0, a fifth, while the clock is low
				const int64_t FULL_SHARE_DIVISOR = 3000;		// 3.0, a third, once the clock allows it
				// Arena tends to loose more than 20 ms handling a move, do not spend that part
				const int64_t MOVE_HANDLING_MARGIN = 20;
				// Never hand out the last of the clock
				const int64_t CLOCK_SAFETY_MARGIN = 10;

				const int64_t timeLeft = _clockSetting.getTimeToThinkForAllMovesInMilliseconds();
				const int64_t timeIncrement = _clockSetting.getTimeIncrementPerMoveInMilliseconds();
				const int32_t movesToGo = computeMovesToGo();

				const int64_t scaledDivisor = std::clamp(
					LOW_TIME_SHARE_DIVISOR - (timeLeft - RAMP_BEGIN_TIME),
					FULL_SHARE_DIVISOR, LOW_TIME_SHARE_DIVISOR);
				const double shareDivisor =
					static_cast<double>(scaledDivisor) / static_cast<double>(SHARE_DIVISOR_SCALE);
				maxTime = static_cast<int64_t>(static_cast<double>(timeLeft) / shareDivisor);
				// Not less than the fair share of the moves still to play
				maxTime = max(maxTime, timeLeft / (movesToGo + 1));
				// Even near zero you may use the time increment.
				maxTime = std::max(maxTime, timeIncrement - MOVE_HANDLING_MARGIN);
				// Hard cut.
				// Note: maxtime is including time increment, because it is added after moving.
				maxTime = std::min(maxTime, timeLeft - CLOCK_SAFETY_MARGIN);
				// But never zero, never negative.
				maxTime = std::max(maxTime, static_cast<int64_t>(1));
			}

			return maxTime;
		}


		/**
		 * 
		 * Returns the system time in milliseconds
		 */
		int64_t getSystemTimeInMilliseconds() const
		{
			return StdTimeControl::getSystemTimeInMilliseconds();
		}

		ply_t _depth;
		int64_t _startTime;
		int64_t _averageTimePerMove;
		int64_t _maxTimePerMove;
		int64_t _nextInfoTime;
		uint64_t _nodeTarget;

		ClockMode _mode;
		
		ClockSetting _clockSetting;
		SearchState _searchState;
		bool _verbose;

		static constexpr int32_t KEEP_TIME_FOR_MOVES = 35;
		static const int32_t AVERAGE_MOVE_COUNT_PER_GAME = 60;
		static const ply_t MIN_DEPTH = 5;
		static const ply_t MIN_DEPTH_EXTERNAL_STOP = 2;
	};
}

#endif // _CLOCKMANAGER_H