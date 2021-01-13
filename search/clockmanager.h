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
			return getSystemTimeInMilliseconds() - startTime;
		}

		void startCalculatingMove(int32_t movesLeftForClockControl, const ClockSetting& clockSetting)
		{
			startTime = getSystemTimeInMilliseconds();
			movesLeftForClockControl = 0;
			if (movesLeftForClockControl <= 0) {
				movesLeftForClockControl = 60 - clockSetting.getPlayedMovesInGame();
				if (movesLeftForClockControl < 20) {
					movesLeftForClockControl = 20;
				}
			}
			averageTimePerMove = clockSetting.getTimeToThinkForAllMovesInMilliseconds() / movesLeftForClockControl;
			averageTimePerMove += clockSetting.getTimeIncrementPerMoveInMilliseconds() / 2;

			maxTimePerMove = clockSetting.getTimeToThinkForAllMovesInMilliseconds() / 10;
			maxTimePerMove += clockSetting.getTimeIncrementPerMoveInMilliseconds() / 2;

			if (averageTimePerMove > maxTimePerMove) {
				maxTimePerMove = averageTimePerMove;
			}
			analyzeMode = clockSetting.getAnalyseMode();
			searchStopped = false;
		}

		/**
		 * Checks, if calculation must be aborded due to time constrains
		 */
		bool mustAbortCalculation(uint32_t ply) const {
			uint64_t timeSpent = getTimeSpentInMilliseconds();
			bool abort = false;
			if (searchStopped) {
				abort = true;
			}
			else if (analyzeMode) {
				abort = false;
			}
			else if (ply == 0) {
				abort = timeSpent > maxTimePerMove || timeSpent > averageTimePerMove * 4;
			}
			else {
				abort = timeSpent > maxTimePerMove;
			}
			return abort;
		}

		/**
		 * Checks if there is suitable to start the calculation of the next depth
		 * @returns true, if calculation of next depth is ok
		 */
		bool mayCalculateNextDepth() const {
			bool result;
			if (searchStopped) {
				result = false;
			}
			else if (analyzeMode) {
				result = true;
			}
			else {
				uint64_t timeSpent = getSystemTimeInMilliseconds() - startTime;
				result = timeSpent < averageTimePerMove / 3;
			}
			return result;
		}

		/**
		 * Call to stop the search immediately
		 */
		void stopSearch() { 
			searchStopped = true; 
		}

		/**
		 * Check, if search must be stopped on mate found
		 */
		bool stopSearchOnMateFound() const {
			return !analyzeMode;
		}

		/**
		 * @returns true, if search is in analyze (never stopping) mode
		 */
		bool isAnalyzeMode() const { 
			return analyzeMode; 
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
		uint64_t startTime;
		uint64_t averageTimePerMove;
		uint64_t maxTimePerMove;

		bool analyzeMode;
		bool searchStopped;
	};
}

#endif // _CLOCKMANAGER_H