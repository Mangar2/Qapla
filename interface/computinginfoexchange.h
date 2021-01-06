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

#ifndef __COMPUTINGINFOEXCHANGE_H
#define __COMPUTINGINFOEXCHANGE_H

#include <string>

using namespace std;

namespace ChessInterface {

	class ComputingInfoExchange {
	public:
		ComputingInfoExchange() {
			elapsedTimeInMilliseconds = 0;
			nodesSearched = 0;
			searchDepth = 1;
			movesLeftToConcider = 0;
			totalAmountOfMovesToConcider = 0;
			currentConsideredMove = "";
			currentMoveNoSearched = 0;
			positionValueInCentiPawn = 0;
		};

		uint64_t elapsedTimeInMilliseconds;
		uint64_t nodesSearched;
		uint32_t searchDepth;
		uint32_t movesLeftToConcider;
		uint32_t totalAmountOfMovesToConcider;
		uint32_t currentMoveNoSearched;
		int32_t positionValueInCentiPawn;
		string currentConsideredMove;
	};
}

#endif // __COMPUTINGINFO_H