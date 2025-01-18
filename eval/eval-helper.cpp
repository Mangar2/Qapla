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

#include "eval-helper.h"

/**
 *  This declaration effectively calls the static constructor of the class when loading the program
 */
EvalHelper::InitStatics EvalHelper::_staticConstructor;
value_t EvalHelper::_distTable[DISTANCES_SIZE];

/**
 * 
 * 
 */
EvalHelper::InitStatics::InitStatics() {
    for (int dx = -7; dx <= 7; ++dx)
    {
        for (int dy = -7; dy <= 7; ++dy)
        {
            int dist = max(abs(dx), abs(dy));
            _distTable[(dx + 7) + 15 * (dy + 7)] = dist;
        }
    }
}
