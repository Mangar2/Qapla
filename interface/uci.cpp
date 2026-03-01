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

#include "uci.h"

#include "../eval/king-attack.h"
#include "../eval/bishop.h"
#include "../eval/knight.h"
#include "../eval/rook.h"
#include "../eval/queen.h"
#include "../eval/threat.h"
#include "../eval/pawn.h"

#include "../basics/imbalance.h"
#include "../basics/pst.h"
#include "../basics/materialbalance.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

using namespace QaplaInterface;
using namespace ChessEval;

static std::string toLowerString(std::string value) {
	std::transform(value.begin(), value.end(), value.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return value;
}

/**
 * Collects all UCI parameter providers from evaluation components
 */
static std::vector<UciParameterProvider*> collectUciProviders() {
	std::vector<UciParameterProvider*> providers;
	providers.push_back(&KingAttack::getUciAccess());
	providers.push_back(&Bishop::getUciAccess());
	providers.push_back(&Knight::getUciAccess());
	providers.push_back(&Rook::getUciAccess());
	providers.push_back(&Queen::getUciAccess());
	providers.push_back(&Threat::getUciAccess());
	providers.push_back(&Pawn::getUciAccess());
	providers.push_back(&Imbalance::getUciAccess());
	providers.push_back(&QaplaBasics::PST::getUciAccess());
	providers.push_back(&QaplaBasics::MaterialBalance::getUciAccess());
	return providers;
}

/**
 * Reply on an "UCI" command
 */
void UCI::uciCommand() {
	_clock.setTimeBetweenInfoInMilliseconds(1000);
	println("id name " + getBoard()->getEngineInfo()["name"]);
	println("id author " + getBoard()->getEngineInfo()["author"]);
	println("option name Hash type spin default 32 min 1 max 32000");
	println("option name ponder type check");
	println("option name MultiPV type spin default 1 min 1 max 40");
	println("option name UCI_EngineAbout type string default " + getBoard()->getEngineInfo()["engine-about"]);
	println("option name qaplaBitbasePath type string");
	println("option name qaplaBitbaseCache type spin default 8 min 1 max 32000");
	
	// Add UCI parameters from evaluation components
	auto providers = collectUciProviders();
	for (auto* provider : providers) {
		for (const auto& param : provider->getUciParameters()) {
			println("option name " + param.name + 
				" type spin default " + std::to_string(param.defaultValue) +
				" min " + std::to_string(param.minValue) +
				" max " + std::to_string(param.maxValue));
		}
	}
	
	getBoard()->initialize();
	println("uciok");
}

/**
 * Sets an UCI option
 */
void UCI::setOption() {
	string name;
	string value;

	const string first = getNextTokenBlocking(true);
	if (first != "name") {
		// Invalid UCI command, ignore rest
		getToEOLBlocking();
		return;
	}

	name = getNextTokenBlocking(true);

	const string next = getNextTokenBlocking(true);
	if (next == "value") {
		value = getToEOLBlocking();
		const auto firstNonSpace = value.find_first_not_of(" \t");
		if (firstNonSpace == string::npos) {
			value.clear();
		}
		else {
			const auto lastNonSpace = value.find_last_not_of(" \t");
			value = value.substr(firstNonSpace, lastNonSpace - firstNonSpace + 1);
		}
	}

	if (name.empty()) {
		return;
	}

	// Try to set parameter via UCI parameter providers
	auto providers = collectUciProviders();
	const std::string lowerName = toLowerString(name);
	for (auto* provider : providers) {
		// Check if the value is a number (simple check)
		bool isNumber = !value.empty() && std::find_if(value.begin(), 
			value.end(), [](unsigned char c) { return !std::isdigit(c) && c != '-'; }) == value.end();
		
		if (isNumber) {
			int32_t intValue = std::atoi(value.c_str());
			if (provider->setUciParameter(name, intValue)) {
				return; // Parameter was handled
			}
			for (const auto& param : provider->getUciParameters()) {
				if (toLowerString(param.name) == lowerName && provider->setUciParameter(param.name, intValue)) {
					return;
				}
			}
		}
	}

	// Fall back to board's setOption
	getBoard()->setOption(name, value);
}
