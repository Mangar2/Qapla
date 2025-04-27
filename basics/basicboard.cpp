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

#include "basicboard.h"


using namespace QaplaBasics;

void BasicBoard::clear() {
	kingStartSquare = { E1, E8 };
	queenRookStartSquare = { A1, A8 };
	kingRookStartSquare = { H1, H8 };
	boardState.initialize();
	_board.fill(NO_PIECE);
}

void BasicBoard::initClearCastleMask() {
	_clearCastleFlagMask.fill(0xFFFF);
	_clearCastleFlagMask[queenRookStartSquare[WHITE]] = static_cast<uint16_t>(~BoardState::WHITE_QUEEN_SIDE_CASTLE_BIT);
	_clearCastleFlagMask[kingRookStartSquare[WHITE]] = static_cast<uint16_t>(~BoardState::WHITE_KING_SIDE_CASTLE_BIT);
	_clearCastleFlagMask[queenRookStartSquare[BLACK]] = static_cast<uint16_t>(~BoardState::BLACK_QUEEN_SIDE_CASTLE_BIT);
	_clearCastleFlagMask[kingRookStartSquare[BLACK]] = static_cast<uint16_t>(~BoardState::BLACK_KING_SIDE_CASTLE_BIT);
	_clearCastleFlagMask[kingStartSquare[WHITE]] = 
		static_cast<uint16_t>(~(BoardState::WHITE_QUEEN_SIDE_CASTLE_BIT + BoardState::WHITE_KING_SIDE_CASTLE_BIT));
	_clearCastleFlagMask[kingStartSquare[BLACK]] = 
		static_cast<uint16_t>(~(BoardState::BLACK_QUEEN_SIDE_CASTLE_BIT + BoardState::BLACK_KING_SIDE_CASTLE_BIT));
}

bool BasicBoard::assertMove(Move move) const {
	assert(move.getMovingPiece() != NO_PIECE);
	assert(move.getDeparture() != move.getDestination());
	if (!(move.getMovingPiece() == operator[](move.getDeparture()))) {
		move.print();
	}
	assert(move.getMovingPiece() == operator[](move.getDeparture()));
	assert((move.getCapture() == operator[](move.getDestination())) || move.isCastleMove() || move.isEPMove());
	return true;
}
