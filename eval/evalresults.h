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

using namespace ChessBasics;

namespace ChessEval {

	struct EvalResults {
		// White and black queens
		bitBoard_t queensBB; 
		// Squares attacked by queens also behind a Rook & Bishop of same color
		bitBoard_t queenAttack[2];
		// Squares attacked by rooks also behind another Rook or Queen of same color or Queen of opposit color
		bitBoard_t rookAttack[2];
		// Squares attacked by two rooks also behind another Rook or Queen of same color or Queen of opposit color
		bitBoard_t doubleRookAttack[2];
		// Squares attacked by bishops also behind another Bishop or Queen of same color or Queen, Rook of opposit color
		bitBoard_t bishopAttack[2];
		// Squares attacked by knighs
		bitBoard_t knightAttack[2];
		// Squares attacked by two knights
		bitBoard_t doubleKnightAttack[2];
		// Squares attacked by pawns
		bitBoard_t pawnAttack[2];
		// Squares attacked by any piece
		bitBoard_t piecesAttack[2];
		// Squares attacked by two pieces (any) ...
		bitBoard_t piecesDoubleAttack[2];
		// Squares attacked by one piece and undefended or by two pieces but only defended by one piece
		bitBoard_t undefendedAttack[2];
		// Squares attacked by two piece and not defended
		bitBoard_t undefendedDoubleAttack[2];
		// Amount of non defended attacks on squares near king
		value_t kingPressureCount[2];
		// Evaluation of the king attack
		value_t kingAttackValue[2];
		// Ranks in front of pawns
		bitBoard_t pawnMoveRay[2];
	};

}

#endif