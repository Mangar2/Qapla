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
 * Implements a move provider for search providing moves in the right order
 */

#ifndef __MOVEPROVIDER_H
#define __MOVEPROVIDER_H

#include "../basics/move.h"
#include "../basics/movelist.h"
#include "searchdef.h"
#include "../eval/eval.h"
//#include "EvalBoard.h"
#include "killermove.h"
#include "see.h"
#include "SearchParameter.h"
#include "../movegenerator/movegenerator.h"

using namespace ChessBasics;
using namespace ChessMoveGenerator;

namespace ChessSearch {

	enum class MoveType {
		CAPTURE_KILLER, 
		PV, WEIGHT_CAPTURES, GOOD_CAPTURES, KILLER1, KILLER2, SORT_MOVES, ALL,
		CAPTURES_ONLY,
	};

	constexpr MoveType operator+(MoveType a, int b) { return MoveType(int(a) + b); }
	inline MoveType operator++(MoveType& a) { return a = MoveType(a + 1); }

	class MoveProvider {
	public:


		MoveProvider() : curMoveNo(0) {
			selectStage = MoveType::PV + 1;
			pvMove;
			hashMove;
			previousMove;
		};

		/**
		 * Returns true, if we are in an all search stage
		 */
		bool isAllSearch() {
			return selectStage == MoveType::ALL;
		}

		/**
		 * Initializes the move provider
		 */
		void init() {
			hashMove.setEmpty();
		}

		/**
		 * Sets a killer-move
		 */
		void setKillerMove(const MoveProvider& moveProvider) {
			killerMove = moveProvider.killerMove;
		}

		void setKillerMove(Move move) {
			killerMove.setKiller(move);
		}
		/**
		 * Gets the killermoves
		 */
		const KillerMove& getKillerMove() const {
			return killerMove;
		}

		/**
		 * Sets the PV move
		 */
		void setPVMove(Move move) {
			pvMove = move;
		}

		/**
		 * Sets the best move coming from the tt
		 */
		void setTTMove(Move move) {
			hashMove = move;
		}

		/**
		 * Sets the previously played move
		 */
		void setPreviousMove(Move move) {
			previousMove = move;
		}

		/**
		 * Initializes the move provider to provide all moves in a sorted order
		 */
		inline void computeMoves(MoveGenerator& board, Move previousPlyMove) {
			previousMove = previousPlyMove;
			board.genMovesOfMovingColor(moveList);
			selectStage = MoveType::PV;
			curMoveNo = 0;
			triedMovesAmount = 0;
			if (pvMove.isEmpty() && hashMove.isEmpty()) {
				++selectStage;
			}
		}

		/**
		 * Initializes the move provider to provide captures
		 */
		inline void computeCaptures(MoveGenerator& board, Move previousPlyMove) {
			previousMove = previousPlyMove;
			board.genNonSilentMovesOfMovingColor(moveList);
			computeAllCaptureWeight(board, false);
			curMoveNo = 0;
			triedMovesAmount = 0;
		}

		/**
		 * Initializes the move provider to provide evades
		 */
		inline void computeEvades(MoveGenerator& board, Move previousPlyMove) {
			previousMove = previousPlyMove;
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
		 * Selects the next move to play
		 */
		Move selectNextMove(const MoveGenerator& board) {
			int32_t selectedMoveNo = -1;
			Move move;

			while (selectedMoveNo == -1 && move.isEmpty()) {
				currentStage = selectStage;
				switch (selectStage) {
				case MoveType::PV:
					selectedMoveNo = selectProposedMove(pvMove.isEmpty() ? hashMove : pvMove, moveList);
					pvMove.setEmpty();
					++selectStage;
					break;
				case MoveType::KILLER1:
					selectedMoveNo = selectProposedMove(killerMove[0], moveList);
					++selectStage;
					break;
				case MoveType::KILLER2:
					selectedMoveNo = selectProposedMove(killerMove[1], moveList);
					++selectStage;
					break;
				case MoveType::WEIGHT_CAPTURES:
					computeAllCaptureWeight(board, false);
					++selectStage;
					break;
				case MoveType::GOOD_CAPTURES:
					selectedMoveNo = selectNextCaptureMoveHandlingLoosingCaptures(board);
					break;
				case MoveType::CAPTURES_ONLY:
					selectedMoveNo = selectNextCaptureMove(board);
					break;
				case MoveType::SORT_MOVES:
					sortNonCaptures();
					++selectStage;
					break;
				case  MoveType::ALL:
					selectedMoveNo = selectNextSilentMove(moveList);
					break;
				default: break;
				}
			}
			if (selectedMoveNo != -1 && selectedMoveNo < (int32_t)moveList.getTotalMoveAmount()) {
				move = moveList[(uint32_t)selectedMoveNo];
				moveList[(uint32_t)selectedMoveNo].setEmpty();

				assert(triedMovesAmount < TRIED_MOVES_STORE_SIZE);
				triedMoves[triedMovesAmount] = move;
				triedMovesAmount++;

			}
			return move;
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
		Move selectNextCapture(const MoveGenerator& board) {
			int16_t selectedMoveNo = -1;
			Move move;
			selectedMoveNo = selectNextCaptureMove(board);

			if (selectedMoveNo != -1) {
				move = moveList[selectedMoveNo];
				moveList[selectedMoveNo].setEmpty();
			}

			return move;
		}

		uint32_t getTotalMoveAmount() const { return moveList.getTotalMoveAmount(); }
		uint32_t getNonSilentMoveAmount() const { return moveList.getNonSilentMoveAmount(); }
		uint32_t getNumberOfMoveProvidedLast() const { return curMoveNo; }
		MoveType getSelectTypeOfLastProvidedMove() const { return currentStage; }
		Move getTTMove() const {
			return hashMove;
		}

		uint32_t getTriedMovesAmount() const {
			return triedMovesAmount;
		}
		Move getTriedMove(uint32_t moveNo) const {
			assert(moveNo < triedMovesAmount);
			return triedMoves[moveNo];
		}

	private:

		/**
		 * Calculate the weight (winning material in centi-pawn) of a capture move for more ordering
		 */
		value_t computeCaptureWeight(const MoveGenerator& board, Move move, bool underWeightLoosingCaptures) {
			value_t weight = board.getAbsolutePieceValue(move.getCapture());
			if (previousMove.isCapture() && (previousMove.getDestination() == move.getDestination())) {
				// order recaptures to the front
				weight += 10;
			}
			return weight;
		}

		/**
		 * Computes the weight of all captures
		 */
		void computeAllCaptureWeight(const MoveGenerator& board, bool underWeightLoosingCaptures) {
			for (uint32_t moveNo = 0; moveNo < moveList.getNonSilentMoveAmount(); moveNo++) {
				Move move = moveList[moveNo];
				if (move != Move::EMPTY_MOVE) {
					moveList.setWeight(moveNo, computeCaptureWeight(board, move, underWeightLoosingCaptures));
				}
			}
		}

		/**
		 * searches the next best capture move according his weight
		 */
		int16_t findNextBestCaptureMove(const MoveGenerator& board) {
			int16_t bestMoveNo = -1;
			value_t maxWeight = -MAX_VALUE;
			value_t curWeight;
			Move move;
			for (uint8_t moveNo = curMoveNo; moveNo < moveList.getNonSilentMoveAmount(); moveNo++) {
				move = moveList[moveNo];
				if (move != Move::EMPTY_MOVE) {
					curWeight = moveList.getWeight(moveNo);
					if (curWeight > maxWeight) {
						maxWeight = curWeight;
						bestMoveNo = moveNo;
					}
				}
			}
			return bestMoveNo;
		}

		/**
		 * Select the next capture move scipping loosing captures according SEE
		 */
		int16_t selectNextCaptureMoveHandlingLoosingCaptures(const MoveGenerator& board) {
			int16_t moveNo;
			moveNo = findNextBestCaptureMove(board);
			while (moveNo != -1 && moveList.getWeight(moveNo) >= 0 && sEE.isLoosingCapture(board, moveList[moveNo])) {
				moveList.setWeight(moveNo, moveList.getWeight(moveNo) - LOOSING_CAPTURE_MALUS);
				moveNo = findNextBestCaptureMove(board);
			}
			if (moveNo == -1) {
				++selectStage;
			}
			else {
				moveList.dragMoveToTheBack(curMoveNo, moveNo);
				moveNo = curMoveNo;
				curMoveNo++;
			}
			return moveNo;
		}

		/**
		 * Gets the index of the next capture move
		 */
		int16_t selectNextCaptureMove(const MoveGenerator& board) {
			int16_t bestMoveNo = findNextBestCaptureMove(board);
			if (bestMoveNo == -1) {
				++selectStage;
			}
			else {
				moveList.dragMoveToTheBack(curMoveNo, bestMoveNo);
				bestMoveNo = curMoveNo;
				curMoveNo++;
			}
			return bestMoveNo;
		}

		/**
		 * searches for a proposed move and if available - selet it
		 */
		int16_t selectProposedMove(Move move, MoveList& moveList) {
			int16_t foundMoveNo = -1;
			if (!move.isEmpty()) {
				for (uint8_t moveNo = 0; moveNo < moveList.getTotalMoveAmount(); moveNo++) {
					if (moveList[moveNo] == move) {
						foundMoveNo = moveNo;
					}

				}
			}
			return foundMoveNo;
		}

		/**
		 * Select the next non-capture move
		 */
		int16_t selectNextSilentMove(MoveList& moveList) {
			while (curMoveNo < moveList.getTotalMoveAmount() && moveList[curMoveNo].isEmpty())
			{
				curMoveNo++;
			}
			curMoveNo++;
			return curMoveNo - 1;
		}

		void sortNonCaptures() {
			/*
			for (uint8_t moveNo = moveList.getNonSilentMoveAmount(); moveNo < moveList.getTotalMoveAmount(); moveNo++) {
				moveList.setWeight(moveNo, HistoryTable::getWeight(moveList.getMove(moveNo)));
			}
			moveList.sortFirstNonCaptureMoves(SearchParameter::AMOUNT_OF_SORTED_NON_CAPTURE_MOVES);
			*/
		}

		static const uint32_t TRIED_MOVES_STORE_SIZE = 200;
		static const value_t LOOSING_CAPTURE_MALUS = 50000;

		MoveType selectStage;
		MoveType currentStage;
		uint32_t curMoveNo;
		Move pvMove;
		Move hashMove;
		Move previousMove;
		KillerMove killerMove;
		MoveList moveList;
		Move triedMoves[TRIED_MOVES_STORE_SIZE];
		uint32_t triedMovesAmount;
		SEE sEE;

	};

}

#endif // #define __MOVEPROVIDER_H
