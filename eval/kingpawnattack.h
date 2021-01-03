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
 * Defines helpers to compute the possibility of a king to attack pawns
 */

#ifndef __KINGPAWNATTACK_H
#define __KINGPAWNATTACK_H

#include "../movegenerator/bitboardmasks.h"
#include "../movegenerator/movegenerator.h"

using namespace ChessMoveGenerator;

namespace ChessEval {
	class KingPawnAttack
	{
	public:
		KingPawnAttack();
		value_t computeKingRace(MoveGenerator& board);

	private:
		template<Piece COLOR>
		inline void initRace(MoveGenerator& board);

		template<Piece COLOR>
		inline void makeMove();

		template<Piece COLOR>
		inline bool capturesPawn();

		bitBoard_t legalPositions[COLOR_AMOUNT];
		bitBoard_t kingPositions[COLOR_AMOUNT];
		bitBoard_t formerPositions[COLOR_AMOUNT];
		bitBoard_t kingAttack[COLOR_AMOUNT];
		bitBoard_t formerAttack[COLOR_AMOUNT];
		bitBoard_t weakPawns[COLOR_AMOUNT];
	};

}

#endif // __KINGPAWNATTACK_H

