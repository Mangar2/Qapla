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

	class MoveProvider {
	public:
		enum select {
			SELECT_CAPTURE_KILLER, SELECT_NULLMOVE,
			SELECT_PV, INIT_CAPTURES, SELECT_GOOD_CAPTURES, SELECT_KILLER1, SELECT_KILLER2, INIT_NON_CAPTURES, SELECT_ALL,
			SELECT_ALL_TRIED_MOVES,
			SELECT_CAPTURES,
		};

		MoveProvider() : curMoveNo(0) {
			selectStage = SELECT_PV + 1;
			pvMove = Move::EMPTY_MOVE;
			hashMove = Move::EMPTY_MOVE;
			previousMove = Move::EMPTY_MOVE;
		};
		MoveProvider& operator=(const MoveProvider& moveProviderToCopy) {
			pvMove = moveProviderToCopy.pvMove;
			hashMove = moveProviderToCopy.hashMove;
			killerMove = moveProviderToCopy.killerMove;
		}
		void init() {
			hashMove = Move::EMPTY_MOVE;
		}

		/// KillerMove
		void setKillerMove(const MoveProvider& moveProvider) {
			killerMove = moveProvider.killerMove;
		}
		void setKillerMove(Move move) {
			killerMove.setKiller(move);
		}
		const KillerMove& getKillerMove() const {
			return killerMove;
		}

		void setPVMove(Move move) {
			pvMove = move;
		}

		void setHashMove(Move move) {
			hashMove = move;
		}

		void setPreviousMove(Move move) {
			previousMove = move;
		}

		void addNullmove() {
			selectStage = SELECT_NULLMOVE;
		}

		inline void computeMoves(MoveGenerator& board, Move previousPlyMove, ply_t remainingSearchDepth) {
			previousMove = previousPlyMove;
			board.genMovesOfMovingColor(moveList);
			selectStage = SELECT_PV;
			curMoveNo = 0;
			triedMovesAmount = 0;
			remainingDepth = remainingSearchDepth;
			if (pvMove == Move::EMPTY_MOVE && hashMove == Move::EMPTY_MOVE) {
				selectStage++;
			}
		}

		inline void computeCaptures(MoveGenerator& board, Move previousPlyMove) {
			previousMove = previousPlyMove;
			board.genNonSilentMovesOfMovingColor(moveList);
			computeAllCaptureWeight(board, false);
			curMoveNo = 0;
			triedMovesAmount = 0;
		}

		inline void computeEvades(MoveGenerator& board, Move previousPlyMove) {
			previousMove = previousPlyMove;
			board.genEvadesOfMovingColor(moveList);
			curMoveNo = 0;
			triedMovesAmount = 0;
		}

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


		Move selectNextMove(const MoveGenerator& board) {
			int32_t selectedMoveNo = -1;
			Move move = Move::EMPTY_MOVE;

			while (selectedMoveNo == -1 && move == Move::EMPTY_MOVE) {
				currentStage = selectStage;
				switch (selectStage) {
				case SELECT_NULLMOVE:
					selectStage++;
					move = Move::NULL_MOVE;
					break;
				case SELECT_PV:
					selectedMoveNo = selectProposedMove(pvMove == Move::EMPTY_MOVE ? hashMove : pvMove, moveList);
					pvMove = Move::EMPTY_MOVE;
					selectStage++;
					break;
				case SELECT_KILLER1:
					selectedMoveNo = selectProposedMove(killerMove[0], moveList);
					selectStage++;
					break;
				case SELECT_KILLER2:
					selectedMoveNo = selectProposedMove(killerMove[1], moveList);
					selectStage++;
					break;
				case INIT_CAPTURES:
					computeAllCaptureWeight(board, false);
					selectStage++;
					break;
				case SELECT_GOOD_CAPTURES:
					selectedMoveNo = selectNextCaptureMoveHandlingLoosingCaptures(board);
					break;
				case SELECT_CAPTURES:
					selectedMoveNo = selectNextCaptureMove(board);
					break;
				case INIT_NON_CAPTURES:
					sortNonCaptures();
					selectStage++;
					break;
				case  SELECT_ALL:
					selectedMoveNo = selectNextSilentMove(moveList);
					break;
				default: break;
				}
			}
			if (selectedMoveNo != -1 && selectedMoveNo < (int32_t)moveList.getTotalMoveAmount()) {
				move = moveList[(uint32_t)selectedMoveNo];
				moveList[(uint32_t)selectedMoveNo] = Move::EMPTY_MOVE;

				assert(triedMovesAmount < TRIED_MOVES_STORE_SIZE);
				triedMoves[triedMovesAmount] = move;
				triedMovesAmount++;

			}
			return move;
		}

		Move getCurrentMove() {
			Move move = Move::EMPTY_MOVE;
			if (triedMovesAmount > 0) {
				move = triedMoves[triedMovesAmount - 1];
			}
			return move;
		}

		Move selectNextCapture(const MoveGenerator& board) {
			int16_t selectedMoveNo = -1;
			Move move = Move::EMPTY_MOVE;
			selectedMoveNo = selectNextCaptureMove(board);

			if (selectedMoveNo != -1) {
				move = moveList[selectedMoveNo];
				moveList[selectedMoveNo] = Move::EMPTY_MOVE;
			}

			return move;
		}

		uint32_t getTotalMoveAmount() const { return moveList.getTotalMoveAmount(); }
		uint32_t getNonSilentMoveAmount() const { return moveList.getNonSilentMoveAmount(); }
		uint32_t getNumberOfMoveProvidedLast() const { return curMoveNo; }
		uint32_t getSelectTypeOfLastProvidedMove() const { return currentStage; }
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

		value_t computeCaptureWeight(const MoveGenerator& board, Move move, bool underWeightLoosingCaptures) {
			value_t weight = board.getAbsolutePieceValue(move.getCapture());
			if (previousMove.isCapture() && (previousMove.getDestination() == move.getDestination())) {
				// order recaptures to the front
				weight += 10;
			}
			return weight;
		}

		void computeAllCaptureWeight(const MoveGenerator& board, bool underWeightLoosingCaptures) {
			for (uint32_t moveNo = 0; moveNo < moveList.getNonSilentMoveAmount(); moveNo++) {
				Move move = moveList[moveNo];
				if (move != Move::EMPTY_MOVE) {
					moveList.setWeight(moveNo, computeCaptureWeight(board, move, underWeightLoosingCaptures));
				}
			}
		}

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

		int16_t selectNextCaptureMoveHandlingLoosingCaptures(const MoveGenerator& board) {
			int16_t moveNo;
			moveNo = findNextBestCaptureMove(board);
			while (moveNo != -1 && moveList.getWeight(moveNo) >= 0 && sEE.isLoosingCapture(board, moveList[moveNo])) {
				moveList.setWeight(moveNo, moveList.getWeight(moveNo) - LOOSING_CAPTURE_MALUS);
				moveNo = findNextBestCaptureMove(board);
			}
			if (moveNo == -1) {
				selectStage++;
			}
			else {
				moveList.dragMoveToTheBack(curMoveNo, moveNo);
				moveNo = curMoveNo;
				curMoveNo++;
			}
			return moveNo;
		}

		int16_t selectNextCaptureMove(const MoveGenerator& board) {
			int16_t bestMoveNo = findNextBestCaptureMove(board);
			if (bestMoveNo == -1) {
				selectStage++;
			}
			else {
				moveList.dragMoveToTheBack(curMoveNo, bestMoveNo);
				bestMoveNo = curMoveNo;
				curMoveNo++;
			}
			return bestMoveNo;
		}

		int16_t selectProposedMove(Move move, MoveList& moveList) {
			int16_t foundMoveNo = -1;
			if (move != Move::EMPTY_MOVE) {
				for (uint8_t moveNo = 0; moveNo < moveList.getTotalMoveAmount(); moveNo++) {

					if (moveList[moveNo] == move) {
						foundMoveNo = moveNo;
					}

				}
			}
			return foundMoveNo;
		}

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

		uint32_t selectStage;
		uint32_t currentStage;
		uint32_t curMoveNo;
		ply_t remainingDepth;
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
