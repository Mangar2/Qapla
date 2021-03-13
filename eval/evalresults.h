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
 * Implements a structure containing various results from eval calculations
 */

#ifndef __EVALRESULTS_H
#define __EVALRESULTS_H

 // Idee 1: Evaluierung in der Ruhesuche ohne positionelle Details
 // Idee 2: Zugsortierung nach lookup Tabelle aus reduziertem Board-Hash
 // Idee 3: Beweglichkeit einer Figur aus der Suche evaluieren. Speichern, wie oft eine Figur von einem Startpunkt erfolgreich bewegt wurde.

#include "../basics/types.h"
#include "../basics/evalvalue.h"

using namespace ChessBasics;

namespace ChessEval {

	struct EvalResults {
		template <Piece COLOR>
		inline void clearAttacksBB() {
			queenAttack[COLOR] = 0;
			rookAttack[COLOR] = 0;
			bishopAttack[COLOR] = 0;
			knightAttack[COLOR] = 0;
			piecesAttack[COLOR] = 0;
			piecesDoubleAttack[COLOR] = 0;
		}
		inline void clearAttacksBB() {
			clearAttacksBB<WHITE>();
			clearAttacksBB<BLACK>();
		}

		// White and black queens
		bitBoard_t queensBB; 
		// White and black pawns
		bitBoard_t pawnsBB;
		// Squares attacked by queens also behind a Rook & Bishop of same color
		colorBB_t queenAttack;
		// Squares attacked by rooks also behind another Rook or Queen of same color or Queen of opposit color
		colorBB_t rookAttack;
		// Squares attacked by bishops also behind another Bishop or Queen of same color or Queen, Rook of opposit color
		colorBB_t bishopAttack;
		// Squares attacked by knighs
		colorBB_t knightAttack;
		// Squares attacked by any piece
		colorBB_t piecesAttack;
		// Squares attacked by two pieces (any) ...
		colorBB_t piecesDoubleAttack;
		// Fields with passed pawns
		colorBB_t passedPawns;
		// The midgame factor in percent
		value_t midgameInPercent;
		value_t midgameInPercentV2;
		// Amount of non defended attacks on squares near king
		value_t kingPressureCount[2];
		// Evaluation of the king attack
		value_t kingAttackValue[2];
	};

}

#endif