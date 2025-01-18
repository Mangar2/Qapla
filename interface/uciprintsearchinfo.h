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

namespace QaplaInterface {

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
			uint64_t tbHits,
			vector<string> primaryVariant
		)
		{
			string info = "info";
			string bound = lowerBound ? " lowerbound" : upperBound ? " upperbound" : "";
			info += " time " +  to_string(timeSpendInMilliseconds);
			info += " nodes " + to_string(nodesSearched);
			info += " tbhits " + to_string(tbHits);
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

		/**
		 * Informs amount an advancement in search like the next move on ply 0
		 * @param searchDepth depth (amount of plys) searched
		 * @param positionValue computed position value (+100 for a white pawn advantage, -100 for a black pawn advantage)
		 * @param timeSpendInMilliseconds (time spend to calculate the current position including the search
		 * until reaching the current depth)
		 * @param nodesSearched number of nodes (usually calls to "set move") searched so far
		 * @param tbHits amount of positions found in table bases or bit bases.
		 * @param movesLeftToConsider number of moves to be searched
		 * @param totalAmountOfMovesToConsider total number of possible moves in the actual chess position
		 * @param currentConsideredMove move currently concidered (in chess notation)
		 * @param hashFullInPercent the hash fill rate in percent
		 */
		virtual void informAboutAdvancementsInSearch(
			uint32_t searchDepth,
			value_t positionValue,
			uint64_t timeSpendInMilliseconds,
			uint64_t nodesSearched,
			uint64_t tbHits,
			uint32_t movesLeftToConsider,
			uint32_t totalAmountOfMovesToConsider,
			const string& currentConsideredMove,
			uint32_t hashFullInPercent
		)
		{
			string info = "info";
			info += " time " + to_string(timeSpendInMilliseconds);
			info += " nodes " + to_string(nodesSearched);
			info += " tbhits " + to_string(tbHits);
			info += " depth " + to_string(searchDepth + 1);
			info += " currmove " + currentConsideredMove;
			info += " currmovenumber " + to_string(totalAmountOfMovesToConsider - movesLeftToConsider);
			info += " hashfull " + to_string(hashFullInPercent);
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
