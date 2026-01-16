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

#include "threat.h"

using namespace ChessEval;

// Static instance of UCI access
static Threat::UciAccess _uciAccessInstance;

UciParameterProvider& Threat::getUciAccess() {
	return _uciAccessInstance;
}

std::vector<UciParam> Threat::UciAccess::getUciParameters() const {
	return {
		{"threatScale", DEFAULT_THREAT_SCALE, 0, 2000}
	};
}

bool Threat::UciAccess::setUciParameter(const std::string& name, int32_t value) {
	if (name == "threatScale") {
		_scale = value;
	} else {
		return false;
	}

	// Regenerate THREAT_LOOKUP array with new parameters
	Threat::THREAT_LOOKUP = Threat::generateThreatLookup(
		_scale
	);

	return true;
}
