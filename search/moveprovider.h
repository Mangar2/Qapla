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
 * Implements a move provider for search providing moves in the right order
 */

#ifndef __MOVEPROVIDER_H
#define __MOVEPROVIDER_H

#include "../basics/move.h"
#include "../basics/movelist.h"
#include "searchdef.h"
#include "see.h"
#include "search-config.h"
#include "butterfly-boards.h"
#include "../movegenerator/movegenerator.h"


using namespace QaplaBasics;
using namespace QaplaMoveGenerator;

namespace QaplaSearch {


	class MoveProvider {
	public:


		MoveProvider() : curMoveNo(0), triedMovesAmount(0) {}

		/**
		 * Generates all moves of the side to move, nothing selected yet
		 */
		inline void computeMoves(MoveGenerator& board) {
			board.genMovesOfMovingColor(moveList);
			curMoveNo = 0;
			triedMovesAmount = 0;
		}

		/**
		 * Generates the evades of the side to move, nothing selected yet
		 */
		inline void computeEvades(MoveGenerator& board) {
			board.genEvadesOfMovingColor(moveList);
			curMoveNo = 0;
			triedMovesAmount = 0;
		}

		/**
		 * Checks for a game end (mate, stale-mate) because the move provider did not find any moves
		 */
		value_t checkForGameEnd(MoveGenerator& board, ply_t ply) {
			value_t bestValue = -MAX_VALUE;
			if (moveList.getTotalMoveAmount() == 0) {
				bestValue = 0;
				if (board.isInCheck()) {
					bestValue = -MAX_VALUE + ply;
				}
			}
			return bestValue;
		}

		/**
		 * Selects a move proposed from outside the list - pv, tt or killer move
		 * @returns an empty move if it is empty itself or not (any more) in the list
		 */
		Move selectProposedMove(Move move) {
			return takeMove(findProposedMove(move));
		}

		/**
		 * Gives every capture its ordering weight. Must come before selectGoodCapture.
		 * @param previousMove the move before, a capture on its destination gets the recapture bonus
		 */
		void weightCaptures(const MoveGenerator& board, Move previousMove) {
			computeAllCaptureWeight(board, previousMove);
		}

		/**
		 * Selects the next capture that SEE does not rate as losing, highest weight first.
		 * Losing captures are deferred and come with selectRemainingMove.
		 * @returns an empty move when no such capture is left
		 */
		Move selectGoodCapture(const MoveGenerator& board) {
			return takeMove(selectNextCaptureMoveHandlingLoosingCaptures(board));
		}

		/**
		 * Sorts the first silent moves by their butterfly value
		 */
		void sortSilentMoves(ButterflyBoard& butterflyBoard) {
			for (uint32_t moveNo = moveList.getNonSilentMoveAmount(); moveNo < moveList.getTotalMoveAmount(); moveNo++) {
				moveList.setWeight(moveNo, butterflyBoard.getValue(moveList.getMove(moveNo)));
			}
			moveList.sortFirstSilentMoves(SearchConfig::AMOUNT_OF_SORTED_NON_CAPTURE_MOVES);
		}

		/**
		 * Selects the next move still in the list, in list order
		 * @returns an empty move at the end of the list
		 */
		Move selectRemainingMove() {
			return takeMove(selectNextSilentMove());
		}

		/**
		 * Retrieves the current move
		 */
		Move getCurrentMove() {
			Move move;
			if (triedMovesAmount > 0) {
				move = triedMoves[triedMovesAmount - 1];
			}
			return move;
		}

		/**
		 * Select the next capture move
		 */

		uint32_t getTotalMoveAmount() const { return moveList.getTotalMoveAmount(); }

		/**
		 * @returns true, if the move selected last is the last one of the list. The list is
		 * complete from the start and every selected move is taken out of it.
		 */
		bool isLastMove() const { return triedMovesAmount >= moveList.getTotalMoveAmount(); }
		uint32_t getNonSilentMoveAmount() const { return moveList.getNonSilentMoveAmount(); }
		uint32_t getNumberOfMoveProvidedLast() const { return curMoveNo; }

		uint32_t getTriedMovesAmount() const {
			return triedMovesAmount;
		}
		Move getTriedMove(uint32_t moveNo) const {
			assert(moveNo < triedMovesAmount);
			return triedMoves[moveNo];
		}
		const Move* getTriedMoves() const {
			return triedMoves;
		}

	private:

		/**
		 * Takes the move with the given number out of the list and records it as tried
		 * @returns the move, an empty move for -1 or a number past the end of the list
		 */
		Move takeMove(int32_t moveNo) {
			Move move;
			if (moveNo != -1 && moveNo < (int32_t)moveList.getTotalMoveAmount()) {
				move = moveList[(uint32_t)moveNo];
				moveList[(uint32_t)moveNo].setEmpty();

				assert(triedMovesAmount < TRIED_MOVES_STORE_SIZE);
				triedMoves[triedMovesAmount] = move;
				triedMovesAmount++;
			}
			return move;
		}

		/**
		 * Calculate the weight (winning material in centi-pawn) of a capture move for more ordering
		 */
		value_t computeCaptureWeight(const MoveGenerator& board, Move move, Move previousMove) {
			value_t weight = board.getAbsolutePieceValue(move.getCapture());
			if (previousMove.isCapture() && (previousMove.getDestination() == move.getDestination())) {
				// order recaptures to the front
				// Tested 0.4.0-039: real piece values, every recapture ahead of every other
				// capture: -4.5 Elo, 10018 games
				weight += 10;
			}
			return weight;
		}

		/**
		 * Computes the weight of all captures
		 */
		void computeAllCaptureWeight(const MoveGenerator& board, Move previousMove) {
			for (uint32_t moveNo = 0; moveNo < moveList.getNonSilentMoveAmount(); moveNo++) {
				Move move = moveList[moveNo];
				if (move != Move::EMPTY_MOVE) {
					moveList.setWeight(moveNo, computeCaptureWeight(board, move, previousMove));
				}
			}
		}

		/**
		 * searches the next best capture move according his weight
		 */
		int32_t findNextBestCaptureMove() {
			int32_t bestMoveNo = -1;
			value_t maxWeight = -MAX_VALUE;
			value_t curWeight;
			Move move;
			for (uint32_t moveNo = curMoveNo; moveNo < moveList.getNonSilentMoveAmount(); moveNo++) {
				move = moveList[moveNo];
				if (move != Move::EMPTY_MOVE) {
					curWeight = moveList.getWeight(moveNo);
					if (curWeight > maxWeight) {
						maxWeight = curWeight;
						bestMoveNo = static_cast<int32_t>(moveNo);
					}
				}
			}
			return bestMoveNo;
		}

		/**
		 * Selects the next capture move, deferring the loosing captures according to SEE.
		 * A deferred capture is invisible to findNextBestCaptureMove from here on, so every
		 * move arriving in the loop still carries its original, non negative weight and no
		 * move is ever tested by SEE twice.
		 */
		int32_t selectNextCaptureMoveHandlingLoosingCaptures(const MoveGenerator& board) {
			int32_t moveNo;
			moveNo = findNextBestCaptureMove();
			while (moveNo != -1 && sEE.isLoosingCapture(board, moveList[moveNo])) {
				// Tested 0.4.0-082: the deferred captures ordered by their exact exchange value
				// instead of the order the move generator produced: about -4 Elo, 12721 games
				moveList.setWeight(moveNo, moveList.getWeight(moveNo) - CAPTURE_DEFERRAL_MALUS);
				moveNo = findNextBestCaptureMove();
			}
			if (moveNo != -1) {
				moveList.dragMoveToTheBack(curMoveNo, moveNo);
				moveNo = curMoveNo;
				curMoveNo++;
			}
			return moveNo;
		}

		/**
		 * @returns the number of the proposed move in the list, -1 if it is not (any more) there
		 */
		int16_t findProposedMove(Move move) {
			int16_t foundMoveNo = -1;
			if (!move.isEmpty()) {
				// uint32_t, not uint8_t: the generator produces up to MAX_MOVE_AMOUNT moves and
				// an index that wraps at 255 never reaches the end of the list.
				// No break on a hit: every move is generated once, so the first hit is the only
				// one and stopping there is free work saved - but it measured 0.5 % slower in
				// five interleaved pairs, consistently. The loop without an early exit is the
				// one the compiler can lay out well, so the scan runs to the end on purpose.
				for (uint32_t moveNo = 0; moveNo < moveList.getTotalMoveAmount(); moveNo++) {
					if (moveList[moveNo] == move) {
						foundMoveNo = int16_t(moveNo);
					}
				}
			}
			return foundMoveNo;
		}

		/**
		 * Select the next non-capture move
		 */
		int32_t selectNextSilentMove() {
			while (curMoveNo < moveList.getTotalMoveAmount() && moveList[curMoveNo].isEmpty())
			{
				curMoveNo++;
			}
			curMoveNo++;
			return static_cast<int32_t>(curMoveNo - 1);
		}

		static const uint32_t TRIED_MOVES_STORE_SIZE = 200;

		// Subtracted from the weight of a loosing capture. findNextBestCaptureMove starts its
		// maximum search at -MAX_VALUE, so the capture ends up below that floor and drops out
		// of the GOOD_CAPTURES stage altogether. selectNextSilentMove picks it up again later,
		// as it walks the list linearly and does not look at weights. This is a deferral to a
		// later stage, not a re-ordering inside the capture stage.
		static const value_t CAPTURE_DEFERRAL_MALUS = 50000;

		// Largest weight computeCaptureWeight can produce: value of a queen plus the recapture
		// bonus, with room to spare. Lowering the malus below this sum would silently bring the
		// loosing captures back into the capture stage, ahead of the killer moves.
		static const value_t MAX_CAPTURE_WEIGHT = 2000;
		static_assert(CAPTURE_DEFERRAL_MALUS > MAX_VALUE + MAX_CAPTURE_WEIGHT,
			"a deferred capture must fall below the -MAX_VALUE floor of findNextBestCaptureMove");

		uint32_t curMoveNo;
		MoveList moveList;
		Move triedMoves[TRIED_MOVES_STORE_SIZE];
		uint32_t triedMovesAmount;
		SEE sEE;

	};

}

#endif // #define __MOVEPROVIDER_H
