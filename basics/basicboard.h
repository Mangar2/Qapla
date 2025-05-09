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

	/**
	 * Checks, if two positions are identical
	 */
	bool isIdenticalPosition(const BasicBoard& boardToCompare) const {
		return _whiteToMove == boardToCompare._whiteToMove &&_board == boardToCompare._board;
	}

	/**
	 * Access a single spot on the board
	 */
	inline Piece operator[] (Square square) const {
		return _board[square];
	}

	
	/**
	 * Update all based for doMove
	 * @param departure departure position of the move
	 * @param destination destination position of the move
	 */
	inline void updateStateOnDoMove(Square departure, Square destination) {
		_whiteToMove = !_whiteToMove;
		_boardState.clearEP();
		_boardState.disableCastlingRightsByMask(
			_clearCastleFlagMask[departure] & _clearCastleFlagMask[destination]);
		_boardState.halfmovesWithoutPawnMoveOrCapture++;
		bool isCapture = _board[destination] != NO_PIECE;
		bool isPawnMove = isPawn(_board[departure]);
		bool isMoveTwoRanks = ((departure - destination) & 0x0F) == 0;
		if (isCapture || isPawnMove) {
			_boardState.halfmovesWithoutPawnMoveOrCapture = 0;
			_boardState.fenHalfmovesWithoutPawnMoveOrCapture = 0;
		}
		if (isPawnMove && isMoveTwoRanks) {
			_boardState.setEP(destination);
		}
	}

	/**
	 * Update all states for undo move
	 * @param recentBoardState the board state stored before doMove
	 */
	inline void updateStateOnUndoMove(BoardState recentBoardState) {
		_whiteToMove = !_whiteToMove;
		_boardState = recentBoardState;
	}

	// Current color to move
	bool _whiteToMove;


	BoardState _boardState;
	array<uint16_t, 64> _clearCastleFlagMask;

	bool isInBoard(Square square) { return square >= A1 && square <= H8; }

	array<Piece, BOARD_SIZE> _board;


};

}

#endif // __BASICBOARD_H

