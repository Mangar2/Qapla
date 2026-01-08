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
 * Implements threat detection for evaluation
 */

#ifndef __THREAT_H
#define __THREAT_H

#include "../basics/types.h"
#include "../basics/evalvalue.h"
#include "../movegenerator/bitboardmasks.h"
#include "../movegenerator/movegenerator.h"
#include "array-generator.h"
#include "../interface/uci-parameter-provider.h"

#include "evalresults.h"

using namespace QaplaBasics;
using namespace QaplaMoveGenerator;

namespace ChessEval {

	class Threat
	{
	public:
		// Forward declaration of UCI parameter handler
		class UciAccess;

		/**
		 * Get UCI parameter access interface
		 * @return Reference to UCI parameter provider
		 */
		static UciParameterProvider& getUciAccess();
		static EvalValue eval(MoveGenerator& position, EvalResults& result) {
			return eval<WHITE>(position, result) - eval<BLACK>(position, result);
		}

		static IndexLookupMap getIndexLookup() {
			IndexLookupMap indexLookup;
			indexLookup["threat"] = std::vector<EvalValue>{ THREAT_LOOKUP.begin(), THREAT_LOOKUP.end() };
			return indexLookup;
		}

		static void addToIndexVector(MoveGenerator& position, const EvalResults& result, IndexVector& indexVector) {
			uint32_t wIndex = computeThreatIndex<WHITE>(position, result);
			uint32_t bIndex = computeThreatIndex<BLACK>(position, result);
			if (wIndex) {
				indexVector.push_back(IndexInfo{ "threat", wIndex, WHITE });
			}
			if (bIndex) {
				indexVector.push_back(IndexInfo{ "threat", bIndex, BLACK });
			}
		}

		template<Piece COLOR>
		static uint32_t computeThreatIndex(MoveGenerator& position, const EvalResults& result) {
			constexpr Piece OPPONENT = switchColor(COLOR);
			const bitBoard_t opponentPieces = position.getPiecesOfOneColorBB<OPPONENT>() &
				~position.getPieceBB(OPPONENT + PAWN);
			const bitBoard_t nonProtectedPieces = opponentPieces & ~position.attackMask[OPPONENT];
			const bitBoard_t minorAttack = result.bishopAttack[COLOR] | result.knightAttack[COLOR];
			const bitBoard_t minorOrRookAttack = minorAttack | result.rookAttack[COLOR];

			const bitBoard_t threats =
				(position.pawnAttack[COLOR] & opponentPieces)
				| (nonProtectedPieces & position.attackMask[COLOR])
				| (position.getPieceBB(OPPONENT + ROOK) & minorAttack)
				| (position.getPieceBB(OPPONENT + QUEEN) & minorOrRookAttack)
				| (position.getPieceBB(OPPONENT + KING) & position.attackMask[COLOR]);

			value_t threatAmout = popCountForSparcelyPopulatedBitBoards(threats);
			if (threatAmout > 10) {
				threatAmout = 10;
			}
			return threatAmout;
		}


	private:
		template<Piece COLOR>
		static EvalValue eval(MoveGenerator& position, const EvalResults& result) {
			const auto threatAmount = computeThreatIndex<COLOR>(position, result);
			const EvalValue evThreats = THREAT_LOOKUP[threatAmount];
			return evThreats;
		}

		// Default values for UCI parameters (scaled for 0-2000 range)
		static constexpr int32_t DEFAULT_THREAT_LINEAR_MG = 400;
		static constexpr int32_t DEFAULT_THREAT_QUADRATIC_MG = 400;
		static constexpr int32_t DEFAULT_THREAT_DAMPENING_MG = 700;
		static constexpr int32_t DEFAULT_THREAT_LINEAR_EG = 400;
		static constexpr int32_t DEFAULT_THREAT_QUADRATIC_EG = 400;
		static constexpr int32_t DEFAULT_THREAT_DAMPENING_EG = 700;
		static constexpr int32_t DEFAULT_THREAT_SCALE = 500;

		/**
		 * Generates THREAT_LOOKUP array using normalized polynomial growth without activation
		 * Separate generation for midgame and endgame values
		 */
		static array<EvalValue, 11> generateThreatLookup(
			double linearTermMg,
			double quadraticTermMg,
			double dampeningRateMg,
			double linearTermEg,
			double quadraticTermEg,
			double dampeningRateEg,
			double scale)
		{
			auto midgameValues = generateArrayPolynomial<11>(
				linearTermMg, quadraticTermMg, dampeningRateMg, scale, 0.0, 1, false, true);
			auto endgameValues = generateArrayPolynomial<11>(
				linearTermEg, quadraticTermEg, dampeningRateEg, scale, 0.0, 1, false, true);

			array<EvalValue, 11> result;
			for (size_t i = 0; i < 11; ++i) {
				result[i] = EvalValue(midgameValues[i], endgameValues[i]);
			}
			return result;
		}

		// Generated threat lookup values - modern C++17 inline static initialization
		inline static array<EvalValue, 11> THREAT_LOOKUP = 
			generateThreatLookup(
				DEFAULT_THREAT_LINEAR_MG,
				DEFAULT_THREAT_QUADRATIC_MG,
				DEFAULT_THREAT_DAMPENING_MG,
				DEFAULT_THREAT_LINEAR_EG,
				DEFAULT_THREAT_QUADRATIC_EG,
				DEFAULT_THREAT_DAMPENING_EG,
				DEFAULT_THREAT_SCALE
			);

		// Original reference values for THREAT_LOOKUP (kept for comparison)
		// static constexpr array<EvalValue, 11> THREAT_LOOKUP = { {
		// 	{  0,   0}, { 50,  50}, { 100,  100 }, { 150, 150 }, { 200, 200 }, { 250, 250 }, 
		// 	{400, 400}, {400, 400}, {400, 400}, {400, 400}, {400, 400}
		// } };
	};

	/**
	 * UCI parameter access implementation for Threat
	 */
	class Threat::UciAccess : public UciParameterProvider {
	public:
		std::vector<UciParam> getUciParameters() const override;
		bool setUciParameter(const std::string& name, int32_t value) override;

	private:
		int32_t _linearTermMg = DEFAULT_THREAT_LINEAR_MG;
		int32_t _quadraticTermMg = DEFAULT_THREAT_QUADRATIC_MG;
		int32_t _dampeningRateMg = DEFAULT_THREAT_DAMPENING_MG;
		int32_t _linearTermEg = DEFAULT_THREAT_LINEAR_EG;
		int32_t _quadraticTermEg = DEFAULT_THREAT_QUADRATIC_EG;
		int32_t _dampeningRateEg = DEFAULT_THREAT_DAMPENING_EG;
		int32_t _scale = DEFAULT_THREAT_SCALE;
	};
}

#endif  // __THREAT_H

