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

#include "pst.h"

using namespace ChessBasics;

EvalValue PST::_pst[PIECE_AMOUNT][BOARD_SIZE];
PST::InitStatics PST::_staticConstructor;

PST::InitStatics::InitStatics() {
	for (auto piece : { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING }) {
		for (Square square = A1; square <= H8; ++square) {
			EvalValue value;
			const uint32_t rank = uint32_t(getRank(square));
			const uint32_t file = uint32_t(getFile(square));
			const uint32_t halfFile = file > uint32_t(File::D) ? uint32_t(File::H) - file : file;
			
			switch (piece) {
			case PAWN: value = PAWN_PST[rank][file]; break;
			case KNIGHT: value = KNIGHT_PST[rank][halfFile]; break;
			case BISHOP: value = BISHOP_PST[rank][halfFile]; break;
			case ROOK: value = ROOK_PST[rank][halfFile]; break;
			case QUEEN: value = QUEEN_PST[rank][halfFile]; break;
			case KING: value = KING_PST[rank][halfFile]; break;
			default: value = 0;
			}

			_pst[WHITE + piece][square] = value;
			_pst[BLACK + piece][square] = -value;
		}
	}
}


