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

#include "../basics/evalvalue.h"

#include <array>
#include <vector>

namespace ChessEval {

	using QaplaBasics::value_t;

	void fillArrayPolynomialDampened(
		value_t* result,
		size_t size,
		double linearTerm,
		double quadraticTerm,
		double activationSpeed,
		double dampeningRate,
		double scale = 500.0,
		int minIndex = 2,
		double add = 0.0,
		bool negate = true,
		bool verbose = false);

	void fillArrayPolynomial(
		value_t* result,
		size_t size,
		double linearTerm,
		double quadraticTerm,
		double dampeningRate,
		double scale = 500.0,
		double add = 0.0,
		int minIndex = 2,
		bool negate = true,
		bool verbose = false);

	void fillArrayPower(
		value_t* result,
		size_t size,
		double scale,
		double exponent,
		double linearTerm,
		bool negate = true);

	void fillArrayBSpline(
		value_t* result,
		size_t size,
		const std::vector<std::pair<int, double>>& points,
		bool negate = true,
		bool verbose = false);


	/**
	 * Generates array values using polynomial growth with activation and dampening
	 * Size-invariant formula with internal scaling factors
	 */
	template<size_t SIZE>
	std::array<value_t, SIZE> generateArrayPolynomialDampened(
		double linearTerm,
		double quadraticTerm,
		double activationSpeed,
		double dampeningRate,
		double scale = 500.0,
		int minIndex = 2,
		double add = 0.0,
		bool negate = true,
		bool verbose = false)
	{
		std::array<value_t, SIZE> result{};
		fillArrayPolynomialDampened(result.data(), SIZE, linearTerm, quadraticTerm, activationSpeed, dampeningRate, scale, minIndex, add, negate, verbose);
		return result;
	}

	/**
	 * Generates array values using polynomial growth with dampening (no activation)
	 * Simplified version without activation factor for cases where immediate growth is desired
	 */
	template<size_t SIZE>
	std::array<value_t, SIZE> generateArrayPolynomial(
		double linearTerm,
		double quadraticTerm,
		double dampeningRate,
		double scale = 500.0,
		double add = 0.0,
		int minIndex = 2,
		bool negate = true,
		bool verbose = false)
	{
		std::array<value_t, SIZE> result{};
		fillArrayPolynomial(result.data(), SIZE, linearTerm, quadraticTerm, dampeningRate, scale, add, minIndex, negate, verbose);
		return result;
	}

	/**
	 * Generates array values using a power function
	 * Simpler model with power-law growth plus linear term
	 */
	template<size_t SIZE>
	std::array<value_t, SIZE> generateArrayPower(
		double scale,
		double exponent,
		double linearTerm,
		bool negate = true)
	{
		std::array<value_t, SIZE> result{};
		fillArrayPower(result.data(), SIZE, scale, exponent, linearTerm, negate);
		return result;
	}

	/**
	 * Generates array values using B-spline interpolation between given points
	 * @param points sorted index/value pairs. If index 0 is missing, value 0 is assumed.
	 *               If size-1 is missing, remaining values stay constant.
	 */
	template<size_t SIZE>
	std::array<value_t, SIZE> generateArrayBSpline(
		const std::vector<std::pair<int, double>>& points,
		bool negate = true,
		bool verbose = false)
	{
		std::array<value_t, SIZE> result{};
		fillArrayBSpline(result.data(), SIZE, points, negate, verbose);
		return result;
	}

	/**
	 * Prints an array to stdout in a format suitable for debugging or copying into code
	 */
	template<size_t SIZE>
	void printArray(const std::string& name, const std::array<value_t, SIZE>& arr) {
		std::cout << name << " = {";
		for (size_t i = 0; i < SIZE; ++i) {
			std::cout << arr[i] << (i == SIZE - 1 ? "" : ", ");
		}
		std::cout << "};" << std::endl;
	}

	/**
	 * Prints an array of EvalValue to stdout
	 */
	template<size_t SIZE>
	void printEvalArray(const std::string& name, const std::array<QaplaBasics::EvalValue, SIZE>& arr) {
		std::cout << name << " = {" << std::endl;
		for (size_t i = 0; i < SIZE; ++i) {
			std::cout << "  {" << arr[i].midgame() << ", " << arr[i].endgame() << "}" << (i == SIZE - 1 ? "" : ",") << std::endl;
		}
		std::cout << "};" << std::endl;
	}

}

