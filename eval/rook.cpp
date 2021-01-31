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
 */

#include "rook.h"

using namespace ChessEval;

array<value_t, Rook::INDEX_SIZE * 2> Rook::indexToValue;
Rook::InitStatics Rook::_staticConstructor;

Rook::InitStatics::InitStatics() {
	indexToValue.fill(0);
	for (uint32_t i = 0; i < INDEX_SIZE; i++) {
		if (i & TRAPPED) { addToIndexMap(i, _trapped); }
		if (i & OPEN_FILE) { addToIndexMap(i, _openFile); }
		if (i & HALF_OPEN_FILE) { addToIndexMap(i, _halfOpenFile); }
	}
}







