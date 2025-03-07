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
 * @author Volker B�hm
 * @copyright Copyright (c) 2021 Volker B�hm
 * @Overview
 * Implements a list holding moves of a chess position
 * Moves are stored in one list - but different for "silent moves" and "non silent moves". Silent moves are moves 
 * not capturing and not promoting - non silent moves are captures and promotes.
 * Silent moves are pushed to the end and non silent moves are inserted to the front. As a result non silent moves
 * are always ordered first
 */

#ifndef __MOVELIST_H
#define __MOVELIST_H

#include <assert.h>
#include <array>
#include "evalvalue.h"
#include "move.h"

namespace QaplaBasics {

	class MoveList
	{
	public:
		MoveList(void) { clear(); };

		void clear() { totalMoveAmount = 0; nonSilentMoveAmount = 0; }

		/**
		 * Adds a move to the move list, inserts non silent moves to the end of the non silent move
		 * list and pushes silent moves to the end
		 */
		inline void addMove(Move move) {
			if (move.isCaptureOrPromote()) {
				moveList[totalMoveAmount] = moveList[nonSilentMoveAmount];
				moveList[nonSilentMoveAmount] = move;
				nonSilentMoveAmount++;
			}
			else {
				moveList[totalMoveAmount] = move;
			}
			totalMoveAmount++;
		}

		/**
		 * Inserts a non silent move to the end of the non silent move list
		 */
		inline void addNonSilentMove(Move move) {
			assert(move.isCaptureOrPromote());
			moveList[totalMoveAmount] = moveList[nonSilentMoveAmount];
			moveList[nonSilentMoveAmount] = move;
			nonSilentMoveAmount++;
			totalMoveAmount++;
		}

		/**
	     * Adds a silent move end of the move list
		 */
		inline void addSilentMove(Move move) {
			moveList[totalMoveAmount] = move;
			totalMoveAmount++;
		}

		/**
		 * Adds all promotion moves, promotion to queen as non silent move, all other 
		 * promotions as silent move
		 */
		template<Piece COLOR>
		void addPromote(Square departure, Square destination, Piece capture) {
			Move move(departure, destination, Move::PROMOTE_UNSHIFTED + PAWN + COLOR, capture);
			addNonSilentMove(Move(move).setPromotion(QUEEN + COLOR));
			addSilentMove(Move(move).setPromotion(ROOK + COLOR));
			addSilentMove(Move(move).setPromotion(BISHOP + COLOR));
			addSilentMove(Move(move).setPromotion(KNIGHT + COLOR));
		}

		/**
		 * Swaps an entry of the move list
		 */
		void swapEntry(uint32_t index1, uint32_t index2) {
			swap(moveList[index1], moveList[index2]);
			swap(moveWeight[index1], moveWeight[index2]);
		}

		/**
		 * Moves a move in the move list from the departureIndex to the destination Index to the back
		 * The destinationIndex must be larger than the departure index
		 * Keeps all other moves in the original order - 
		 */
		void dragMoveToTheBack(uint32_t departureIndex, uint32_t destinationIndex) {
			assert(destinationIndex >= departureIndex);
			Move tempMove = moveList[destinationIndex];
			value_t tempWeight = moveWeight[destinationIndex];
			for (uint32_t index = destinationIndex; index > departureIndex; index--) {
				moveList[index] = moveList[index - 1];
				moveWeight[index] = moveWeight[index - 1];
			}
			moveList[departureIndex] = tempMove;
			moveWeight[departureIndex] = tempWeight;
		}

		/**
		 * Gets the best amount moves and sorts them to the beginning of the silent moves list
		 * Sorting is done by a kind of insertion sort (search the next best move and swaps it to the front).
		 */
		void sortFirstSilentMoves(uint32_t amount) {
			for (uint32_t sortIndex = nonSilentMoveAmount; sortIndex < totalMoveAmount && amount > 0; sortIndex++, amount--) {
				value_t moveWeight = 0;
				uint32_t bestIndex = sortIndex;
				for (uint32_t searchBestIndex = sortIndex; searchBestIndex < totalMoveAmount; searchBestIndex++) {
					if (getWeight(searchBestIndex) > moveWeight) {
						bestIndex = searchBestIndex;
					}
				}
				if (bestIndex != sortIndex) {
					swapEntry(sortIndex, bestIndex);
				}
			}
		}

		// Gets a move from an index
		Move getMove(uint32_t index) const { return moveList[index]; }

		Move operator[](uint32_t index) const { return moveList[index]; }
		Move& operator[](uint32_t index) { return moveList[index]; }

		bool isMoveAvailable(uint32_t index) const { return totalMoveAmount > index; }

		uint32_t getTotalMoveAmount() const { return totalMoveAmount; }
		uint32_t getNonSilentMoveAmount() const { return nonSilentMoveAmount; }

		value_t getWeight(uint32_t index) const { return moveWeight[index]; }
		void setWeight(uint32_t index, value_t weight) { moveWeight[index] = weight; }

		// Prints all mvoes to stdout
		void PrintMoves()
		{
			for (uint32_t i = 0; i < totalMoveAmount; i++)
			{
				moveList[i].print();
				cout << endl;
			}
		}


	protected:
		static const int32_t MAX_MOVE_AMOUNT = 200;

		array<Move, MAX_MOVE_AMOUNT> moveList;
		array<value_t, MAX_MOVE_AMOUNT> moveWeight;
	public:
		uint32_t totalMoveAmount;
		uint32_t nonSilentMoveAmount;
	};

}

#endif 


