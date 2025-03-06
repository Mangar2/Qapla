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
#include <map>
#include "../basics/types.h"
#include "../movegenerator/movegenerator.h"
#include "evalresults.h"

using namespace QaplaMoveGenerator;

namespace ChessEval {

	class Eval {

	public:

		/**
		 * Checks, that eval is symmetically identical
		 */
		static void assertSymetry(MoveGenerator& board, value_t evalResult) {
			MoveGenerator symBoard;
			symBoard.setToSymetricBoard(board);
			value_t symEvalResult = eval(symBoard, -MAX_VALUE);

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
		static value_t eval(MoveGenerator& board, value_t ply = 0, value_t alpha = -MAX_VALUE) {
#ifdef USE_STOCKFISH_EVAL
			return Stockfish::Engine::evaluate();
#else
			static const value_t tempo = 8;
			EvalResults evalResults;
			value_t positionValue = lazyEval<false>(board, evalResults, ply);
			positionValue = board.isWhiteToMove() ? positionValue : -positionValue;
			if (abs(positionValue) < WINNING_BONUS) {
				positionValue += tempo;
			}
			// If a value == 0, the position will not be stored in hash tables
			// Value == 0 indicates a forced draw situation like repetetive moves 
			// or move count without pawn move or capture == 50
			if (positionValue == 0) positionValue = 1;
			return positionValue;
#endif
		}

		/**
		 * Prints the evaluation results
		 */
		static void printEval(MoveGenerator& board);

		/**
		 * Gets a map of relevant factors to examine eval
		 */
		template <Piece COLOR>
		map<string, value_t> getEvalFactors(MoveGenerator& board);

		map<string, value_t> getEvalFactors(MoveGenerator& board);
	
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
		 * Calculates the midgame factor in percent
		 */
		static value_t computeMidgameV2InPercent(MoveGenerator& board) {
			value_t pieces = board.getStaticPiecesValue<WHITE>() + board.getStaticPiecesValue<BLACK>();
			if (pieces > 64) {
				pieces = 64;
			}
			return midgameV2InPercent[pieces];
		}

		/**
		 * Calculates an evaluation for the current board position
		 */
		template <bool PRINT>
		static value_t lazyEval(MoveGenerator& board, EvalResults& evalResults, value_t ply);

	

		/**
		 * Initializes some fields of eval results
		 */
		static void initEvalResults(MoveGenerator& board, EvalResults& evalResults);

		/**
		 * Determines the game phase based on a static piece value
		 * queens = 9, rooks = 5, bishop/knights = 3, pawns +1 if >= 3.
		 */
		static constexpr array<value_t, 65> midgameV2InPercent = {
			0, 0,  0,  0,  0,  0,  0,  0,  0,
			0,  0,  0,  3,  6,  9,  12,  12,
			12, 16, 20, 24, 28, 32, 36, 40,
			44, 47, 50, 53, 56, 60, 64, 66,
			68, 70, 72, 74, 76, 78, 80, 82,
			84, 86, 88, 90, 92, 94, 96, 98,
			100, 100, 100, 100, 100, 100, 100, 100,
			100, 100, 100, 100, 100, 100, 100, 100 };

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

		

	};
}

#endif