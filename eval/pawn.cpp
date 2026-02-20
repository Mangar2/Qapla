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

#include "pawn.h"
#include <cstdint>

using namespace ChessEval;

namespace ChessEval {

#ifdef PARAM_OPTIMIZE_PAWN

std::array<value_t, Pawn::PP_INDEX_SIZE> Pawn::generatePPThreatMap(
	int64_t rankMultiplier,
	int64_t attackMultiplier,
	int64_t supportMultiplier,
	int64_t notBlockedMultiplier,
	int64_t advanceMultiplier)
{
	std::array<value_t, Pawn::PP_INDEX_SIZE> map{};
	for (uint32_t bitmask = 0; bitmask < Pawn::PP_INDEX_SIZE; ++bitmask) {
		int64_t value = 0;
		const value_t rank = bitmask & Pawn::RANK_MASK;

		auto computeRankValue = [rankMultiplier](uint32_t rankValue) -> int64_t {
			if (rankValue < 3) {
				return 0;
			}
			int64_t result = 10;
			for (uint32_t rankIndex = 4; rankIndex <= rankValue; ++rankIndex) {
				result *= rankMultiplier;
				result /= 10;
			}
			return result;
		};

		int64_t newThreatValue = computeRankValue(rank);
		bool isAttacked = static_cast<bool>(bitmask & Pawn::PP_IS_ATTACKED_INDEX);
		newThreatValue *= isAttacked ? attackMultiplier : 10;

		value_t maxAdvance = 7 - rank;
		if (maxAdvance > 2) {
			maxAdvance = 2;
		}
		for (value_t advance = 1; advance <= maxAdvance; ++advance) {
			bool isNotBlocked = (bitmask & (Pawn::PP_NOT_BLOCKED_INDEX * advance)) != 0;
			if (!isNotBlocked) {
				break;
			}
			bool isSupported = (bitmask & (Pawn::PP_IS_SUPPORTED_INDEX * advance)) != 0;
			int64_t multiplier = isSupported ? supportMultiplier : notBlockedMultiplier;
			multiplier *= advance == 1 ? 10 : advanceMultiplier;
			value += newThreatValue * multiplier;
		}
		value /= 1000;
		map[bitmask] = static_cast<value_t>(value);
	}
	return map;
}

std::array<EvalValue, Pawn::INDEX_SIZE> Pawn::generateEvalValueMap(
	int32_t singleConnectFactorMg,
	int32_t singleConnectFactorEg,
	int32_t singleConnectRank6Mg,
	int32_t singleConnectRank6Eg,
	int32_t doubleConnectFactorMg,
	int32_t doubleConnectFactorEg,
	int32_t doubleConnectRank6Mg,
	int32_t doubleConnectRank6Eg,
	int32_t passedFactorMg,
	int32_t passedFactorEg,
	int32_t passedRank6Mg,
	int32_t passedRank6Eg,
	int32_t protectedPassedFactorMg,
	int32_t protectedPassedFactorEg,
	int32_t protectedPassedRank6Mg,
	int32_t protectedPassedRank6Eg,
	int32_t connectedPassedFactorMg,
	int32_t connectedPassedFactorEg,
	int32_t connectedPassedRank6Mg,
	int32_t connectedPassedRank6Eg,
	int32_t distantPassedFactorMg,
	int32_t distantPassedFactorEg,
	int32_t distantPassedRank6Mg,
	int32_t distantPassedRank6Eg,
	int32_t doublePawnFactorMg,
	int32_t doublePawnFactorEg,
	int32_t isolatedPawnFactorMg,
	int32_t isolatedPawnFactorEg,
	int32_t weakPawnFactorMg,
	int32_t weakPawnFactorEg)
{
	auto applyFactor = [](EvalValue base, int32_t factorMg, int32_t factorEg) -> EvalValue {
		return EvalValue(base.midgame() * factorMg / 10, base.endgame() * factorEg / 10);
	};

	auto getRankValue = [&](Rank rank,
		const RankEvalArray_t& values,
		int32_t factorMg,
		int32_t factorEg,
		int32_t rank6Mg,
		int32_t rank6Eg) -> EvalValue {
		if (rank == Rank::R6) {
			return EvalValue(rank6Mg, rank6Eg);
		}
		return applyFactor(values[uint32_t(rank)], factorMg, factorEg);
	};

	const EvalValue doublePawnValue = applyFactor(DOUBLE_PAWN_VALUE, doublePawnFactorMg, doublePawnFactorEg);
	const EvalValue isolatedPawnValue = applyFactor(ISOLATED_PAWN_VALUE, isolatedPawnFactorMg, isolatedPawnFactorEg);
	const EvalValue weakPawnValue = applyFactor(WEAK_PAWN_VALUE, weakPawnFactorMg, weakPawnFactorEg);

	std::array<EvalValue, INDEX_SIZE> map;
	for (uint32_t bitmask = 0; bitmask < INDEX_SIZE; ++bitmask) {
		EvalValue value;
		const value_t rankValue = bitmask & RANK_MASK;
		if (rankValue == 0 || rankValue == 7) {
			map[bitmask] = value;
			continue;
		}

		Rank rank = Rank(rankValue);
		const auto ppIndex = bitmask & PASSED_PAWN_MASK;
		const bool weakPawn = ((bitmask & NON_WEAK_PAWN_MASK) == 0) && ((bitmask & UNOPPOSED_PAWN_INDEX) != 0);
		if (bitmask & DOUBLE_PAWN_INDEX) { value += doublePawnValue; }
		if (bitmask & SINGLE_CONNECT_INDEX) {
			value += getRankValue(rank, SINGLE_CONNECT_VALUES,
				singleConnectFactorMg, singleConnectFactorEg,
				singleConnectRank6Mg, singleConnectRank6Eg);
		}
		if (bitmask & DOUBLE_CONNECT_INDEX) {
			value += getRankValue(rank, DOUBLE_CONNECT_VALUES,
				doubleConnectFactorMg, doubleConnectFactorEg,
				doubleConnectRank6Mg, doubleConnectRank6Eg);
		}
		if (bitmask & ISOLATED_PAWN_INDEX) { value += isolatedPawnValue; }
		if (weakPawn) { value += weakPawnValue; }
		if (ppIndex == PASSED_PAWN_INDEX) {
			value += getRankValue(rank, PASSED_VALUES,
				passedFactorMg, passedFactorEg,
				passedRank6Mg, passedRank6Eg);
		}
		if (ppIndex == PROTECTED_PASSED_PAWN_INDEX) {
			value += getRankValue(rank, PROTECTED_PASSED_VALUES,
				protectedPassedFactorMg, protectedPassedFactorEg,
				protectedPassedRank6Mg, protectedPassedRank6Eg);
		}
		if (ppIndex == CONNECTED_PASSED_PAWN_INDEX) {
			value += getRankValue(rank, CONNECTED_PASSED_VALUES,
				connectedPassedFactorMg, connectedPassedFactorEg,
				connectedPassedRank6Mg, connectedPassedRank6Eg);
		}
		if (ppIndex == DISTANT_PASSED_PAWN_INDEX) {
			value += getRankValue(rank, DISTANT_PASSED_VALUES,
				distantPassedFactorMg, distantPassedFactorEg,
				distantPassedRank6Mg, distantPassedRank6Eg);
		}

		map[bitmask] = value;
	}
	return map;
}

class PawnUciAccess : public UciParameterProvider {
public:
	std::vector<UciParam> getUciParameters() const override {
		return {
			{ .name = "ppThreatRankMultiplier", .defaultValue = static_cast<int32_t>(rankMultiplier_), .minValue = 0, .maxValue = 100 },
			{ .name = "ppThreatAttackMultiplier", .defaultValue = static_cast<int32_t>(attackMultiplier_), .minValue = 0, .maxValue = 100 },
			{ .name = "ppThreatSupportMultiplier", .defaultValue = static_cast<int32_t>(supportMultiplier_), .minValue = 0, .maxValue = 100 },
			{ .name = "ppThreatNotBlockedMultiplier", .defaultValue = static_cast<int32_t>(notBlockedMultiplier_), .minValue = 0, .maxValue = 100 },
			{ .name = "ppThreatAdvanceMultiplier", .defaultValue = static_cast<int32_t>(advanceMultiplier_), .minValue = 0, .maxValue = 100 },
			{ .name = "pawnSingleConnectFactorMg", .defaultValue = singleConnectFactorMg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnSingleConnectFactorEg", .defaultValue = singleConnectFactorEg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnSingleConnectRank6Mg", .defaultValue = singleConnectRank6Mg_, .minValue = 0, .maxValue = 1000 },
			{ .name = "pawnSingleConnectRank6Eg", .defaultValue = singleConnectRank6Eg_, .minValue = 0, .maxValue = 1000 },
			{ .name = "pawnDoubleConnectFactorMg", .defaultValue = doubleConnectFactorMg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnDoubleConnectFactorEg", .defaultValue = doubleConnectFactorEg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnDoubleConnectRank6Mg", .defaultValue = doubleConnectRank6Mg_, .minValue = 0, .maxValue = 1000 },
			{ .name = "pawnDoubleConnectRank6Eg", .defaultValue = doubleConnectRank6Eg_, .minValue = 0, .maxValue = 1000 },
			{ .name = "pawnPassedFactorMg", .defaultValue = passedFactorMg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnPassedFactorEg", .defaultValue = passedFactorEg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnPassedRank6Mg", .defaultValue = passedRank6Mg_, .minValue = 0, .maxValue = 1000 },
			{ .name = "pawnPassedRank6Eg", .defaultValue = passedRank6Eg_, .minValue = 0, .maxValue = 1000 },
			{ .name = "pawnProtectedPassedFactorMg", .defaultValue = protectedPassedFactorMg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnProtectedPassedFactorEg", .defaultValue = protectedPassedFactorEg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnProtectedPassedRank6Mg", .defaultValue = protectedPassedRank6Mg_, .minValue = 0, .maxValue = 1000 },
			{ .name = "pawnProtectedPassedRank6Eg", .defaultValue = protectedPassedRank6Eg_, .minValue = 0, .maxValue = 1000 },
			{ .name = "pawnConnectedPassedFactorMg", .defaultValue = connectedPassedFactorMg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnConnectedPassedFactorEg", .defaultValue = connectedPassedFactorEg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnConnectedPassedRank6Mg", .defaultValue = connectedPassedRank6Mg_, .minValue = 0, .maxValue = 1000 },
			{ .name = "pawnConnectedPassedRank6Eg", .defaultValue = connectedPassedRank6Eg_, .minValue = 0, .maxValue = 1000 },
			{ .name = "pawnDistantPassedFactorMg", .defaultValue = distantPassedFactorMg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnDistantPassedFactorEg", .defaultValue = distantPassedFactorEg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnDistantPassedRank6Mg", .defaultValue = distantPassedRank6Mg_, .minValue = 0, .maxValue = 1000 },
			{ .name = "pawnDistantPassedRank6Eg", .defaultValue = distantPassedRank6Eg_, .minValue = 0, .maxValue = 1000 },
			{ .name = "pawnDoublePawnFactorMg", .defaultValue = doublePawnFactorMg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnDoublePawnFactorEg", .defaultValue = doublePawnFactorEg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnIsolatedPawnFactorMg", .defaultValue = isolatedPawnFactorMg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnIsolatedPawnFactorEg", .defaultValue = isolatedPawnFactorEg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnWeakPawnFactorMg", .defaultValue = weakPawnFactorMg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnWeakPawnFactorEg", .defaultValue = weakPawnFactorEg_, .minValue = 0, .maxValue = 100 }
		};
	}

	bool setUciParameter(const std::string& name, int32_t value) override {
		if (name == "ppThreatRankMultiplier") {
			rankMultiplier_ = value;
		} else if (name == "ppThreatAttackMultiplier") {
			attackMultiplier_ = value;
		} else if (name == "ppThreatSupportMultiplier") {
			supportMultiplier_ = value;
		} else if (name == "ppThreatNotBlockedMultiplier") {
			notBlockedMultiplier_ = value;
		} else if (name == "ppThreatAdvanceMultiplier") {
			advanceMultiplier_ = value;
		} else if (name == "pawnSingleConnectFactorMg") {
			singleConnectFactorMg_ = value;
		} else if (name == "pawnSingleConnectFactorEg") {
			singleConnectFactorEg_ = value;
		} else if (name == "pawnSingleConnectRank6Mg") {
			singleConnectRank6Mg_ = value;
		} else if (name == "pawnSingleConnectRank6Eg") {
			singleConnectRank6Eg_ = value;
		} else if (name == "pawnDoubleConnectFactorMg") {
			doubleConnectFactorMg_ = value;
		} else if (name == "pawnDoubleConnectFactorEg") {
			doubleConnectFactorEg_ = value;
		} else if (name == "pawnDoubleConnectRank6Mg") {
			doubleConnectRank6Mg_ = value;
		} else if (name == "pawnDoubleConnectRank6Eg") {
			doubleConnectRank6Eg_ = value;
		} else if (name == "pawnPassedFactorMg") {
			passedFactorMg_ = value;
		} else if (name == "pawnPassedFactorEg") {
			passedFactorEg_ = value;
		} else if (name == "pawnPassedRank6Mg") {
			passedRank6Mg_ = value;
		} else if (name == "pawnPassedRank6Eg") {
			passedRank6Eg_ = value;
		} else if (name == "pawnProtectedPassedFactorMg") {
			protectedPassedFactorMg_ = value;
		} else if (name == "pawnProtectedPassedFactorEg") {
			protectedPassedFactorEg_ = value;
		} else if (name == "pawnProtectedPassedRank6Mg") {
			protectedPassedRank6Mg_ = value;
		} else if (name == "pawnProtectedPassedRank6Eg") {
			protectedPassedRank6Eg_ = value;
		} else if (name == "pawnConnectedPassedFactorMg") {
			connectedPassedFactorMg_ = value;
		} else if (name == "pawnConnectedPassedFactorEg") {
			connectedPassedFactorEg_ = value;
		} else if (name == "pawnConnectedPassedRank6Mg") {
			connectedPassedRank6Mg_ = value;
		} else if (name == "pawnConnectedPassedRank6Eg") {
			connectedPassedRank6Eg_ = value;
		} else if (name == "pawnDistantPassedFactorMg") {
			distantPassedFactorMg_ = value;
		} else if (name == "pawnDistantPassedFactorEg") {
			distantPassedFactorEg_ = value;
		} else if (name == "pawnDistantPassedRank6Mg") {
			distantPassedRank6Mg_ = value;
		} else if (name == "pawnDistantPassedRank6Eg") {
			distantPassedRank6Eg_ = value;
		} else if (name == "pawnDoublePawnFactorMg") {
			doublePawnFactorMg_ = value;
		} else if (name == "pawnDoublePawnFactorEg") {
			doublePawnFactorEg_ = value;
		} else if (name == "pawnIsolatedPawnFactorMg") {
			isolatedPawnFactorMg_ = value;
		} else if (name == "pawnIsolatedPawnFactorEg") {
			isolatedPawnFactorEg_ = value;
		} else if (name == "pawnWeakPawnFactorMg") {
			weakPawnFactorMg_ = value;
		} else if (name == "pawnWeakPawnFactorEg") {
			weakPawnFactorEg_ = value;
		} else {
			return false;
		}

		Pawn::ppThreatMap = Pawn::generatePPThreatMap(
			rankMultiplier_,
			attackMultiplier_,
			supportMultiplier_,
			notBlockedMultiplier_,
			advanceMultiplier_);

		Pawn::evalValueMap = Pawn::generateEvalValueMap(
			singleConnectFactorMg_,
			singleConnectFactorEg_,
			singleConnectRank6Mg_,
			singleConnectRank6Eg_,
			doubleConnectFactorMg_,
			doubleConnectFactorEg_,
			doubleConnectRank6Mg_,
			doubleConnectRank6Eg_,
			passedFactorMg_,
			passedFactorEg_,
			passedRank6Mg_,
			passedRank6Eg_,
			protectedPassedFactorMg_,
			protectedPassedFactorEg_,
			protectedPassedRank6Mg_,
			protectedPassedRank6Eg_,
			connectedPassedFactorMg_,
			connectedPassedFactorEg_,
			connectedPassedRank6Mg_,
			connectedPassedRank6Eg_,
			distantPassedFactorMg_,
			distantPassedFactorEg_,
			distantPassedRank6Mg_,
			distantPassedRank6Eg_,
			doublePawnFactorMg_,
			doublePawnFactorEg_,
			isolatedPawnFactorMg_,
			isolatedPawnFactorEg_,
			weakPawnFactorMg_,
			weakPawnFactorEg_);
		return true;
	}

private:
	int64_t rankMultiplier_ = Pawn::RANK_MULTIPLIER;
	int64_t attackMultiplier_ = Pawn::ATTACK_MULTIPLIER;
	int64_t supportMultiplier_ = Pawn::SUPPORT_MULTIPLIER;
	int64_t notBlockedMultiplier_ = Pawn::NOT_BLOCKED_MULTIPLIER;
	int64_t advanceMultiplier_ = Pawn::ADVANCE_MULTIPLIER;

	int32_t singleConnectFactorMg_ = 10;
	int32_t singleConnectFactorEg_ = 10;
	int32_t singleConnectRank6Mg_ = Pawn::SINGLE_CONNECT_VALUES[6].midgame();
	int32_t singleConnectRank6Eg_ = Pawn::SINGLE_CONNECT_VALUES[6].endgame();

	int32_t doubleConnectFactorMg_ = 10;
	int32_t doubleConnectFactorEg_ = 10;
	int32_t doubleConnectRank6Mg_ = Pawn::DOUBLE_CONNECT_VALUES[6].midgame();
	int32_t doubleConnectRank6Eg_ = Pawn::DOUBLE_CONNECT_VALUES[6].endgame();

	int32_t passedFactorMg_ = 10;
	int32_t passedFactorEg_ = 10;
	int32_t passedRank6Mg_ = Pawn::PASSED_VALUES[6].midgame();
	int32_t passedRank6Eg_ = Pawn::PASSED_VALUES[6].endgame();

	int32_t protectedPassedFactorMg_ = 10;
	int32_t protectedPassedFactorEg_ = 10;
	int32_t protectedPassedRank6Mg_ = Pawn::PROTECTED_PASSED_VALUES[6].midgame();
	int32_t protectedPassedRank6Eg_ = Pawn::PROTECTED_PASSED_VALUES[6].endgame();

	int32_t connectedPassedFactorMg_ = 10;
	int32_t connectedPassedFactorEg_ = 10;
	int32_t connectedPassedRank6Mg_ = Pawn::CONNECTED_PASSED_VALUES[6].midgame();
	int32_t connectedPassedRank6Eg_ = Pawn::CONNECTED_PASSED_VALUES[6].endgame();

	int32_t distantPassedFactorMg_ = 10;
	int32_t distantPassedFactorEg_ = 10;
	int32_t distantPassedRank6Mg_ = Pawn::DISTANT_PASSED_VALUES[6].midgame();
	int32_t distantPassedRank6Eg_ = Pawn::DISTANT_PASSED_VALUES[6].endgame();

	int32_t doublePawnFactorMg_ = 10;
	int32_t doublePawnFactorEg_ = 10;
	int32_t isolatedPawnFactorMg_ = 10;
	int32_t isolatedPawnFactorEg_ = 10;
	int32_t weakPawnFactorMg_ = 10;
	int32_t weakPawnFactorEg_ = 10;
};

#endif

UciParameterProvider& Pawn::getUciAccess() {
#ifdef PARAM_OPTIMIZE_PAWN
	static PawnUciAccess instance;
#else
	static EmptyParameterProvider instance;
#endif
	return instance;
}

/*
 * Computes a lookup table where for each 8-bit file occupancy (pawnPresenceMask),
 * we get a bitboard marking all isolated pawn files (all ranks of that file set to 1).
 */
std::array<bitBoard_t, Pawn::LOOKUP_TABLE_SIZE> Pawn::computeIsolatedPawnLookupTable() {
	std::array<bitBoard_t, LOOKUP_TABLE_SIZE> table{};
	const bitBoard_t FILE_MASK_A = 0x0101010101010101ULL;
	table[0] = 0;  // No pawns -> no isolated files

	for (uint32_t pawnPresenceMask = 1; pawnPresenceMask < LOOKUP_TABLE_SIZE; pawnPresenceMask++) {
		bitBoard_t result = 0;

		for (int file = 0; file < 8; file++) {
			bool hasPawn = (pawnPresenceMask >> file) & 1;

			if (!hasPawn)
				continue;

			bool leftHasPawn = (file > 0) && ((pawnPresenceMask >> (file - 1)) & 1);
			bool rightHasPawn = (file < 7) && ((pawnPresenceMask >> (file + 1)) & 1);

			if (!leftHasPawn && !rightHasPawn) {
				// Isolated pawn on this file
				result |= (FILE_MASK_A << file);
			}
		}
		table[pawnPresenceMask] = result;
	}
	return table;
}

}

