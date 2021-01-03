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
 * Implements some type and constant defintions used for search algorithms
 */

#ifndef __SEARCHDEF_H
#define __SEARCHDEF_H

#include <assert.h>
#include "../basics/types.h"

namespace ChessSearch {
	typedef uint32_t ply_t;

	const ply_t ONE_PLY = 1;
}

#endif // __SEARCHDEF_H