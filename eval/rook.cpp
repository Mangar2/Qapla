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

#include "rook.h"

using namespace ChessEval;

// Static instance of UCI access
static Rook::UciAccess _uciAccessInstance;

UciParameterProvider& Rook::getUciAccess() {
	return _uciAccessInstance;
}

#ifdef PARAM_OPTIMIZE
std::vector<UciParam> Rook::UciAccess::getUciParameters() const {
	return {
		{"rookMobilityMgP0", DEFAULT_ROOK_MOBILITY_MG_P0, -1000, 1000},
		{"rookMobilityMgP2", DEFAULT_ROOK_MOBILITY_MG_P2, -1000, 1000},
		{"rookMobilityMgP5", DEFAULT_ROOK_MOBILITY_MG_P5, -1000, 1000},
		{"rookMobilityMgP12", DEFAULT_ROOK_MOBILITY_MG_P12, -1000, 1000},
		{"rookMobilityEgP0", DEFAULT_ROOK_MOBILITY_EG_P0, -1000, 1000},
		{"rookMobilityEgP2", DEFAULT_ROOK_MOBILITY_EG_P2, -1000, 1000},
		{"rookMobilityEgP5", DEFAULT_ROOK_MOBILITY_EG_P5, -1000, 1000},
		{"rookMobilityEgP12", DEFAULT_ROOK_MOBILITY_EG_P12, -1000, 1000}
	};
}

bool Rook::UciAccess::setUciParameter(const std::string& name, int32_t value) {
	if (name == "rookMobilityMgP0") {
		_mgP0 = value;
	} else if (name == "rookMobilityMgP2") {
		_mgP2 = value;
	} else if (name == "rookMobilityMgP5") {
		_mgP5 = value;
	} else if (name == "rookMobilityMgP12") {
		_mgP12 = value;
	} else if (name == "rookMobilityEgP0") {
		_egP0 = value;
	} else if (name == "rookMobilityEgP2") {
		_egP2 = value;
	} else if (name == "rookMobilityEgP5") {
		_egP5 = value;
	} else if (name == "rookMobilityEgP12") {
		_egP12 = value;
	} else {
		return false;
	}

	// Regenerate ROOK_MOBILITY_MAP array with new parameters
	Rook::ROOK_MOBILITY_MAP = Rook::generateMobilityMap(
		_mgP0 / 10.0, _mgP2 / 10.0, _mgP5 / 10.0, _mgP12 / 10.0,
		_egP0 / 10.0, _egP2 / 10.0, _egP5 / 10.0, _egP12 / 10.0
	);

	printEvalArray("ROOK_MOBILITY_MAP", Rook::ROOK_MOBILITY_MAP);

	return true;
}
#else
std::vector<UciParam> Rook::UciAccess::getUciParameters() const { return {}; }
bool Rook::UciAccess::setUciParameter(const std::string& name, int32_t value) { return false; }
#endif
