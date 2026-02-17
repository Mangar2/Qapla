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
#include <cstdint>
 
namespace ChessEval {

#ifdef PARAM_OPTIMIZE_KNIGHT

/**
 * Generates KNIGHT_MOBILITY_MAP array using B-spline interpolation
 */
static std::array<EvalValue, 9> generateMobilityMap(EvalValue mob0, EvalValue mob1, EvalValue mob3, EvalValue mob6)
{
	std::array<EvalValue, 9> result;
	std::vector<std::pair<int, double>> ptsMg = { {0, mob0.midgame() }, {1, mob1.midgame() }, {3, mob3.midgame() }, {6, mob6.midgame() } };
	std::vector<std::pair<int, double>> ptsEg = { {0, mob0.endgame() }, {1, mob1.endgame() }, {3, mob3.endgame() }, {6, mob6.endgame() } };
	auto mg = generateArrayBSpline<9>(ptsMg, false);
	auto eg = generateArrayBSpline<9>(ptsEg, false);
	for (size_t i = 0; i < 9; ++i) {
		result[i] = EvalValue(mg[i], eg[i]);
	}
	return result;
}

/**
* UCI parameter access implementation for Knight
*/
class KnightUciAccess : public UciParameterProvider {
public:
	std::vector<UciParam> getUciParameters() const override {
		return {
			{ .name = "knightMobilityMgP0", .defaultValue = mobP0_.midgame() },
			{ .name = "knightMobilityMgP1", .defaultValue = mobP1_.midgame() },
			{ .name = "knightMobilityMgP3", .defaultValue = mobP3_.midgame() },
			{ .name = "knightMobilityMgP6", .defaultValue = mobP6_.midgame() },
			{ .name = "knightMobilityEgP0", .defaultValue = mobP0_.endgame() },
			{ .name = "knightMobilityEgP1", .defaultValue = mobP1_.endgame() },
			{ .name = "knightMobilityEgP3", .defaultValue = mobP3_.endgame() },
			{ .name = "knightMobilityEgP6", .defaultValue = mobP6_.endgame() },
			{ .name = "knightOutpostMg", .defaultValue = outpost_.midgame() },
			{ .name = "knightOutpostEg", .defaultValue = outpost_.endgame() },
			{ .name = "knightPinnedMg", .defaultValue = pinned_.midgame() },
			{ .name = "knightPinnedEg", .defaultValue = pinned_.endgame() }
		};
	};

	bool setUciParameter(const std::string& name, int32_t value) override {
		if (name == "knightMobilityMgP0") {
			mobP0_.midgame() = value;
		} else if (name == "knightMobilityMgP1") {
			mobP1_.midgame() = value;
		} else if (name == "knightMobilityMgP3") {
			mobP3_.midgame() = value;
		} else if (name == "knightMobilityMgP6") {
			mobP6_.midgame() = value;
		} else if (name == "knightMobilityEgP0") {
			mobP0_.endgame() = value;
		} else if (name == "knightMobilityEgP1") {
			mobP1_.endgame() = value;
		} else if (name == "knightMobilityEgP3") {
			mobP3_.endgame() = value;
		} else if (name == "knightMobilityEgP6") {
			mobP6_.endgame() = value;
		} else if (name == "knightOutpostMg") {
			outpost_.midgame() = value;
		} else if (name == "knightOutpostEg") {
			outpost_.endgame() = value;
		} else if (name == "knightPinnedMg") {
			pinned_.midgame() = value;
		} else if (name == "knightPinnedEg") {
			pinned_.endgame() = value;
		} else {
			return false;
		}

		// Regenerate KNIGHT_MOBILITY_MAP array with new parameters
		Knight::KNIGHT_MOBILITY_MAP = generateMobilityMap(
			mobP0_, mobP1_, mobP3_, mobP6_
		);

		Knight::KNIGHT_PROPERTY_MAP = generateKnightLookup();

		return true;
	}

private:
	std::array<EvalValue, 4> generateKnightLookup() {
		std::array<EvalValue, 4> result;
		for (uint32_t bitmask = 0; bitmask < 4; ++bitmask) {
			EvalValue value;
			if (bitmask & Knight::OUTPOST) { value += outpost_; }
			if (bitmask & Knight::PINNED) { value += pinned_; }
			result[bitmask] = value;
		}
		return result;
	}

	EvalValue mobP0_ = Knight::KNIGHT_MOBILITY_MAP_DEFAULT[0];
	EvalValue mobP1_ = Knight::KNIGHT_MOBILITY_MAP_DEFAULT[1];
	EvalValue mobP3_ = Knight::KNIGHT_MOBILITY_MAP_DEFAULT[3];
	EvalValue mobP6_ = Knight::KNIGHT_MOBILITY_MAP_DEFAULT[6];

	EvalValue outpost_ = Knight::_outpost;
	EvalValue pinned_ = Knight::_pinned;
};
#endif

UciParameterProvider& Knight::getUciAccess() {
#ifdef PARAM_OPTIMIZE_KNIGHT
	static KnightUciAccess instance;
#else
	static EmptyParameterProvider instance;
#endif
	return instance;
}

}
