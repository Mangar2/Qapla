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
 * Interface for classes that provide UCI configuration options.
 *
 * Same principle as UciParameterProvider, but for user facing settings instead
 * of tuning parameters: the option is declared where its value is needed, and
 * the UCI interface only collects and forwards. Unlike a tuning parameter an
 * option is not always a number, so it carries a type and is set from the raw
 * string the GUI sent.
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace QaplaInterface {

	enum class UciOptionType {
		Spin,      // integer with limits
		Check,     // true or false
		String     // free text, for example a path
	};

	/**
	 * Description of a single option. The declaration line the UCI protocol
	 * expects is derived from it, so no caller has to know the type rules.
	 */
	struct UciOption {
		std::string   name;
		UciOptionType type = UciOptionType::Spin;
		std::string   defaultValue;              // as text, whatever the type
		int32_t       minValue = 0;              // Spin only
		int32_t       maxValue = 0;              // Spin only

		static UciOption spin(std::string name, int32_t defaultValue, int32_t minValue, int32_t maxValue) {
			return UciOption{ std::move(name), UciOptionType::Spin,
				std::to_string(defaultValue), minValue, maxValue };
		}

		static UciOption check(std::string name, bool defaultValue) {
			return UciOption{ std::move(name), UciOptionType::Check,
				defaultValue ? "true" : "false", 0, 0 };
		}

		static UciOption string(std::string name, std::string defaultValue = "") {
			return UciOption{ std::move(name), UciOptionType::String,
				std::move(defaultValue), 0, 0 };
		}

		/** The "option name ..." line without the leading "option name ". */
		std::string declaration() const {
			switch (type) {
			case UciOptionType::Spin:
				return name + " type spin default " + defaultValue
					+ " min " + std::to_string(minValue)
					+ " max " + std::to_string(maxValue);
			case UciOptionType::Check:
				return name + " type check default " + defaultValue;
			case UciOptionType::String:
				return name + " type string" + (defaultValue.empty() ? "" : " default " + defaultValue);
			}
			return name;
		}
	};

	/**
	 * Implemented by every component that owns user facing settings.
	 */
	class UciOptionProvider {
	public:
		virtual ~UciOptionProvider() = default;

		/** All options this component owns. */
		virtual std::vector<UciOption> getUciOptions() const = 0;

		/**
		 * Applies an option.
		 * @param name option name as sent by the GUI, compare case insensitively
		 * @param value raw value text, empty when the GUI sent none
		 * @return true if this provider owns the option and accepted the value
		 */
		virtual bool setUciOption(const std::string& name, const std::string& value) = 0;
	};

	/** "true" and everything a GUI might send for it; anything else is false. */
	inline bool uciValueToBool(const std::string& value) {
		return value == "true" || value == "1" || value == "on";
	}

	/** Reads an integer, keeping the fallback when the text is not a number. */
	inline int32_t uciValueToInt(const std::string& value, int32_t fallback) {
		try {
			size_t consumed = 0;
			const int32_t result = std::stoi(value, &consumed);
			return consumed == value.size() ? result : fallback;
		}
		catch (...) {
			return fallback;
		}
	}

}
