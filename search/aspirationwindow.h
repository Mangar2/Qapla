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
 * Implements an aspiration window for iterative deepening search algorithm
 * The aspiration window defines the estimation bounds for the next search
 * to reduce the nodes needed to search the position. If the search result
 * is out of the window, a research is needed
 */

#ifndef __ASPIRATIONWINDOW_H
#define __ASPIRATIONWINDOW_H

#include <math.h>
#include "../basics/types.h"
#include "search-config.h"
#include "search-param.h"

using namespace QaplaBasics;

namespace QaplaSearch {



	class AspirationWindow {
	private:
		enum class State {
			Search, Dropping, Rising, Alternating
		};
	public:

		AspirationWindow() : _state(State::Search), _retryCount(0), _alpha(-MAX_VALUE), _beta(MAX_VALUE), _positionValue(0), _searchDepth(0), _multiPV(1) {}

		void initSearch() {
			_alpha = -MAX_VALUE;
			_beta = MAX_VALUE;
			_state = State::Search;
			_retryCount = 0;
			_positionValue = 0;
			_bestMovesFound = 0;
		}

		/**
		 * Checks, if a position value is inside the aspiration window
		 */
		bool isInside(value_t positionValue) {
			return (positionValue > _alpha && positionValue < _beta);
		}

		/**
		 * Sets the next search depth
		 */
		void newDepth(ply_t searchDepth) {
			_searchDepth = searchDepth;
			_state = State::Search;
			_alternateCount = 0;
			_retryCount /= 2;
			value_t windowSize = calculateWindowSize(searchDepth, _positionValue);
			setWindow(_positionValue, windowSize);
		}

		/**
		 * Retries the search with an updated window
		 */
		void setSearchResult(value_t positionValue) {
			if (!isInside(positionValue)) {
				switch (_state) {
				case State::Search:
					_state = (positionValue > _positionValue) ? State::Rising : State::Dropping;
					break;
				case State::Rising:
					_state = (positionValue > _positionValue) ? State::Rising : State::Alternating;
					break;
				case State::Dropping:
					_state = (positionValue < _positionValue) ? State::Dropping : State::Alternating;
					break;
				case State::Alternating:
				    _state = State::Alternating;
					break;
				}
				_retryCount++;
			}
			if (_state == State::Alternating) {
				_alternateCount++;
			}
			_positionValue = positionValue;
			value_t windowSize = calculateWindowSize(_searchDepth, _positionValue);
			setWindow(_positionValue, windowSize);
		}

		/**
		 * Retrieves the aspiration window values (alpha, beta)
		 */
		value_t getAlpha() const { return _alpha; }
		value_t getBeta() const { return _beta; }

		/**
		 * Gets the amount of retries
		 */
		int32_t getRetryCount() const { return _retryCount; }

		/**
		 * Gets the current state as string
		 */
		string getState() const {
			const char* names[4] = { "Search", "Dropping", "Rising", "Alternating" };
			return names[int(_state)];
		}

		void setMultiPV(uint32_t count) {
			_multiPV = count;
		}

		uint32_t getMultiPV() const {
			return _multiPV;
		}	

		void print() {
			cout
				<< "[" << getAlpha() << ", " << getBeta() << "]"
				<< " [" << getState() << "]"
				<< " [r" << getRetryCount() << "]"
				<< " [a" << _alternateCount << "]" << endl;
		}


	private:

		/**
		 * Computes the size of the original window
		 *
		 * The size carried a term over the jump of the position value between iterations, but it
		 * was always zero: the previous value had been overwritten before the difference was
		 * taken. Repairing it lost elo twice, with the old values and with values tuned for it
		 * (0.4.0-058 flat over 20000 games, 0.4.0-059 −4.0 Elo, `dead/aspiration-delta`), so the
		 * term is gone instead of silently computing nothing.
		 *
		 * @param searchDepth depth of the current search
		 * @param positionValue positionValue of the current search
		 */
		value_t calculateWindowSize(ply_t searchDepth, value_t positionValue) {
			constexpr bool OPT = SearchConfig::optimizeAspiration;
			// Every influence carries its own coefficient
			const value_t depthRelatedSize = std::max(0, STABLE_DEPTH - searchDepth)
				* param<OPT, "awDepthFactor", 10, 0, 20>();
			const value_t valueRelatedSize = std::abs(positionValue)
				* param<OPT, "awValueFactor", 5, 0, 10>() / 100;
			const value_t retryRelatedSize = _retryCount * param<OPT, "awRetryFactor", 30, 0, 60>();
			const value_t minSize = param<OPT, "awMinSize", 15, 0, 30>();
			return minSize + depthRelatedSize + valueRelatedSize + retryRelatedSize;
		}

		/**
		 * Sets the window position-value and size
		 */
		void setWindow(value_t value, value_t windowSize = 2 * MAX_VALUE) {
			if (_state == State::Rising) {
				//_alpha = _alpha > value - windowSize ? _alpha : value - 1 - windowSize / 10;
				//_alpha = value - windowSize;
				_beta = value + windowSize;
				if (value > 1000) {
					_beta = MAX_VALUE;
				}
			}
			else if (_state == State::Dropping) {
				_alpha = value - windowSize;
				_beta = value + windowSize;
				// _beta = _alpha + windowSize / 2;
				//_beta = _beta < value + windowSize ? _beta :  value + 1 + windowSize / 10;
			} else {
				_alpha = value - windowSize;
				_beta = value + windowSize;
			}
			if (_alternateCount >= 2) {
				_alpha = -MAX_VALUE;
				_beta = MAX_VALUE;
			}
			if (_alpha < -MIN_MATE_VALUE) {
				_alpha = -MAX_VALUE;
			}
			if (_beta > 2000) {
				_beta = MAX_VALUE;
			}
		}



		State _state;
		int32_t _retryCount;
		value_t _alpha;
		value_t _beta;
		value_t _positionValue;
		value_t _searchDepth;
		uint32_t _multiPV;
		uint32_t _bestMovesFound;
		uint32_t _alternateCount = 0;

		static const ply_t STABLE_DEPTH = 8;
	};

	/**
	 * Prints the window
	 */
	[[maybe_unused]] static ostream& operator<<(ostream& stream, AspirationWindow& window) {
		stream
			<< "[" << window.getAlpha() << ", " << window.getBeta() << "]"
			<< " [" << window.getState() << "]"
			<< " [r" << window.getRetryCount() << "]";
		return stream;
	}
}

#endif // __ASPIRATIONWINDOW_H
