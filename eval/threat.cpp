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

namespace ChessEval {

#ifdef PARAM_OPTIMIZE_THREAT

/**
 * Generates THREAT_LOOKUP array using B-spline interpolation
 */
static std::array<EvalValue, 11> generateThreatLookup(EvalValue p0, EvalValue p1, EvalValue p2, EvalValue p10)
{
	std::array<EvalValue, 11> result;
	std::vector<std::pair<int, double>> ptsMg = { {0, p0.midgame() }, {1, p1.midgame() }, {2, p2.midgame() }, {10, p10.midgame() } };
	std::vector<std::pair<int, double>> ptsEg = { {0, p0.endgame() }, {1, p1.endgame() }, {2, p2.endgame() }, {10, p10.endgame() } };
	auto mg = generateArrayBSpline<11>(ptsMg, false);
	auto eg = generateArrayBSpline<11>(ptsEg, false);
	for (size_t i = 0; i < 11; ++i) {
		result[i] = EvalValue(mg[i], eg[i]);
	}
	return result;
}

/**
* UCI parameter access implementation for Threat
*/
class UciAccess : public UciParameterProvider {
public:
	std::vector<UciParam> getUciParameters() const override {
		return {
			{ .name = "threatMgP0", .defaultValue = p0_.midgame() },
			{ .name = "threatMgP1", .defaultValue = p1_.midgame() },
			{ .name = "threatMgP2", .defaultValue = p2_.midgame() },
			{ .name = "threatMgP10", .defaultValue = p10_.midgame() },
			{ .name = "threatEgP0", .defaultValue = p0_.endgame() },
			{ .name = "threatEgP1", .defaultValue = p1_.endgame() },
			{ .name = "threatEgP2", .defaultValue = p2_.endgame() },
			{ .name = "threatEgP10", .defaultValue = p10_.endgame() }
		};
	}

	bool setUciParameter(const std::string& name, int32_t value) override {
		if (name == "threatMgP0") {
			p0_.midgame() = value;
		} else if (name == "threatMgP1") {
			p1_.midgame() = value;
		} else if (name == "threatMgP2") {
			p2_.midgame() = value;
		} else if (name == "threatMgP10") {
			p10_.midgame() = value;
		} else if (name == "threatEgP0") {
			p0_.endgame() = value;
		} else if (name == "threatEgP1") {
			p1_.endgame() = value;
		} else if (name == "threatEgP2") {
			p2_.endgame() = value;
		} else if (name == "threatEgP10") {
			p10_.endgame() = value;
		} else {
			return false;
		}

		Threat::THREAT_LOOKUP = generateThreatLookup(p0_, p1_, p2_, p10_);

		return true;
	}

private:
	EvalValue p0_ = Threat::THREAT_LOOKUP_DEFAULT[0];
	EvalValue p1_ = Threat::THREAT_LOOKUP_DEFAULT[1];
	EvalValue p2_ = Threat::THREAT_LOOKUP_DEFAULT[2];
	EvalValue p10_ = Threat::THREAT_LOOKUP_DEFAULT[10];
};
#endif

UciParameterProvider& Threat::getUciAccess() {
#ifdef PARAM_OPTIMIZE_THREAT
	static UciAccess instance;
#else
	static EmptyParameterProvider instance;
#endif
	return instance;
}

}
