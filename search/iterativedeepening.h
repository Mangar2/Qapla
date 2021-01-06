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
 * Iteratively deepens the search ply by ply
 */

#ifndef __ITERATIVEDEEPENING_H
#define __ITERATIVEDEEPENING_H

#include "../movegenerator/movegenerator.h"
#include "search.h"
#include "../interface/clocksetting.h"
#include "computinginfo.h"
#include "clockmanager.h"
#include "../interface/isendsearchinfo.h"
#include "tt.h"
#include "AspirationWindow.h"

namespace ChessSearch {

	class IterativeDeepening {

	public:
		IterativeDeepening() { tt.setSizeInKilobytes(32736); }

		static const uint64_t ESTIMATED_TIME_FACTOR_FOR_NEXT_DEPTH = 4;

		static const uint32_t MAX_SEARCH_DEPTH = 128;

		bool hasMateFound(ComputingInfo& computingInfo) {
			const value_t SECURITY_BUFFER = 2;
			bool result = false;
			if (abs(computingInfo.positionValueInCentiPawn) > MAX_VALUE - (value_t)computingInfo.searchDepth + SECURITY_BUFFER) {
				result = true;
			}
			return result;
		}

		void searchByIterativeDeepening(
			const MoveGenerator& board, const ClockSetting& clockSetting, ComputingInfo& computingInfo, MoveHistory& moveHistory)
		{

			MoveGenerator searchBoard = board;
			computingInfo.timeControl.storeStartTime();
			timeManager.startCalculatingMove(60, clockSetting);
			computingInfo.initSearch();
			aspirationWindow.initSearch();

			uint32_t curDepth;
			uint32_t maxDepth = MAX_SEARCH_DEPTH;
			Move result = Move::EMPTY_MOVE;
			HistoryTable::clear();
			if (timeManager.isAnalyzeMode()) {
				tt.clear();
			}
			else {
				tt.setToNextMove();
			}
			moveHistory.setDrawPositionsToHash(board, tt);

			if (clockSetting.getSearchDepthLimit() > 0) {
				maxDepth = clockSetting.getSearchDepthLimit();
			}
			Eval eval;
			for (curDepth = 0; curDepth < maxDepth; curDepth++) {
				searchOneIteration(searchBoard, computingInfo, curDepth);
				if (!timeManager.mayCalculateNextDepth()) {
					break;
				}
				if (hasMateFound(computingInfo) && timeManager.stopSearchIfMateFound()) {
					break;
				}
			}
			computingInfo.statisticForMoveOrdering.print();
		}

		void stopSearch() {
			timeManager.stopSearch();
		}

	private:

		uint64_t calcSearchTime(const ClockSetting& clockSetting) {
			uint32_t movesToSearchInTime = clockSetting.getMoveAmountForClock();
			if (movesToSearchInTime == 0) {
				movesToSearchInTime = 80;
			}

			uint64_t timeToSearchForNextMove =
				clockSetting.getTimeToThinkForAllMovesInMilliseconds() / movesToSearchInTime +
				clockSetting.getTimeIncrementPerMoveInMilliseconds();

			return timeToSearchForNextMove;
		}


		void searchOneIteration(MoveGenerator& board, ComputingInfo& computingInfo, uint32_t searchDepth)
		{
			SearchStack stack(&tt);
			do {
				stack.initSearch(board, aspirationWindow.alpha, aspirationWindow.beta, searchDepth);
				if (searchDepth != 0) {
					stack.setPV(computingInfo.pvMovesStore);
				}
				// Keep the first move and use it if the following search is aborded without result
				computingInfo.pvMovesStore.setMove(1, Move::EMPTY_MOVE);

				computingInfo.searchDepth = searchDepth;
				search.searchRec(board, stack, computingInfo, timeManager);
			} while (aspirationWindow.retryWithNewWindow(computingInfo));
			computingInfo.printSearchResult();
		}


	private:
		ThinkingTimeManager timeManager;
		TT tt;
		Search search;
		AspirationWindow aspirationWindow;

	};

	static void searchByInterativeDeepening(
		const MoveGenerator& board, const ClockSetting& clockSetting, ComputingInfo& computingInfo, MoveHistory& moveHistory) {
		IterativeDeepening iterativeDeepening;
		iterativeDeepening.searchByIterativeDeepening(board, clockSetting, computingInfo, moveHistory);
	}

}


#endif // __ITERATIVEDEEPENING_H