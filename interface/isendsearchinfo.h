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
 * Interface for Input/Output handling - usually to receive token from the console and to print
 * text to the console. Still the interface is independent and can be used for any input/output.
 */

#ifndef __SENDSEARCHINFO_H
#define __SENDSEARCHINFO_H

#include "../basics/types.h"
#include "../basics/evalvalue.h"
#include "iinputoutput.h"
#include <string>
#include <vector>

using namespace std;
using namespace ChessBasics;

namespace QaplaInterface {

	typedef vector<string> MoveStringList;

	class ISendSearchInfo {

	public:
		ISendSearchInfo(IInputOutput* pointerToIOHandler) : ioHandler(pointerToIOHandler) {}

		/**
		 * Call this function to inform about a finished iteration of an 
		 * iterative deepening algorithm for chess search
		 * @param searchDepth depth (amount of plys) searched
		 * @param positionValue computed position value (+100 for a white pawn advantage, -100 for a black pawn advantage)
		 * @param timeSpendInMilliseconds (time spend to calculate the current position including the search 
		 * until reaching the current depth)
		 * @param nodesSearched amount of nodes (usually calls to "set move") searched so far
		 * @param tbHits amount of positions found in table bases or bit bases.
		 * @param primaryVariant List of expected moves in a chess notation (like e4 e5 NF3 Nc6)
		 */
		virtual void informAboutFinishedSearchAtCurrentDepth(
			uint32_t searchDepth,
			value_t positionValue,
			bool lowerbound,
			bool upperbound,
			uint64_t timeSpendInMilliseconds,
			uint64_t nodesSearched,
			uint64_t tbHits,
			MoveStringList primaryVariant) = 0;

		/**
		 * Informs that the primary variant has changed. 
		 */
		virtual void informAboutChangedPrimaryVariant() = 0;

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
			uint32_t hashFullInPercent) = 0;

		//virtual void printStatistic() = 0;

	protected:
		IInputOutput* ioHandler;
	};

}

#endif // __SENDSEARCHINFO_H