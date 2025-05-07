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
 * Implements a state controller recoding the search criticalness
 * The more critical a search finding is (fail low on PV for example)
 * the more time we will take to search for a better move
 */

#ifndef _CLOCKSTATE_H
#define _CLOCKSTATE_H

#include <time.h>
#include <algorithm>
#include "../basics/types.h"
#include "../basics/evalvalue.h"
#include "../interface/clocksetting.h"
#include "searchdef.h"
#include "searchparameter.h"

using namespace std;
using namespace QaplaBasics;

namespace QaplaSearch {

	enum class SearchFinding {
		normal, critical, suddenDeath, book
	};
	
	class SearchState {
	public:
		SearchState() {
			_hasBookMove = false;
			_values.fill(0);
		}

		/**
		 * Sets to new move
		 */
		void setNewMove() {
			_depth = 0;
			_values[_depth] = 0;
			_state = SearchFinding::normal;
			_rootSearchState = _state;
		}

		/**
		 * Stores a new search result that is inside the aspiration window
		 */
		void setSearchResult(ply_t depth, value_t positionValue)
		{
			if (depth < _values.size()) {
				_values[depth] = positionValue;
			}
			_depth = depth;
		}

		/**
		 * Adjust the state accoring an iteration result that might be in or outside the
		 * aspiration window
		 */
		void setIterationResult(value_t alpha, [[maybe_unused]]value_t beta, value_t positionValue)
		{
			if (_depth < 3) return;

			switch (_state)
			{
			case SearchFinding::normal:
				if (positionValue <= alpha) {
					_state = SearchFinding::suddenDeath;
				}
				else if (_hasBookMove)
					_state = SearchFinding::book;
				break;
			case SearchFinding::critical:
			case SearchFinding::suddenDeath:
				if (positionValue <= alpha) {
					_state = SearchFinding::suddenDeath;
				}
				else {
					reduceState(positionValue);
				}
				break;
			case SearchFinding::book:
				break;
			}
			_rootSearchState = _state;
		}

		/**
		 * Adjust the state according having a new move in the root search
		 */
		void setSearchedRootMove(bool failLow, value_t positionValue) {
			if (failLow) {
				_rootSearchState = SearchFinding::suddenDeath;
			} 
			else if (_depth > 4 && positionValue < _values[_depth] - CRITICAL_DROP) {
				_rootSearchState = SearchFinding::critical;
			}
			else if (positionValue >= _values[_depth]) {
				_rootSearchState = _state;
			}
		}


		/**
		 * Sets the search related finding about the position value change
		 */
		void setState(SearchFinding finding) {
			_state = finding;
		}

		/**
		 * Modifies the average time by the situation found by search
		 */
		int64_t modifyTimeBySearchFinding(int64_t averageTime) {
			switch (_rootSearchState)
			{
			case SearchFinding::normal:
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

	private:

		/**
		 * Reduce the criticality of the search state
		 */
		void reduceState(value_t positionValue)
		{
			if(_depth < 3) return;

			value_t lastValue = max(_values[_depth], _values[_depth - 1]);

			const bool massiveDrop = positionValue + DEATH_DROP < lastValue;
			const bool clearSituation = abs(lastValue) > WINNING_SITUATION;
			const bool significantDrop = positionValue + CRITICAL_DROP < lastValue;

			if (!massiveDrop) {
				if (clearSituation) {
					_state = SearchFinding::normal;
				} 
				else if (significantDrop) {
					_state = SearchFinding::critical;
				} 
				else {
					_state = SearchFinding::normal;
				}
			}
		}
		
		ply_t _depth;
		bool _hasBookMove;
		SearchFinding _state;
		SearchFinding _rootSearchState;
		array<value_t, SearchParameter::MAX_SEARCH_DEPTH> _values;
		static const value_t ONE_PAWN = 100;
		static const value_t DEATH_DROP = ONE_PAWN;
		static const value_t CRITICAL_DROP = ONE_PAWN / 5;
		static const value_t WINNING_SITUATION = ONE_PAWN * 3;
	};
}

#endif // _CLOCKSTATE_H