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
#include <cstdlib>
#include "searchdef.h"
#include "search-config.h"
#include "tunable.h"
#include "../basics/move.h"

#include <iomanip>

using namespace QaplaBasics;

namespace QaplaSearch {

	using statistic_t = int32_t;

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
	
		statistic_t getValue(QaplaBasics::Move move) {
			uint32_t index = computeIndex(move);
			return _board[index];
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


		void count() {
			uint32_t positive = 0;
			uint32_t negative = 0;
			for (uint32_t i = 0; i < ButterflyBoard::SIZE; i++) {
				if (_board[i] > 0) {
					positive++;
				}
				if (_board[i] < 0) {
					negative++;
				}
			}
			std::cout << "Positive: " << positive << " Negative: " << negative << std::endl;
		}
	private:

		void addToValue(QaplaBasics::Move move, statistic_t value) {
			uint32_t index = computeIndex(move);
			if (_board[index] < 0) {
				_board[index] /= 2;
			}
			_board[index] += value - _board[index] * std::abs(value) / gravity();
			if (_board[index] > MAX_HIST) {
				reduce();
			}
		}

		void subFromValue(QaplaBasics::Move move, statistic_t value) {
			uint32_t index = computeIndex(move);
			if (_board[index] > 0) {
				_board[index] /= 2;
			}
			_board[index] += -value - _board[index] * std::abs(value) / gravity();
			if (_board[index] < -MAX_HIST) {
				reduce();
			}
		}

		constexpr uint32_t computeIndex(QaplaBasics::Move move) {
			return move.getPiceAndDestination();
		}

		// The change grows linearly with the depth up to a cap. The slope 128 is fixed: the values only sort
		// moves, so scaling every constant of the update by one factor changes nothing, and one of them has
		// to stay fixed for the others to be tunable. The cap is reached at depth 20 only, which test games
		// at short time controls never search, so it cannot be tuned there and stays a constant.
		statistic_t computeChange(ply_t depth) {
			constexpr bool OPT = SearchConfig::optimizeHistory;
			return std::min<statistic_t>(128 * depth - tunable<OPT, "historyBonusOffset", 131, 0, 256>(),
				128 * 20);
		}

		// Gravity: every update pulls the entry towards zero by |change| / gravity of its value, the
		// entries saturate at +-gravity.
		statistic_t gravity() {
			constexpr bool OPT = SearchConfig::optimizeHistory;
			return tunable<OPT, "historyGravity", 14000, 1, 25599>();
		}

		void reduce() {
			for (uint32_t i = 0; i < SIZE; i++) {
				_board[i] /= 2;
			}
		}

		static const statistic_t MAX_HIST = 0x70000000;
		static const uint32_t SIZE = 0x1000;
		std::array<statistic_t, SIZE> _board;

	};
};



#endif // __BUTTERFLY_BOARDS_H