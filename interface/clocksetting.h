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
 * Implements settings for a chess board clock control
 */

#ifndef __CLOCKSETTING_H
#define __CLOCKSETTING_H

#include "../basics/types.h"
#include <sys/timeb.h>

namespace ChessInterface {

	class ClockSetting {

	public:

		ClockSetting() : searchDepth(0), nodeRate(0), userClock(0) {
			moveAmountForClock = 40;
			timeToThinkForAllMovesInMilliseconds = 10 * 60 * 1000;
			timeIncrementPerMoveInMilliseconds = 1 * 1000;
			exactTimePerMoveInMilliseconds = 0;
			playedMovesInGame = 0;
			calculationStartTime = 0;
		}
		ClockSetting(const ClockSetting& clockControlSetting) { operator=(clockControlSetting); }

		ClockSetting& operator=(const ClockSetting& clockSetting) {
			searchDepth = clockSetting.searchDepth;
			nodeRate = clockSetting.nodeRate;
			userClock = clockSetting.userClock;
			timeToThinkForAllMovesInMilliseconds = clockSetting.timeToThinkForAllMovesInMilliseconds;
			timeIncrementPerMoveInMilliseconds = clockSetting.timeIncrementPerMoveInMilliseconds;
			exactTimePerMoveInMilliseconds = clockSetting.exactTimePerMoveInMilliseconds;
			analyseMode = clockSetting.analyseMode;
			playedMovesInGame = clockSetting.playedMovesInGame;
			return *this;
		}

		void limitSearchDepth(uint32_t depth) { searchDepth = depth; };
		void setSearchDepthToUnlimited() { searchDepth = 0; }
		bool isSearchDepthLimited() const { return searchDepth != 0; }
		uint32_t getSearchDepthLimit() const { return searchDepth; }

		/**
		 * Sets the time to think for all moves for the computer
		 */
		void setComputerClockInMilliseconds(uint64_t clockInMilliseconds) {
			timeToThinkForAllMovesInMilliseconds = clockInMilliseconds;
		}

		/**
		 * Sets the time to think for all moves for the user
		 */
		void setUserClockInMilliseconds(uint64_t clockInMilliseconds) {
			userClock = clockInMilliseconds;
		}

		/**
		 * Sets the move amount to be played in the clock time
		 */
		void setMoveAmountForClock(uint32_t moveAmount) {
			moveAmountForClock = moveAmount;
		}

		/**
		 * Sets the time to think for all moves
		 */
		void setTimeToThinkForAllMovesInMilliseconds(uint64_t milliseconds) {
			timeToThinkForAllMovesInMilliseconds = milliseconds;
			exactTimePerMoveInMilliseconds = 0;
		}

		/**
		 * Sets the time increment per move
		 */
		void setTimeIncrementPerMoveInMilliseconds(uint64_t milliseconds) {
			timeIncrementPerMoveInMilliseconds = milliseconds;
			exactTimePerMoveInMilliseconds = 0;
		}

		/**
		 * Sets the exact time in milliseconds per move
		 */
		void setExactTimePerMoveInMilliseconds(uint64_t milliseconds) {
			exactTimePerMoveInMilliseconds = milliseconds;
			timeToThinkForAllMovesInMilliseconds = 0;
			timeIncrementPerMoveInMilliseconds = 0;
			moveAmountForClock = 0;
		}

		/**
		 * Starts recording the calculation time
		 */
		void storeCalculationStartTime() {
			calculationStartTime = getSystemTimeInMilliseconds();
		}

		/**
		 * Stores the time spent for the current calculation
		 */
		void storeTimeSpent() {
			int64_t timeSpent = getSystemTimeInMilliseconds() - calculationStartTime;
			timeToThinkForAllMovesInMilliseconds -= timeSpent;
		}

		uint64_t getTimeToThinkForAllMovesInMilliseconds() const { return timeToThinkForAllMovesInMilliseconds; }
		uint64_t getTimeIncrementPerMoveInMilliseconds() const { return timeIncrementPerMoveInMilliseconds; }
		uint64_t getExactTimePerMoveInMilliseconds() const { return exactTimePerMoveInMilliseconds; }
		uint32_t getMoveAmountForClock() const { return moveAmountForClock; }
		void setAnalyseMode(bool analyse) { analyseMode = analyse; }
		bool getAnalyseMode() const { return analyseMode; }

		void setPlayedMovesInGame(uint32_t playedMoves) { playedMovesInGame = playedMoves; }
		uint32_t getPlayedMovesInGame() const { return playedMovesInGame; }

	private:

		uint64_t getSystemTimeInMilliseconds() const
		{
			timeb aCurrentTime;
			ftime(&aCurrentTime);
			return ((uint64_t)(aCurrentTime.time) * 1000 +
				(uint64_t)(aCurrentTime.millitm));
		}


		uint32_t searchDepth;
		uint64_t nodeRate;
		uint64_t userClock;
		uint32_t moveAmountForClock;
		uint32_t playedMovesInGame;
		int64_t timeToThinkForAllMovesInMilliseconds;
		uint64_t timeIncrementPerMoveInMilliseconds;
		uint64_t exactTimePerMoveInMilliseconds;
		uint64_t calculationStartTime;
		bool analyseMode;

	};

}

#endif // __CLOCKSETTING_H
