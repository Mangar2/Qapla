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
 * The user facing options of the own bitbases, declared where they are used.
 *
 * Loading bitbase files from disk is what a compile flag will switch off. Because
 * the options live here and not in the protocol interfaces, switching it off
 * removes them from the option list by itself - no conditional compilation in
 * uci.cpp or in the board adapter.
 */

#pragma once

#include <iostream>

#include "bitbase.h"
#include "bitbase-config.h"
#include "bitbase-reader.h"
#include "../interface/uci-option-provider.h"

namespace QaplaBitbase {

	class BitbaseOptions : public QaplaInterface::UciOptionProvider {
	public:
		static BitbaseOptions& getUciAccess() {
			static BitbaseOptions instance;
			return instance;
		}

		std::vector<QaplaInterface::UciOption> getUciOptions() const override {
#ifndef QAPLA_USE_BITBASE_FILES
			// The engine does not read bitbase files, so it offers nothing to configure.
			// This is the whole switch: no protocol interface needs to know about it.
			return {};
#else
			using QaplaInterface::UciOption;
			return {
				UciOption::string("qaplaBitbasePath"),
				UciOption::spin("qaplaBitbaseCache", 8, 1, 32000)
			};
#endif
		}

		bool setUciOption([[maybe_unused]] const std::string& name,
			[[maybe_unused]] const std::string& value) override {
#ifndef QAPLA_USE_BITBASE_FILES
			return false;
#else
			if (name == "qaplaBitbasePath") {
				if (value.empty() || !BitbaseReader::setBitbasePath(value)) return true;
				for (const auto& message : BitbaseReader::loadBitbase()) {
					std::cout << "info string " << message << std::endl;
				}
				return true;
			}

			if (name == "qaplaBitbaseCache") {
				Bitbase::setCacheSize(QaplaInterface::uciValueToInt(value, 8));
				return true;
			}

			return false;
#endif
		}
	};

}
