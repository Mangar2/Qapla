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
 * Defines a list of standard constants for calculating transposition table hashes
 */

#ifndef __HASHCONSTANTS_H
#define __HASHCONSTANTS_H

#include <cstdint>
#include "types.h"

namespace QaplaBasics {

	typedef uint64_t hash_t;

	class HashConstants
	{
	public:
		static const hash_t cHashBoardRandoms[BOARD_SIZE][PIECE_AMOUNT];
		static const hash_t EP_RANDOMS[BOARD_SIZE];
		static const hash_t COLOR_RANDOMS[2];
		static const hash_t CASTLE_RANDOMS[64];

	private:
		HashConstants(void) {};

	};

}
#endif

