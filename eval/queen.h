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
 * Implements evaluation for queens
 */

#ifndef __QUEEN_H
#define __QUEEN_H

#include <map>
#include "../movegenerator/movegenerator.h"
#include "../basics/types.h"
#include "../basics/pst.h"
#include "evalresults.h"

using namespace QaplaMoveGenerator;

namespace ChessEval {

	class Queen {
	public:
		/**
		 * Evaluates the evaluation value for queens
		 */
		static EvalValue eval(const MoveGenerator& position, EvalResults& results) {
			return evalColor<WHITE, false>(position, results, nullptr) - evalColor<BLACK, false>(position, results, nullptr);
		}

		static EvalValue evalWithDetails(const MoveGenerator& position, EvalResults& results, std::vector<PieceInfo>& details) {
			return evalColor<WHITE, true>(position, results, &details) - evalColor<BLACK, true>(position, results, &details);
		}

	private:

		/**
		 * Calculates the evaluation value for Queens
		 */
		template<Piece COLOR, bool STORE_DETAILS>
		static EvalValue evalColor(const MoveGenerator& position, EvalResults& results, std::vector<PieceInfo>* details) {
			bitBoard_t queens = position.getPieceBB(QUEEN + COLOR);
			results.queenAttack[COLOR] = 0;
			if (queens == 0) {
				return 0;
			}

			bitBoard_t occupied = position.getAllPiecesBB();
			bitBoard_t removeMask = ~position.pawnAttack[opponentColor<COLOR>()] & ~position.getPiecesOfOneColorBB<COLOR>();

			EvalValue value = 0;
			while (queens)
			{
				const Square square = popLSB(queens);
				const auto mobilityIndex = calcMobilityIndex<COLOR>(position, results, square, occupied, removeMask);
				const auto mobilityValue = EvalValue(QUEEN_MOBILITY_MAP[mobilityIndex]);

				const auto propertyIndex = isPinned(position.pinnedMask[COLOR], square);
				const auto propertyValue = EvalValue(_pinned[propertyIndex]);

				value += mobilityValue + propertyValue;

				if constexpr (STORE_DETAILS) {
					const auto materialValue = position.getPieceValue(QUEEN + COLOR);
					const auto pstValue = PST::getValue(square, QUEEN + COLOR);
					const auto mobility = COLOR == WHITE ? mobilityValue : -mobilityValue;
					const auto property = COLOR == WHITE ? propertyValue : -propertyValue;
					details->push_back({
						QUEEN + COLOR,
						square,
						mobilityIndex,
						propertyIndex,
						QUEEN_PROPERTY_INFO[propertyIndex],
						mobility,
						property,
						materialValue,
						pstValue,
						mobility + property + materialValue + pstValue });
				}
			}
			return value;
		}

		/**
		 * Calculates the mobility of a queen
		 */
		template<Piece COLOR>
		static inline uint32_t calcMobilityIndex(const MoveGenerator& position,
			EvalResults& results, Square square, bitBoard_t occupiedBB, bitBoard_t removeBB)
		{
			bitBoard_t attackBB = Magics::genRookAttackMask(square, occupiedBB & ~position.getPieceBB(ROOK + COLOR));
			attackBB |= Magics::genBishopAttackMask(square, occupiedBB & ~position.getPieceBB(BISHOP + COLOR));
			results.piecesDoubleAttack[COLOR] |= results.piecesAttack[COLOR] & attackBB;
			results.piecesAttack[COLOR] |= attackBB;
			results.queenAttack[COLOR] |= attackBB;

			attackBB &= removeBB;
			return popCount(attackBB);
		}

		/**
		 * Returns true, if the queen is pinned
		 */
		static constexpr uint32_t isPinned(bitBoard_t pinnedBB, Square square) {
			return (pinnedBB & squareToBB(square)) != 0;
		};

		static constexpr value_t _pinned[2] = { 0, 0 };
		static inline std::string QUEEN_PROPERTY_INFO[2] = {
			"", "<pin>"
		};

		static constexpr value_t QUEEN_MOBILITY_MAP[30] = {
			-10, -10, -10, -5, 0, 2, 4, 5, 6, 10, 10, 10, 10, 10, 10,
			10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10
		};

	};
};

#endif // __QUEEN_H