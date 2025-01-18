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
 * Implements threat detection for evaluation
 */

#ifndef __THREAT_H
#define __THREAT_H

#include "../basics/types.h"
#include "../basics/evalvalue.h"
#include "../movegenerator/bitboardmasks.h"
#include "../movegenerator/movegenerator.h"

#include "evalresults.h"

using namespace ChessBasics;
using namespace QaplaMoveGenerator;

namespace ChessEval {

	class Threat
	{
	public:
		template<bool PRINT>
		static EvalValue eval(MoveGenerator& position, EvalResults& result) {
			return eval<WHITE, PRINT>(position, result) -
				eval<BLACK, PRINT>(position, result);
		}
	private:
		template<Piece COLOR, bool PRINT>
		static EvalValue eval(MoveGenerator& position, EvalResults& result) {
			constexpr Piece OPPONENT = switchColor(COLOR);
			const bitBoard_t opponentPieces = position.getPiecesOfOneColorBB<OPPONENT>() &
				~position.getPieceBB(OPPONENT + PAWN);
			const bitBoard_t nonProtectedPieces = opponentPieces & ~position.attackMask[OPPONENT];
			const bitBoard_t minorAttack = result.bishopAttack[COLOR] | result.knightAttack[COLOR];
			const bitBoard_t minorOrRookAttack = minorAttack | result.rookAttack[COLOR];
			
			const bitBoard_t threats =
				position.pawnAttack[COLOR] & opponentPieces
				| nonProtectedPieces & position.attackMask[COLOR]
				| position.getPieceBB(OPPONENT + ROOK) & minorAttack
				| position.getPieceBB(OPPONENT + QUEEN) & minorOrRookAttack
				| position.getPieceBB(OPPONENT + KING) & position.attackMask[COLOR];

			value_t threatAmout = popCountForSparcelyPopulatedBitBoards(threats);
			if (threatAmout > 10) {
				threatAmout = 10;
			}
			const EvalValue evThreats = THREAT_LOOKUP[threatAmout];
			if (PRINT) cout
				<< colorToString(COLOR) << " threats (" << threatAmout << "): " << std::right << std::setw(14) << evThreats << endl;
			return evThreats;
		}

		static constexpr value_t THREAT_LOOKUP[11][2] =
		{
			{ 0, 0 }, { 25, 20 }, { 70, 60 }, { 120, 100 }, { 200, 180 }, { 300, 300 },
			{ 400, 400 }, { 400, 400 }, { 400, 400 }, { 400, 400 }, { 400, 400 }
		};

	};
}

#endif  // __THREAT_H

