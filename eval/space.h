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
 * @Overview
 * Implements space evaluation: bonus for safe squares in the central files (c-f)
 * on ranks 2-4 from own side, scaled by piece count and pawn blockage.
 */

#pragma once

#include "evalresults.h"
#include "../basics/types.h"
#include "../basics/evalvalue.h"
#include "../basics/bits.h"
#include "../movegenerator/movegenerator.h"
#include "../movegenerator/bitboardmasks.h"
#include "../interface/uci-parameter-provider.h"

#include <algorithm>

#ifdef PARAM_OPTIMIZE
#define PARAM_OPTIMIZE_SPACE
#endif

using namespace QaplaBasics;
using namespace QaplaMoveGenerator;

namespace ChessEval {

	class Space {
	public:
		friend class SpaceUciAccess;

		/**
		 * Get UCI parameter access interface
		 */
		static UciParameterProvider& getUciAccess();

		/**
		 * Production evaluation entry point.
		 * Phase 4: calls both evalSimple and evalEfficient, aborts on mismatch.
		 */
		static EvalValue eval(const MoveGenerator& position, EvalResults& results);

	private:
		static constexpr value_t NON_PAWN_MATERIAL_THRESHOLD_DEFAULT = 5250;
		static constexpr value_t SPACE_WEIGHT_MG_DEFAULT = 0;

#ifndef PARAM_OPTIMIZE_SPACE
		static constexpr value_t NON_PAWN_MATERIAL_THRESHOLD = NON_PAWN_MATERIAL_THRESHOLD_DEFAULT;
		static constexpr value_t SPACE_WEIGHT_MG = SPACE_WEIGHT_MG_DEFAULT;
#else
		inline static value_t NON_PAWN_MATERIAL_THRESHOLD = NON_PAWN_MATERIAL_THRESHOLD_DEFAULT;
		inline static value_t SPACE_WEIGHT_MG = SPACE_WEIGHT_MG_DEFAULT;
#endif

		// Space area masks: files c-f, ranks 2-4 for white / ranks 5-7 for black (from white's view)
		static constexpr bitBoard_t SPACE_MASK_WHITE =
			(BitBoardMasks::FILE_C_BITMASK | BitBoardMasks::FILE_D_BITMASK |
			 BitBoardMasks::FILE_E_BITMASK | BitBoardMasks::FILE_F_BITMASK) &
			(BitBoardMasks::RANK_2_BITMASK | BitBoardMasks::RANK_3_BITMASK |
			 BitBoardMasks::RANK_4_BITMASK);

		static constexpr bitBoard_t SPACE_MASK_BLACK =
			(BitBoardMasks::FILE_C_BITMASK | BitBoardMasks::FILE_D_BITMASK |
			 BitBoardMasks::FILE_E_BITMASK | BitBoardMasks::FILE_F_BITMASK) &
			(BitBoardMasks::RANK_5_BITMASK | BitBoardMasks::RANK_6_BITMASK |
			 BitBoardMasks::RANK_7_BITMASK);

		/**
		 * Simple reference evaluation (direct JavaScript port, iterates squares).
		 * Kept as dead code after Phase 5. Never called in production.
		 */
		static EvalValue evalSimple(const MoveGenerator& position, EvalResults& results);

		/**
		 * Efficient bitboard-based evaluation (production path).
		 */
		static EvalValue evalEfficient(const MoveGenerator& position, EvalResults& results);

		/**
		 * Reverses rank order of a bitboard: equivalent to applying switchSide to every square.
		 * Implemented as byte-swap since each byte holds one rank.
		 */
		static constexpr bitBoard_t flipRanks(bitBoard_t bb) noexcept {
			return __builtin_bswap64(bb);
		}

		/**
		 * Computes total non-pawn midgame material for both sides combined.
		 */
		static value_t computeNonPawnMaterial(const MoveGenerator& position) noexcept;
	};

} // namespace ChessEval
