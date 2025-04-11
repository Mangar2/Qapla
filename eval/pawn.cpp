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

#include "pawn.h"

using namespace ChessEval;

/*
 * Computes a lookup table where for each 8-bit file occupancy (pawnPresenceMask),
 * we get a bitboard marking all isolated pawn files (all ranks of that file set to 1).
 */
std::array<bitBoard_t, Pawn::LOOKUP_TABLE_SIZE> Pawn::computeIsolatedPawnLookupTable() {
	std::array<bitBoard_t, LOOKUP_TABLE_SIZE> table{};
	const bitBoard_t FILE_MASK_A = 0x0101010101010101ULL;
	table[0] = 0;  // No pawns -> no isolated files

	for (uint32_t pawnPresenceMask = 1; pawnPresenceMask < LOOKUP_TABLE_SIZE; pawnPresenceMask++) {
		bitBoard_t result = 0;

		for (int file = 0; file < 8; file++) {
			bool hasPawn = (pawnPresenceMask >> file) & 1;

			if (!hasPawn)
				continue;

			bool leftHasPawn = (file > 0) && ((pawnPresenceMask >> (file - 1)) & 1);
			bool rightHasPawn = (file < 7) && ((pawnPresenceMask >> (file + 1)) & 1);

			if (!leftHasPawn && !rightHasPawn) {
				// Isolated pawn on this file
				result |= (FILE_MASK_A << file);
			}
		}
		table[pawnPresenceMask] = result;
	}
	return table;
}

