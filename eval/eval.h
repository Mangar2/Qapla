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
 * Implements chess board evaluation. Returns +100, if white is one pawn up
 * Used to evaluate in quiescense search
 */

#pragma once

 // Idee 1: Evaluierung in der Ruhesuche ohne positionelle Details
 // Idee 2: Zugsortierung nach lookup Tabelle aus reduziertem Board-Hash
 // Idee 3: Beweglichkeit einer Figur aus der Suche evaluieren. Speichern, wie oft eine Figur von einem Startpunkt erfolgreich bewegt wurde.

#include <vector>
#include "../basics/types.h"
#include "../movegenerator/movegenerator.h"
#include "evalresults.h"
#include "pawntt.h"
#ifdef QAPLA_USE_NNUE
#include "../src/nnue/nnue-accumulator.h"
#endif

using namespace QaplaMoveGenerator;

namespace ChessEval {

	class Eval {

	public:

		/**
		 * Checks, that eval is symmetically identical
		 */
		static void assertSymetry(MoveGenerator& position, value_t evalResult, PawnTT* ttPtr) {
			MoveGenerator symPosition;
			symPosition.setToSymetricBoard(position);
			value_t symEvalResult = eval(symPosition, ttPtr, -MAX_VALUE);

			if (symEvalResult != evalResult) {
				printEval(position);
				printEval(symPosition);
				symEvalResult = evalResult;
				assert(false);
			}
		}

		/**
		 * Calculates an evaluation for the current board position
		 */
		static value_t eval(MoveGenerator& position, PawnTT* pawnttPtr = nullptr, value_t ply = 0,
			[[maybe_unused]] value_t alpha = -MAX_VALUE) {
#ifdef USE_STOCKFISH_EVAL
			return Stockfish::Engine::evaluate();
#else
#ifdef QAPLA_USE_NNUE
			if (QaplaNnue::network() != nullptr) {
				// The net is a value of the pieces on the board like the one the hand
				// written evaluation computes, and it gets the same corrections on top:
				// without them a won endgame is worth a pawn to it and a dead drawn one
				// is worth trading into.
				const value_t netValue = QaplaNnue::accumulators.evaluate(position);
				const value_t whiteValue = position.isWhiteToMove() ? netValue : -netValue;
				const value_t corrected = correctValue<false>(position, whiteValue, ply, false);
				return position.isWhiteToMove() ? corrected : -corrected;
			}
			// Built with the net and playing without one. Falling through to the hand
			// written evaluation keeps the engine usable, and saying so keeps the
			// difference from being mistaken for a result.
			QaplaNnue::reportMissingNetwork();
#endif
			value_t positionValue = lazyEval<false>(position, ply, pawnttPtr);
			return position.isWhiteToMove() ? positionValue : -positionValue;
#endif
		}

		/**
		 * Prints the evaluation results
		 */
		static void printEval(MoveGenerator& position);

		/**
		 * Fetches the list of indices calculated by the evaluation
		 */
		IndexVector computeIndexVector(MoveGenerator& position);
		/**
		 * Fetches the lookup map to get the value of an index value
		 */
		IndexLookupMap computeIndexLookupMap(MoveGenerator& position);


	private:

		static void printEvalBoard(const std::vector<PieceInfo>& details, value_t midgameInPercent);
		static const PieceInfo* getPiece(const std::vector<PieceInfo>& details, Square square);
		static void printPieceRow(int row, const PieceInfo& pieceInfo, value_t midgameInPercent);

		/**
		 * Calculates the midgame factor in percent
		 */
		static value_t computeMidgameInPercent(MoveGenerator& position) {
			value_t pieces = position.getStaticPiecesValue<WHITE>() + position.getStaticPiecesValue<BLACK>();
			if (pieces > 64) {
				pieces = 64;
			}
			return midgameInPercent[pieces];
		}

		/**
		 * Calculates the midgame factor in percent
		 */
		static value_t computeMidgameV2InPercent(MoveGenerator& position) {
			value_t pieces = position.getStaticPiecesValue<WHITE>() + position.getStaticPiecesValue<BLACK>();
			if (pieces > 64) {
				pieces = 64;
			}
			return midgameV2InPercent[pieces];
		}

		/**
		 * Calculates an evaluation for the current board position
		 */
		template <bool PRINT>
		static value_t lazyEval(MoveGenerator& position, value_t ply, PawnTT* pawnttPtr = nullptr);

		/**
		 * Turns a value of the pieces on the board into the final one. The endgame
		 * knowledge corrects it and may replace it outright - a won endgame with the
		 * winning bonus, a known draw with nothing - and what survives that is given
		 * the tempo bonus, damped towards the fifty move rule and scaled for bishops
		 * on opposite colours.
		 *
		 * It works on a value seen from white, as lazyEval does, and it is a
		 * correction of a value rather than an evaluation of its own - which is what
		 * lets the net use the same knowledge.
		 *
		 * @param addTempo false for the net: it is told which side is to move by the
		 *        order of its two accumulators and has the bonus in its value already
		 */
		template <bool PRINT = false>
		static value_t correctValue(MoveGenerator& position, value_t result, value_t ply,
			bool addTempo = true);

		/**
		 * Fetches details for the evaluation
		 */
		static std::vector<PieceInfo> fetchDetails(MoveGenerator& position);


		/**
		 * Initializes some fields of eval results
		 */
		static void initEvalResults(MoveGenerator& position, EvalResults& evalResults);

		/**
		 * Determines the game phase based on a static piece value
		 * queens = 9, rooks = 5, bishop/knights = 3, pawns +1 if >= 3.
		 * Size is enlarged to handle extreme cases like 3 queens + 3 rooks + ...
		 */
		static constexpr std::array<value_t, 123> midgameV2InPercent = [] {
			std::array<value_t, 123> arr{};
			arr.fill(100); // alles auf 100 setzen
			
			constexpr std::array<value_t, 65> base = {
			0, 0,  0,  0,  0,  0,  0,  0,  0,
			0,  0,  0,  3,  6,  9,  12,  12,
			12, 16, 20, 24, 28, 32, 36, 40,
			44, 47, 50, 53, 56, 60, 64, 66,
			68, 70, 72, 74, 76, 78, 80, 82,
			84, 86, 88, 90, 92, 94, 96, 98,
			100, 100, 100, 100, 100, 100, 100, 100,
			100, 100, 100, 100, 100, 100, 100, 100 };
			
			for (uint32_t i = 0; i < base.size(); ++i) {
				arr[i] = base[i];
			}
			return arr;
			}();

		/** 
		 * Determines the game phase based on a static piece value 
		 * queens = 9, rooks = 5, bishop/knights = 3, pawns +1 if >= 3.
		 * Size is enlarged to handle extreme cases like 3 queens + 3 rooks + ...
		 */
		static constexpr std::array<value_t, 123> midgameInPercent = [] {
			std::array<value_t, 123> arr{};
			arr.fill(100); 

			constexpr std::array<value_t, 65> base = {
			0,  0,  0,  0,  0,  0,  4,  8,
			12, 16, 20, 24, 28, 32, 36, 40,
			44, 47, 50, 53, 56, 60, 64, 66,
			68, 70, 72, 74, 76, 78, 80, 82,
			84, 86, 88, 90, 92, 94, 96, 98,
			100, 100, 100, 100, 100, 100, 100, 100,
			100, 100, 100, 100, 100, 100, 100, 100,
			100, 100, 100, 100, 100, 100, 100, 100, 100 };

			for (uint32_t i = 0; i < base.size(); ++i) {
				arr[i] = base[i];
			}
			return arr;
			}();


		static const value_t tempo = 8;

	};
}

