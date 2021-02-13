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
using namespace ChessMoveGenerator;

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

			const bitBoard_t pawnThreat = position.pawnAttackMask[COLOR] & opponentPieces;
			const bitBoard_t hanging = nonProtectedPieces & position.attackMask[COLOR] & ~pawnThreat;
			EvalValue evPawnThreat = BitBoardMasks::popCount(pawnThreat) * EvalValue(PAWN_THREAT);
			EvalValue evHanging = BitBoardMasks::popCount(hanging) * EvalValue(HANGING);
			if (PRINT) cout
				<< colorToString(COLOR) << " pawn threat: " << std::right << std::setw(14) << evPawnThreat << endl
				<< colorToString(OPPONENT) << " hanging: " << std::right << std::setw(18) << evHanging << endl;
			return evPawnThreat + evHanging;
		}

		static constexpr value_t PAWN_THREAT[2] = { 50, 25 };
		static constexpr value_t HANGING[2] = { 20, 10 };

	};
}

#endif  // __THREAT_H

