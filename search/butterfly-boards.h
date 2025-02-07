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
 */

#ifndef __BUTTERFLY_BOARDS_H
#define __BUTTERFLY_BOARDS_H

#include <array>
#include "searchDef.h"
#include "../basics/move.h"

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
			if (change == 0) {
				return;
			}
			addToValue(move, change);
			const auto reduceCount = std::min(triedMoves, 7U);
			for (uint32_t i = 0; i < reduceCount; i++) {
				const auto move = moves[i];
				if (!move.isCapture()) {
					subFromValue(move, change);
				}
			}
		}
	
		statistic_t getValue(QaplaBasics::Move move) {
			uint32_t index = computeIndex(move);
			return _board[index];
		}

		void newSearch() {
			reduce();
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
			_board[index] += value;
			if (_board[index] > MAX_HIST) {
				reduce();
			}
		}

		void subFromValue(QaplaBasics::Move move, statistic_t value) {
			uint32_t index = computeIndex(move);
			if (_board[index] > 0) {
				_board[index] /= 2;
			}
			_board[index] -= value;
			if (_board[index] < -MAX_HIST) {
				reduce();
			}
		}

		constexpr uint32_t computeIndex(QaplaBasics::Move move) {
			return move.getPiceAndDestination();
		}

		constexpr statistic_t computeChange(ply_t depth) {
			return depth * depth / 16;
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