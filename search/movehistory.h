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
 * Stores a move history for the current game
 */

#ifndef __MOVEHISTORY_H
#define __MOVEHISTORY_H

#include <vector>
#include "../basics/move.h"
#include "../movegenerator/movegenerator.h"
#include "tt.h"

using namespace std;
using namespace QaplaBasics;

namespace QaplaSearch {

	class MoveHistory {
	public:
		MoveHistory() {};
		const MoveGenerator& getStartPosition() const { return startPosition; }

		/**
		 * Sets a new start poisition
		 */
		void setStartPosition(const MoveGenerator& aBoard) {
			startPosition = aBoard;
			_history.resize(0);
		}

		/**
		 * removes all moves from the move history
		 */
		void clearMoves() {
			_history.resize(0);
		}

		/**
		 * Adds a move to the history
		 */
		void addMove(Move move) {
			_history.push_back(move);
		}

		/**
		 * Undoes a move and returns the generated board
		 */
		MoveGenerator undoMove() {
			MoveGenerator board = startPosition;
			_history.pop_back();
			for (auto move : _history) {
				board.doMove(move);
			}
			
			return board;
		}

		/**
		 * Checks for draw by three-fold repetition
		 */
		bool isDrawByRepetition(const MoveGenerator& board) {

			Board checkBoard = startPosition;
			uint16_t moveNo = 0;
			uint8_t samePositionCount = 1;
			while (moveNo + board.getHalfmovesWithoutPawnMoveOrCapture() < _history.size()) {
				checkBoard.doMove(_history[moveNo]);
				moveNo++;
			}
			for (; moveNo < _history.size(); moveNo++) {
				if (checkBoard.isIdenticalPosition(board)) {
					samePositionCount++;
				}
				checkBoard.doMove(_history[moveNo]);
			}
			return samePositionCount >= 3;

		}

		/**
		 * Sets all positions already played to hash to identify draw in the search
		 */
		void setDrawPositionsToHash(const MoveGenerator& board, TT& tt) {
			value_t drawPositionValue = board.isWhiteToMove() ? -1 : 1;
			computeDrawHashes(board);
			auto iterator = _drawHashes.rbegin();
			for (; iterator != _drawHashes.rend(); ++iterator) {
				tt.setEntry(*iterator, true, TTEntry::MAX_DEPTH, 0,
					Move::EMPTY_MOVE, drawPositionValue, -MAX_VALUE, MAX_VALUE, 0);
				drawPositionValue = -drawPositionValue;
			}
		}

		/**
		 * Sets all positions already played to hash to identify draw in the search
		 */
		void removeDrawPositionsFromHash(TT& tt) {
			for (auto drawHash : _drawHashes) {
				uint32_t entryIndex = tt.getTTEntryIndex(drawHash);
				if (entryIndex != TT::INVALID_INDEX) {
					tt.getEntry(entryIndex).clear();
				}
			}
		}


	private:

		/**
		 * Computes the draw hashes out of the history
		 */
		void computeDrawHashes(const MoveGenerator& board) {
			_drawHashes.clear();
			Board checkBoard = startPosition;
			uint16_t moveNo = 0;
			while (moveNo + board.getHalfmovesWithoutPawnMoveOrCapture() < _history.size()) {
				checkBoard.doMove(_history[moveNo]);
				moveNo++;
			}
			for (; ; moveNo++) {
				_drawHashes.push_back(checkBoard.computeBoardHash());
				if (moveNo == _history.size()) {
					break;
				}
				checkBoard.doMove(_history[moveNo]);
			}
		}

		vector<Move> _history;
		vector<hash_t> _drawHashes;
		MoveGenerator startPosition;
	};

}

#endif // __MOVEHISTORY_H