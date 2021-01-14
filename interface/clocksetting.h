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
 * @author Volker B�hm
 * @copyright Copyright (c) 2021 Volker B�hm
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

		ClockSetting() {
			reset();
		}

		/**
		 * Initializes all members
		 */
		void reset() {
			_searchDepth = 0;
			_nodeCount = 0;
			_userClock = 0;
			_mate = 0;
			_mode = Mode::compute;
			_moveAmountForClock = 40;
			_timeToThinkForAllMovesInMilliseconds = 10 * 60 * 1000;
			_timeIncrementPerMoveInMilliseconds = 1 * 1000;
			_exactTimePerMoveInMilliseconds = 0;
			_playedMovesInGame = 0;
			_calculationStartTime = 0;
		}

		/**
		 * Sets a search depth limit
		 */
		void setSearchDepthLimit(uint32_t depth) { _searchDepth = depth; };
		
		/**
		 * Removes a search depth limit
		 */
		void setSearchDepthToUnlimited() { _searchDepth = 0; }

		/**
		 * Checks, if search depth is limited
		 */
		bool isSearchDepthLimited() const { return _searchDepth != 0; }

		/**
		 * Gets the search depth limit
		 */
		uint32_t getSearchDepthLimit() const { return _searchDepth; }

		/**
		 * Sets the time to think for all moves for the computer
		 */
		void setComputerClockInMilliseconds(uint64_t clockInMilliseconds) {
			_timeToThinkForAllMovesInMilliseconds = clockInMilliseconds;
		}

		/**
		 * Sets the time to think for all moves for the user
		 */
		void setUserClockInMilliseconds(uint64_t clockInMilliseconds) {
			_userClock = clockInMilliseconds;
		}

		/**
		 * Sets the move amount to be played in the clock time
		 */
		void setMoveAmountForClock(uint32_t moveAmount) {
			_moveAmountForClock = moveAmount;
		}

		/**
		 * Sets the time to think for all moves
		 */
		void setTimeToThinkForAllMovesInMilliseconds(uint64_t milliseconds) {
			_timeToThinkForAllMovesInMilliseconds = milliseconds;
			_exactTimePerMoveInMilliseconds = 0;
		}

		/**
		 * Sets the time increment per move
		 */
		void setTimeIncrementPerMoveInMilliseconds(uint64_t milliseconds) {
			_timeIncrementPerMoveInMilliseconds = milliseconds;
			_exactTimePerMoveInMilliseconds = 0;
		}

		/**
		 * Sets the exact time in milliseconds per move
		 */
		void setExactTimePerMoveInMilliseconds(uint64_t milliseconds) {
			_exactTimePerMoveInMilliseconds = milliseconds;
			_timeToThinkForAllMovesInMilliseconds = 0;
			_timeIncrementPerMoveInMilliseconds = 0;
			_moveAmountForClock = 0;
		}

		/**
		 * Sets the amount of nodes to search (0 -> not relevant)
		 */
		void setNodeCount(uint64_t nodeCount) {
			_nodeCount = nodeCount;
		}

		/**
		 * Searches for a mate in x moves (0 = not relevant)
		 */
		void setMate(uint64_t mate) {
			_mate = uint32_t(mate);
		}

		/**
		 * Starts recording the calculation time
		 */
		void storeCalculationStartTime() {
			_calculationStartTime = getSystemTimeInMilliseconds();
		}

		/**
		 * Stores the time spent for the current calculation
		 */
		void storeTimeSpent() {
			int64_t timeSpent = getSystemTimeInMilliseconds() - _calculationStartTime;
			_timeToThinkForAllMovesInMilliseconds -= timeSpent;
		}

		/**
		 * Gets the total time to think for all moves until the next clock control
		 */
		uint64_t getTimeToThinkForAllMovesInMilliseconds() const { return _timeToThinkForAllMovesInMilliseconds; }

		/**
		 * Gets the amount of milliseconds the search time increments after each move
		 */
		uint64_t getTimeIncrementPerMoveInMilliseconds() const { return _timeIncrementPerMoveInMilliseconds; }

		/**
		 * Gets the exact amount of milliseconds to be searched
		 */
		uint64_t getExactTimePerMoveInMilliseconds() const { return _exactTimePerMoveInMilliseconds; }

		/**
		 * Gets the amount of moves to be played in the current clock time
		 */
		uint32_t getMoveAmountForClock() const { return _moveAmountForClock; }
		
		/**
		 * Sets analyze mode (infinite search)
		 */
		void setAnalyseMode(bool analyse) { _mode = analyze ?  Mode::analyze : Mode::compute; }
		
		/**
		 * @returns true, if the engine is analyzing (endless search) 
		 */
		bool getAnalyseMode() const { return _mode == Mode::analyze; }
		
		/**
		 * Sets the ponder mode to true -> the position will be pondered (endless search)
		 */
		void setPonderMode() { _mode = Mode::ponder; }

		/**
		 * @returns true, if the engine is currently pondering (calculating while not at move)
		 */
		bool isPonderMode() const { return _mode == Mode::ponder; }

		/**
		 * Sets the amount of moves already played in the game
		 */
		void setPlayedMovesInGame(uint32_t playedMoves) { _playedMovesInGame = playedMoves; }

		/**
		 * Gets the amount of moves already played in the game
		 */
		uint32_t getPlayedMovesInGame() const { return _playedMovesInGame; }

	private:

		uint64_t getSystemTimeInMilliseconds() const
		{
			timeb aCurrentTime;
			ftime(&aCurrentTime);
			return ((uint64_t)(aCurrentTime.time) * 1000 +
				(uint64_t)(aCurrentTime.millitm));
		}


		uint32_t _searchDepth;
		uint64_t _nodeCount;
		uint32_t _mate;
		uint64_t _userClock;
		uint32_t _moveAmountForClock;
		uint32_t _playedMovesInGame;
		int64_t _timeToThinkForAllMovesInMilliseconds;
		uint64_t _timeIncrementPerMoveInMilliseconds;
		uint64_t _exactTimePerMoveInMilliseconds;
		uint64_t _calculationStartTime;
		enum Mode {
			compute, analyze, ponder
		};
		Mode _mode;

	};

}

#endif // __CLOCKSETTING_H
