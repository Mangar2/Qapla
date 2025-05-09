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
 * Defines a chess board with 64 squares holding a chess postion
 */

#ifndef __BASICBOARD_H
#define __BASICBOARD_H

#include "types.h"
#include "move.h"
#include "boardstate.h"
#include <assert.h>

namespace QaplaBasics {

class BasicBoard
{
public:

	array<Piece, BOARD_SIZE> _board;
};

}

#endif // __BASICBOARD_H

