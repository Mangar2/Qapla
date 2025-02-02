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
 * Implements a killer move - storing moves working well in similar positions
 * Stores two killers, tests with one or three showed that two killers are best
 */

#ifndef __KILLERMOVE_H
#define __KILLERMOVE_H

#include <assert.h>
#include <array>
#include "../basics/move.h"

using namespace std;
using namespace QaplaBasics;

namespace QaplaSearch {
	class KillerMove {
	public:
		KillerMove() { }
		KillerMove(const KillerMove& killerMove) { operator=(killerMove); }
		KillerMove& operator=(const KillerMove& killerMove) {
			_killer = killerMove._killer;
			captureKiller = killerMove.captureKiller;
			return *this;
		}
		/**
		 * Gets a killer move
		 */
		Move operator[](uint32_t killerNo) const {
			assert(killerNo < MAX_KILLER_PER_PLY);
			return _killer[killerNo];
		}

		/**
		 * Sets the best move of the position as killer
		 */
		void setKiller(Move move) {
			if (!move.isEmpty()) {
				if (move.isCapture()) {
					captureKiller = move;
				}
				else {
					_killer[1] = _killer[0];
					_killer[0] = move;
				}
			}
		}

		/**
		 * Gets a capturing killer move
		 */
		Move getCaptureKiller() {
			return captureKiller;
		}

	private:
		static const uint32_t MAX_KILLER_PER_PLY = 2;
		array<Move, MAX_KILLER_PER_PLY> _killer;
		Move captureKiller;
	};

}

#endif // __KILLERMOVE_H

