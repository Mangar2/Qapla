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

#ifndef __WINBOARDPRINTSEARCHINFO_H
#define __WINBOARDPRINTSEARCHINFO_H

#include "ISendSearchInfo.h"
#include "IInputOutput.h"

namespace ChessInterface {

	class WinboardPrintSearchInfo : public ISendSearchInfo {
	public:
		WinboardPrintSearchInfo(IInputOutput* pointerToIOHandler) : ISendSearchInfo(pointerToIOHandler) {};

		virtual void informAboutFinishedSearchAtCurrentDepth(
			uint32_t searchDepth,
			value_t positionValue,
			bool lowerbound,
			bool upperbound,
			uint64_t timeSpendInMilliseconds,
			uint64_t nodesSearched,
			vector<string> primaryVariant
		)
		{
			const auto BUF_SIZE = 256;
			char resultBuf[BUF_SIZE];
			sprintf_s<BUF_SIZE>(resultBuf, "%d %d %lld %lld",
				searchDepth + 1,
				convertPositionValueToWinboardFormat(positionValue),
				timeSpendInMilliseconds / 10,
				nodesSearched);
			ioHandler->print(resultBuf);

			for (uint8_t ply = 0; ply < primaryVariant.size(); ply++) {
				ioHandler->print(" ");
				ioHandler->print(primaryVariant[ply].c_str());
			}

			ioHandler->println("");
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
			uint32_t movesLeftToConsider,
			uint32_t totalAmountOfMovesToConsider,
			const string& currentConsideredMove,
			uint32_t hashFullInPercent
		)
		{
			const auto BUF_SIZE = 256;
			char resultBuf[BUF_SIZE];
			sprintf_s<BUF_SIZE>(resultBuf, "stat01: %lld %lld %ld %ld %ld",
				timeSpendInMilliseconds / 10,
				nodesSearched,
				searchDepth + 1,
				movesLeftToConsider,
				totalAmountOfMovesToConsider);

			ioHandler->print(resultBuf);
			ioHandler->print(" ");
			ioHandler->println(currentConsideredMove.c_str());
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

#endif // __WINBOARDPRINTSEARCHINFO_H
