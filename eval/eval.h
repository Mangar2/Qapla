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

struct RookValues {
#ifdef _T0 
#endif
#ifdef _TEST1 
static constexpr value_t pawnIndexFactor[8] = { 150, 130, 130, 120, 100, 100, 100, 100 };
#endif
#ifdef _TEST2 
static constexpr value_t pawnIndexFactor[8] = { 100, 100, 100, 100, 100, 80, 80, 50 };
#endif
#ifdef _T3 
static constexpr value_t pawnIndexFactor[8] = { 150, 130, 130, 120, 100, 80, 80, 50 };
#endif
#ifdef _T4 
static constexpr value_t pawnIndexFactor[8] = { 130, 115, 115, 110, 100, 90, 90, 70 };
#endif
#ifdef _T5
static constexpr value_t pawnIndexFactor[8] = { 180, 150, 150, 130, 100, 70, 70, 40 };
#endif
};

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
			value_t evalValue;
			value_t endGameResult;

			board.print();

			evalResults.materialValue = board.getMaterialValue();
			evalResults.midgameInPercent = computeMidgameInPercent(board);
			evalValue = evalResults.materialValue;
			evalValue += EvalPawn::eval(board, evalResults);
			endGameResult = EvalEndgame::eval(board, evalValue);

			printf("Midgame factor      : %ld%%\n", evalResults.midgameInPercent);
			printf("Marerial            : %ld\n", evalResults.materialValue);
			EvalPawn::print(board, evalResults);
			board.getMaterialValue();
			endGameResult = EvalEndgame::print(board, evalValue);

			if (endGameResult != evalValue) {
				evalValue = endGameResult;
			}
			else {
				EvalMobility::print(board, evalResults);
				if (evalResults.midgameInPercent > 0) {
					KingAttack::print(board, evalResults);
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
			auto mobilityFactors = EvalMobility::factors<COLOR>(board, evalResults);
			result.insert(mobilityFactors.begin(), mobilityFactors.end());
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

			evalResults.materialValue = board.getMaterialValue();
			evalResults.midgameInPercent = computeMidgameInPercent(board);
			result = evalResults.materialValue;
			result += EvalPawn::eval(board, evalResults);
			endGameResult = EvalEndgame::eval(board, result);

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
			100, 100, 100, 100, 100, 100, 100, 100, 100 };


		// BitBase kpk;

	};
}

#endif