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

#include "space.h"

#include <algorithm>
#include <iostream>

#ifdef PARAM_OPTIMIZE_SPACE
#include <cstdint>
#endif

namespace ChessEval {

#ifdef PARAM_OPTIMIZE_SPACE

/**
 * UCI parameter access for Space evaluation.
 */
class SpaceUciAccess : public UciParameterProvider {
public:
	std::vector<UciParam> getUciParameters() const override {
		return {
			{ .name = "spaceWeightMg", .defaultValue = Space::SPACE_WEIGHT_MG_DEFAULT,
			  .minValue = 0, .maxValue = 500 },
			{ .name = "spaceNonPawnThreshold", .defaultValue = Space::NON_PAWN_MATERIAL_THRESHOLD_DEFAULT,
			  .minValue = 0, .maxValue = 20000 }
		};
	}

	bool setUciParameter(const std::string& name, int32_t value) override {
		if (name == "spaceWeightMg") {
			Space::SPACE_WEIGHT_MG = value;
		} else if (name == "spaceNonPawnThreshold") {
			Space::NON_PAWN_MATERIAL_THRESHOLD = value;
		} else {
			return false;
		}
		return true;
	}
};

#endif // PARAM_OPTIMIZE_SPACE

UciParameterProvider& Space::getUciAccess() {
#ifdef PARAM_OPTIMIZE_SPACE
	static SpaceUciAccess instance;
#else
	static EmptyParameterProvider instance;
#endif
	return instance;
}

value_t Space::computeNonPawnMaterial(const MoveGenerator& position) noexcept {
	value_t result = 0;
	for (Piece p : {WHITE_KNIGHT, BLACK_KNIGHT, WHITE_BISHOP, BLACK_BISHOP,
	                WHITE_ROOK,   BLACK_ROOK,   WHITE_QUEEN,  BLACK_QUEEN}) {
		result += position.getAbsolutePieceValue(p) * popCount(position.getPieceBB(p));
	}
	return result;
}

EvalValue Space::eval(const MoveGenerator& position, EvalResults& results) {
	return EvalValue(evalEfficient(position, results).midgame() * SPACE_WEIGHT_MG / 100, 0);
}

EvalValue Space::evalEfficient(const MoveGenerator& position, EvalResults&) {
	if (computeNonPawnMaterial(position) < NON_PAWN_MATERIAL_THRESHOLD) {
		return EvalValue(0, 0);
	}

	const bitBoard_t whitePawnsBB = position.getPieceBB(WHITE_PAWN);
	const bitBoard_t blackPawnsBB = position.getPieceBB(BLACK_PAWN);

	// ---- Blocked pawn count (both sides) ----
	// Directly blocked: own pawn with enemy pawn on the square directly ahead
	const bitBoard_t directlyBlockedWhite = whitePawnsBB & (blackPawnsBB >> 8);
	const bitBoard_t directlyBlockedBlack = blackPawnsBB & (whitePawnsBB << 8);

	// Doubly blocked: own pawn with enemy pawns on BOTH diagonal squares 2 ranks ahead.
	// White: black at (file-1, rank+2) → blackPawnsBB>>15, exclude black h-file to prevent wrap
	//        black at (file+1, rank+2) → blackPawnsBB>>17, exclude black a-file to prevent wrap
	const bitBoard_t doublyBlockedWhite = whitePawnsBB
		& ((blackPawnsBB & ~BitBoardMasks::FILE_H_BITMASK) >> 15)
		& ((blackPawnsBB & ~BitBoardMasks::FILE_A_BITMASK) >> 17);

	// Black: white at (file-1, rank-2) → whitePawnsBB<<17, exclude white h-file
	//        white at (file+1, rank-2) → whitePawnsBB<<15, exclude white a-file
	const bitBoard_t doublyBlockedBlack = blackPawnsBB
		& ((whitePawnsBB & ~BitBoardMasks::FILE_H_BITMASK) << 17)
		& ((whitePawnsBB & ~BitBoardMasks::FILE_A_BITMASK) << 15);

	const int blockedCount = popCount(directlyBlockedWhite | doublyBlockedWhite)
	                       + popCount(directlyBlockedBlack | doublyBlockedBlack);
	const int capped = std::min(blockedCount, 9);

	// ---- Piece counts ----
	const int whitePieceCount = popCount(position.getPiecesOfOneColorBB<WHITE>());
	const int blackPieceCount = popCount(position.getPiecesOfOneColorBB<BLACK>());
	const int weightWhite = whitePieceCount - 3 + capped;
	const int weightBlack = blackPieceCount - 3 + capped;

	// ---- Space area for white ----
	// Safe: in space mask, not own pawn, not attacked by enemy pawns
	const bitBoard_t safeWhite = SPACE_MASK_WHITE & ~whitePawnsBB & ~position.pawnAttack[BLACK];
	// Squares with a white pawn 1-3 ranks ahead (shift pawn BB back toward own side)
	const bitBoard_t behindPawnWhite =
		((whitePawnsBB >> 8) | (whitePawnsBB >> 16) | (whitePawnsBB >> 24)) & SPACE_MASK_WHITE;
	// flipRanks(attackMask[BLACK]): bit at s is set if black attacks switchSide(s)
	const bitBoard_t blackAttackFlipped = flipRanks(position.attackMask[BLACK]);
	const bitBoard_t bonusWhite = safeWhite & behindPawnWhite & ~blackAttackFlipped;
	const int spaceAreaWhite = popCount(safeWhite) + popCount(bonusWhite);

	// ---- Space area for black (symmetric) ----
	const bitBoard_t safeBlack = SPACE_MASK_BLACK & ~blackPawnsBB & ~position.pawnAttack[WHITE];
	const bitBoard_t behindPawnBlack =
		((blackPawnsBB << 8) | (blackPawnsBB << 16) | (blackPawnsBB << 24)) & SPACE_MASK_BLACK;
	const bitBoard_t whiteAttackFlipped = flipRanks(position.attackMask[WHITE]);
	const bitBoard_t bonusBlack = safeBlack & behindPawnBlack & ~whiteAttackFlipped;
	const int spaceAreaBlack = popCount(safeBlack) + popCount(bonusBlack);

	// ---- Final bonus (unweighted; weight applied in eval()) ----
	const value_t rawBonus = (spaceAreaWhite * weightWhite * weightWhite
	                        - spaceAreaBlack * weightBlack * weightBlack) / 16;
	return EvalValue(rawBonus, 0);
}

EvalValue Space::evalSimple(const MoveGenerator& position, EvalResults&) {
	if (computeNonPawnMaterial(position) < NON_PAWN_MATERIAL_THRESHOLD) {
		return EvalValue(0, 0);
	}

	// ---- Piece counts ----
	const int whitePieceCount = popCount(position.getPiecesOfOneColorBB<WHITE>());
	const int blackPieceCount = popCount(position.getPiecesOfOneColorBB<BLACK>());

	// ---- Blocked pawn count (JS: all blocked pawns both sides) ----
	int blockedCount = 0;
	for (Square square = A1; square <= H8; ++square) {
		const Piece piece = position[square];
		if (piece == WHITE_PAWN) {
			// Directly blocked: black pawn one rank ahead (toward rank 8)
			if (position[square + NORTH] == BLACK_PAWN) {
				++blockedCount;
			} else {
				// Doubly blocked: black pawns on both diagonals 2 ranks ahead
				const File f = getFile(square);
				const Rank r = getRank(square);
				if (r <= Rank::R6 && f > File::A && f < File::H) {
					if (position[square + NORTH_2 + WEST] == BLACK_PAWN &&
					    position[square + NORTH_2 + EAST] == BLACK_PAWN) {
						++blockedCount;
					}
				}
			}
		} else if (piece == BLACK_PAWN) {
			// Directly blocked: white pawn one rank ahead (toward rank 1)
			if (position[square + SOUTH] == WHITE_PAWN) {
				++blockedCount;
			} else {
				// Doubly blocked: white pawns on both diagonals 2 ranks ahead (toward rank 1)
				const File f = getFile(square);
				const Rank r = getRank(square);
				if (r >= Rank::R3 && f > File::A && f < File::H) {
					if (position[square + SOUTH_2 + WEST] == WHITE_PAWN &&
					    position[square + SOUTH_2 + EAST] == WHITE_PAWN) {
						++blockedCount;
					}
				}
			}
		}
	}
	const int capped = std::min(blockedCount, 9);

	// ---- Space area for white ----
	// Squares in ranks 2-4, files c-f that are safe (not own pawn, not attacked by black pawn).
	// +1 bonus if white pawn exists 1-3 ranks ahead AND mirrored square not attacked by black.
	int spaceAreaWhite = 0;
	for (Square square = A1; square <= H8; ++square) {
		const Rank r = getRank(square);
		const File f = getFile(square);
		if (r < Rank::R2 || r > Rank::R4 || f < File::C || f > File::F) continue;
		if (position[square] == WHITE_PAWN) continue;
		if (position.pawnAttack[BLACK] & squareToBB(square)) continue;
		++spaceAreaWhite;
		// Bonus: white pawn 1-3 ranks ahead on same file
		// Space ranks 2-4: pawn ahead at ranks 3-7, all within board; no bounds check needed.
		bool hasPawnAhead = position[square + NORTH] == WHITE_PAWN
		                 || position[square + NORTH_2] == WHITE_PAWN
		                 || position[square + NORTH_2 + NORTH] == WHITE_PAWN;
		if (hasPawnAhead) {
			const Square mirrored = switchSide(square);
			if (!(position.attackMask[BLACK] & squareToBB(mirrored))) {
				++spaceAreaWhite;
			}
		}
	}

	// ---- Space area for black (symmetric) ----
	int spaceAreaBlack = 0;
	for (Square square = A1; square <= H8; ++square) {
		const Rank r = getRank(square);
		const File f = getFile(square);
		if (r < Rank::R5 || r > Rank::R7 || f < File::C || f > File::F) continue;
		if (position[square] == BLACK_PAWN) continue;
		if (position.pawnAttack[WHITE] & squareToBB(square)) continue;
		++spaceAreaBlack;
		// Bonus: black pawn 1-3 ranks ahead (toward rank 1) on same file
		// Space ranks 5-7: pawn ahead at ranks 4-2, all within board; no bounds check needed.
		bool hasPawnAhead = position[square + SOUTH] == BLACK_PAWN
		                 || position[square + SOUTH_2] == BLACK_PAWN
		                 || position[square + SOUTH_2 + SOUTH] == BLACK_PAWN;
		if (hasPawnAhead) {
			const Square mirrored = switchSide(square);
			if (!(position.attackMask[WHITE] & squareToBB(mirrored))) {
				++spaceAreaBlack;
			}
		}
	}

	const int weightWhite = whitePieceCount - 3 + capped;
	const int weightBlack = blackPieceCount - 3 + capped;
	const value_t rawBonus = (spaceAreaWhite * weightWhite * weightWhite
	                        - spaceAreaBlack * weightBlack * weightBlack) / 16;
	// Unweighted; weight applied in eval()
	return EvalValue(rawBonus, 0);
}

} // namespace ChessEval
