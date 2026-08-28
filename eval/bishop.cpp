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

#ifdef PARAM_OPTIMIZE_BISHOP
#include <cstdint>
#endif

namespace ChessEval {

#ifdef PARAM_OPTIMIZE_BISHOP

/**
 * Generates BISHOP_MOBILITY_MAP array using B-spline interpolation
 */
static std::array<EvalValue, 15> generateMobilityMap(EvalValue mob0, EvalValue mob2, EvalValue mob5, EvalValue mob12)
{
	std::array<EvalValue, 15> result;
	std::vector<std::pair<int, double>> ptsMg = { {0, mob0.midgame() }, { 2, mob2.midgame() }, { 5, mob5.midgame() }, {12, mob12.midgame() }};
	std::vector<std::pair<int, double>> ptsEg = { {0, mob0.endgame() }, { 2, mob2.endgame() }, { 5, mob5.endgame() }, {12, mob12.endgame() }};
	auto mg = generateArrayBSpline<15>(ptsMg, false);
	auto eg = generateArrayBSpline<15>(ptsEg, false);
	for (size_t i = 0; i < 15; ++i) {
		result[i] = EvalValue(mg[i], eg[i]);
	}
	return result;
}

/**
* UCI parameter access implementation for Bishop
*/
class BishopUciAccess : public UciParameterProvider {
public:
	std::vector<UciParam> getUciParameters() const override {
		return {
			{ .name = "bishopMobilityMgP0", .defaultValue = mobP0_.midgame(), .minValue = -1000, .maxValue = 1000 },
			{ .name = "bishopMobilityMgP2", .defaultValue = mobP2_.midgame(), .minValue = -1000, .maxValue = 1000 },
			{ .name = "bishopMobilityMgP5", .defaultValue = mobP5_.midgame(), .minValue = -1000, .maxValue = 1000 },
			{ .name = "bishopMobilityMgP12", .defaultValue = mobP12_.midgame(), .minValue = -1000, .maxValue = 1000 },
			{ .name = "bishopMobilityEgP0", .defaultValue = mobP0_.endgame(), .minValue = -1000, .maxValue = 1000 },
			{ .name = "bishopMobilityEgP2", .defaultValue = mobP2_.endgame(), .minValue = -1000, .maxValue = 1000 },
			{ .name = "bishopMobilityEgP5", .defaultValue = mobP5_.endgame(), .minValue = -1000, .maxValue = 1000 },
			{ .name = "bishopMobilityEgP12", .defaultValue = mobP12_.endgame(), .minValue = -1000, .maxValue = 1000 },
			{ .name = "bishopPinnedMg", .defaultValue = pinned_.midgame(), .minValue = -1000, .maxValue = 0 },
			{ .name = "bishopPinnedEg", .defaultValue = pinned_.endgame(), .minValue = -1000, .maxValue = 0 },
			{ .name = "bishopDoubleBishopMg", .defaultValue = doubleBishop_.midgame(), .minValue = 0, .maxValue = 1000 },
			{ .name = "bishopDoubleBishopEg", .defaultValue = doubleBishop_.endgame(), .minValue = 0, .maxValue = 1000 }
		};
	};

	bool setUciParameter(const std::string& name, int32_t value) override {
			
		if (name == "bishopMobilityMgP0") {
			mobP0_.midgame() = value;
		} else if (name == "bishopMobilityMgP2") {
			mobP2_.midgame() = value;
		} else if (name == "bishopMobilityMgP5") {
			mobP5_.midgame() = value;
		} else if (name == "bishopMobilityMgP12") {
			mobP12_.midgame() = value;
		} else if (name == "bishopMobilityEgP0") {
			mobP0_.endgame() = value;
		} else if (name == "bishopMobilityEgP2") {
			mobP2_.endgame() = value;
		} else if (name == "bishopMobilityEgP5") {
			mobP5_.endgame() = value;
		} else if (name == "bishopMobilityEgP12") {
			mobP12_.endgame() = value;
		} else if (name == "bishopPinnedMg") {
			pinned_.midgame() = value;
		} else if (name == "bishopPinnedEg") {
			pinned_.endgame() = value;
		} else if (name == "bishopDoubleBishopMg") {
			doubleBishop_.midgame() = value;
		} else if (name == "bishopDoubleBishopEg") {
			doubleBishop_.endgame() = value;
		} else {
			return false;
		}

		// Regenerate BISHOP_MOBILITY_MAP array with new parameters
		Bishop::BISHOP_MOBILITY_MAP = generateMobilityMap(
			mobP0_, mobP2_, mobP5_, mobP12_
		);

		Bishop::BISHOP_PROPERTY_MAP = generateBishopLookup();

		//printEvalArray("BISHOP_MOBILITY_MAP", Bishop::BISHOP_MOBILITY_MAP);

		return true;
	}

private:
	std::array<EvalValue, Bishop::BISHOP_PROPERTY_SIZE> generateBishopLookup() {
		std::array<EvalValue, Bishop::BISHOP_PROPERTY_SIZE> result;
		for (uint32_t bitmask = 0; bitmask < Bishop::BISHOP_PROPERTY_SIZE; ++bitmask) {
			EvalValue value;
			if (bitmask & Bishop::DOUBLE_BISHOP_INDEX) { value += doubleBishop_; }
			if (bitmask & Bishop::PINNED_INDEX) { value += pinned_; }
			result[bitmask] = value;
		}
		return result;
	}

	EvalValue mobP0_ = Bishop::BISHOP_MOBILITY_MAP_DEFAULT[0];
	EvalValue mobP2_ = Bishop::BISHOP_MOBILITY_MAP_DEFAULT[2];
	EvalValue mobP5_ = Bishop::BISHOP_MOBILITY_MAP_DEFAULT[5];
	EvalValue mobP12_ = Bishop::BISHOP_MOBILITY_MAP_DEFAULT[12];

	EvalValue pinned_ = Bishop::_pinned;
	EvalValue doubleBishop_ = Bishop::_doubleBishop;

};
#endif

UciParameterProvider& Bishop::getUciAccess() {
#ifdef PARAM_OPTIMIZE_BISHOP
	static BishopUciAccess instance;
#else 
	static EmptyParameterProvider instance;
#endif	
	return instance;
}

}