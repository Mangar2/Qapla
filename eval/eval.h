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
 * Implements chess board evaluation. Returns +100, if white is one pawn up
 * Used to evaluate in quiescense search 
 */

#ifndef __EVAL_H
#define __EVAL_H

// Idee 1: Evaluierung in der Ruhesuche ohne positionelle Details
// Idee 2: Zugsortierung nach lookup Tabelle aus reduziertem Board-Hash
// Idee 3: Beweglichkeit einer Figur aus der Suche evaluieren. Speichern, wie oft eine Figur von einem Startpunkt erfolgreich bewegt wurde.

#include "../basics/types.h"
#include "../movegenerator/movegenerator.h"
#include "evalendgame.h"
#include "evalpawn.h"
#include "evalmobility.h"

namespace ChessEval {

	class Eval {

	public:

		static void initStatics() {
			EvalEndgame::initStatics();
		}

		void assertSymetry(MoveGenerator& board, value_t evalResult) {
			MoveGenerator symBoard;
			board.setToSymetricBoard(symBoard);
			value_t symEvalResult = evaluateBoardPosition(symBoard, -MAX_VALUE);
			if (symEvalResult != 1 || evalResult != 1) {
				symEvalResult = -symEvalResult;
			}
			if (symEvalResult != evalResult) {
				printEval(board);
				printEval(symBoard);
				symEvalResult = evalResult;
				assert(false);
			}
		}

		/**
		 * Calculates an evaluation for the current board position
		 */
		value_t evaluateBoardPosition(MoveGenerator& board, value_t alpha = -MAX_VALUE) {

			value_t evalResult;
			value_t endGameResult;
			EvalPawn evalPawn;

			evalResult = board.getMaterialValue();
			evalResult += evalPawn.eval(board);

			endGameResult = EvalEndgame::eval(board, evalResult);
			endGameResult = cutValueOnDrawPositions(board, endGameResult);

			if (endGameResult != evalResult) {
				evalResult = endGameResult;
			}
			else {
				EvalMobility evalMobility(board);
				evalResult += evalMobility.eval(board);
			}

			// Zero is dedicated to repetetive draw values.
			if (evalResult == 0) {
				evalResult = DRAW_VALUE;
			}

			return evalResult;
		}

		/**
		 * Prints the evaluation results
		 */
		void printEval(MoveGenerator& board) {
			value_t evalValue = evaluateBoardPosition(board);
			value_t endGameResult;

			value_t valueSum = 0;
			EvalPawn evalPawn;
			EvalMobility evalMobility(board);
			board.print();

			valueSum += evalPawn.print(board);

			printf("Marerial            : %ld\n", board.getMaterialValue());
			valueSum += board.getMaterialValue();

			endGameResult = EvalEndgame::print(board, valueSum);
			endGameResult = cutValueOnDrawPositions(board, endGameResult);

			if (endGameResult != valueSum) {
				valueSum = endGameResult;
			}
			else {
				valueSum += evalMobility.print(board);
			}
			if (evalValue != valueSum) {
				printf("Error, false value sum     : %ld\n", valueSum);
			}
			printf("Total               : %ld\n", evalValue);
		}


	private:

		/**
		 * Ensure that a side never gets more than 0 points if it has not enough material to win
		 */
		static value_t cutValueOnDrawPositions(MoveGenerator& board, value_t currentValue) {
			if (currentValue > 0 && !board.hasEnoughMaterialToMate<WHITE>()) {
				currentValue = 0;
			}
			if (currentValue < 0 && !board.hasEnoughMaterialToMate<BLACK>()) {
				currentValue = 0;
			}
			return currentValue;
		}


		/**
		 * ??
		 */
		struct ValueStatistic {
			ValueStatistic() : minValue(0), maxValue(0) {}
			void newValue(value_t value) {
				if (value < minValue) { minValue = value; }
				if (value > maxValue) { maxValue = value; }
			}
			value_t minValue;
			value_t maxValue;
		};

		ValueStatistic mobilityStatistic;
		// BitBase kpk;

	};
}

#endif