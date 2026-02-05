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
 */

#include "array-generator.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace ChessEval {

	void fillArrayPolynomialDampened(
		value_t* result,
		size_t size,
		double linearTerm,
		double quadraticTerm,
		double activationSpeed,
		double dampeningRate,
		double scale,
		int minIndex,
		double add,
		bool negate,
		bool verbose)
	{
		// Internal scaling factors
		constexpr double LINEAR_SCALE = 1.0;
		constexpr double QUADRATIC_SCALE = 1.0;
		constexpr double ACTIVATION_SCALE = 0.005;
		constexpr double DAMPENING_SCALE = 0.001;
		constexpr double SCALE_INTERNAL = 1.0 / 500.0;
		constexpr double DAMPENING_MOVE = 0.3;
		
		constexpr double MAX_SAFE_VALUE = QaplaBasics::MAX_VALUE / 10.0;

		for (size_t index = 0; index < size; ++index) {
			if (index < static_cast<size_t>(minIndex)) {
				result[index] = 0;
				continue;
			}

			double x_norm = static_cast<double>(index) / (size - 1);
			double polynomial = (LINEAR_SCALE * linearTerm * x_norm) + 
			                   (QUADRATIC_SCALE * quadraticTerm * x_norm * x_norm);
			double activationFactor = 1.0 - std::exp(-ACTIVATION_SCALE * activationSpeed * x_norm);
			double x_damp = std::max(0.0, x_norm - DAMPENING_MOVE) / (1.0 - DAMPENING_MOVE);
			double dampeningFactor = 1.0 / (1.0 + DAMPENING_SCALE * dampeningRate * x_damp * x_damp);

			double value = (polynomial * activationFactor * dampeningFactor * scale * SCALE_INTERNAL) + add;
			value = std::clamp(value, -MAX_SAFE_VALUE, MAX_SAFE_VALUE) + 0.5;

			result[index] = static_cast<value_t>(negate ? -value : value);
		}
	
		if (verbose) {
			for (size_t i = 0; i < size; ++i) std::cout << result[i] << (i == size - 1 ? "" : ",");
			std::cout << std::endl;
		}
	}

	void fillArrayPolynomial(
		value_t* result,
		size_t size,
		double linearTerm,
		double quadraticTerm,
		double dampeningRate,
		double scale,
		double add,
		int minIndex,
		bool negate,
		bool verbose)
	{
		constexpr double LINEAR_SCALE = 1.0;
		constexpr double QUADRATIC_SCALE = 1.0;
		constexpr double DAMPENING_SCALE = 0.001;
		constexpr double SCALE_INTERNAL = 1.0 / 500.0;
		constexpr double DAMPENING_MOVE = 0.3;
		
		constexpr double MAX_SAFE_VALUE = QaplaBasics::MAX_VALUE / 10.0;

		for (size_t index = 0; index < size; ++index) {
			if (index < static_cast<size_t>(minIndex)) {
				result[index] = 0;
				continue;
			}

			double x_norm = static_cast<double>(index) / (size - 1);
			double polynomial = (LINEAR_SCALE * linearTerm * x_norm) + 
			                   (QUADRATIC_SCALE * quadraticTerm * x_norm * x_norm);
			double x_damp = std::max(0.0, x_norm - DAMPENING_MOVE) / (1.0 - DAMPENING_MOVE);
			double dampeningFactor = 1.0 / (1.0 + DAMPENING_SCALE * dampeningRate * x_damp * x_damp);

			double value = (polynomial * dampeningFactor * scale * SCALE_INTERNAL) + add;
			value = std::clamp(value, -MAX_SAFE_VALUE, MAX_SAFE_VALUE) + 0.5;
			result[index] = static_cast<value_t>(negate ? -value : value);
		}
	
		if (verbose) {
			for (size_t i = 0; i < size; ++i) std::cout << result[i] << (i == size - 1 ? "" : ",");
			std::cout << std::endl;
		}
	}

	void fillArrayPower(
		value_t* result,
		size_t size,
		double scale,
		double exponent,
		double linearTerm,
		bool negate)
	{
		constexpr double MAX_SAFE_VALUE = 2000000000.0;

		for (size_t index = 0; index < size; ++index) {
			if (index == 0 || index == 1) {
				result[index] = 0;
				continue;
			}
			double x = static_cast<double>(index);
			double value = scale * std::pow(x, exponent) + linearTerm * x;
			value = std::min(value, MAX_SAFE_VALUE);
			result[index] = negate ? -static_cast<value_t>(value + 0.5) : static_cast<value_t>(value + 0.5);
		}
	}

    /**
     * Catmull-Rom cubic interpolation between p1 and p2 using points p0 and p3 as tangents
     */
    static double cubicInterpolate(double p0, double p1, double p2, double p3, double t) {
        return p1 + 0.5 * t * (p2 - p0 + t * (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3 + t * (3.0 * (p1 - p2) + p3 - p0)));
    }

	void fillArrayBSpline(
		value_t* result,
		size_t size,
		const std::vector<std::pair<int, double>>& points,
		bool negate,
		bool verbose)
	{
		if (size == 0) return;

		std::vector<std::pair<int, double>> pts = points;
		
		// 1. Ensure index 0 exists
		bool hasZero = false;
		for (const auto& p : pts) if (p.first == 0) hasZero = true;
		if (!hasZero) pts.push_back({ 0, 0.0 });

		// 2. Sort by index
		std::sort(pts.begin(), pts.end(), [](const auto& a, const auto& b) {
			return a.first < b.first;
		});

		// 3. Remove duplicates (take last)
		auto last = std::unique(pts.begin(), pts.end(), [](const auto& a, const auto& b) {
			return a.first == b.first;
		});
		pts.erase(last, pts.end());

		// 4. Fill result
		for (size_t i = 0; i < size; ++i) {
			int targetIdx = static_cast<int>(i);
			
			// Find interval: targetIdx is in [pts[pIdx-1].first, pts[pIdx].first]
			size_t pIdx = 0;
			while (pIdx < pts.size() && pts[pIdx].first < targetIdx) {
				pIdx++;
			}

			double value = 0;
			if (pIdx == 0) {
				// targetIdx <= pts[0].first (always true if 0 is earliest)
				value = pts[0].second;
			}
			else if (pIdx >= pts.size()) {
				// targetIdx > pts.back().first
				value = pts.back().second;
			}
			else {
				// Interpolate between pts[pIdx-1] and pts[pIdx]
				const auto& pA = pts[pIdx - 1];
				const auto& pB = pts[pIdx];
				
                double t = static_cast<double>(targetIdx - pA.first) / (pB.first - pA.first);
                
                // For a smooth curve passing through points, we need neighboring points.
                // Indices of points: pIdx-2, pIdx-1, pIdx, pIdx+1
                int i0 = std::max(0, (int)pIdx - 2);
                int i1 = (int)pIdx - 1;
                int i2 = (int)pIdx;
                int i3 = std::min((int)pts.size() - 1, (int)pIdx + 1);
                
                value = cubicInterpolate(pts[i0].second, pts[i1].second, pts[i2].second, pts[i3].second, t);
			}

			result[i] = static_cast<value_t>(value + (value >= 0 ? 0.5 : -0.5)) * (negate ? -1 : 1);
		}

		if (verbose) {
			for (size_t i = 0; i < size; ++i) std::cout << result[i] << (i == size - 1 ? "" : ",");
			std::cout << std::endl;
		}
	}

}
