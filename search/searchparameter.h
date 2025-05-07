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
#ifdef _OPTIMIZE
		static uint32_t getNullmoveReduction(ply_t ply, int32_t depth, value_t beta, value_t staticEval) {
			const uint32_t reduction = getParameter("rnm", 3);
			const uint32_t depthRed = getParameter("dnm", 0);
			return reduction + (depthRed > 0 ? depth / depthRed : 0);
		}
#else
		constexpr static uint32_t getNullmoveReduction(
			[[maybe_unused]]ply_t ply, 
			[[maybe_unused]]int32_t depth, 
			[[maybe_unused]]value_t beta, 
			[[maybe_unused]]value_t staticEval) {
			return 4;
		}
#endif

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

		static void parseCommandLine(int argc, char* argv[]) {
			for (int i = 1; i < argc - 1; i += 2) {
				std::string key = argv[i];
				try {
					int32_t value = std::stoi(argv[i + 1]);
					parameters[key] = value;
				}
				catch (const std::exception&) {
					// Intentionally do nothing
				}
			}
		}

		static int32_t getParameter(const std::string& key, int32_t defaultValue) {
			auto it = parameters.find(key);
			if (it == parameters.end()) {
				return defaultValue;
			}
			return it->second;
		}

		static const uint32_t MAX_SEARCH_DEPTH = 128;
		static const uint32_t AMOUNT_OF_SORTED_NON_CAPTURE_MOVES = 7;

		static const bool DO_NULLMOVE = true;
		static const ply_t NULLMOVE_REMAINING_DEPTH = 0;

		static const bool DO_IID = true;

		static const bool QUIESCENSE_USE_SEE_PRUNINT = false;
		static const bool USE_HASH_IN_QUIESCENSE = true;
		static const bool EVADES_CHECK_IN_QUIESCENSE = true;
		static const value_t PRUING_SAFETY_MARGIN_IN_CP = 50;

		static const bool DO_MOVE_ORDERING_STATISTIC = false;
		static const bool CLEAR_ORDERING_STATISTIC_BEFORE_EACH_MOVE = false;

		static const bool DO_CHECK_EXTENSIONS = true;

		static const bool DO_SE_EXTENSION = true;
		static value_t singularExtensionMargin(ply_t depth) {
			const auto marginC = getParameter("semc", 1);
			const auto marginF = getParameter("semf", 4);
			return marginC + marginF * depth;
			//return 30 + depth * 4;
		}

		static const bool DO_PASSED_PAWN_EXTENSIONS = false;

		static const ply_t DO_FUTILITY_DEPTH = 10;
		static value_t cmdLineParam[10];
		static value_t futilityMargin(ply_t depth, bool isImproving) {
			// const auto improvingReduction = getParameter("rf", 0);
			return 100 * (depth + 1) - 100 * isImproving;
		}

		static const bool DO_RAZORING = false;
		static const ply_t RAZORING_DEPTH = 3;
		static constexpr value_t RAZORING_MARGIN[RAZORING_DEPTH + 1] = { 200, 250, 300, 400 };

		static const Rank PASSED_PAWN_EXTENSION_WHITE_MIN_TARGET_RANK = Rank::R7;
		static const Rank PASSED_PAWN_EXTENSION_BLACK_MIN_TARGET_RANK = Rank::R2;

		inline static std::map<std::string, int32_t> parameters;

	};
}

#endif // __SEARCHPARAMETER_H