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

#include "kingpawnattack.h"

using namespace ChessEval;

KingPawnAttack::KingPawnAttack()
{
}


template<Piece COLOR>
inline void KingPawnAttack::initRace(MoveGenerator& board) {
	legalPositions[COLOR] = ~(board.getPieceBB(PAWN + COLOR) | board.pawnAttack[switchColor(COLOR)]);
	weakPawns[COLOR] = board.getPieceBB(PAWN + COLOR) & ~board.pawnAttack[COLOR];
	kingPositions[COLOR] = board.getPieceBB(KING + COLOR);
	kingAttack[COLOR] = BitBoardMasks::moveInAllDirections(kingPositions[COLOR]);
	formerPositions[COLOR] = kingPositions[COLOR];
	formerAttack[COLOR] = kingAttack[COLOR];
}

template<Piece COLOR>
inline void KingPawnAttack::makeMove() {
	kingPositions[COLOR] = kingAttack[COLOR] & ~formerPositions[COLOR] & legalPositions[COLOR];
	kingPositions[COLOR] &= ~kingAttack[switchColor(COLOR)];
	kingAttack[COLOR] = BitBoardMasks::moveInAllDirections(kingPositions[COLOR]);
	kingPositions[COLOR] &= ~formerAttack[switchColor(COLOR)];
	formerPositions[COLOR] |= kingPositions[COLOR];
	formerAttack[COLOR] |= kingAttack[COLOR];
}

template<Piece COLOR>
inline bool KingPawnAttack::capturesPawn() {
	bool result = (kingPositions[COLOR] & weakPawns[switchColor(COLOR)]) != 0;
	return result;
}

value_t KingPawnAttack::computeKingRace(MoveGenerator& board) {
	initRace<WHITE>(board);
	initRace<BLACK>(board);
	bool wtm = board.isWhiteToMove();
	value_t pawnCaptured = 0;
	Piece winningColor = NO_PIECE;

	while (kingPositions[WHITE] != 0 || kingPositions[BLACK] != 0) {
		if (wtm) {
			makeMove<WHITE>();
			if (capturesPawn<WHITE>()) {
				pawnCaptured++;
				if (pawnCaptured != 1) {
					break;
				}
			}
		}
		else {
			makeMove<BLACK>();
			if (capturesPawn<BLACK>()) {
				pawnCaptured--;
				if (pawnCaptured != -1) {
					break;
				}
			}
		}
		wtm = !wtm;
	}
	return pawnCaptured;
}
