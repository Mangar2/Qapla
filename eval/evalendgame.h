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
 * Implements chess board evaluation for end games. 
 * Returns +100, if white is one pawn up
 */

#pragma once

#include <string>
#include <vector>
#include "../basics/move.h"
#include "../basics/hashed-lookup.h"
#include "../movegenerator/movegenerator.h"
#include "piece-signature-lookup.h"

using namespace std;
using namespace QaplaMoveGenerator;

namespace ChessEval {

	

	class EvalEndgame {

	public:

		typedef value_t evalFunction_t(MoveGenerator& board, value_t currentValue);

		struct EvalEntry {
			bool isFunction;
			union {
				evalFunction_t* fn;
				int32_t value;
			};
			EvalEntry() : isFunction(false), value(0) {}
			EvalEntry(evalFunction_t* f) : isFunction(true), fn(f) {}
			EvalEntry(int32_t v) : isFunction(false), value(v) {}
		};

		/**
		 * Attempts to evaluate the current position using specialized endgame evaluation functions.
		 *
		 * If no such function is registered for the current piece signature, or if the function
		 * determines that the position is not a recognizable endgame pattern, the function returns
		 * `currentValue`. This signals the caller to proceed with the standard evaluation logic.
		 *
		 * @param board         The current board state.
		 * @param currentValue  The evaluation value from earlier stages.
		 * @return              The evaluated score, either from endgame logic or the unchanged currentValue.
		 */
		static value_t eval(MoveGenerator& board, value_t currentValue) {
			auto result = pieceSignatureHash.lookup(board.getPiecesSignature());
			if (!result.has_value()) return currentValue;

			const auto& entry = result.value();
			return entry.isFunction
				? entry.fn(board, currentValue)
				: currentValue + entry.value;
		}

		/**
		 * Registers the use of a bitbase
		 */
		static void registerBitbase(string pieces) {
			registerEntry(pieces, EvalEntry(getFromBitbase), false);
			registerEntry(pieces, EvalEntry(getFromBitbase), true);
		}

	private:


		/**
		 * Checks, if a square is set in a bitmask handling color symmetry
		 * @returns true, if the square is set in the bitmask
		 */
		template <Piece COLOR>
		inline static bool isSquareInBitMask(Square square, bitBoard_t mask) {
			if (COLOR == BLACK) {
				// mapping the colum of a position keeping the row (example B6 will become B2)
				square ^= 0x38;
			}
			return (mask & (1ULL << square)) != 0;
		}


		/**
		 * Register an endgame evaluation funciton for a dedicated piece selection
		 */
		//static void registerFunction(string pieces, evalFunction_t function, bool changeSide = false);
		static void registerEntry(string pieces, EvalEntry entry, bool changeSide = false);
		static void regFun(string pieces, evalFunction_t function) {
			registerEntry(pieces, EvalEntry(function), false);
			registerEntry(pieces, EvalEntry(function), true);
		}
		static void regVal(string pieces, value_t evalCorrection) {
			registerEntry(pieces, EvalEntry(evalCorrection), false);
			registerEntry(pieces, EvalEntry(-evalCorrection), true);
		}

		/**
		 * Forces a draw value
		 */
		template <Piece COLOR>
		static value_t drawValue(MoveGenerator& board, value_t currentValue);

		/**
		 * Forces a near draw value (one side has more material, but probably will not win)
		 */
		template <Piece COLOR>
		static value_t nearDrawValue(MoveGenerator& board, value_t currentValue);

		/**
		 * Forces a winning value
		 */
		template <Piece COLOR>
		static value_t winningValue(MoveGenerator& board, value_t currentValue);

		/**
		 * Gets a value from a bitbase
		 */
		static value_t getFromBitbase(MoveGenerator& position, value_t currentValue);

		template <Piece COLOR>
		static value_t KQKR(MoveGenerator& board, value_t currentValue);

		template <Piece COLOR>
		static value_t KQPsKRPs(MoveGenerator& board, value_t currentValue);

		template <Piece COLOR>
		static value_t KPsK(MoveGenerator& board, value_t currentValue);

		static value_t KPsKPs(MoveGenerator& board, value_t currentValue);

		template <Piece COLOR>
		static value_t KPsKR(MoveGenerator& position, value_t value);

		template <Piece COLOR>
		static value_t KBNK(MoveGenerator& board, value_t currentValue);

		template <Piece COLOR>
		static value_t KBBK(MoveGenerator& board, value_t currentValue);

		template <const PieceSignatureLookup& Table>
		static value_t evalByLookup(MoveGenerator& board, value_t currentValue) {
			return Table.lookup(board);
		}

		template <Piece COLOR>
		static value_t KBsPsK(MoveGenerator& board, value_t currentValue);

		template <Piece COLOR>
		static value_t KNPsK(MoveGenerator& board, value_t currentValue);

		/**
		 * Tries to trap the king in any corner
		 */
		template <Piece COLOR>
		static value_t forceToAnyCorner(MoveGenerator& board, value_t currentValue);

		/**
		 * Mating king in any corner or edge
		 */
		template <Piece COLOR>
		static value_t forceToAnyCornerToMate(MoveGenerator& board, value_t currentValue);

		/**
		 * Tries to trap the king in any corner, but evaluate near draw
		 */
		template <Piece COLOR>
		static value_t forceToAnyCornerButDraw(MoveGenerator& board, value_t currentValue);

		/**
		 * Tries to trap the opponent king in a white or black corner
		 */
		template <Piece COLOR>
		static value_t forceToCorrectCorner(MoveGenerator& board, value_t currentValue, bool whiteCorner);


		template <Piece COLOR>
		static value_t forceToCornerWithBonus(MoveGenerator& board, value_t currentValue);

		/**
		 * Check, if a bishop is able to attack the promotion field of a pawn
		 */
		template <Piece COLOR>
		static bool bishopIsAbleToAttacksPromotionField(Square column, bitBoard_t bishops);

		/**
		 * Checks, if a square is set in a bit board
		 */
		template <Piece COLOR>
		inline static bool isSquareInBB(Square position, bitBoard_t mask);

		/**
		 * Static loopup tables initializer
		 */
		static struct InitStatics {
			InitStatics();
		} _staticConstructor;

		/**
		 * Computes the distance between two squares
		 */
		static value_t manhattenDistance(Square square1, Square square2) {
			value_t fileDistance = abs(value_t(getFile(square1)) - value_t(getFile(square2)));
			value_t rankDistance = abs(value_t(getRank(square1)) - value_t(getRank(square2)));
			return fileDistance + rankDistance;
		}

		/**
		 * Checks, if king cannot prevent pawn from promoting
		 */
		template <Piece COLOR>
		static bool isRunner(MoveGenerator& board, Square pawnSquare);

		/**
		 * Computes the distance between two kings
		 */
		static value_t manhattenKingDistance(MoveGenerator& board);

		/**
		 * Computes the distance to any border
		 */
		static value_t distanceToBorder(Square kingPos);

		/**
		 * Computes the distance to any corner
		 */
		static value_t distanceToAnyCorner(Square kingPos);

		/**
		 * Computes the distance to a white or black corner
		 */
		static value_t distanceToCorrectColorCorner(Square kingPos, bool whiteCorner);

		static const bitBoard_t WHITE_FIELDS = 0x55AA55AA55AA55AAULL;
		static const bitBoard_t BLACK_FIELDS = 0xAA55AA55AA55AA55ULL;

		static constexpr value_t NEAR_DRAW[COLOR_COUNT] = { 20, -20 };
		static constexpr Square UP[COLOR_COUNT] = { NORTH, SOUTH };
		static constexpr value_t RUNNER_VALUE[NORTH] = { 0, 0, 100,  150, 200, 300, 500, 0 };
		static const value_t KING_RACED_PAWN_BONUS = 150;

		static inline PieceSignatureHashedLookup<EvalEntry, 32768, PieceSignature::SIG_SHIFT_BLACK>  pieceSignatureHash;
	};

}

