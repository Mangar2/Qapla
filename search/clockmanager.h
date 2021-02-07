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
#include <sys/timeb.h>
#include "../basics/types.h"
#include "../interface/clocksetting.h"

namespace ChessSearch {
	
	class ClockManager {
	public:
		ClockManager() {}

		uint64_t getTimeSpentInMilliseconds() const {
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
		 * @param movesLeftForClockControl amount of moves to play in the time provided by clockSetting
		 * @param clockSetting the time available to play the chess game
		 */
		void startCalculatingMove(int32_t movesLeftForClockControl, const ClockSetting& clockSetting)
		{
			setStartTime();
			movesLeftForClockControl = 0;
			if (movesLeftForClockControl <= 0) {
				movesLeftForClockControl = 60 - clockSetting.getPlayedMovesInGame();
				if (movesLeftForClockControl < 20) {
					movesLeftForClockControl = 20;
				}
			}
			_averageTimePerMove = clockSetting.getTimeToThinkForAllMovesInMilliseconds() / movesLeftForClockControl;
			_averageTimePerMove += clockSetting.getTimeIncrementPerMoveInMilliseconds() / 2;
			_maxTimePerMove = clockSetting.getTimeToThinkForAllMovesInMilliseconds() / 10;
			_timeBetweenInfoInMilliseconds = clockSetting.getTimeBetweenInfoInMilliseconds();
			_nextInfoTime = 0;
			if (clockSetting.getTimeToThinkForAllMovesInMilliseconds() < 10 * 1000) {
				// Very short time left -> urgent move
				_maxTimePerMove /= 4;
			} 
		
			_maxTimePerMove += clockSetting.getTimeIncrementPerMoveInMilliseconds() / 2;

			if (_averageTimePerMove > _maxTimePerMove) {
				_maxTimePerMove = _averageTimePerMove;
			}
			_analyzeMode = clockSetting.getAnalyseMode();
			_searchStopped = false;
		}

		/**
		 * Checks, if calculation must be aborded due to time constrains
		 */
		bool mustAbortCalculation(uint32_t ply) const {
			uint64_t timeSpent = getTimeSpentInMilliseconds();
			bool abort = false;
			if (_searchStopped) {
				abort = true;
			}
			else if (_analyzeMode) {
				abort = false;
			}
			else if (ply == 0) {
				abort = timeSpent > _maxTimePerMove || timeSpent > _averageTimePerMove * 4;
			}
			else {
				abort = timeSpent > _maxTimePerMove;
			}
			return abort;
		}

		/**
		 * Checks if there is suitable to start the calculation of the next depth
		 * @returns true, if calculation of next depth is ok
		 */
		bool mayCalculateNextDepth() const {
			bool result;
			if (_searchStopped) {
				result = false;
			}
			else if (_analyzeMode) {
				result = true;
			}
			else {
				uint64_t timeSpent = getSystemTimeInMilliseconds() - _startTime;
				result = timeSpent < _averageTimePerMove / 3;
			}
			return result;
		}

		/**
		 * Checks, if it is time to send the next information to the gui
		 */
		bool isTimeToSendNextInfo() {
			bool sendInfo = _timeBetweenInfoInMilliseconds > 0 && 
				getSystemTimeInMilliseconds() > _nextInfoTime;

			if (sendInfo) {
				_nextInfoTime = getSystemTimeInMilliseconds() + _timeBetweenInfoInMilliseconds;
			}
			return sendInfo;
		}

		/**
		 * Call to stop the search immediately
		 */
		void stopSearch() { 
			_searchStopped = true; 
		}

		/**
		 * Check, if search must be stopped on mate found
		 */
		bool stopSearchOnMateFound() const {
			return !_analyzeMode;
		}

		/**
		 * @returns true, if search is in analyze (never stopping) mode
		 */
		bool isAnalyzeMode() const { 
			return _analyzeMode; 
		}

	private:
		/**
		 * 
		 * Returns the system time in milliseconds
		 */
		uint64_t getSystemTimeInMilliseconds() const
		{
			timeb aCurrentTime;
			ftime(&aCurrentTime);
			return ((uint64_t)(aCurrentTime.time) * 1000 +
				(uint64_t)(aCurrentTime.millitm));
		}
		uint64_t _startTime;
		uint64_t _averageTimePerMove;
		uint64_t _maxTimePerMove;
		uint64_t _nextInfoTime;
		uint64_t _timeBetweenInfoInMilliseconds;

		bool _analyzeMode;
		bool _searchStopped;
	};
}

#endif // _CLOCKMANAGER_H