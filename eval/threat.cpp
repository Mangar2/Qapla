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
		{"threatLinearMg", DEFAULT_THREAT_LINEAR_MG, 0, 2000},
		{"threatQuadraticMg", DEFAULT_THREAT_QUADRATIC_MG, 0, 2000},
		{"threatDampeningMg", DEFAULT_THREAT_DAMPENING_MG, 0, 2000},
		{"threatLinearEg", DEFAULT_THREAT_LINEAR_EG, 0, 2000},
		{"threatQuadraticEg", DEFAULT_THREAT_QUADRATIC_EG, 0, 2000},
		{"threatDampeningEg", DEFAULT_THREAT_DAMPENING_EG, 0, 2000},
		{"threatScale", DEFAULT_THREAT_SCALE, 0, 2000}
	};
}

bool Threat::UciAccess::setUciParameter(const std::string& name, int32_t value) {
	if (name == "threatLinearMg") {
		_linearTermMg = value;
	} 
	else if (name == "threatQuadraticMg") {
		_quadraticTermMg = value;
	} 
	else if (name == "threatDampeningMg") {
		_dampeningRateMg = value;
	}
	else if (name == "threatLinearEg") {
		_linearTermEg = value;
	} 
	else if (name == "threatQuadraticEg") {
		_quadraticTermEg = value;
	} 
	else if (name == "threatDampeningEg") {
		_dampeningRateEg = value;
	}
	else if (name == "threatScale") {
		_scale = value;
	} else {
		return false;
	}

	// Regenerate THREAT_LOOKUP array with new parameters
	Threat::THREAT_LOOKUP = Threat::generateThreatLookup(
		_linearTermMg,
		_quadraticTermMg,
		_dampeningRateMg,
		_linearTermEg,
		_quadraticTermEg,
		_dampeningRateEg,
		_scale
	);

	return true;
}
