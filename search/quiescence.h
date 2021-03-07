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
 * Implements the quiescense search algorithm for the chess engine
 * Quiescense search searches captures and some checking moves until 
 * a quiet position is reached.
 */

#ifndef __QUIESCENCESEARCH_H
#define __QUIESCENCESEARCH_H

#include "../basics/evalvalue.h"
#include "computinginfo.h"
#include "../search/tt.h"
#include "../movegenerator/movegenerator.h"

using namespace ChessMoveGenerator;

namespace ChessSearch {

	class Quiescence {

	private:
		Quiescence() {}

	public:

		/**
		 * Sets a pointer to the transposition table for later use
		 */
		static void setTT(TT* tt) { _tt = tt; }

		/**
	     * Performs the quiescense search
	     */
		static value_t search(
			MoveGenerator& board, ComputingInfo& computingInfo, Move lastMove,
			value_t alpha, value_t beta, ply_t ply);


	private:

		/**
		 * Computes the maximal value a capture move can gain + safety margin
		 * If this value is not enough to make it a valuable move, the move is skipped
		 */
		static value_t computePruneForewardValue(MoveGenerator& board, value_t standPatValue, Move move);

		/**
		 * Gets an entry from the transposition table
		 * @returns hash value or -MAX_VALUE, if no value found
		 */
		static value_t probeTT(MoveGenerator& board, value_t alpha, value_t beta, ply_t ply);

	public:

		static TT* _tt;

	};

}

#endif // __QUIESCENCESEARCH_H