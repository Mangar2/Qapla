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
#ifdef PARAM_OPTIMIZE_PAWN
#include <iostream>
#endif

using namespace ChessEval;

namespace ChessEval {

namespace {

Square mapWhiteViewToBoardSquare(Piece color, int x, int y) {
	if (x < 0 || x > 7 || y < 0 || y > 7) {
		return NO_SQUARE;
	}
	const Square whiteViewSquare = computeSquare(File(x), Rank(y));
	if (color == WHITE) {
		return switchSide(whiteViewSquare);
	}
	return whiteViewSquare;
}

char board(const MoveGenerator& pos, Piece color, int x, int y) {
	const Square square = mapWhiteViewToBoardSquare(color, x, y);
	if (square == NO_SQUARE) {
		return '.';
	}
	const Piece piece = pos[square];
	if (piece == Piece(PAWN + color)) {
		return 'P';
	}
	if (piece == Piece(PAWN + switchColor(color))) {
		return 'p';
	}
	return '.';
}

int supported(const MoveGenerator& pos, Piece color, int x, int y) {
	return (board(pos, color, x - 1, y + 1) == 'P' ? 1 : 0)
		+ (board(pos, color, x + 1, y + 1) == 'P' ? 1 : 0);
}

int candidate_passed(const MoveGenerator& pos, Piece color, int x, int y) {
	if (board(pos, color, x, y) != 'P') return 0;
	int ty1 = 8;
	int ty2 = 8;
	for (int yy = y - 1; yy >= 0; yy--) {
		if (board(pos, color, x    , yy) == 'p') ty1 = yy;
		if (board(pos, color, x - 1, yy) == 'p'
		 || board(pos, color, x + 1, yy) == 'p') ty2 = yy;
	}
	if (ty1 == 8 && ty2 >= y - 1) return 1;
	if (ty2 < y - 2 || ty1 < y - 1) return 0;
	if (ty2 >= y && ty1 == y - 1 && y < 4) {
		if (board(pos, color, x - 1, y + 1) == 'P'
		 && board(pos, color, x - 1, y    ) != 'p'
		 && board(pos, color, x - 2, y - 1) != 'p') return 1;
		if (board(pos, color, x + 1, y + 1) == 'P'
		 && board(pos, color, x + 1, y    ) != 'p'
		 && board(pos, color, x + 2, y - 1) != 'p') return 1;
	}
	if (board(pos, color, x, y - 1) == 'p') return 0;
	int lever = (board(pos, color, x - 1, y - 1) == 'p' ? 1 : 0)
		      + (board(pos, color, x + 1, y - 1) == 'p' ? 1 : 0);
	int leverpush = (board(pos, color, x - 1, y - 2) == 'p' ? 1 : 0)
		          + (board(pos, color, x + 1, y - 2) == 'p' ? 1 : 0);
	int phalanx = (board(pos, color, x - 1, y) == 'P' ? 1 : 0)
		      + (board(pos, color, x + 1, y) == 'P' ? 1 : 0);
	if (lever - supported(pos, color, x, y) > 1) return 0;
	if (leverpush - phalanx  > 0) return 0;
	if (lever > 0 && leverpush > 0) return 0;
	return 1;
}

bitBoard_t candidate_passed(const MoveGenerator& pos, Piece color) {
	bitBoard_t result = 0;
	for (int y = 0; y < 8; ++y) {
		for (int x = 0; x < 8; ++x) {
			if (candidate_passed(pos, color, x, y) != 0) {
				const Square square = mapWhiteViewToBoardSquare(color, x, y);
				if (square != NO_SQUARE) {
					result |= squareToBB(square);
				}
			}
		}
	}
	return result;
}

}

void Pawn::verifyPassedPawnBBAlternative(const MoveGenerator& position, Piece color, bitBoard_t passedPawns) {
	const bitBoard_t alternativePassedPawns = candidate_passed(position, color);
	if (alternativePassedPawns != passedPawns) {
		std::cout << "Passed pawn mismatch in computePassedPawnBB" << (color == WHITE ? " WHITE" : " BLACK") << std::endl;
		std::cout << "Current algorithm:" << std::endl;
		printBB(passedPawns);
		std::cout << "Alternative algorithm:" << std::endl;
		printBB(alternativePassedPawns);
		position.print();
	}
}

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
		if (map[bitmask] != ppThreatMapDefault[bitmask]) {
			std::cout << "info string pawn ppThreatMap[" << bitmask << "] " << ppThreatMapDefault[bitmask] << " -> " << map[bitmask] << std::endl;
		}
	}
	return map;
}

std::array<EvalValue, Pawn::INDEX_SIZE> Pawn::generateEvalValueMap(
	int32_t singleConnectFactorMg,
	int32_t singleConnectFactorEg,
	int32_t singleConnectRank7Mg,
	int32_t singleConnectRank7Eg,
	int32_t doubleConnectFactorMg,
	int32_t doubleConnectFactorEg,
	int32_t doubleConnectRank7Mg,
	int32_t doubleConnectRank7Eg,
	int32_t passedFactorMg,
	int32_t passedFactorEg,
	int32_t passedRank7Mg,
	int32_t passedRank7Eg,
	int32_t distantPassedFactorMg,
	int32_t distantPassedFactorEg,
	int32_t distantPassedRank7Mg,
	int32_t distantPassedRank7Eg,
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
		int32_t rank7Mg,
		int32_t rank7Eg) -> EvalValue {
		if (rank == Rank::R7) {
			return EvalValue(rank7Mg, rank7Eg);
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
				singleConnectRank7Mg, singleConnectRank7Eg);
		}
		if (bitmask & DOUBLE_CONNECT_INDEX) {
			value += getRankValue(rank, DOUBLE_CONNECT_VALUES,
				doubleConnectFactorMg, doubleConnectFactorEg,
				doubleConnectRank7Mg, doubleConnectRank7Eg);
		}
		if (bitmask & ISOLATED_PAWN_INDEX) { value += isolatedPawnValue; }
		if (weakPawn) { value += weakPawnValue; }
		if (ppIndex == PASSED_PAWN_INDEX) {
			value += getRankValue(rank, PASSED_VALUES,
				passedFactorMg, passedFactorEg,
				passedRank7Mg, passedRank7Eg);
		}
		if (ppIndex == DISTANT_PASSED_PAWN_INDEX) {
			value += getRankValue(rank, DISTANT_PASSED_VALUES,
				distantPassedFactorMg, distantPassedFactorEg,
				distantPassedRank7Mg, distantPassedRank7Eg);
		}

		map[bitmask] = value;
		if (map[bitmask].midgame() != evalValueMapDefault[bitmask].midgame() || map[bitmask].endgame() != evalValueMapDefault[bitmask].endgame()) {
			std::cout << "info string pawn evalValueMap[" << bitmask << "] " << evalValueMapDefault[bitmask].midgame() << "," << evalValueMapDefault[bitmask].endgame() << " -> " << map[bitmask].midgame() << "," << map[bitmask].endgame() << std::endl;
		}
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
			{ .name = "pawnSingleConnectFactor", .defaultValue = singleConnectFactor_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnSingleConnectFactorMg", .defaultValue = singleConnectFactorMg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnSingleConnectFactorEg", .defaultValue = singleConnectFactorEg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnSingleConnectRank7Mg", .defaultValue = singleConnectRank7Mg_, .minValue = 0, .maxValue = 1000 },
			{ .name = "pawnSingleConnectRank7Eg", .defaultValue = singleConnectRank7Eg_, .minValue = 0, .maxValue = 1000 },
			{ .name = "pawnDoubleConnectFactor", .defaultValue = doubleConnectFactor_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnDoubleConnectFactorMg", .defaultValue = doubleConnectFactorMg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnDoubleConnectFactorEg", .defaultValue = doubleConnectFactorEg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnDoubleConnectRank7Mg", .defaultValue = doubleConnectRank7Mg_, .minValue = 0, .maxValue = 1000 },
			{ .name = "pawnDoubleConnectRank7Eg", .defaultValue = doubleConnectRank7Eg_, .minValue = 0, .maxValue = 1000 },
			{ .name = "pawnPassedPawnFactor", .defaultValue = passedFactor_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnPassedFactorMg", .defaultValue = passedFactorMg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnPassedFactorEg", .defaultValue = passedFactorEg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnPassedRank7Mg", .defaultValue = passedRank7Mg_, .minValue = 0, .maxValue = 1000 },
			{ .name = "pawnPassedRank7Eg", .defaultValue = passedRank7Eg_, .minValue = 0, .maxValue = 1000 },
			{ .name = "pawnDistantPassedFactor", .defaultValue = distantPassedFactor_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnDistantPassedFactorMg", .defaultValue = distantPassedFactorMg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnDistantPassedFactorEg", .defaultValue = distantPassedFactorEg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnDistantPassedRank7Mg", .defaultValue = distantPassedRank7Mg_, .minValue = 0, .maxValue = 1000 },
			{ .name = "pawnDistantPassedRank7Eg", .defaultValue = distantPassedRank7Eg_, .minValue = 0, .maxValue = 1000 },
			{ .name = "pawnDoublePawnFactor", .defaultValue = doublePawnFactor_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnDoublePawnFactorMg", .defaultValue = doublePawnFactorMg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnDoublePawnFactorEg", .defaultValue = doublePawnFactorEg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnIsolatedPawnFactor", .defaultValue = isolatedPawnFactor_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnIsolatedPawnFactorMg", .defaultValue = isolatedPawnFactorMg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnIsolatedPawnFactorEg", .defaultValue = isolatedPawnFactorEg_, .minValue = 0, .maxValue = 100 },
			{ .name = "pawnWeakPawnFactor", .defaultValue = weakPawnFactor_, .minValue = 0, .maxValue = 100 },
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
		} else if (name == "pawnSingleConnectFactor") {
			singleConnectFactor_ = value;
		} else if (name == "pawnSingleConnectFactorMg") {
			singleConnectFactorMg_ = value;
		} else if (name == "pawnSingleConnectFactorEg") {
			singleConnectFactorEg_ = value;
		} else if (name == "pawnSingleConnectRank7Mg") {
			singleConnectRank7Mg_ = value;
		} else if (name == "pawnSingleConnectRank7Eg") {
			singleConnectRank7Eg_ = value;
		} else if (name == "pawnDoubleConnectFactor") {
			doubleConnectFactor_ = value;
		} else if (name == "pawnDoubleConnectFactorMg") {
			doubleConnectFactorMg_ = value;
		} else if (name == "pawnDoubleConnectFactorEg") {
			doubleConnectFactorEg_ = value;
		} else if (name == "pawnDoubleConnectRank7Mg") {
			doubleConnectRank7Mg_ = value;
		} else if (name == "pawnDoubleConnectRank7Eg") {
			doubleConnectRank7Eg_ = value;
		} else if (name == "pawnPassedPawnFactor") {
			passedFactor_ = value;
		} else if (name == "pawnPassedFactorMg") {
			passedFactorMg_ = value;
		} else if (name == "pawnPassedFactorEg") {
			passedFactorEg_ = value;
		} else if (name == "pawnPassedRank7Mg") {
			passedRank7Mg_ = value;
		} else if (name == "pawnPassedRank7Eg") {
			passedRank7Eg_ = value;
		} else if (name == "pawnDistantPassedFactor") {
			distantPassedFactor_ = value;
		} else if (name == "pawnDistantPassedFactorMg") {
			distantPassedFactorMg_ = value;
		} else if (name == "pawnDistantPassedFactorEg") {
			distantPassedFactorEg_ = value;
		} else if (name == "pawnDistantPassedRank7Mg") {
			distantPassedRank7Mg_ = value;
		} else if (name == "pawnDistantPassedRank7Eg") {
			distantPassedRank7Eg_ = value;
		} else if (name == "pawnDoublePawnFactor") {
			doublePawnFactor_ = value;
		} else if (name == "pawnDoublePawnFactorMg") {
			doublePawnFactorMg_ = value;
		} else if (name == "pawnDoublePawnFactorEg") {
			doublePawnFactorEg_ = value;
		} else if (name == "pawnIsolatedPawnFactor") {
			isolatedPawnFactor_ = value;
		} else if (name == "pawnIsolatedPawnFactorMg") {
			isolatedPawnFactorMg_ = value;
		} else if (name == "pawnIsolatedPawnFactorEg") {
			isolatedPawnFactorEg_ = value;
		} else if (name == "pawnWeakPawnFactor") {
			weakPawnFactor_ = value;
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
			singleConnectFactorMg_ * singleConnectFactor_ / 10,
			singleConnectFactorEg_ * singleConnectFactor_ / 10,
			singleConnectRank7Mg_ * singleConnectFactor_ / 10,
			singleConnectRank7Eg_ * singleConnectFactor_ / 10,
			doubleConnectFactorMg_ * doubleConnectFactor_ / 10,
			doubleConnectFactorEg_ * doubleConnectFactor_ / 10,
			doubleConnectRank7Mg_ * doubleConnectFactor_ / 10,
			doubleConnectRank7Eg_ * doubleConnectFactor_ / 10,
			passedFactorMg_ * passedFactor_ / 10,
			passedFactorEg_ * passedFactor_ / 10,
			passedRank7Mg_ * passedFactor_ / 10,
			passedRank7Eg_ * passedFactor_ / 10,
			distantPassedFactorMg_ * distantPassedFactor_ / 10,
			distantPassedFactorEg_ * distantPassedFactor_ / 10,
			distantPassedRank7Mg_ * distantPassedFactor_ / 10,
			distantPassedRank7Eg_ * distantPassedFactor_ / 10,
			doublePawnFactorMg_ * doublePawnFactor_ / 10,
			doublePawnFactorEg_ * doublePawnFactor_ / 10,
			isolatedPawnFactorMg_ * isolatedPawnFactor_ / 10,
			isolatedPawnFactorEg_ * isolatedPawnFactor_ / 10,
			weakPawnFactorMg_ * weakPawnFactor_ / 10,
			weakPawnFactorEg_ * weakPawnFactor_ / 10);

		// dumpPawnParameterMaps(name, value, Pawn::ppThreatMap, Pawn::evalValueMap);
		return true;
	}

private:
	int64_t rankMultiplier_ = Pawn::THREAT_RANK_MULTIPLIER;
	int64_t attackMultiplier_ = Pawn::ATTACK_MULTIPLIER;
	int64_t supportMultiplier_ = Pawn::SUPPORT_MULTIPLIER;
	int64_t notBlockedMultiplier_ = Pawn::NOT_BLOCKED_MULTIPLIER;
	int64_t advanceMultiplier_ = Pawn::ADVANCE_MULTIPLIER;

	int32_t singleConnectFactorMg_ = 10;
	int32_t singleConnectFactorEg_ = 10;
	int32_t singleConnectFactor_ = 10;
	int32_t singleConnectRank7Mg_ = Pawn::SINGLE_CONNECT_VALUES[6].midgame();
	int32_t singleConnectRank7Eg_ = Pawn::SINGLE_CONNECT_VALUES[6].endgame();

	int32_t doubleConnectFactorMg_ = 10;
	int32_t doubleConnectFactorEg_ = 10;
	int32_t doubleConnectFactor_ = 10;
	int32_t doubleConnectRank7Mg_ = Pawn::DOUBLE_CONNECT_VALUES[6].midgame();
	int32_t doubleConnectRank7Eg_ = Pawn::DOUBLE_CONNECT_VALUES[6].endgame();

	int32_t passedFactorMg_ = 10;
	int32_t passedFactorEg_ = 10;
	int32_t passedFactor_ = 10;
	int32_t passedRank7Mg_ = Pawn::PASSED_VALUES[6].midgame();
	int32_t passedRank7Eg_ = Pawn::PASSED_VALUES[6].endgame();

	int32_t distantPassedFactorMg_ = 10;
	int32_t distantPassedFactorEg_ = 10;
	int32_t distantPassedFactor_ = 10;
	int32_t distantPassedRank7Mg_ = Pawn::DISTANT_PASSED_VALUES[6].midgame();
	int32_t distantPassedRank7Eg_ = Pawn::DISTANT_PASSED_VALUES[6].endgame();

	int32_t doublePawnFactorMg_ = 10;
	int32_t doublePawnFactorEg_ = 10;
	int32_t doublePawnFactor_ = 10;
	int32_t isolatedPawnFactorMg_ = 10;
	int32_t isolatedPawnFactorEg_ = 10;
	int32_t isolatedPawnFactor_ = 10;
	int32_t weakPawnFactorMg_ = 10;
	int32_t weakPawnFactorEg_ = 10;
	int32_t weakPawnFactor_ = 10;
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

