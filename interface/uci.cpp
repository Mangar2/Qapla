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
#include "../eval/threat.h"
#include "../basics/pst.h"

using namespace QaplaInterface;
using namespace ChessEval;

/**
 * Collects all UCI parameter providers from evaluation components
 */
static std::vector<UciParameterProvider*> collectUciProviders() {
	std::vector<UciParameterProvider*> providers;
	providers.push_back(&KingAttack::getUciAccess());
	providers.push_back(&Threat::getUciAccess());
	providers.push_back(&QaplaBasics::PST::getUciAccess());
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
	}

	if (name.empty()) {
		return;
	}

	// Try to set parameter via UCI parameter providers
	auto providers = collectUciProviders();
	for (auto* provider : providers) {
		try {
			int32_t intValue = std::stoi(value);
			if (provider->setUciParameter(name, intValue)) {
				return; // Parameter was handled
			}
		} catch (...) {
			// Not an integer or parameter not found, try next provider
		}
	}

	// Fall back to board's setOption
	getBoard()->setOption(name, value);
}
