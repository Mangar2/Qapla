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
 * Provides single call site definition of tunable search parameters.
 *
 * A parameter is used directly where it is needed:
 *
 *     const value_t margin = param<optimizeSE, "seMarginConst", 1, -100, 300>();
 *
 * The first template parameter decides how the value is provided:
 * - false: the default value is returned as a compile time constant, the call is
 *          fully optimized away and the parameter is not visible via UCI.
 * - true:  the value is read from a variable that is registered as an UCI option
 *          and can be changed with "setoption name <name> value <value>".
 *
 * Registration happens before main() as soon as a parameter is used with the
 * optimize flag set, so all parameters are listed on the "uci" command even
 * though no search has been started yet.
 */

#ifndef __SEARCH_PARAM_H
#define __SEARCH_PARAM_H

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include "../interface/uci-parameter-provider.h"

namespace QaplaSearch {

	/**
	 * String literal usable as template parameter, names a search parameter
	 */
	template <std::size_t SIZE>
	struct ParamName {
		char text[SIZE]{};

		consteval ParamName(const char(&literal)[SIZE]) {
			for (std::size_t index = 0; index < SIZE; ++index) {
				text[index] = literal[index];
			}
		}

		constexpr std::string_view view() const { return { text, SIZE - 1 }; }
	};
	template <std::size_t SIZE> ParamName(const char(&)[SIZE]) -> ParamName<SIZE>;

	/**
	 * Registry of all search parameters currently exposed to UCI. Only parameters
	 * used with the optimize flag set are registered here.
	 */
	class SearchParams : public ChessEval::UciParameterProvider {
	public:
		static SearchParams& getUciAccess() {
			static SearchParams instance;
			return instance;
		}

		/**
		 * Registers a parameter. Called during static initialization, thus before main.
		 * @param param parameter description as reported to the UCI interface
		 * @param valuePtr pointer to the variable holding the current value
		 * @return always true, the result is only used to trigger the registration
		 */
		static bool add(const ChessEval::UciParam& param, int32_t* valuePtr) {
			for (const auto& entry : entries()) {
				if (entry.param.name == param.name) {
					// Two call sites using the same name would only be changeable in part
					std::cerr << "search parameter registered twice: " << param.name << std::endl;
					return true;
				}
			}
			entries().push_back({ param, valuePtr });
			return true;
		}

		std::vector<ChessEval::UciParam> getUciParameters() const override {
			std::vector<ChessEval::UciParam> result;
			result.reserve(entries().size());
			for (const auto& entry : entries()) {
				result.push_back(entry.param);
			}
			return result;
		}

		bool setUciParameter(const std::string& name, int32_t value) override {
			for (auto& entry : entries()) {
				if (entry.param.name == name) {
					*entry.value = value;
					return true;
				}
			}
			return false;
		}

	private:
		struct Entry {
			ChessEval::UciParam param;
			int32_t* value;
		};

		static std::vector<Entry>& entries() {
			static std::vector<Entry> registeredEntries;
			return registeredEntries;
		}
	};

	/**
	 * Holds the current value of one tunable parameter and registers it on UCI
	 */
	template <ParamName NAME, int32_t DEFAULT, int32_t MIN, int32_t MAX>
	struct ParamSlot {
		inline static int32_t value = DEFAULT;
		inline static const bool registered =
			SearchParams::add({ std::string(NAME.view()), DEFAULT, MIN, MAX }, &value);

		static int32_t get() {
			// Uses "registered" to make sure the registration is not optimized away
			return registered ? value : value;
		}
	};

	/**
	 * Provides the value of a search parameter, either as compile time constant or
	 * as value settable by UCI
	 * @tparam OPTIMIZE true, to make the parameter settable by UCI
	 * @tparam NAME name of the UCI option
	 * @tparam DEFAULT value used, if the parameter is not settable or not set
	 * @tparam MIN minimal value reported to the UCI interface
	 * @tparam MAX maximal value reported to the UCI interface
	 */
	template <bool OPTIMIZE, ParamName NAME, int32_t DEFAULT, int32_t MIN = -32000, int32_t MAX = 32000>
	constexpr int32_t param() {
		if constexpr (!OPTIMIZE) {
			return DEFAULT;
		}
		else {
			return ParamSlot<NAME, DEFAULT, MIN, MAX>::get();
		}
	}

}

#endif // __SEARCH_PARAM_H
