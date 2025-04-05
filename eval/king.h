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
 * Implements a static class to evaluate knights 
 */

#pragma once

#include <cstdint>
#include "../basics/types.h"
#include "../movegenerator/bitboardmasks.h"
#include "../movegenerator/movegenerator.h"
#include "../basics/evalvalue.h"
#include "../basics/pst.h"
#include "evalresults.h"

using namespace QaplaBasics;
using namespace QaplaMoveGenerator;

namespace ChessEval {


	class King {
	public:

		static EvalValue eval(const MoveGenerator& position, EvalResults& results) {
			return evalColor<WHITE, false>(position, results, nullptr) - evalColor<BLACK, false>(position, results, nullptr);
		}

		static EvalValue evalWithDetails(const MoveGenerator& position, EvalResults& results, std::vector<PieceInfo>& details) {
			return evalColor<WHITE, true>(position, results, &details) - evalColor<BLACK, true>(position, results, &details);
		}

		static IndexLookupMap getIndexLookup() {
			IndexLookupMap indexLookup;
			indexLookup["kPST"] = PST::getPSTLookup(KNIGHT);
			return indexLookup;
		}
	private:

		/**
		 * Evaluates Kings
		 */
		template<Piece COLOR, bool STORE_DETAILS>
		static EvalValue evalColor(const MoveGenerator& position, EvalResults& results, std::vector<PieceInfo>* details) {
			const Square kingSquare = position.getKingSquare<COLOR>();
			const value_t kingDistance = minDistance(kingSquare, position.getPieceBB(PAWN + COLOR));
			return kingDistance * DISTANCE_PENALTY;
		}

		static inline std::array<std::array<uint64_t, 6>, 64> king_distance_masks = [] {
			std::array<std::array<uint64_t, 6>, 64> result{};

			for (int king = 0; king < 64; ++king) {
				for (int sq = 0; sq < 64; ++sq) {
					int d = std::max(std::abs(king % 8 - sq % 8), std::abs(king / 8 - sq / 8));
					if (d > 0 && d <= 6)
						result[king][d - 1] |= 1ULL << sq;
				}
				result[king][2] |= result[king][0] | result[king][1];
				result[king][4] |= result[king][3];
			}

			return result;
		}();

		static value_t minDistance(Square kingSquare, bitBoard_t pawns) {
			if (pawns == 0) {
				return 0;
			}
			const auto& masks = king_distance_masks[kingSquare];
			if (pawns & masks[2]) {
				return (pawns & masks[1]) ? 2 : 1;
			}
			else if (pawns& masks[4]) {
				return (pawns & masks[3]) ? 4 : 5;
			}
			else return (pawns & masks[5]) ? 6 : 7;
		}

		static const value_t DISTANCE_PENALTY = -5;
	};
}


