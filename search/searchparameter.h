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
 * @copyright Copyright (c) 2021 Volker Böhm
 * @Overview
 * Implements functions and constants for search parameters
 */

#ifndef __SEARCHPARAMETER_H
#define __SEARCHPARAMETER_H

#include "../basics/piecesignature.h"
#include "searchdef.h"

using namespace ChessBasics;

namespace ChessSearch {
	class SearchParameter {
	public:

		/**
		 * Calculates the reduction by nullmove
		 */
		static uint32_t getNullmoveReduction(ply_t ply, int32_t remainingSearchDepth) {
			return remainingSearchDepth >= 4 ? 3 : 2;
		}

		/**
		 * Calculates the depth for nullmove verification searches
		 */
		static uint32_t getNullmoveVerificationDepthReduction(ply_t ply, int32_t remainingSearchDepth) {
			return 5;
		}

		/**
		 * Calculates the reduction for internal iterative deepening
		 */
		static inline ply_t getIIDReduction(int32_t remainingSearchDepth, bool isPV)
		{
			return isPV ? 2 : 4;
		}

		/**
		 * Calculates the minimal depth for internal iterative deepening
		 */
		static inline ply_t getIIDMinDepth(bool isPV)
		{
			return isPV ? 4 : 6;
		}

		/**
		 * Calculates the late move reduction
		 */
		static ply_t getLateMoveReduction(bool pv, ply_t ply, uint32_t moveNo) {
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
		static const uint32_t AMOUNT_OF_SORTED_NON_CAPTURE_MOVES = 5;

		static const bool DO_NULLMOVE = true;
		static const ply_t NULLMOVE_REMAINING_DEPTH = 2;

		static const bool DO_IID = true;
		static const bool DO_IID_FOR_CUT_NODES = true; 

		static const bool QUIESCENSE_USE_SEE_PRUNINT = false;
		static const bool USE_HASH_IN_QUIESCENSE = false;
		static const value_t PRUING_SAFETY_MARGIN_IN_CP = 50;

		static const bool DO_MOVE_ORDERING_STATISTIC = false;
		static const bool CLEAR_ORDERING_STATISTIC_BEFORE_EACH_MOVE = false;

		static const bool DO_CHECK_EXTENSIONS = true;
		static const bool DO_PASSED_PAWN_EXTENSIONS = false;

		static const ply_t DO_FUTILITY_DEPTH = 3;
		constexpr static value_t futilityMargin(ply_t depth) { return 200 * (depth + 1); }

		static const bool DO_RAZORING = false;
		static const ply_t RAZORING_DEPTH = 3;
		static constexpr value_t RAZORING_MARGIN[RAZORING_DEPTH + 1] = { 200, 250, 300, 400 };

		static const Rank PASSED_PAWN_EXTENSION_WHITE_MIN_TARGET_RANK = Rank::R7;
		static const Rank PASSED_PAWN_EXTENSION_BLACK_MIN_TARGET_RANK = Rank::R2;
	};
}

#endif // __SEARCHPARAMETER_H