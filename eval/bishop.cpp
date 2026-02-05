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


#include "bishop.h"

using namespace ChessEval;

// Static instance of UCI access
static Bishop::UciAccess _uciAccessInstance;

UciParameterProvider& Bishop::getUciAccess() {
	return _uciAccessInstance;
}

#ifdef PARAM_OPTIMIZE
std::vector<UciParam> Bishop::UciAccess::getUciParameters() const {
	return {
		{"bishopMobilityMgP0", DEFAULT_BISHOP_MOBILITY_MG_P0, -1000, 1000},
		{"bishopMobilityMgP2", DEFAULT_BISHOP_MOBILITY_MG_P2, -1000, 1000},
		{"bishopMobilityMgP5", DEFAULT_BISHOP_MOBILITY_MG_P5, -1000, 1000},
		{"bishopMobilityMgP12", DEFAULT_BISHOP_MOBILITY_MG_P12, -1000, 1000},
		{"bishopMobilityEgP0", DEFAULT_BISHOP_MOBILITY_EG_P0, -1000, 1000},
		{"bishopMobilityEgP2", DEFAULT_BISHOP_MOBILITY_EG_P2, -1000, 1000},
		{"bishopMobilityEgP5", DEFAULT_BISHOP_MOBILITY_EG_P5, -1000, 1000},
		{"bishopMobilityEgP12", DEFAULT_BISHOP_MOBILITY_EG_P12, -1000, 1000}
	};
}

bool Bishop::UciAccess::setUciParameter(const std::string& name, int32_t value) {
	if (name == "bishopMobilityMgP0") {
		_mgP0 = value;
	} else if (name == "bishopMobilityMgP2") {
		_mgP2 = value;
	} else if (name == "bishopMobilityMgP5") {
		_mgP5 = value;
	} else if (name == "bishopMobilityMgP12") {
		_mgP12 = value;
	} else if (name == "bishopMobilityEgP0") {
		_egP0 = value;
	} else if (name == "bishopMobilityEgP2") {
		_egP2 = value;
	} else if (name == "bishopMobilityEgP5") {
		_egP5 = value;
	} else if (name == "bishopMobilityEgP12") {
		_egP12 = value;
	} else {
		return false;
	}

	// Regenerate BISHOP_MOBILITY_MAP array with new parameters
	Bishop::BISHOP_MOBILITY_MAP = Bishop::generateMobilityMap(
		_mgP0 / 10.0, _mgP2 / 10.0, _mgP5 / 10.0, _mgP12 / 10.0,
		_egP0 / 10.0, _egP2 / 10.0, _egP5 / 10.0, _egP12 / 10.0
	);

	printEvalArray("BISHOP_MOBILITY_MAP", Bishop::BISHOP_MOBILITY_MAP);

	return true;
}
#else
std::vector<UciParam> Bishop::UciAccess::getUciParameters() const { return {}; }
bool Bishop::UciAccess::setUciParameter(const std::string& name, int32_t value) { return false; }
#endif
