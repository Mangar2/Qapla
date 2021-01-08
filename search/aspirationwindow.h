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
 * Implements an aspiration window for iterative deepening search algorithm
 * The aspiration window defines the estimation bounds for the next search
 * to reduce the nodes needed to search the position. If the search result
 * is out of the window, a research is needed
 */

#ifndef __ASPIRATIONWINDOW_H
#define __ASPIRATIONWINDOW_H

#include "../basics/types.h"

using namespace ChessBasics;

namespace ChessSearch {
	class AspirationWindow {
	public:
		void initSearch() {
			alpha = -MAX_VALUE;
			beta = MAX_VALUE;
			stage = 0;
		}

		/**
		 * sets the window to the maximal size
		 */
		void setMaxWindow(value_t positionValue) {
			if (positionValue < alpha) {
				beta = positionValue + 1;
				alpha = -MAX_VALUE;
			}
			if (positionValue >= beta) {
				alpha = positionValue - 1;
				beta = MAX_VALUE;
			}
		}

		/**
		 * increases the size of the window
		 */
		void setWiderWindow(value_t positionValue, value_t windowSize) {
			if (positionValue <= alpha) {
				beta = positionValue + 1;
				alpha = positionValue - windowSize;
			}
			if (positionValue >= beta) {
				alpha = positionValue - 1;
				beta = positionValue + windowSize;
			}
			if (beta > MAX_VALUE) {
				beta = MAX_VALUE;
			}
			if (alpha < -MAX_VALUE) {
				alpha = -MAX_VALUE;
			}
		}

		/**
		 * Sets the window position-value and size
		 */
		void setWindow(value_t positionValue, value_t windowSize) {
			beta = positionValue + windowSize;
			alpha = positionValue - windowSize;
			if (beta > MAX_VALUE) {
				beta = MAX_VALUE;
			}
			if (alpha < -MAX_VALUE) {
				alpha = -MAX_VALUE;
			}
		}

		/**
		 * computes the size of the window for the search - retry
		 */
		void calculateNewRetryWindow(value_t positionValue) {
			switch (stage) {
			case 0: setWiderWindow(positionValue, 100); break;
			case 1: setWiderWindow(positionValue, 500); break;
			case 2: setMaxWindow(positionValue); break;
			default: alpha = -MAX_VALUE; beta = MAX_VALUE; break;
			}
			//printf("[retry window a:%ld, b:%ld]\n", alpha, beta);
		}

		/**
		 * Computes the size of the original window
		 */
		void calculateNewInitialWindow(value_t positionValue) {
			switch (stage) {
			case 0: setWindow(positionValue, 100); break;
			case 1: setWindow(positionValue, 200); break;
			case 2: setWindow(positionValue, 400); break;
			default: alpha = -MAX_VALUE; beta = MAX_VALUE; break;
			}
			//printf("[initial window a:%ld, b:%ld]\n", alpha, beta);
		}

		/**
		 * Retries the search with an updated window
		 */
		bool retryWithNewWindow(ComputingInfo& computingInfo) {
			bool retry = (computingInfo.positionValueInCentiPawn <= alpha || computingInfo.positionValueInCentiPawn >= beta);

			if (retry) {
				stage++;
				calculateNewRetryWindow(computingInfo.positionValueInCentiPawn);
				//printf("[retry window a:%ld, b:%ld]\n", alpha, beta);
			}
			else {
				stage = stage <= 0 ? 0 : stage - 1;
				calculateNewInitialWindow(computingInfo.positionValueInCentiPawn);
			}
			return retry;
		}

		int32_t stage;
		value_t alpha;
		value_t beta;
		value_t windowSize;
	};
}

#endif // __ASPIRATIONWINDOW_H