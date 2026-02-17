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

namespace ChessEval {

#ifdef PARAM_OPTIMIZE_QUEEN

/**
 * Generates QUEEN_MOBILITY_MAP array using B-spline interpolation
 */
static std::array<EvalValue, 30> generateMobilityMap(EvalValue mob0, EvalValue mob3, EvalValue mob6, EvalValue mob15)
{
	std::array<EvalValue, 30> result;
	std::vector<std::pair<int, double>> ptsMg = { {0, mob0.midgame() }, {3, mob3.midgame() }, {6, mob6.midgame() }, {15, mob15.midgame() } };
	std::vector<std::pair<int, double>> ptsEg = { {0, mob0.endgame() }, {3, mob3.endgame() }, {6, mob6.endgame() }, {15, mob15.endgame() } };
	auto mg = generateArrayBSpline<30>(ptsMg, false);
	auto eg = generateArrayBSpline<30>(ptsEg, false);
	for (size_t i = 0; i < 30; ++i) {
		result[i] = EvalValue(mg[i], eg[i]);
	}
	return result;
}

/**
* UCI parameter access implementation for Queen
*/
class QueenUciAccess : public UciParameterProvider {
public:
	std::vector<UciParam> getUciParameters() const override {
		return {
			{ .name = "queenMobilityMgP0", .defaultValue = mobP0_.midgame() },
			{ .name = "queenMobilityMgP3", .defaultValue = mobP3_.midgame() },
			{ .name = "queenMobilityMgP6", .defaultValue = mobP6_.midgame() },
			{ .name = "queenMobilityMgP15", .defaultValue = mobP15_.midgame() },
			{ .name = "queenMobilityEgP0", .defaultValue = mobP0_.endgame() },
			{ .name = "queenMobilityEgP3", .defaultValue = mobP3_.endgame() },
			{ .name = "queenMobilityEgP6", .defaultValue = mobP6_.endgame() },
			{ .name = "queenMobilityEgP15", .defaultValue = mobP15_.endgame() },
			{ .name = "queenPinnedMg", .defaultValue = pinned_.midgame() },
			{ .name = "queenPinnedEg", .defaultValue = pinned_.endgame() }
		};
	};

	bool setUciParameter(const std::string& name, int32_t value) override {
		if (name == "queenMobilityMgP0") {
			mobP0_.midgame() = value;
		} else if (name == "queenMobilityMgP3") {
			mobP3_.midgame() = value;
		} else if (name == "queenMobilityMgP6") {
			mobP6_.midgame() = value;
		} else if (name == "queenMobilityMgP15") {
			mobP15_.midgame() = value;
		} else if (name == "queenMobilityEgP0") {
			mobP0_.endgame() = value;
		} else if (name == "queenMobilityEgP3") {
			mobP3_.endgame() = value;
		} else if (name == "queenMobilityEgP6") {
			mobP6_.endgame() = value;
		} else if (name == "queenMobilityEgP15") {
			mobP15_.endgame() = value;
		} else if (name == "queenPinnedMg") {
			pinned_.midgame() = value;
		} else if (name == "queenPinnedEg") {
			pinned_.endgame() = value;
		} else {
			return false;
		}

		// Regenerate QUEEN_MOBILITY_MAP array with new parameters
		Queen::QUEEN_MOBILITY_MAP = generateMobilityMap(
			mobP0_, mobP3_, mobP6_, mobP15_
		);

		Queen::QUEEN_PROPERTY_MAP = generateQueenLookup();

		return true;
	}

private:
	std::array<EvalValue, 2> generateQueenLookup() {
		std::array<EvalValue, 2> result;
		result[0] = EvalValue(0, 0);
		result[1] = pinned_;
		return result;
	}

	EvalValue mobP0_ = Queen::QUEEN_MOBILITY_MAP_DEFAULT[0];
	EvalValue mobP3_ = Queen::QUEEN_MOBILITY_MAP_DEFAULT[3];
	EvalValue mobP6_ = Queen::QUEEN_MOBILITY_MAP_DEFAULT[6];
	EvalValue mobP15_ = Queen::QUEEN_MOBILITY_MAP_DEFAULT[15];

	EvalValue pinned_ = Queen::_pinned;
};
#endif

UciParameterProvider& Queen::getUciAccess() {
#ifdef PARAM_OPTIMIZE_QUEEN
	static QueenUciAccess instance;
#else
	static EmptyParameterProvider instance;
#endif
	return instance;
}

}
