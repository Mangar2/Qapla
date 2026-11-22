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
 * Base value of the late move reduction over remaining depth and move number. It is the product of two curves, one
 * over the move number and one over the depth, each a monotone spline through five points. The product is kept as
 * a table, so the search only looks a value up.
 */

#pragma once

#include "searchdef.h"
#include "../eval/array-generator.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace QaplaSearch {

	class LmrBaseTable {
	public:
		static constexpr int32_t SIZE = 64;
		static constexpr int32_t POINTS = 5;
		// Where the two curves have their points. Before move 2 the move curve is 0 (the first move is never
		// reduced), after the last point both curves stay constant.
		static constexpr std::array<int32_t, POINTS> MOVE_POINTS = { 2, 5, 9, 16, 30 };
		static constexpr std::array<int32_t, POINTS> DEPTH_POINTS = { 1, 4, 8, 15, 30 };
		// The depth curve at its last point is fixed: scaling one curve up and the other down by the same factor
		// gives the same table, so one value must not move.
		static constexpr int32_t DEPTH_LAST_VALUE = 55;

		/**
		 * Builds the table from the values of the two curves at their points
		 */
		void build(const std::array<int32_t, POINTS>& moveValues, const std::array<int32_t, POINTS>& depthValues) {
			const auto moveCurve = curve(MOVE_POINTS, moveValues);
			const auto depthCurve = curve(DEPTH_POINTS, depthValues);
			for (int32_t depth = 0; depth < SIZE; ++depth) {
				for (int32_t moveNo = 0; moveNo < SIZE; ++moveNo) {
					const double value = moveNo < MOVE_POINTS[0] ? 0.0 : moveCurve[moveNo] * depthCurve[depth];
					_table[depth][moveNo] = static_cast<int32_t>(value + (value >= 0 ? 0.5 : -0.5));
				}
			}
		}

		/**
		 * The base value in units of the reduction divisor, depth and move number clamped to the table
		 */
		int32_t get(ply_t depth, int32_t moveNo) const {
			return _table[std::clamp<int32_t>(depth, 0, SIZE - 1)][std::clamp<int32_t>(moveNo, 0, SIZE - 1)];
		}

	private:
		static std::array<double, SIZE> curve(
			const std::array<int32_t, POINTS>& positions, const std::array<int32_t, POINTS>& values)
		{
			std::vector<std::pair<int, double>> points;
			for (int32_t i = 0; i < POINTS; ++i) {
				points.push_back({ positions[i], static_cast<double>(values[i]) });
			}
			std::array<double, SIZE> result{};
			ChessEval::fillArrayMonotoneSpline(result.data(), SIZE, points);
			return result;
		}

		std::array<std::array<int32_t, SIZE>, SIZE> _table{};
	};

}
