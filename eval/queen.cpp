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
		{"queenMobilityMgP0", DEFAULT_QUEEN_MOBILITY_MG_P0, -1000, 1000},
		{"queenMobilityMgP3", DEFAULT_QUEEN_MOBILITY_MG_P3, -1000, 1000},
		{"queenMobilityMgP6", DEFAULT_QUEEN_MOBILITY_MG_P6, -1000, 1000},
		{"queenMobilityMgP15", DEFAULT_QUEEN_MOBILITY_MG_P15, -1000, 1000},
		{"queenMobilityEgP0", DEFAULT_QUEEN_MOBILITY_EG_P0, -1000, 1000},
		{"queenMobilityEgP3", DEFAULT_QUEEN_MOBILITY_EG_P3, -1000, 1000},
		{"queenMobilityEgP6", DEFAULT_QUEEN_MOBILITY_EG_P6, -1000, 1000},
		{"queenMobilityEgP15", DEFAULT_QUEEN_MOBILITY_EG_P15, -1000, 1000}
	};
}

bool Queen::UciAccess::setUciParameter(const std::string& name, int32_t value) {
	if (name == "queenMobilityMgP0") {
		_mgP0 = value;
	} else if (name == "queenMobilityMgP3") {
		_mgP3 = value;
	} else if (name == "queenMobilityMgP6") {
		_mgP6 = value;
	} else if (name == "queenMobilityMgP15") {
		_mgP15 = value;
	} else if (name == "queenMobilityEgP0") {
		_egP0 = value;
	} else if (name == "queenMobilityEgP3") {
		_egP3 = value;
	} else if (name == "queenMobilityEgP6") {
		_egP6 = value;
	} else if (name == "queenMobilityEgP15") {
		_egP15 = value;
	} else {
		return false;
	}

	// Regenerate QUEEN_MOBILITY_MAP array with new parameters
	Queen::QUEEN_MOBILITY_MAP = Queen::generateMobilityMap(
		_mgP0 / 10.0, _mgP3 / 10.0, _mgP6 / 10.0, _mgP15 / 10.0,
		_egP0 / 10.0, _egP3 / 10.0, _egP6 / 10.0, _egP15 / 10.0
	);

	printEvalArray("QUEEN_MOBILITY_MAP", Queen::QUEEN_MOBILITY_MAP);

	return true;
}
#else
std::vector<UciParam> Queen::UciAccess::getUciParameters() const { return {}; }
bool Queen::UciAccess::setUciParameter(const std::string& name, int32_t value) { return false; }
#endif
