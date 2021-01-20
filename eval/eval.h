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

#include <vector>
#include "../basics/types.h"
#include "../movegenerator/movegenerator.h"
#include "evalresults.h"
#include "evalendgame.h"
#include "evalpawn.h"
#include "evalmobility.h"
#include "kingattack.h"

namespace ChessEval {

	class Eval {

	public:

		static void assertSymetry(MoveGenerator& board, value_t evalResult) {
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
		static value_t evaluateBoardPosition(MoveGenerator& board, value_t alpha = -MAX_VALUE) {
			EvalResults evalResults;
			return lazyEval(board, evalResults);
		}

		/**
		 * Prints the evaluation results
		 */
		static void printEval(MoveGenerator& board) {
			EvalResults evalResults;
			value_t evalValue = lazyEval(board, evalResults);
			value_t endGameResult;

			EvalPawn evalPawn;
			board.print();

			evalPawn.print(board, evalResults);

			printf("Midgame factor      : %ld%%\n", evalResults.midgameInPercent);
			printf("Marerial            : %ld\n", evalResults.materialValue);
			board.getMaterialValue();

			endGameResult = EvalEndgame::print(board, evalValue);
			endGameResult = cutValueOnDrawPositions(board, endGameResult);

			if (endGameResult != evalValue) {
				evalValue = endGameResult;
			}
			else {
				if (evalResults.midgameInPercent > 0) {
					EvalMobility::print(board, evalResults);
					KingAttack::print(evalResults);
				}
			}
			printf("Total               : %ld\n", evalValue);
		}

		/**
		 * Gets a map of relevant factors to examine eval
		 */
		template <Piece COLOR>
		map<string, value_t> getEvalFactors(MoveGenerator& board) {
			EvalResults evalResults;
			lazyEval(board, evalResults);
			map<string, value_t> result;
			auto factors = KingAttack::factors<COLOR>(board, evalResults);
			result.insert(factors.begin(), factors.end());
			return result;
		}


	private:

		/**
		 * Calculates the midgame factor in percent
		 */
		static value_t computeMidgameInPercent(MoveGenerator& board) {
			value_t pieces = board.getStaticPiecesValue<WHITE>() + board.getStaticPiecesValue<BLACK>();
			if (pieces > 64) {
				pieces = 64;
			}
			return midgameInPercent[pieces];
		}

		/**
		 * Calculates an evaluation for the current board position
		*/
		static value_t lazyEval(MoveGenerator& board, EvalResults& evalResults) {

			value_t result;
			value_t endGameResult;
			EvalPawn evalPawn;

			evalResults.materialValue = board.getMaterialValue();
			evalResults.midgameInPercent = computeMidgameInPercent(board);
			result = evalResults.materialValue;
			result += evalPawn.eval(board, evalResults);
			endGameResult = EvalEndgame::eval(board, result);
			endGameResult = cutValueOnDrawPositions(board, endGameResult);

			if (endGameResult != result) {
				result = endGameResult;
			}
			else {
				// Do not change ordering of the following calls. King attack needs result from Mobility
				result += EvalMobility::eval(board, evalResults);
				if (evalResults.midgameInPercent > 0) {
					result += KingAttack::eval(board, evalResults);
				}
			}
			return result;
		}



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
		 * Determines the game phase based on a static piece value 
		 * queens = 9, rooks = 5, bishop/knights = 3, pawns +1 if >= 3.
		 */
		static constexpr array<value_t, 65> midgameInPercent = {
			0,  0,  0,  0,  0,  0,  4,  8,
			12, 16, 20, 24, 28, 32, 36, 40,
			44, 47, 50, 53, 56, 60, 64, 66,
			68, 70, 72, 74, 76, 78, 80, 82,
			84, 86, 88, 90, 92, 94, 96, 98,
			100, 100, 100, 100, 100, 100, 100, 100,
			100, 100, 100, 100, 100, 100, 100, 100,
			100, 100, 100, 100, 100, 100, 100, 100 };


		// BitBase kpk;

	};
}

#endif