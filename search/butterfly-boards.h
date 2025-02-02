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

	class ButterFlyBoard {
	public:

		static void clear() {
			_board.fill(0);
		}

		static void addToValue(QaplaBasics::Move move, ply_t depth) {
			uint32_t index = computeIndex(move);
			statistic_t value = computeChange(depth);
			if (_board[index] < 0) {
				_board[index] /= 2;
			}
			_board[index] += value;
			if (_board[index] > MAX_HIST) {
				reduce();
			}
		}

		static void subFromValue(QaplaBasics::Move move, ply_t depth) {
			uint32_t index = computeIndex(move);
			statistic_t value = computeChange(depth);
			if (_board[index] > 0) {
				_board[index] /= 2;
			}
			_board[index] -= value;
			if (_board[index] < -MAX_HIST) {
				reduce();
			}
		}

		static statistic_t getValue(QaplaBasics::Move move) {
			uint32_t index = computeIndex(move);
			return _board[index];
		}

		static void newSearch() {
			reduce();
		}

	private:
		static constexpr uint32_t computeIndex(QaplaBasics::Move move) {
			return move.getPiceAndDestination();
		}

		static constexpr statistic_t computeChange(ply_t depth) {
			return depth * depth / 16;
		}

		static void reduce() {
			for (uint32_t i = 0; i < SIZE; i++) {
				_board[i] /= 2;
			}
		}

		static const statistic_t MAX_HIST = 0x70000000;
		static const uint32_t SIZE = 0x1000;
		static std::array<statistic_t, SIZE> _board;

	};
};



#endif // __BUTTERFLY_BOARDS_H