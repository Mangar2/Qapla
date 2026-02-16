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

namespace ChessEval {

#ifdef PARAM_OPTIMIZE_ROOK

/**
 * Generates ROOK_MOBILITY_MAP array using B-spline interpolation
 */
static std::array<EvalValue, 15> generateMobilityMap(EvalValue mob0, EvalValue mob2, EvalValue mob5, EvalValue mob12)
{
	std::array<EvalValue, 15> result;
	std::vector<std::pair<int, double>> ptsMg = { {0, mob0.midgame() }, {2, mob2.midgame() }, {5, mob5.midgame() }, {12, mob12.midgame() } };
	std::vector<std::pair<int, double>> ptsEg = { {0, mob0.endgame() }, {2, mob2.endgame() }, {5, mob5.endgame() }, {12, mob12.endgame() } };
	auto mg = generateArrayBSpline<15>(ptsMg, false);
	auto eg = generateArrayBSpline<15>(ptsEg, false);
	for (size_t i = 0; i < 15; ++i) {
		result[i] = EvalValue(mg[i], eg[i]);
	}
	return result;
}

/**
* UCI parameter access implementation for Rook
*/
class UciAccess : public UciParameterProvider {
public:
	std::vector<UciParam> getUciParameters() const override {
		return {
			{ .name = "rookMobilityMgP0", .defaultValue = mobP0_.midgame() },
			{ .name = "rookMobilityMgP2", .defaultValue = mobP2_.midgame() },
			{ .name = "rookMobilityMgP5", .defaultValue = mobP5_.midgame() },
			{ .name = "rookMobilityMgP12", .defaultValue = mobP12_.midgame() },
			{ .name = "rookMobilityEgP0", .defaultValue = mobP0_.endgame() },
			{ .name = "rookMobilityEgP2", .defaultValue = mobP2_.endgame() },
			{ .name = "rookMobilityEgP5", .defaultValue = mobP5_.endgame() },
			{ .name = "rookMobilityEgP12", .defaultValue = mobP12_.endgame() },
			{ .name = "rookTrappedMg", .defaultValue = trapped_.midgame() },
			{ .name = "rookTrappedEg", .defaultValue = trapped_.endgame() },
			{ .name = "rookOpenFileMg", .defaultValue = openFile_.midgame() },
			{ .name = "rookOpenFileEg", .defaultValue = openFile_.endgame() },
			{ .name = "rookHalfOpenFileMg", .defaultValue = halfOpenFile_.midgame() },
			{ .name = "rookHalfOpenFileEg", .defaultValue = halfOpenFile_.endgame() },
			{ .name = "rookProtectsPassedPawnMg", .defaultValue = protectsPP_.midgame() },
			{ .name = "rookProtectsPassedPawnEg", .defaultValue = protectsPP_.endgame() },
			{ .name = "rookPinnedMg", .defaultValue = pinned_.midgame() },
			{ .name = "rookPinnedEg", .defaultValue = pinned_.endgame() },
			{ .name = "rookDoubleRookMg", .defaultValue = doubleRook_.midgame() },
			{ .name = "rookDoubleRookEg", .defaultValue = doubleRook_.endgame() }
		};
	}

	bool setUciParameter(const std::string& name, int32_t value) override {
		if (name == "rookMobilityMgP0") {
			mobP0_.midgame() = value;
		} else if (name == "rookMobilityMgP2") {
			mobP2_.midgame() = value;
		} else if (name == "rookMobilityMgP5") {
			mobP5_.midgame() = value;
		} else if (name == "rookMobilityMgP12") {
			mobP12_.midgame() = value;
		} else if (name == "rookMobilityEgP0") {
			mobP0_.endgame() = value;
		} else if (name == "rookMobilityEgP2") {
			mobP2_.endgame() = value;
		} else if (name == "rookMobilityEgP5") {
			mobP5_.endgame() = value;
		} else if (name == "rookMobilityEgP12") {
			mobP12_.endgame() = value;
		} else if (name == "rookTrappedMg") {
			trapped_.midgame() = value;
		} else if (name == "rookTrappedEg") {
			trapped_.endgame() = value;
		} else if (name == "rookOpenFileMg") {
			openFile_.midgame() = value;
		} else if (name == "rookOpenFileEg") {
			openFile_.endgame() = value;
		} else if (name == "rookHalfOpenFileMg") {
			halfOpenFile_.midgame() = value;
		} else if (name == "rookHalfOpenFileEg") {
			halfOpenFile_.endgame() = value;
		} else if (name == "rookProtectsPassedPawnMg") {
			protectsPP_.midgame() = value;
		} else if (name == "rookProtectsPassedPawnEg") {
			protectsPP_.endgame() = value;
		} else if (name == "rookPinnedMg") {
			pinned_.midgame() = value;
		} else if (name == "rookPinnedEg") {
			pinned_.endgame() = value;
		} else if (name == "rookDoubleRookMg") {
			doubleRook_.midgame() = value;
		} else if (name == "rookDoubleRookEg") {
			doubleRook_.endgame() = value;
		} else {
			return false;
		}

		Rook::ROOK_MOBILITY_MAP = generateMobilityMap(
			mobP0_, mobP2_, mobP5_, mobP12_
		);

		Rook::evalMap = generateRookLookup();

		return true;
	}

private:
	std::array<EvalValue, Rook::INDEX_SIZE> generateRookLookup() {
		std::array<EvalValue, Rook::INDEX_SIZE> map;
		for (uint32_t bitmask = 0; bitmask < Rook::INDEX_SIZE; ++bitmask) {
			EvalValue value;
			if (bitmask & Rook::TRAPPED) { value += trapped_; }
			if (bitmask & Rook::OPEN_FILE) { value += openFile_; }
			if (bitmask & Rook::HALF_OPEN_FILE) { value += halfOpenFile_; }
			if (bitmask & Rook::PROTECTS_PP) { value += protectsPP_; }
			if (bitmask & Rook::PINNED) { value += pinned_; }
			if (bitmask & Rook::DOUBLE_ROOK_INDEX) { value += doubleRook_; }
			if (bitmask / Rook::ROW_7_INDEX) { value += Rook::ROOK_ROW_7_MAP[bitmask / Rook::ROW_7_INDEX]; }
			map[bitmask] = value;
		}
		return map;
	}

	EvalValue mobP0_ = Rook::ROOK_MOBILITY_MAP_DEFAULT[0];
	EvalValue mobP2_ = Rook::ROOK_MOBILITY_MAP_DEFAULT[2];
	EvalValue mobP5_ = Rook::ROOK_MOBILITY_MAP_DEFAULT[5];
	EvalValue mobP12_ = Rook::ROOK_MOBILITY_MAP_DEFAULT[12];

	EvalValue trapped_ = Rook::_trapped;
	EvalValue openFile_ = Rook::_openFile;
	EvalValue halfOpenFile_ = Rook::_halfOpenFile;
	EvalValue protectsPP_ = Rook::_protectsPP;
	EvalValue pinned_ = Rook::_pinned;
	EvalValue doubleRook_ = Rook::_doubleRook;
};
#endif

UciParameterProvider& Rook::getUciAccess() {
#ifdef PARAM_OPTIMIZE_ROOK
	static UciAccess instance;
#else
	static EmptyParameterProvider instance;
#endif
	return instance;
}

}
