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
 * @copyright Copyright (c) 2024 Volker Böhm
 * @Overview
 * Implements helper functions for eval
 */

#ifndef __EVAL_HELPER_H
#define __EVAL_HELPER_H

#include "../basics/types.h"
#include "../basics/evalvalue.h"

using namespace ChessBasics;

class EvalHelper {
public:
	/**
	 * Distance of two squares is the maximum of rank and file distance
	 * We use a 15x15 lookup table to get the distance without jumps
	 */
	static inline value_t computeDistance(Square a, Square b) {
		int32_t dx = (a & 7) - (b & 7);
		int32_t dy = (a >> 3) - (b >> 3);
		return _distTable[(dx + 7) + 15 * (dy + 7)];
	}

private: 
	static const value_t DISTANCES_SIZE = 15 * 15;
	static value_t _distTable[DISTANCES_SIZE];

	/**
	 * Initializes static tables for the move generator
	 */
	static struct InitStatics {
		InitStatics();
	} _staticConstructor;
};

#endif