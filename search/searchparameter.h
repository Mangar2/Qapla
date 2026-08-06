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
 * Implements functions and constants for search parameters
 */

#ifndef __SEARCHPARAMETER_H
#define __SEARCHPARAMETER_H

#include <map>
#include <algorithm>
#include "../basics/piecesignature.h"
#include "searchdef.h"

using namespace QaplaBasics;

namespace QaplaSearch {
	class SearchParameter {
	public:

		/**
		 * Calculates the reduction by nullmove
		 */
		constexpr static uint32_t getNullmoveReduction(
			[[maybe_unused]]ply_t ply, 
			[[maybe_unused]]int32_t depth, 
			[[maybe_unused]]value_t beta, 
			[[maybe_unused]]value_t staticEval,
			[[maybe_unused]]bool isImproving) {
			return 4;
		}

		/**
		 * Calculates the depth for nullmove verification searches
		 */
		constexpr static uint32_t getNullmoveVerificationDepthReduction(
			[[maybe_unused]]ply_t ply, 
			[[maybe_unused]]int32_t remainingSearchDepth) {
			return 5;
		}

		/**
		 * Calculates the reduction for internal iterative deepening
		 */
		constexpr static ply_t getIIDReduction([[maybe_unused]]int32_t remainingSearchDepth)
		{
			return 2;
		}

		/**
		 * Calculates the minimal depth for internal iterative deepening
		 */
		constexpr static ply_t getIIDMinDepth()
		{
			return 4;
		}


		/**
		 * Calculates the late move reduction
		 */
		constexpr static ply_t getLateMoveReduction(bool pv, ply_t ply, uint32_t moveNo) {
			return 0;
			ply_t res = 0;
			if (ply >= 3) {
				if (moveNo > 8) {
					res++;
				}
				if (ply > 8 && moveNo > 5) {
					res++;
				}
				if (pv && res > 0) {
					res--;
				}
			}
			return res;
		}

		static const uint32_t MAX_SEARCH_DEPTH = 128;
		static const uint32_t AMOUNT_OF_SORTED_NON_CAPTURE_MOVES = 7;

		static const bool DO_NULLMOVE = true;
		static const ply_t NULLMOVE_REMAINING_DEPTH = 0;

		static const bool DO_IID = true;

		static const bool USE_HASH_IN_QUIESCENSE = true;
		static const bool EVADES_CHECK_IN_QUIESCENSE = true;
		// Set to true to make the quiescence parameters settable by UCI, see search-param.h
		static constexpr bool optimizeQS = true;

		static const bool DO_MOVE_ORDERING_STATISTIC = false;
		static const bool CLEAR_ORDERING_STATISTIC_BEFORE_EACH_MOVE = false;

		static const bool DO_CHECK_EXTENSIONS = true;

		static const bool DO_SE_EXTENSION = true;
		// The singular extension in non pv nodes costs search depth. Switched off while the
		// engine searches shallower than its opponents, see se(). The values tuned for it are
		// the defaults of seNonPvMargin*, seTTMinDepthReduction 3 and seDepthDivisor 263,
		// they belong to tag 0.4.0-032 and have to come back when this is switched on again.
		static constexpr bool DO_SE_IN_NON_PV = false;
		// Set to true to make the singular extension parameters settable by UCI, see search-param.h
		static constexpr bool optimizeSE = false;

		// Multi cut, computed within the singular extension search, see se(). Switched off
		// together with the non pv singular extension, it lives inside that search
		static const bool DO_MULTI_CUT = false;

		static const bool DO_PASSED_PAWN_EXTENSIONS = false;

		static const ply_t FOREWARD_FUTILITY_DEPTH = 10;
		constexpr static value_t FOREWARD_FUTILITY_FACTOR = 75;
		constexpr static value_t forewardFutilityMargin(ply_t depth, bool isImproving) {
			return FOREWARD_FUTILITY_FACTOR * (depth + 1) - 100 * isImproving;
		}

		// Futility Pruning (in move loop) - predicts forward futility will prune
		static const ply_t FUTILITY_DEPTH = 7;
		static const uint32_t FUTILITY_PRUNING_MIN_MOVE_NUMBER = 3;
		// 1.6 100, 25: 50,9%, 50: 51,8%, 60: 51,1% 75: 51,6%
		// 50 vs. 100: 50,9%, 50 vs. 75%: 50%
		constexpr static value_t FUTILITY_FACTOR = 75;
		constexpr static value_t futilityMargin(ply_t depth, bool isImproving) {
			return FUTILITY_FACTOR * (depth + 1) + 100 * isImproving;
		}

		static const Rank PASSED_PAWN_EXTENSION_WHITE_MIN_TARGET_RANK = Rank::R7;
		static const Rank PASSED_PAWN_EXTENSION_BLACK_MIN_TARGET_RANK = Rank::R2;

	};
}

#endif // __SEARCHPARAMETER_H
