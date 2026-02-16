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
 * Interface for classes that provide UCI parameters for SPSA optimization
 */

#pragma once

#include <string>
#include <vector>

namespace ChessEval {

	/**
	 * UCI parameter descriptor
	 */
	struct UciParam {
		std::string name;
		int32_t defaultValue;
		int32_t minValue;
		int32_t maxValue;
	};

	/**
	 * Interface for classes that provide UCI-configurable parameters
	 * Allows generic collection and modification of parameters for SPSA optimization
	 */
	class UciParameterProvider {
	public:
		virtual ~UciParameterProvider() = default;

		/**
		 * Get all UCI parameters provided by this class
		 * @return Vector of parameter descriptors
		 */
		virtual std::vector<UciParam> getUciParameters() const = 0;

		/**
		 * Set a parameter by name
		 * @param name Parameter name
		 * @param value New value (will be scaled internally if needed)
		 * @return true if parameter was found and set, false otherwise
		 */
		virtual bool setUciParameter(const std::string& name, int32_t value) = 0;
	};

	class EmptyParameterProvider : public UciParameterProvider {
	public:
		std::vector<UciParam> getUciParameters() const override { return {}; }
		bool setUciParameter(const std::string&, int32_t) override { return false; }
	};

}
