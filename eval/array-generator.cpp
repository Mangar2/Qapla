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
     * Solves a tridiagonal matrix system for cubic spline interpolation.
     * Uses Thomas algorithm to find the second derivatives at each point.
     */
    void solveCubicSpline(const std::vector<double>& x, const std::vector<double>& y, std::vector<double>& m) {
        size_t n = x.size();
        if (n < 2) return;
        
        std::vector<double> h(n - 1);
        for (size_t i = 0; i < n - 1; ++i) h[i] = x[i + 1] - x[i];

        std::vector<double> d(n);
        std::vector<double> a(n), b(n), c(n);

        // Natural spline boundary conditions: second derivative at ends is 0
        // Or "Clamped" at the end if we want slope 0 into the plateau.
        // Let's use Natural for the start and Clamped (slope=0) for the end transition.
        
        // Equation for i=1 to n-2: h[i-1]*m[i-1] + 2*(h[i-1]+h[i])*m[i] + h[i]*m[i+1] = 6*((y[i+1]-y[i])/h[i] - (y[i]-y[i-1])/h[i-1])
        
        // Start: Natural boundary (m[0] = 0)
        b[0] = 1.0; c[0] = 0.0; d[0] = 0.0;
        
        for (size_t i = 1; i < n - 1; ++i) {
            a[i] = h[i - 1];
            b[i] = 2.0 * (h[i - 1] + h[i]);
            c[i] = h[i];
            d[i] = 6.0 * ((y[i + 1] - y[i]) / h[i] - (y[i] - y[i - 1]) / h[i - 1]);
        }

        // End: Clamped boundary (y' = 0 at last point) to ensure smooth transition to constant
        // 1st derivative at n-1: (y[n-1]-y[n-2])/h[n-2] + h[n-2]*m[n-2]/6 + h[n-2]*m[n-1]/3 = 0
        a[n - 1] = h[n - 2];
        b[n - 1] = 2.0 * h[n - 2];
        d[n - 1] = 6.0 * (0.0 - (y[n - 1] - y[n - 2]) / h[n - 2]);

        // Thomas Algorithm (Forward Sweep)
        for (size_t i = 1; i < n; ++i) {
            double w = a[i] / b[i - 1];
            b[i] = b[i] - w * c[i - 1];
            d[i] = d[i] - w * d[i - 1];
        }

        // Back substitution
        m.resize(n);
        m[n - 1] = d[n - 1] / b[n - 1];
        for (int i = static_cast<int>(n) - 2; i >= 0; --i) {
            m[i] = (d[i] - c[i] * m[i + 1]) / b[i];
        }
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
		bool hasZero = false;
		for (const auto& p : pts) if (p.first == 0) hasZero = true;
		if (!hasZero) pts.push_back({ 0, 0.0 });

		std::sort(pts.begin(), pts.end(), [](const auto& a, const auto& b) {
			return a.first < b.first;
		});

		auto last = std::unique(pts.begin(), pts.end(), [](const auto& a, const auto& b) {
			return a.first == b.first;
		});
		pts.erase(last, pts.end());

        size_t n = pts.size();
        std::vector<double> x(n), y(n), m;
        for (size_t i = 0; i < n; ++i) {
            x[i] = pts[i].first;
            y[i] = pts[i].second;
        }

        if (n >= 2) {
            solveCubicSpline(x, y, m);
        }

		for (size_t i = 0; i < size; ++i) {
			double targetX = static_cast<double>(i);
			double value = 0;

			if (targetX <= x[0]) {
				value = y[0];
			}
			else if (targetX >= x[n - 1]) {
				value = y[n - 1];
			}
			else {
                // Find segment
				size_t j = 0;
				while (j < n - 1 && x[j + 1] < targetX) j++;
                
                double h = x[j + 1] - x[j];
                double t = (targetX - x[j]) / h;
                
                // Cubic spline formula using second derivatives m[j] and m[j+1]
                value = m[j] * std::pow(h, 2) / 6.0 * (std::pow(1 - t, 3) - (1 - t)) +
                        m[j + 1] * std::pow(h, 2) / 6.0 * (std::pow(t, 3) - t) +
                        y[j] * (1 - t) +
                        y[j + 1] * t;
			}

			result[i] = static_cast<value_t>(value + (value >= 0 ? 0.5 : -0.5)) * (negate ? -1 : 1);
		}

		if (verbose) {
			for (size_t i = 0; i < size; ++i) std::cout << result[i] << (i == size - 1 ? "" : ",");
			std::cout << std::endl;
		}
	}

	void fillArrayMonotoneSpline(
		double* result,
		size_t size,
		const std::vector<std::pair<int, double>>& points)
	{
		const size_t n = points.size();
		if (size == 0 || n == 0) return;
		std::vector<double> x(n), y(n);
		for (size_t i = 0; i < n; ++i) {
			x[i] = points[i].first;
			y[i] = points[i].second;
		}

		// Slopes at the points: weighted harmonic mean of the neighbouring secants, 0 at a local extremum
		std::vector<double> slope(n, 0.0);
		if (n >= 2) {
			std::vector<double> h(n - 1), secant(n - 1);
			for (size_t i = 0; i + 1 < n; ++i) {
				h[i] = x[i + 1] - x[i];
				secant[i] = (y[i + 1] - y[i]) / h[i];
			}
			for (size_t i = 1; i + 1 < n; ++i) {
				if (secant[i - 1] * secant[i] <= 0) continue;
				const double w1 = 2 * h[i] + h[i - 1];
				const double w2 = h[i] + 2 * h[i - 1];
				slope[i] = (w1 + w2) / (w1 / secant[i - 1] + w2 / secant[i]);
			}
			// One sided three point slope at both ends, limited so that the curve keeps its shape
			const auto endSlope = [](double h0, double h1, double s0, double s1) {
				const double result = ((2 * h0 + h1) * s0 - h0 * s1) / (h0 + h1);
				if (result * s0 <= 0) return 0.0;
				if (s0 * s1 < 0 && std::abs(result) > std::abs(3 * s0)) return 3 * s0;
				return result;
			};
			if (n == 2) {
				slope[0] = slope[1] = secant[0];
			}
			else {
				slope[0] = endSlope(h[0], h[1], secant[0], secant[1]);
				slope[n - 1] = endSlope(h[n - 2], h[n - 3], secant[n - 2], secant[n - 3]);
			}
		}

		size_t segment = 0;
		for (size_t i = 0; i < size; ++i) {
			const double target = static_cast<double>(i);
			if (target <= x[0]) { result[i] = y[0]; continue; }
			if (target >= x[n - 1]) { result[i] = y[n - 1]; continue; }
			while (x[segment + 1] < target) segment++;
			const double h = x[segment + 1] - x[segment];
			const double t = (target - x[segment]) / h;
			const double t2 = t * t;
			const double t3 = t2 * t;
			result[i] = (2 * t3 - 3 * t2 + 1) * y[segment] + (t3 - 2 * t2 + t) * h * slope[segment]
				+ (-2 * t3 + 3 * t2) * y[segment + 1] + (t3 - t2) * h * slope[segment + 1];
		}
	}


}
