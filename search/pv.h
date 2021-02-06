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
 * Implements a storage for primary variants of the search
 * An empty move in the PV line indicates the end of the PV line
 */

#ifndef __PV_H
#define __PV_H

#include <array>
#include <string>
#include "searchdef.h"
#include "../basics/move.h"

using namespace std;
using namespace ChessBasics;

namespace ChessSearch {
	class PV
	{

	public:
		static const uint8_t MAX_PV_LENGTH = 25;
		typedef uint32_t pvIndex_t;

		PV() { clear(); }

		/**
		 * Copy operator - just copying moves until the first empty move
		 * (for efficiency).
		 */
		void operator=(const PV& pvToCopy) {
			for (uint32_t index = 0; index < MAX_PV_LENGTH; index++) {
				movesStore[index] = pvToCopy.movesStore[index];
				if (movesStore[index].isEmpty()) {
					break;
				}
			}
		}

		/**
		 * Checks, if two PV contains different moves up to the first empty move
		 */
		bool operator!=(const PV& pv) {
			bool result = false;
			for (ply_t ply = 0; ply < MAX_PV_LENGTH; ply++) {
				if (movesStore[ply] != pv.movesStore[ply]) {
					result = true;
					break;
				}
				if (movesStore[ply].isEmpty()) {
					break;
				}
			}
			return result;
		}

		/**
		 * Copies the remaining moves from another PV
		 * @param pvToCopy PV containing the moves to copy
		 * @param firstPly ply of the first move to copy
		 */
		void copyFromPV(const PV& pvToCopy, uint32_t firstPly) {
			for (uint32_t index = firstPly; index < MAX_PV_LENGTH; index++) {
				movesStore[index] = pvToCopy.movesStore[index];
				if (movesStore[index] == Move::EMPTY_MOVE) {
					break;
				}
			}
		}

		/**
		 * Clears the pv by setting the first move to empty
		 */
		void clear()
		{
			movesStore[0].setEmpty();
		}

		/**
		 * Gets a move from the PV line - returns an empty move, if there is no move available
		 */
		Move getMove(uint32_t ply) const
		{
			Move pvMove;
			if (ply < MAX_PV_LENGTH) {
				pvMove = movesStore[ply];
			}
			return pvMove;
		}

		/**
		 * Sets a move to the PV, if ply is in the storange range
		 */
		void setMove(uint32_t ply, Move move)
		{
			if (ply < MAX_PV_LENGTH) {
				movesStore[ply] = move;
			}
		}

		void setEmpty(uint32_t ply)
		{
			if (ply < MAX_PV_LENGTH) {
				movesStore[ply].setEmpty();
			}
		}

		/**
		 * Gets the PV as string
		 */
		string toString() const {
			string result;
			string spacer = "";
			for (ply_t ply = 0; ply < MAX_PV_LENGTH && !movesStore[ply].isEmpty(); ply++) {
				result += spacer + movesStore[ply].getLAN();
				spacer = " ";
			}
			return result;
		}

		/**
		 * Prints the pv
		 */
		void print(uint32_t startPly) const {
			for (ply_t ply = startPly; ply < MAX_PV_LENGTH && !movesStore[ply].isEmpty(); ply++) {
				if (ply % 2 == 0) {
					printf("%ld.", ply / 2 + 1);
				}
				else if (ply == startPly) {
					printf("%ld...... ", ply / 2 + 1);
				}
				printf("%5s ", movesStore[ply].getLAN().c_str());
			}
		}
	private:
		array<Move, MAX_PV_LENGTH> movesStore;

	};
}

#endif // __PV