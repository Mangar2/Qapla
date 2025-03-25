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
 * Implements a static class to evaluate bishops 
 */

#ifndef __BISHOP_H
#define __BISHOP_H

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
	class Bishop {
	public:
		static EvalValue eval(const MoveGenerator& position, EvalResults& results) {
			return evalColor<WHITE, false>(position, results, nullptr) - evalColor<BLACK, false>(position, results, nullptr);
		}

		static EvalValue evalWithDetails(const MoveGenerator& position, EvalResults& results, std::vector<PieceInfo>& details) {
			return evalColor<WHITE, true>(position, results, &details) - evalColor<BLACK, true>(position, results, &details);
		}

		static IndexLookupMap getIndexLookup() {
			IndexLookupMap indexLookup;
			indexLookup["bMobility"] = std::vector<EvalValue>{ BISHOP_MOBILITY_MAP, BISHOP_MOBILITY_MAP + 15 };
			indexLookup["bProperty"] = std::vector<EvalValue>{ BISHOP_PROPERTY_MAP, BISHOP_PROPERTY_MAP + 4 };
			indexLookup["bPST"] = PST::getPSTLookup(BISHOP);
			return indexLookup;
		}
	private:

		/**
		 * Evaluates Bishops
		 */
		template<Piece COLOR, bool STORE_DETAILS>
		static EvalValue evalColor(const MoveGenerator& position, EvalResults& results, std::vector<PieceInfo>* details) {
			EvalValue value;
			bitBoard_t bishops = position.getPieceBB(BISHOP + COLOR);
			results.bishopAttack[COLOR] = 0;
			if (bishops == 0) {
				return 0;
			}

			uint32_t allBishopsIndex = hasDoubleBishop(bishops);

			bitBoard_t passThroughBB = results.queensBB | position.getPieceBB(ROOK + opponentColor<COLOR>());
			bitBoard_t occupiedBB = position.getAllPiecesBB();
			bitBoard_t removeMask = (~position.getPiecesOfOneColorBB<COLOR>() | passThroughBB)
				& ~position.pawnAttack[opponentColor<COLOR>()];
			occupiedBB &= ~passThroughBB;

			while (bishops)
			{
				const Square bishopSquare = popLSB(bishops);
				const auto mobilityIndex = calcMobilityIndex<COLOR>(results, bishopSquare, occupiedBB, removeMask);
				//const auto mobilityValue = EvalValue(BISHOP_MOBILITY_MAP[mobilityIndex]);
				EvalValue mobilityValue = position.getEvalVersion() == 0 ? EvalValue(BISHOP_MOBILITY_MAP[mobilityIndex]) : CandidateTrainer::getCurrentCandidate().getWeightVector(2)[mobilityIndex];

				const auto propertyIndex = allBishopsIndex | (isPinned(position.pinnedMask[COLOR], bishopSquare) * PINNED_INDEX);
				const auto propertyValue = EvalValue(BISHOP_PROPERTY_MAP[propertyIndex]);

				const auto totalValue = mobilityValue + propertyValue;
				value += totalValue;

				if constexpr (STORE_DETAILS) {
					const auto materialValue = position.getPieceValue(BISHOP + COLOR);
					const auto pstValue = PST::getValue(bishopSquare, BISHOP + COLOR);
					const auto mobility = COLOR == WHITE ? mobilityValue : -mobilityValue;
					const auto property = COLOR == WHITE ? propertyValue : -propertyValue;
					IndexVector indexVector{ 
						{ "bMobility", mobilityIndex, COLOR }, 
						{ "bPST", uint32_t(switchSideToWhite<COLOR>(bishopSquare)), COLOR },
						{ "material", BISHOP, COLOR } };
					if (propertyIndex) {
						indexVector.push_back({ "bProperty", propertyIndex, COLOR });
					}
					const auto value = materialValue + pstValue + mobility + property;
					details->push_back({ BISHOP + COLOR, bishopSquare, indexVector, BISHOP_PROPERTY_INFO[propertyIndex], value });
				}
				
			}

			return value;
		}

		/**
		 * Checks, if we have at least two bishops on different colored fields
		 */
		static bool hasDoubleBishop(bitBoard_t bishops) {
			return ((bishops & WHITE_FIELDS) != 0 && (bishops & (~WHITE_FIELDS)) != 0);
		}

		/**
		 * Calculates the mobility of a bishop
		 */
		template<Piece COLOR>
		static inline uint32_t calcMobilityIndex(EvalResults& results, Square square, bitBoard_t occupiedBB, bitBoard_t removeBB)
		{
			bitBoard_t attackBB = Magics::genBishopAttackMask(square, occupiedBB);
			results.bishopAttack[COLOR] |= attackBB;
			results.piecesDoubleAttack[COLOR] |= results.piecesAttack[COLOR] & attackBB;
			results.piecesAttack[COLOR] |= attackBB;

			attackBB &= removeBB;
			return popCount(attackBB);
		}

		/**
		 * Returns true, if the bishop is pinned
		 */
		static inline bool isPinned(bitBoard_t pinnedBB, Square square) {
			return (pinnedBB & squareToBB(square)) != 0;
		}

		/**
		 * Bitmap to white fields, to check if bishops are on opposit colors
		 */
		static const bitBoard_t WHITE_FIELDS = 0x55AA55AA55AA55AA;

		static const uint32_t PINNED_INDEX = 2;

		/**
		 * Additional value for two bishops on different colors
		 */
		static constexpr value_t _doubleBishop[2] = { 10, 5 };
		static constexpr value_t _pinned[2] = { 0, 0 };
		static constexpr value_t BISHOP_PROPERTY_MAP[4][2] = {
			{ 0, 0 }, {_doubleBishop[0], _doubleBishop[1]}, {_pinned[0], _pinned[1]},
			{_doubleBishop[0] + _pinned[0], _doubleBishop[1] + _pinned[1]}
		};
		static inline std::string BISHOP_PROPERTY_INFO[4] = {
			"", "<par>", "<pin>", "<pin><par>"
		};

		/**
		 * Mobility of a bishop. Having negative values for 0 or 1 fields to move to did not bring ELO
		 * Still I kept it.
		 */
		static constexpr value_t BISHOP_MOBILITY_MAP[15][2] = {
			{ -15, -25 }, { -10, -15 }, { 0, 0 }, { 5, 5 }, { 8, 8 }, { 13, 13 }, { 16, 16 }, { 18, 18 },
			{ 20, 20 }, { 22, 22 }, { 24, 24 }, { 25, 25 }, { 25, 25 }, { 25, 25 }, { 25, 25 }
		};

	};
}


#endif

