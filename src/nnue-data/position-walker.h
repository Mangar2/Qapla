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
 * @copyright Copyright (c) 2026 Volker Böhm
 * @Overview
 * Walks a board along a line of book moves and back again. Both the generator
 * of the position library and the generator of the training games need it: a
 * packed move has no departure square, so reading a book means replaying it
 * from the start position.
 */

#pragma once

#include <vector>

#include "../book/packed-move.h"
#include "../../basics/movelist.h"
#include "../../movegenerator/movegenerator.h"

namespace QaplaNnueData {

	/**
	 * Sets a board to the position a game starts at.
	 */
	inline void setToStartPosition(QaplaMoveGenerator::MoveGenerator& position) {
		using namespace QaplaBasics;
		static constexpr std::array<Piece, 8> BACK_RANK = {
			ROOK, KNIGHT, BISHOP, QUEEN, KING, BISHOP, KNIGHT, ROOK };
		position.clear();
		for (uint32_t file = 0; file < 8; file++) {
			const File currentFile = File(file);
			position.setPiece(computeSquare(currentFile, Rank::R1), BACK_RANK[file] + WHITE);
			position.setPiece(computeSquare(currentFile, Rank::R2), WHITE_PAWN);
			position.setPiece(computeSquare(currentFile, Rank::R7), BLACK_PAWN);
			position.setPiece(computeSquare(currentFile, Rank::R8), BACK_RANK[file] + BLACK);
		}
		position.setWhiteToMove(true);
		for (const Piece color : { WHITE, BLACK }) {
			position.setCastlingRight(color, true, true);
			position.setCastlingRight(color, false, true);
		}
		position.computeAttackMasksForBothColors();
	}

	/**
	 * The move of the position that plays the two squares of a book move, or an
	 * empty move if there is none. The move list is pseudo legal, the caller
	 * checks legality after playing it.
	 */
	inline QaplaBasics::Move findMove(QaplaMoveGenerator::MoveGenerator& position,
		const QaplaBook::BookMove& bookMove) {
		using namespace QaplaBasics;
		MoveList moveList;
		position.computeAttackMasksForBothColors();
		position.genMovesOfMovingColor(moveList);
		for (uint32_t index = 0; index < moveList.getTotalMoveAmount(); index++) {
			const Move move = moveList[index];
			if (move.getDeparture() != bookMove.from) continue;
			if (move.getDestination() != bookMove.to) continue;
			const Piece promotion = move.isPromote() ? move.getPromotion() : NO_PIECE;
			if (promotion != bookMove.promotion) continue;
			return move;
		}
		return Move::EMPTY_MOVE;
	}

	/**
	 * A board plus the line that was played on it, so that the line can be taken
	 * back move by move. It keeps the snapshots the board needs for that.
	 */
	class LineWalker {
	public:
		explicit LineWalker(QaplaMoveGenerator::MoveGenerator& position) : _position(position) {}

		/**
		 * Plays a book move. Returns false if the move is not legal in the
		 * position, in which case nothing has been played.
		 */
		bool play(const QaplaBook::BookMove& bookMove) {
			const QaplaBasics::Move move = findMove(_position, bookMove);
			if (move.isEmpty()) return false;
			const QaplaBasics::PositionSnapshot snapshot = _position.getSnapshot();
			_position.doMove(move);
			if (!_position.isLegal()) {
				unplayMove(move, snapshot);
				return false;
			}
			_path.push_back(bookMove);
			_moves.push_back(move);
			_snapshots.push_back(snapshot);
			return true;
		}

		/**
		 * Takes moves back until the line is that many plies long.
		 */
		void unplayTo(size_t plies) {
			while (_moves.size() > plies) {
				unplayMove(_moves.back(), _snapshots.back());
				_moves.pop_back();
				_snapshots.pop_back();
				_path.pop_back();
			}
		}

		void unplayAll() {
			unplayTo(0);
		}

		const std::vector<QaplaBook::BookMove>& path() const {
			return _path;
		}

		size_t plies() const {
			return _path.size();
		}

	private:
		void unplayMove(QaplaBasics::Move move, const QaplaBasics::PositionSnapshot& snapshot) {
			_position.undoMove(move, snapshot);
			// undoMove does not restore the attack masks and the move generation
			// reads them.
			_position.computeAttackMasksForBothColors();
		}

		QaplaMoveGenerator::MoveGenerator& _position;
		std::vector<QaplaBook::BookMove> _path;
		std::vector<QaplaBasics::Move> _moves;
		std::vector<QaplaBasics::PositionSnapshot> _snapshots;
	};
}
