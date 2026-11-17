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
 * @copyright Copyright (c) 2025 Volker Böhm
 * @Overview
 */

#ifndef __BUTTERFLY_BOARDS_H
#define __BUTTERFLY_BOARDS_H

#include <array>
#include <cassert>
#include <cstdlib>
#include "searchdef.h"
#include "search-config.h"
#include "tunable.h"
#include "../basics/move.h"

#include <iomanip>

using namespace QaplaBasics;

namespace QaplaSearch {

	using statistic_t = int32_t;

	/**
	 * One entry of type T per moving piece and destination square of a move, in one flat array.
	 * Index = (moving piece - WHITE_PAWN) * 64 + destination: the pieces run from WHITE_PAWN (2) to
	 * BLACK_KING (13), so 12 x 64 = 768 entries without gaps. The constant -128 folds into the
	 * address offset.
	 */
	template<typename T>
	class MoveKeyedTable {
	public:
		static constexpr uint32_t SIZE = (QaplaBasics::BLACK_KING - QaplaBasics::WHITE_PAWN + 1) * QaplaBasics::BOARD_SIZE;

		T& operator[](QaplaBasics::Move move) { return _table[index(move)]; }
		const T& operator[](QaplaBasics::Move move) const { return _table[index(move)]; }

		void fill(const T& value) { _table.fill(value); }

		/**
		 * @returns true, if the move can key the table. An empty move (root, start of the search)
		 * and the null move carry no piece. Checked by assert only: the caller knows these cases
		 * and does not access the table for them.
		 */
		static constexpr bool isKey(QaplaBasics::Move move) {
			return !move.isEmpty() && !move.isNullMove();
		}

	private:
		static constexpr uint32_t index(QaplaBasics::Move move) {
			assert(isKey(move));
			return (uint32_t(move.getMovingPiece()) - QaplaBasics::WHITE_PAWN) * QaplaBasics::BOARD_SIZE
				+ uint32_t(move.getDestination());
		}

		std::array<T, SIZE> _table;
	};

	class ButterflyBoard {
	public:

		void clear() {
			_board.fill(0);
		}
		
		void newBestMove(QaplaBasics::Move move, ply_t depth, const Move* moves, uint32_t triedMoves) {
			if (move.isCapture() || move.isEmpty()) {
				return;
			}
			const auto change = computeChange(depth + 1);
			if (change <= 0) {
				return;
			}
			addToValue(move, change);
			for (uint32_t i = 0; i < triedMoves; i++) {
				const auto subMove = moves[i];
				if (subMove == move) {
					break;
				}
				if (!subMove.isCapture()) {
					subFromValue(subMove, change);
				}
			}
		}
	
		statistic_t getValue(QaplaBasics::Move move) const {
			return _board[move];
		}

		void print() {
			constexpr int valueWidth = 5; // Breite f�r die rechtsb�ndige Werteanzeige

			std::cout << "       A         B         C         D         E         F         G         H\n";
			std::cout << "  +---------+---------+---------+---------+---------+---------+---------+---------+\n";

			for (Rank r = Rank::R8; r >= Rank::R1; --r) {
				for (Piece p = WHITE_PAWN; p <= BLACK_KING; ++p) {
					if (p == WHITE_ROOK) {
						std::cout << uint32_t(r) + 1 << " |";
					}
					else {
						std::cout << "  |";
					}

					for (File f = File::A; f <= File::H; ++f) {
						const auto pch = QaplaBasics::pieceToChar(p);
						const auto move = Move(NO_SQUARE, computeSquare(f, r), p);
						const auto value = getValue(move);

						std::cout << " " << pch << ":" << std::setw(valueWidth) << value << " |";
					}
					std::cout << "\n";
				}

				std::cout << "  +---------+---------+---------+---------+---------+---------+---------+---------+\n";
			}
		}


	private:

		// A change against the sign of the entry first halves it; then gravity pulls the entry towards
		// zero by |change| / gravity of its value, so the entries saturate at +-gravity.
		void addToValue(QaplaBasics::Move move, statistic_t value) {
			statistic_t& entry = _board[move];
			if (entry < 0) {
				entry /= 2;
			}
			entry += value - entry * std::abs(value) / gravity();
		}

		void subFromValue(QaplaBasics::Move move, statistic_t value) {
			statistic_t& entry = _board[move];
			if (entry > 0) {
				entry /= 2;
			}
			entry += -value - entry * std::abs(value) / gravity();
		}

		// The change grows linearly with the depth up to a cap. The slope 128 is fixed: the values only sort
		// moves, so scaling every constant of the update by one factor changes nothing, and one of them has
		// to stay fixed for the others to be tunable. The cap is reached at depth 20 only, which test games
		// at short time controls never search, so it cannot be tuned there and stays a constant.
		statistic_t computeChange(ply_t depth) {
			constexpr bool OPT = SearchConfig::optimizeHistory;
			return std::min<statistic_t>(128 * depth - tunable<OPT, "historyBonusOffset", 131, 0, 262>(),
				128 * 20);
		}

		// Gravity: every update pulls the entry towards zero by |change| / gravity of its value, the
		// entries saturate at +-gravity.
		statistic_t gravity() {
			constexpr bool OPT = SearchConfig::optimizeHistory;
			return tunable<OPT, "historyGravity", 14000, 7000, 21000>();
		}

		MoveKeyedTable<statistic_t> _board;

	};

	/**
	 * One row of the continuation history: the history of every move played after one earlier move
	 * of the search line - a butterfly board of its own, filled the same way. A node holds a pointer
	 * to the row of the move that led into it, chosen once when the move is made.
	 */
	using ContinuationHistoryRow = ButterflyBoard;

	/**
	 * Continuation history: one butterfly board per earlier move, keyed by its moving piece and
	 * destination - 768 rows of 768 values. All distances (the move one ply back, two plies back,
	 * ...) share this one table; they differ only in which ply's row pointer a node uses, the
	 * caller's choice. Moves that cannot key a row (the empty move at the root, the null move) get
	 * the dummy row: it is read like any other and never updated.
	 */
	class ContinuationButterflyBoard {
	public:
		void clear() {
			_dummy.clear();
			_rows.fill(_dummy);
		}

		/**
		 * @returns the row of the moves played after previousMove, the dummy row for an empty or
		 * null move
		 */
		ContinuationHistoryRow* getRow(QaplaBasics::Move previousMove) {
			return MoveKeyedTable<ContinuationHistoryRow>::isKey(previousMove) ? &_rows[previousMove] : &_dummy;
		}

		/**
		 * @returns true, if row is the dummy row, which is never updated
		 */
		bool isDummy(const ContinuationHistoryRow* row) const {
			return row == &_dummy;
		}

	private:
		MoveKeyedTable<ContinuationHistoryRow> _rows;
		ContinuationHistoryRow _dummy;
	};
};



#endif // __BUTTERFLY_BOARDS_H