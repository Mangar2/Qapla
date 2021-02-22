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

using namespace std;

namespace ChessSearch {

	enum class SearchFinding {
		forced, normal, silent, critical, suddenDeath, book
	};
	
	class ClockManager {
	public:
		ClockManager() {
		}

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
		 * @param movesToGo amount of moves to play in the time provided by clockSetting
		 * @param clockSetting the time available to play the chess game
		 */
		void startCalculatingMove(int32_t movesToGo, const ClockSetting& clockSetting)
		{
			setStartTime();
			_clockSetting = clockSetting;
			_movesToGo = movesToGo;
			if (movesToGo <= 0) {
				movesToGo = 60 - clockSetting.getPlayedMovesInGame();
				if (movesToGo < 20) {
					movesToGo = 20;
				}
			}
			_averageTimePerMove = clockSetting.getTimeToThinkForAllMovesInMilliseconds() / movesToGo;
			_averageTimePerMove += clockSetting.getTimeIncrementPerMoveInMilliseconds() / 2;
			_maxTimePerMove = clockSetting.getTimeToThinkForAllMovesInMilliseconds() / 10;
			_timeBetweenInfoInMilliseconds = clockSetting.getTimeBetweenInfoInMilliseconds();
			_nextInfoTime = 0;
			if (clockSetting.getTimeToThinkForAllMovesInMilliseconds() < 10 * 1000) {
				// Very short time left -> urgent move
				_maxTimePerMove /= 2;
			} 
		
			_maxTimePerMove += clockSetting.getTimeIncrementPerMoveInMilliseconds();

			if (_averageTimePerMove > _maxTimePerMove) {
				_maxTimePerMove = _averageTimePerMove;
			}
			_mode = clockSetting.getMode();
		}

		/**
		 * Checks, if calculation must be aborded due to time constrains
		 */
		bool mustAbortCalculation(uint32_t ply) {
			uint64_t timeSpent = getTimeSpentInMilliseconds();
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
			else if (ply == 0 && timeSpent > _averageTimePerMove * 4) {
				abort = true;
				stopSearch();
			}
			// Ensure that the information to stop the search is stable
			return abort;
		}

		/**
		 * Checks if there is suitable to start the calculation of the next depth
		 * @returns true, if calculation of next depth is ok
		 */
		bool mayCalculateNextDepth() const {
			bool result;
			if (_mode == ClockMode::stopped) {
				result = false;
			}
			else if (_mode != ClockMode::search) {
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
			_mode = ClockMode::stopped; 
		}

		/**
		 * Checks, if search has been stopped
		 */
		bool isSearchStopped() {
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

	private:

		/**
		 * Gets a predicted amount of moves to play until next time control
		 */
		uint32_t computeMovesToGo()
		{
			int32_t movesToGo = _clockSetting.getMoveAmountForClock();
			const uint32_t movesPlayed = _clockSetting.getPlayedMovesInGame();
			if (movesToGo == 0) {
				movesToGo = max(AVERAGE_MOVE_COUNT_PER_GAME - (movesPlayed / 2), KEEP_TIME_FOR_MOVES);
			}
			movesToGo = max(1, movesToGo);
			return movesToGo;
		}

		/**
		 * Modifies the average time by the situation found by search
		 */
		uint64_t modifyTimeBySearchFinding(SearchFinding type, uint64_t averageTime) {
			switch (type)
			{
			case SearchFinding::forced:
				if (averageTime >= 1000) averageTime /= 5;
				else averageTime = min(averageTime, 333ULL);
				break;
			case SearchFinding::silent:
				averageTime = averageTime / 10 * 9;
				break;
			case SearchFinding::normal:
				averageTime = averageTime / 8 * 10;
				break;
			case SearchFinding::critical:
				averageTime *= 4;
				break;
			case SearchFinding::suddenDeath:
				averageTime *= 15;
				break;
			case SearchFinding::book:
				averageTime /= 5;
				break;
			}
			return averageTime;
		}
		
		/**
		 * Computes the avarage move time
		 */
		uint64_t computeAverageTime(SearchFinding type)
		{
			uint64_t averageTime = 0;

			// Zeitlimit pro Zug
			switch (_clockType)
			{
			case ClockType::infinite:
			case ClockType::nodeCount:
				return numeric_limits<uint64_t>::max();
			case ClockType::move:
				return _clockSetting.getExactTimePerMoveInMilliseconds();
			case ClockType::total:
			case ClockType::timeLeft:
				const uint64_t timeLeft = _clockSetting.getTimeToThinkForAllMovesInMilliseconds();
				const uint32_t movesToGo = computeMovesToGo();
				
				// use movesToGo + 2 to not loose on time
				averageTime = timeLeft / (movesToGo + 2);

				// Infinite amount of moves:
				if (_clockSetting.getMoveAmountForClock() == 0)
				{
					if ((timeLeft < 10000) && (_clockSetting.getTimeIncrementPerMoveInMilliseconds() <= 1))
						averageTime /= 2;
					averageTime *= min(2000ULL, max(1000ULL, uint64_t((6810000 + timeLeft) / (6810 + 300))));
					averageTime /= 1000;
				}
				break;
			}
			averageTime = modifyTimeBySearchFinding(type, averageTime);
			averageTime += _clockSetting.getTimeIncrementPerMoveInMilliseconds();
			return averageTime;
		}

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
		uint32_t _movesToGo;
		uint64_t _timeBetweenInfoInMilliseconds;

		enum class ClockType {
			move, total, timeLeft, nodeCount, infinite
		};

		ClockType _clockType;
		MoveType _moveType;
		ClockMode _mode;
		
		ClockSetting _clockSetting;

		static const uint32_t KEEP_TIME_FOR_MOVES = 35;
		static const uint32_t AVERAGE_MOVE_COUNT_PER_GAME = 60;
	};
}

#endif // _CLOCKMANAGER_H