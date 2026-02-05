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

#include "queen.h"

using namespace ChessEval;

// Static instance of UCI access
static Queen::UciAccess _uciAccessInstance;

UciParameterProvider& Queen::getUciAccess() {
	return _uciAccessInstance;
}

#ifdef PARAM_OPTIMIZE
std::vector<UciParam> Queen::UciAccess::getUciParameters() const {
	return {
		{"queenMobilityP0", DEFAULT_QUEEN_MOBILITY_P0, -1000, 1000},
		{"queenMobilityP3", DEFAULT_QUEEN_MOBILITY_P3, -1000, 1000},
		{"queenMobilityP6", DEFAULT_QUEEN_MOBILITY_P6, -1000, 1000},
		{"queenMobilityP15", DEFAULT_QUEEN_MOBILITY_P15, -1000, 1000}
	};
}

bool Queen::UciAccess::setUciParameter(const std::string& name, int32_t value) {
	if (name == "queenMobilityP0") {
		_p0 = value;
	} else if (name == "queenMobilityP3") {
		_p3 = value;
	} else if (name == "queenMobilityP6") {
		_p6 = value;
	} else if (name == "queenMobilityP15") {
		_p15 = value;
	} else {
		return false;
	}

	// Regenerate QUEEN_MOBILITY_MAP array with new parameters
	Queen::QUEEN_MOBILITY_MAP = Queen::generateMobilityMap(
		_p0 / 10.0, _p3 / 10.0, _p6 / 10.0, _p15 / 10.0
	);

	return true;
}
#else
std::vector<UciParam> Queen::UciAccess::getUciParameters() const { return {}; }
bool Queen::UciAccess::setUciParameter(const std::string& name, int32_t value) { return false; }
#endif
