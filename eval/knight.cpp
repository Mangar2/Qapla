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

#include "knight.h"

using namespace ChessEval;

// Static instance of UCI access
static Knight::UciAccess _uciAccessInstance;

UciParameterProvider& Knight::getUciAccess() {
	return _uciAccessInstance;
}

#ifdef PARAM_OPTIMIZE
std::vector<UciParam> Knight::UciAccess::getUciParameters() const {
	return {
		{"knightMobilityMgP0", DEFAULT_KNIGHT_MOBILITY_MG_P0, -1000, 1000},
		{"knightMobilityMgP1", DEFAULT_KNIGHT_MOBILITY_MG_P1, -1000, 1000},
		{"knightMobilityMgP3", DEFAULT_KNIGHT_MOBILITY_MG_P3, -1000, 1000},
		{"knightMobilityMgP6", DEFAULT_KNIGHT_MOBILITY_MG_P6, -1000, 1000},
		{"knightMobilityEgP0", DEFAULT_KNIGHT_MOBILITY_EG_P0, -1000, 1000},
		{"knightMobilityEgP1", DEFAULT_KNIGHT_MOBILITY_EG_P1, -1000, 1000},
		{"knightMobilityEgP3", DEFAULT_KNIGHT_MOBILITY_EG_P3, -1000, 1000},
		{"knightMobilityEgP6", DEFAULT_KNIGHT_MOBILITY_EG_P6, -1000, 1000}
	};
}

bool Knight::UciAccess::setUciParameter(const std::string& name, int32_t value) {
	if (name == "knightMobilityMgP0") {
		_mgP0 = value;
	} else if (name == "knightMobilityMgP1") {
		_mgP1 = value;
	} else if (name == "knightMobilityMgP3") {
		_mgP3 = value;
	} else if (name == "knightMobilityMgP6") {
		_mgP6 = value;
	} else if (name == "knightMobilityEgP0") {
		_egP0 = value;
	} else if (name == "knightMobilityEgP1") {
		_egP1 = value;
	} else if (name == "knightMobilityEgP3") {
		_egP3 = value;
	} else if (name == "knightMobilityEgP6") {
		_egP6 = value;
	} else {
		return false;
	}

	// Regenerate KNIGHT_MOBILITY_MAP array with new parameters
	Knight::KNIGHT_MOBILITY_MAP = Knight::generateMobilityMap(
		_mgP0 / 10.0, _mgP1 / 10.0, _mgP3 / 10.0, _mgP6 / 10.0,
		_egP0 / 10.0, _egP1 / 10.0, _egP3 / 10.0, _egP6 / 10.0
	);

	printEvalArray("KNIGHT_MOBILITY_MAP", Knight::KNIGHT_MOBILITY_MAP);

	return true;
}
#else
std::vector<UciParam> Knight::UciAccess::getUciParameters() const { return {}; }
bool Knight::UciAccess::setUciParameter(const std::string& name, int32_t value) { return false; }
#endif
