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

		static IndexLookupMap getIndexLookup() {
			IndexLookupMap indexLookup;
			indexLookup["qMobility"] = std::vector<EvalValue>{ QUEEN_MOBILITY_MAP.begin(), QUEEN_MOBILITY_MAP.end()};
			indexLookup["qProperty"] = std::vector<EvalValue>{ QUEEN_PROPERTY_MAP.begin(), QUEEN_PROPERTY_MAP.end()};
			indexLookup["qPST"] = PST::getPSTLookup(QUEEN);
			return indexLookup;
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
				//const auto mobilityValue = position.getEvalVersion() == 0 ? EvalValue(QUEEN_MOBILITY_MAP[mobilityIndex]) : CandidateTrainer::getCurrentCandidate().getWeightVector(0)[mobilityIndex];

				const auto propertyIndex = isPinned(position.pinnedMask[COLOR], square);
				//const auto propertyValue = QUEEN_PROPERTY_MAP[propertyIndex];
				const auto propertyValue = position.getEvalVersion() == 0 ? QUEEN_PROPERTY_MAP[propertyIndex] : CandidateTrainer::getCurrentCandidate().getWeightVector(0)[propertyIndex];

				value += mobilityValue + propertyValue;

				if constexpr (STORE_DETAILS) {
					const auto materialValue = position.getPieceValue(QUEEN + COLOR);
					const auto pstValue = PST::getValue(square, QUEEN + COLOR);
					const auto mobility = COLOR == WHITE ? mobilityValue : -mobilityValue;
					const auto property = COLOR == WHITE ? propertyValue : -propertyValue;
					IndexVector indexVector{ 
						  { "qMobility", mobilityIndex, COLOR },
						  { "qPST", uint32_t(switchSideToWhite<COLOR>(square)), COLOR },
						  { "material", QUEEN, COLOR } };
					if (propertyIndex) {
						indexVector.push_back({ "qProperty", propertyIndex, COLOR });
					}

					const auto value = materialValue + pstValue + mobility + property;
					details->push_back({ QUEEN + COLOR, square, indexVector, QUEEN_PROPERTY_INFO[propertyIndex], value });
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

		static constexpr std::array<EvalValue, 2> QUEEN_PROPERTY_MAP = { { { 0, 0 }, { 0, 0 } } };
		static inline std::string QUEEN_PROPERTY_INFO[2] = {
			"", "<pin>"
		};

		static constexpr array<value_t, 30> QUEEN_MOBILITY_MAP = { {
			-10, -10, -10, -5, 0, 2, 4, 5, 6, 10, 10, 10, 10, 10, 10,
			10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10
		} };

	};
};

#endif // __QUEEN_H