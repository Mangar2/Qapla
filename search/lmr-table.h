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
 * Base value of the late move reduction over remaining depth and move number, one table per node type. A table is the
 * product of two curves, one over the move number and one over the depth, each a monotone cubic spline (Fritsch-Carlson)
 * through five points. Everything is constexpr: with constant values the tables are built by the compiler, with the
 * values of a tuning run the same constructor runs at run time.
 */

#pragma once

#include "searchdef.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace QaplaSearch {

	class LmrTable {
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

		using Values = std::array<int32_t, POINTS>;

		/**
		 * The values of the two curves at their points
		 */
		struct Curves {
			Values move;
			Values depth;
			constexpr bool operator==(const Curves&) const = default;
		};

		constexpr explicit LmrTable(const Curves& curves) : _table{} {
			const auto moveCurve = curve(MOVE_POINTS, curves.move);
			const auto depthCurve = curve(DEPTH_POINTS, curves.depth);
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
		constexpr int32_t get(ply_t depth, int32_t moveNo) const {
			return _table[std::clamp<int32_t>(depth, 0, SIZE - 1)][std::clamp<int32_t>(moveNo, 0, SIZE - 1)];
		}

	private:
		/**
		 * Monotone cubic interpolation (Fritsch-Carlson) through the points: never overshoots, so monotone points give
		 * a monotone curve. Before the first point the first value, after the last point the last value.
		 */
		static constexpr std::array<double, SIZE> curve(const std::array<int32_t, POINTS>& positions, const Values& values) {
			const auto abs = [](double v) { return v < 0 ? -v : v; };
			std::array<double, POINTS> x{};
			std::array<double, POINTS> y{};
			for (int32_t i = 0; i < POINTS; ++i) {
				x[i] = positions[i];
				y[i] = values[i];
			}

			// Slopes at the points: weighted harmonic mean of the neighbouring secants, 0 at a local extremum
			std::array<double, POINTS - 1> h{};
			std::array<double, POINTS - 1> secant{};
			for (int32_t i = 0; i + 1 < POINTS; ++i) {
				h[i] = x[i + 1] - x[i];
				secant[i] = (y[i + 1] - y[i]) / h[i];
			}
			std::array<double, POINTS> slope{};
			for (int32_t i = 1; i + 1 < POINTS; ++i) {
				if (secant[i - 1] * secant[i] <= 0) continue;
				const double w1 = 2 * h[i] + h[i - 1];
				const double w2 = h[i] + 2 * h[i - 1];
				slope[i] = (w1 + w2) / (w1 / secant[i - 1] + w2 / secant[i]);
			}
			// One sided three point slope at both ends, limited so that the curve keeps its shape
			const auto endSlope = [&abs](double h0, double h1, double s0, double s1) {
				const double result = ((2 * h0 + h1) * s0 - h0 * s1) / (h0 + h1);
				if (result * s0 <= 0) return 0.0;
				if (s0 * s1 < 0 && abs(result) > abs(3 * s0)) return 3 * s0;
				return result;
			};
			slope[0] = endSlope(h[0], h[1], secant[0], secant[1]);
			slope[POINTS - 1] = endSlope(h[POINTS - 2], h[POINTS - 3], secant[POINTS - 2], secant[POINTS - 3]);

			std::array<double, SIZE> result{};
			int32_t segment = 0;
			for (int32_t i = 0; i < SIZE; ++i) {
				const double target = static_cast<double>(i);
				if (target <= x[0]) { result[i] = y[0]; continue; }
				if (target >= x[POINTS - 1]) { result[i] = y[POINTS - 1]; continue; }
				while (x[segment + 1] < target) segment++;
				const double hs = x[segment + 1] - x[segment];
				const double t = (target - x[segment]) / hs;
				const double t2 = t * t;
				const double t3 = t2 * t;
				result[i] = (2 * t3 - 3 * t2 + 1) * y[segment] + (t3 - 2 * t2 + t) * hs * slope[segment]
					+ (-2 * t3 + 3 * t2) * y[segment + 1] + (t3 - t2) * hs * slope[segment + 1];
			}
			return result;
		}

		std::array<std::array<int32_t, SIZE>, SIZE> _table;
	};

}
