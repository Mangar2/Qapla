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
 * Defines a chess move coded in a single 32 bit integer
 * The bit code of the move is
 * (msb) QQQQ CCCC UXAA PPPP UUDD DDDD UUOO OOOO (lsb)
 * where
 * O = departure square
 * D = destination square 
 * P = moving piece
 * A = Action
 * C = Captured piece
 * X = Capture flag (1 == is capture move)
 * Q = Promotion piece
 * N = Unused
 */

#ifndef __MOVE_H
#define __MOVE_H

#include "types.h"
#include <string>
#include <iostream>
#include <fstream>

using namespace std;

namespace ChessBasics {

class Move
{
public:
	constexpr Move(uint32_t move) : _move(move) {}
	constexpr Move(const Move& move) : _move(move._move) {}
	constexpr Move() : _move(EMPTY_MOVE) {}
	constexpr bool operator==(const Move& moveToCompare) { return moveToCompare._move == _move;  }
	constexpr bool operator!=(const Move& moveToCompare) { return moveToCompare._move != _move; }

	/**
	 * Creates a silent move
	 */
	constexpr Move(Square departure, Square destination, uint32_t movingPiece) 
		: _move(uint32_t(departure) + (destination << DESTINATION_SHIFT) +
			(movingPiece << MOVING_PIECE_SHIFT))
	{}

	/**
	 * Creates a capture move
	 */
	constexpr Move(Square departure, Square destination, uint32_t movingPiece, Piece capture):
		_move(uint32_t(departure) +
			(destination << DESTINATION_SHIFT) +
			(movingPiece << MOVING_PIECE_SHIFT) +
			(capture << CAPTURE_SHIFT))
	{}

	enum shifts : uint32_t {
		DESTINATION_SHIFT = 8,
		MOVING_PIECE_SHIFT = 16,
		CAPTURE_SHIFT = 24,
		PROMOTION_SHIFT = 28
	};

	static const uint32_t WHITE_PAWN_SHIFT = WHITE_PAWN << MOVING_PIECE_SHIFT;
	static const uint32_t BLACK_PAWN_SHIFT = BLACK_PAWN << MOVING_PIECE_SHIFT;
	static const uint32_t WHITE_KING_SHIFT = WHITE_KING << MOVING_PIECE_SHIFT;
	static const uint32_t BLACK_KING_SHIFT = BLACK_KING << MOVING_PIECE_SHIFT;

	static const uint32_t EMPTY_MOVE = 0;
	static const uint32_t NULL_MOVE = 1;
	// Action Promotion
	static const uint32_t PROMOTE = 0x00100000;
	static const uint32_t PROMOTE_UNSHIFTED = 0x00000010;
	static const uint32_t WHITE_PROMOTE = PROMOTE + WHITE_PAWN_SHIFT;
	static const uint32_t BLACK_PROMOTE = PROMOTE + BLACK_PAWN_SHIFT;
	// En passant
	static const uint32_t EP_CODE_UNSHIFTED = 0x00000020;
	static const uint32_t EP_CODE = 0x00200000;
	static const uint32_t WHITE_EP = EP_CODE + WHITE_PAWN_SHIFT;
	static const uint32_t BLACK_EP = EP_CODE + BLACK_PAWN_SHIFT;
	
	// pawn moved by two
	static const uint32_t PAWN_MOVED_TWO_ROWS = 0x00300000;
	static const uint32_t WHITE_PAWN_MOVED_TWO_ROWS = PAWN_MOVED_TWO_ROWS + WHITE_PAWN_SHIFT;
	static const uint32_t BLACK_PAWN_MOVED_TWO_ROWS = PAWN_MOVED_TWO_ROWS + BLACK_PAWN_SHIFT;
	// casteling 
	static const uint32_t KING_CASTLES_KING_SIDE = 0x00000010 + KING;
	static const uint32_t KING_CASTLES_QUEEN_SIDE = 0x00000020 + KING;
	static const uint32_t CASTLES_KING_SIDE = 0x00100000;
	static const uint32_t CASTLES_QUEEN_SIDE = 0x00200000;
	static const uint32_t WHITE_CASTLES_KING_SIDE = CASTLES_KING_SIDE + WHITE_KING_SHIFT;
	static const uint32_t BLACK_CASTLES_KING_SIDE = CASTLES_KING_SIDE + BLACK_KING_SHIFT;
	static const uint32_t WHITE_CASTLES_QUEEN_SIDE = CASTLES_QUEEN_SIDE + WHITE_KING_SHIFT;
	static const uint32_t BLACK_CASTLES_QUEEN_SIDE = CASTLES_QUEEN_SIDE + BLACK_KING_SHIFT;

	constexpr Square getDeparture() const { return Square(_move & 0x0000003F); }
	constexpr Square getDestination() const { return Square((_move & 0x00003F00) >> DESTINATION_SHIFT); }
	constexpr Piece getMovingPiece() const { return Piece((_move & 0x000F0000) >> MOVING_PIECE_SHIFT); }
	constexpr auto getAction() const { return (_move & 0x00300000); }
	constexpr auto getActionAndMovingPiece() const { return (_move & 0x003F0000); }
	constexpr auto getCaptureFlag() const { return (_move & 0x00400000); }
	constexpr Piece getCapture() const { return Piece((_move & 0x0F000000) >> CAPTURE_SHIFT); }
	constexpr Piece getPromotion() const { return Piece((_move & 0xF0000000) >> PROMOTION_SHIFT); }

	constexpr auto isEmpty() const { return _move == EMPTY_MOVE; }
	constexpr void setEmpty() { _move = EMPTY_MOVE;  }

	constexpr auto isNullMove() const { return _move == NULL_MOVE; }

	/**
	 * True, if the move is a castle move
	 */
	constexpr auto isCastleMove() const {
		const auto action = getActionAndMovingPiece();
		return action == WHITE_CASTLES_KING_SIDE || action == WHITE_CASTLES_QUEEN_SIDE || action == BLACK_CASTLES_KING_SIDE || action == BLACK_CASTLES_QUEEN_SIDE;
	}

	/**
	 * True, if the move is an en passant move
	 */
	constexpr auto isEPMove() const {
		const auto action = getActionAndMovingPiece();
		return action == WHITE_EP || action == BLACK_EP;
	}

	/**
	 * Checks, if a move is a Capture
	 */
	constexpr auto isCapture() const {
		return getCapture() != 0;
	}

	/**
	 * Checks, if a move is a Capture, but not an EP move
	 */
	constexpr auto isCaptureMoveButNotEP() const {
		return getCapture() != 0 && !isEPMove();
	}

	/**
	 * Checks, if a move is promoting a pawn
	 */
	constexpr auto isPromote() const
	{
		return (_move & 0xF0000000) != 0;
	}

	/**
     * Checks, if a move is a Capture or a promote move
     */
	constexpr auto isCaptureOrPromote() const
	{
		return (_move & 0xFF000000) != 0;
	}

	constexpr inline Move& setDeparture(square_t square) {
		_move |= square;
		return *this;
	}

	constexpr inline Move& setDestination(square_t square) {
		_move |= square << DESTINATION_SHIFT;
		return *this;
	}

	constexpr inline Move& setMovingPiece(Piece piece) {
		_move |= piece << MOVING_PIECE_SHIFT;
		return *this;
	}

	constexpr inline Move& setAction(uint32_t action) {
		_move |= action << 20;
		return *this;
	}

	constexpr inline Move& setCapture(Piece capture) {
		_move |= (capture << CAPTURE_SHIFT) + 0x00400000;
		return *this;
	}

	constexpr inline Move& setPromotion(Piece promotion) {
		_move |= promotion << PROMOTION_SHIFT;
		return *this;
	}


	/**
	 * Gets a long algebraic notation of the current move
	 */
	auto getLAN() const {
		std::string result = "??";
		if (isNullMove()) {
			result = "null";
		}
		else if (isEmpty()) {
			result = "emty";
		}
		else {
			result = "";
			if (!isPawn(getMovingPiece())) {
				// result += pieceToChar(getMovingPiece());
			}
			result += squareToString(getDeparture());
			if (getCapture() != NO_PIECE) {
				// result += 'x';
				/*
				if (!isPawn(getCapture())) {
					result += pieceToChar(getCapture());
				}
				*/
			}
			result += squareToString(getDestination());
			if (isPromote()) {
				result += pieceToPromoteChar(getPromotion());
			}
		}
		return result;
	}

	// friend class ostream& operator<<(ostream& stream, const Move& move);

	void print() {
		string moveString = getLAN();
		cout << moveString;
	}

	uint32_t getData() const { return _move;  }

private:

	uint32_t _move;
};

}

#endif // __MOVE_H

