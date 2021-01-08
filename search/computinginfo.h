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
 * Class containing a set of information taken from the Chess-Search algorithm
 * - The elapsed time in milliseconds for the current search
 * - The amount of nodes (calls to the move generator) searched
 * - The search depth - the horizont for the search
 * - The amount of moves left in the current search depth to consider
 * - The total amount of moves to consider in the current search depth
 * - The currently concidered move
 */

#ifndef __COMPUTINGINFO_H
#define __COMPUTINGINFO_H

#include "../basics/move.h"
#include "../interface/stdtimecontrol.h"
#include "pv.h"
#include "../interface/isendsearchinfo.h"
//#include "StatisticForMoveOrdering.h"
#include "searchparameter.h"

using namespace ChessInterface;

namespace ChessSearch {
	class ComputingInfo {
	public:
		ComputingInfo(ISendSearchInfo* sendSearchInfoPointer) : sendSearchInfo(sendSearchInfoPointer) {
			clear();
		}

		void clear() {
			searchDepth = 0;
			nodesSearched = 0;
			debug = false;
			totalAmountOfMovesToConcider = 0;
			currentMoveNoSearched = 0;
			positionValueInCentiPawn = 0;
			printRequest = false;
			timeControl.storeStartTime();
		}

		void initSearch() {
			nodesSearched = 0;
			if (SearchParameter::CLEAR_ORDERING_STATISTIC_BEFORE_EACH_MOVE) {
				// statisticForMoveOrdering.clear();
			}
		}

		void requestPrintSearchInfo() { printRequest = true; }

		void printSearchInfoIfRequested() {
			if (printRequest && verbose && sendSearchInfo != 0) {
				sendSearchInfo->informAboutAdvancementsInSearch(
					searchDepth,
					positionValueInCentiPawn,
					timeControl.getTimeSpentInMilliseconds(),
					nodesSearched,
					totalAmountOfMovesToConcider - currentMoveNoSearched,
					totalAmountOfMovesToConcider,
					currentConcideredMove.getLAN()
				);
				printRequest = false;
			}
		}

		void printSearchResult()
		{
			MoveStringList primaryVariant;
			for (uint8_t ply = 0; ply <= searchDepth; ply++) {
				Move move = pvMovesStore.getMove(ply);
				if (move == Move::EMPTY_MOVE) {
					break;
				}
				primaryVariant.push_back(move.getLAN());
			}
			if (verbose && sendSearchInfo != 0) {
				sendSearchInfo->informAboutFinishedSearchAtCurrentDepth(
					searchDepth,
					positionValueInCentiPawn,
					timeControl.getTimeSpentInMilliseconds(),
					nodesSearched,
					primaryVariant);
			}
		}

		void updatePV(PV& pv) {
			if (pv != pvMovesStore && !pv.getMove(0).isEmpty()) {
				pvMovesStore = pv;
				printSearchResult();
			}
		}

		void setVerbose(bool isVerbose) {
			verbose = isVerbose;
		}

		ISendSearchInfo* sendSearchInfo;
		StdTimeControl timeControl;
		value_t positionValueInCentiPawn;
		uint32_t searchDepth;
		uint32_t totalAmountOfMovesToConcider;
		Move currentConcideredMove;
		uint32_t currentMoveNoSearched;
		uint64_t nodesSearched;
		// StatisticForMoveOrdering statisticForMoveOrdering;

		volatile bool printRequest;
		PV pvMovesStore;
		bool debug;
		bool verbose;
	};

}

#endif // __COMPUTINGINFO_H