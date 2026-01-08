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
 * Provides template functions to generate lookup table arrays with controllable growth characteristics
 * for SPSA (Simultaneous Perturbation Stochastic Approximation) optimization
 */

#pragma once

#include "../basics/types.h"
#include "../basics/evalvalue.h"

#include <array>
#include <cmath>
#include <algorithm>

namespace ChessEval {

	/**
	 * Generates array values using polynomial growth with activation and dampening
	 * Size-invariant formula with internal scaling factors
	 * 
	 * Formula: 
	 *   x_norm = x / (SIZE - 1)  [normalized to 0..1]
	 *   poly = (linearScale * linearTerm * x_norm) + (quadraticScale * quadraticTerm * x_norm²)
	 *   activation = 1 - e^(-activationScale * activationSpeed * x_norm)
	 *   dampening = 1 / (1 + dampeningScale * dampeningRate * x_norm²)
	 *   value = poly * activation * dampening
	 * 
	 * Internal scaling factors adjust parameters to work in 0-100 range
	 * 
	 * @param SIZE Size of the array to generate
	 * @param linearTerm Linear growth component (0-100 typical)
	 * @param quadraticTerm Quadratic growth component (0-100 typical)
	 * @param activationSpeed How quickly function ramps up from 0 (0-100 typical)
	 * @param dampeningRate How much growth slows down at high indices, quadratically (0-100 typical)
	 * @param negate If true, returns negative values (for penalties)
	 */
	using QaplaBasics::value_t;

	template<size_t SIZE>
	std::array<value_t, SIZE> generateArrayPolynomialDampened(
		double linearTerm,
		double quadraticTerm,
		double activationSpeed,
		double dampeningRate,
		double scale = 500.0,
		double add = 0.0,
		bool negate = true)
	{
		// Internal scaling factors - adjust these to make parameters work in 0-100 range
		constexpr double LINEAR_SCALE = 1.0;
		constexpr double QUADRATIC_SCALE = 1.0;
		constexpr double ACTIVATION_SCALE = 0.005;
		constexpr double DAMPENING_SCALE = 0.001;
		constexpr double SCALE = 1.0 / 500.0;
		constexpr double DAMPENING_MOVE = 0.3;
		
		std::array<value_t, SIZE> result{};
		constexpr double MAX_SAFE_VALUE = QaplaBasics::MAX_VALUE / 10.0; // Chess centipawn max safety limit

		for (size_t index = 0; index < SIZE; ++index) {
			if (index == 0 || index == 1) {
				result[index] = 0;
				continue;
			}

			// Normalize x to [0, 1] range - makes function size-invariant
			double x_norm = static_cast<double>(index) / (SIZE - 1);

			// Polynomial growth: scaled linear and quadratic terms
			double polynomial = (LINEAR_SCALE * linearTerm * x_norm) + 
			                   (QUADRATIC_SCALE * quadraticTerm * x_norm * x_norm);

			// Activation factor: 1 - e^(-scaled_speed * x_norm)
			// Provides smooth ramp-up from 0
			double activationFactor = 1.0 - std::exp(-ACTIVATION_SCALE * activationSpeed * x_norm);

			// Quadratic dampening - weak at start, strong at end, using extra move - to avoid the redundancy of 1/quadratic = ca. damping.
			double x_damp = std::max(0.0, x_norm - DAMPENING_MOVE) / (1.0 - DAMPENING_MOVE);
			double dampeningFactor = 1.0 / (1.0 + DAMPENING_SCALE * dampeningRate * x_damp * x_damp);

			// Combine all factors with overflow protection
			double value = (polynomial * activationFactor * dampeningFactor * scale * SCALE) + add;
			value = std::clamp(value, -MAX_SAFE_VALUE, MAX_SAFE_VALUE) + 0.5;

			result[index] = static_cast<value_t>(negate ? -value : value);
		}

		return result;
	}

	/**
	 * Generates array values using a power function
	 * Simpler model with power-law growth plus linear term
	 * 
	 * @param SIZE Size of the array to generate
	 * @param scale Scaling factor for the power term
	 * @param exponent Power exponent (1.0=linear, 2.0=quadratic, values between for smooth curves)
	 * @param linearTerm Linear component added to power term
	 * @param negate If true, returns negative values (for penalties)
	 */
	template<size_t SIZE>
	std::array<value_t, SIZE> generateArrayPower(
		double scale,
		double exponent,
		double linearTerm,
		bool negate = true)
	{
		std::array<value_t, SIZE> result{};
		constexpr double MAX_SAFE_VALUE = 2000000000.0; // Safety limit for int32_t

		for (size_t index = 0; index < SIZE; ++index) {
			if (index == 0 || index == 1) {
				result[index] = 0;
				continue;
			}

			double x = static_cast<double>(index);
			
			// Power function: scale * x^exponent + linearTerm * x
			double value = scale * std::pow(x, exponent) + linearTerm * x;
			value = std::min(value, MAX_SAFE_VALUE);

			result[index] = negate ? -static_cast<value_t>(value + 0.5) : static_cast<value_t>(value + 0.5);
		}

		return result;
	}

}
