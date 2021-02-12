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
 * Prints the search information for the winboard interface
 */

#ifndef __UCIPRINTSEARCHINFO_H
#define __UCIPRINTSEARCHINFO_H

#include "ISendSearchInfo.h"
#include "IInputOutput.h"
#include <cmath>

namespace ChessInterface {

	class UCIPrintSearchInfo : public ISendSearchInfo {
	public:
		UCIPrintSearchInfo(IInputOutput* pointerToIOHandler) : ISendSearchInfo(pointerToIOHandler) {};

		virtual void informAboutFinishedSearchAtCurrentDepth(
			uint32_t searchDepth,
			value_t positionValue,
			bool lowerBound,
			bool upperBound,
			uint64_t timeSpendInMilliseconds,
			uint64_t nodesSearched,
			vector<string> primaryVariant
		)
		{
			string info = "info";
			string bound = lowerBound ? " lowerbound" : upperBound ? " upperbound" : "";
			info += " time " +  to_string(timeSpendInMilliseconds);
			info += " nodes " + to_string(nodesSearched);
			info += " depth " + to_string(searchDepth + 1);
			if (positionValue >= MIN_MATE_VALUE) {
				info += " score mate " + to_string((MAX_VALUE - positionValue + 1) / 2) + bound;
			}
			else if (positionValue <= -MIN_MATE_VALUE) {
				info += " score mate " + to_string(-(MAX_VALUE + positionValue + 1) / 2) + bound;
			}
			else {
				info += " score cp " + to_string(positionValue) + bound;
			}
			info += " pv";
			for (auto& move : primaryVariant) {
				info += " " + move;
			}
			ioHandler->println(info);
		}

		virtual void informAboutChangedPrimaryVariant() {
		}

		virtual void informAboutAdvancementsInSearch(
			uint32_t searchDepth,
			value_t positionValue,
			uint64_t timeSpendInMilliseconds,
			uint64_t nodesSearched,
			uint32_t movesLeftToConsider,
			uint32_t totalAmountOfMovesToConsider,
			const string& currentConsideredMove
		)
		{
			string info = "info";
			info += " time " + to_string(timeSpendInMilliseconds);
			info += " nodes " + to_string(nodesSearched);
			info += " depth " + to_string(searchDepth + 1);
			info += " currmove " + currentConsideredMove;
			info += " currmovenumber " + to_string(totalAmountOfMovesToConsider - movesLeftToConsider);
			info += " nps " + to_string(std::llround(double(nodesSearched) * 1000.0 / timeSpendInMilliseconds));
			ioHandler->println(info);
		}

		value_t convertPositionValueToWinboardFormat(value_t positionValue) {
			if (positionValue >= MIN_MATE_VALUE) {
				positionValue = MAX_VALUE - positionValue + 100000;
			}
			if (positionValue <= -MIN_MATE_VALUE) {
				positionValue = -MAX_VALUE - positionValue - 100000;
			}
			return positionValue;
		}

	};
}

#endif // __UCIPRINTSEARCHINFO_H
