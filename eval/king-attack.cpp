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

#include "king-attack.h"

using namespace ChessEval;

// Static instance of UCI access
static KingAttack::UciAccess _uciAccessInstance;

UciParameterProvider& KingAttack::getUciAccess() {
	return _uciAccessInstance;
}

std::vector<UciParam> KingAttack::UciAccess::getUciParameters() const {
	return {
		{"kAttackLinear", DEFAULT_KATTACK_LINEAR, 0, 2000},
		{"kAttackQuadratic", DEFAULT_KATTACK_QUADRATIC, 0, 2000},
		{"kAttackActivation", DEFAULT_KATTACK_ACTIVATION, 0, 2000},
		{"kAttackDampening", DEFAULT_KATTACK_DAMPENING, 0, 2000},
		{"kAttackScale", DEFAULT_KATTACK_SCALE, 0, 2000}
	};
}

bool KingAttack::UciAccess::setUciParameter(const std::string& name, int32_t value) {
	if (name == "kAttackLinear") {
		_linearTerm = value;
	} 
	else if (name == "kAttackQuadratic") {
		_quadraticTerm = value;
	} 
	else if (name == "kAttackActivation") {
		_activationSpeed = value;
	}
	else if (name == "kAttackDampening") {
		_dampeningRate = value;
	} 
	else if (name == "kAttackScale") {
		_scale = value;
	} else {
		return false;
	}

	// Regenerate attackWeight array with new parameters
	KingAttack::attackWeight = KingAttack::generateAttackWeight(
		_linearTerm,
		_quadraticTerm,
		_activationSpeed,
		_dampeningRate,
		_scale
	);

	return true;
}
