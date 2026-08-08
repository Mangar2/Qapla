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
 * Implements a list holding moves of a chess position
 * Moves are stored in one list - but different for "silent moves" and "non silent moves". Silent moves are moves 
 * not capturing and not promoting - non silent moves are captures and promotes.
 * Silent moves are pushed to the end and non silent moves are inserted to the front. As a result non silent moves
 * are always ordered first
 */

#pragma once

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
		 * Adds a move to the list.
		 * Non-silent moves (captures or promotions) are inserted at the front of the silent region,
		 * all others (silent moves) are added to the back.
		 *
		 * This keeps non-silent moves contiguous and in order of addition.
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
		 * Adds all promotion variants for a given pawn move.
		 *
		 * Queen promotion is considered non-silent (likely best),
		 * others (rook, bishop, knight) are inserted as silent moves.
		 *
		 * @tparam COLOR Color of the moving side (WHITE or BLACK).
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
			swap(moveWeights[index1], moveWeights[index2]);
		}

		/**
		 * Moves a move from `destinationIndex` to `departureIndex` by shifting
		 * the entire range one step forward, preserving order.
		 *
		 * Use case: reordering after move sorting.
		 * Assumes: destinationIndex >= departureIndex
		 */
		void dragMoveToTheBack(uint32_t departureIndex, uint32_t destinationIndex) {
			assert(destinationIndex >= departureIndex);
			Move tempMove = moveList[destinationIndex];
			value_t tempWeight = moveWeights[destinationIndex];
			for (uint32_t index = destinationIndex; index > departureIndex; index--) {
				moveList[index] = moveList[index - 1];
				moveWeights[index] = moveWeights[index - 1];
			}
			moveList[departureIndex] = tempMove;
			moveWeights[departureIndex] = tempWeight;
		}

		/**
		 * Sorts the best `amount` silent moves to the beginning of the silent move list.
		 * Selection sort: search the best remaining move and swap it to the front, repeated
		 * until `amount` moves are placed. Only moves above SORT_WEIGHT_FLOOR take part, all
		 * others keep the order the move generator produced.
		 */
		void sortFirstSilentMoves(uint32_t amount) {
			// Weight a move has to exceed to be sorted to the front. It doubles as the
			// "nothing found" marker of bestWeight, which is what makes the early exit below
			// work. Raising it to a value real weights can reach would break that.
			// At -MAX_VALUE moves with a negative history take part in the sorting as well.
			// The butterfly values are not bounded by MAX_VALUE, so a move whose history has
			// dropped below -MAX_VALUE still keeps its generated position.
			constexpr value_t SORT_WEIGHT_FLOOR = -MAX_VALUE;
			for (uint32_t sortIndex = nonSilentMoveAmount; sortIndex < totalMoveAmount && amount > 0; sortIndex++, amount--) {
				value_t bestWeight = SORT_WEIGHT_FLOOR;
				uint32_t bestIndex = sortIndex;
				for (uint32_t searchBestIndex = sortIndex; searchBestIndex < totalMoveAmount; searchBestIndex++) {
					const auto weight = getWeight(searchBestIndex);
					if (weight > bestWeight) {
						bestWeight = weight;
						bestIndex = searchBestIndex;
					}
				}
				// Nothing above the floor is left. Every later pass searches a subrange of
				// this one, so none of them can find anything either.
				if (bestWeight == SORT_WEIGHT_FLOOR) {
					break;
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

		value_t getWeight(uint32_t index) const { return moveWeights[index]; }
		void setWeight(uint32_t index, value_t weight) { moveWeights[index] = weight; }

		// Prints all moves to stdout
		void print()
		{
			for (uint32_t i = 0; i < totalMoveAmount; i++)
			{
				moveList[i].print();
				cout << endl;
			}
		}

	protected:
		// Maximum number of moves stored in a single list.
		// Must be large enough to hold all possible legal moves including promotions.
		// The current known maximum is 218 legal moves, e.g. in the position:
		//   3Q4/1Q4Q1/4Q3/2Q4R/Q4Q2/3Q4/1Q4Rp/1K1BBNNk w - - 0 1
		// However, positions with artificially high material (e.g. 20 queens) can be set via FEN
		// Thus, MAX_MOVE_AMOUNT is set conservatively to 300.
		static const int32_t MAX_MOVE_AMOUNT = 300;

		array<Move, MAX_MOVE_AMOUNT> moveList;
		array<value_t, MAX_MOVE_AMOUNT> moveWeights;
	public:
		uint32_t totalMoveAmount;
		uint32_t nonSilentMoveAmount;
	};

}



