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
 */


#include "kingAttack.h"

using namespace ChessEval;

array<bitBoard_t, BOARD_SIZE> KingAttack::_kingAttackBB[2];
KingAttack::InitStatics KingAttack::_staticConstructor;

KingAttack::InitStatics::InitStatics() {
	// ToDo: Add a third file for kings on the 1 or 8 files
	for (Square kingSquare = A1; kingSquare <= H8; ++kingSquare) {
		bitBoard_t attackArea = 1ULL << kingSquare;
		attackArea |= BitBoardMasks::shift<WEST>(attackArea);
		attackArea |= BitBoardMasks::shift<EAST>(attackArea);
		attackArea |= BitBoardMasks::shift<SOUTH>(attackArea);
		attackArea |= BitBoardMasks::shift<NORTH>(attackArea);
		// The king shall not have a smaller attack area, if at the edges of the board
		if (getFile(kingSquare) == File::A) {
			attackArea |= BitBoardMasks::shift<EAST>(attackArea);
		}
		if (getFile(kingSquare) == File::H) {
			attackArea |= BitBoardMasks::shift<WEST>(attackArea);
		}
		if (getRank(kingSquare) == Rank::R1) {
			attackArea |= BitBoardMasks::shift<NORTH>(attackArea);
		}
		if (getRank(kingSquare) == Rank::R8) {
			attackArea |= BitBoardMasks::shift<SOUTH>(attackArea);
		}
		// _kingAttackBB[WHITE][kingSquare] = attackArea | BitBoardMasks::shift<NORTH>(attackArea);
		// _kingAttackBB[BLACK][kingSquare] = attackArea | BitBoardMasks::shift<SOUTH>(attackArea);
	}
}