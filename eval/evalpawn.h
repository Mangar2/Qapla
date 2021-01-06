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
 * Provides functions to evaluate the pawn structure
 */

#ifndef __EVALPAWN_H
#define __EVALPAWN_H

#include "../basics/types.h"
#include "../basics/move.h"
#include "../movegenerator/bitboardmasks.h"
#include "../movegenerator/movegenerator.h"
#include "pawnrace.h"

using namespace ChessBasics;
using namespace ChessMoveGenerator;

namespace ChessEval {

	struct EvalValues {
		typedef array<value_t, uint32_t(Rank::COUNT)> RankArray_t;
		typedef array<value_t, uint32_t(File::COUNT)> FileArray_t;

		/**
		 * For rank 2 to 6, ignored for rank 7
		 * Double isolated pawn are counted as one isolated pawn and a double pawn
		 */
		const static value_t ISOLATED_PAWN_PENALTY = -15;

		const static value_t MOBILITY_VALUE = 2;

		const static value_t DOUBLE_PAWN_PENALTY = -20;

		static constexpr RankArray_t ADVANCED_PAWN_VALUE = { 0,  0,   0,   5,  10,  15,   0, 0 };
		static constexpr RankArray_t PASSED_PAWN_VALUE = { 0, 10,  10,  25,  45,  70, 100, 0 };
		static constexpr FileArray_t PROTECTED_PASSED_PAWN_VALUE = { 0, 10,  25,  45, 70, 100, 200, 0 };
		static constexpr FileArray_t CONNECTED_PASSED_PAWN_VALUE = { 0, 25,  50,  80, 130, 200, 300, 0 };
		static constexpr RankArray_t DISTANT_PASSED_PAWN_VALUE = { 0, 25,  50,  60,  80, 100, 150, 0 };
	};

	class EvalPawn {
	public:
		EvalPawn() {}

		typedef array<value_t, uint32_t(Rank::COUNT)> RankArray_t;
		typedef array<value_t, uint32_t(File::COUNT)> FileArray_t;

		/**
		 * Prints pawn evaluation details
		 */
		value_t print(MoveGenerator& board);

		/**
		 * Calculates the evaluation for the pawn values on the board
		 */
		value_t eval(MoveGenerator& board) {
			init<WHITE>(board);
			init<BLACK>(board);
			return eval<WHITE>(board) + eval<BLACK>(board);
		}

		/**
		 * Computes the current passed pawn as bitboards
		 */
		void computeAllPassedPawns()
		{
			computePassedPawns<WHITE>();
			computePassedPawns<BLACK>();
		}

		/**
		 * Computes the value of the pawn structure in the case there is no
		 * piece on the board
		 */
		value_t computePawnValueNoPiece(MoveGenerator& board) {
			init<WHITE>(board);
			init<BLACK>(board);
			value_t result = board.getMaterialValue();
			result += computePawnValueNoPieceButPawn<WHITE>(board);
			result -= computePawnValueNoPieceButPawn<BLACK>(board);

			//value_t whiteRunner = computeRunnerPace<WHITE>(board, passedPawnFld[WHITE]);
			//value_t blackRunner = computeRunnerPace<BLACK>(board, passedPawnFld[BLACK]);
			//value_t runnerValue = (blackRunner - whiteRunner) * 2;
			//runnerValue += board.whiteToMove ? 1 : -1;
			PawnRace pawnRace;
			value_t runnerValue = pawnRace.runnerRace(board, passedPawnFld[WHITE], passedPawnFld[BLACK]);
			if (runnerValue != 0) {
				result /= 4;
				result += runnerValue;
			}
			return result;
		}

	private:

		/**
		 * Evaluates pawns per color
		 */
		template <Piece COLOR>
		inline value_t eval(MoveGenerator& board) {
			value_t result = 0;
			if (pawns[COLOR] != 0) {
				result += computeAdvancedPawnValue<COLOR>();
				result += computeIsolatedPawnValue<COLOR>();
				result += computeDoublePawnValue<COLOR>();
				result += computePassedPawnValue<COLOR>(board);
			}
			return result;
		}

		/**
		 * Initializes the eval structures
		 */
		template<Piece COLOR>
		void init(MoveGenerator& board) {
			pawns[COLOR] = board.getPieceBB(PAWN + COLOR);
			pawnMoveRay[COLOR] = computePawnMoveRay<COLOR>();
		}

		/**
		 * Computes the pawn value for a board with no piece but pawns
		 */
		template<Piece COLOR>
		value_t computePawnValueNoPieceButPawn(MoveGenerator& board) {
			const bool NO_PIECES_BUT_PAWNS_ON_BOARD = true;
			bitBoard_t pawns = board.getPieceBB(PAWN + COLOR);
			bitBoard_t passedPawns = computePassedPawns<COLOR>();

			value_t pawnValue = computePawnValueForSparcelyPolulatedBitboards<COLOR>(pawns & ~passedPawns, 
				EvalValues::ADVANCED_PAWN_VALUE);
			pawnValue += computePassedPawnValue<COLOR>(board, passedPawns, NO_PIECES_BUT_PAWNS_ON_BOARD);
			return pawnValue;
		}

		/**
		 * Computes a value for running pawn - use only if there are only pawns on the board
		 */
		template<Piece COLOR>
		value_t computeRunnerPace(Board& board, bitBoard_t passedPawns) {
			const Piece OPPONENT_COLOR = OPPONENT[COLOR];
			const Square CHANGE_SIDE = COLOR == WHITE ? 0 : 0x38;

			Square opponentKingPos = board.getKingSquare<OPPONENT_COLOR>();
			Square myKingPos = board.getKingSquare<COLOR>;
			Piece colorAtMove = board.isWhiteToMove() ? WHITE : BLACK;

			bitBoard_t kingSupport = kingSupportPawnTable[COLOR][myKingPos];
			bitBoard_t opponentInfluence = kingInfluenceTable[OPPONENT_COLOR][colorAtMove][opponentKingPos];

			bitBoard_t runner = (passedPawns & ~opponentInfluence) | (passedPawns & kingSupport);
			value_t smallestDistance = value_t(Rank::COUNT);

			for (; runner != 0; runner &= runner - 1) {
				Square pawnPos = BitBoardMasks::lsb(runner) ^ CHANGE_SIDE;
				Square curDistance = 7 - getRow(pawnPos);
				bool ownKingBlocksPawn = myKingPos > pawnPos && getFile(myKingPos) == getFile(pawnPos);
				if (ownKingBlocksPawn) {
					curDistance++;
				}
				if (curDistance < smallestDistance) {
					smallestDistance = curDistance;
				}
			}

			return smallestDistance;
		}


		/**
		 * Computes pawn values for boards with only few pawns
		 */
		template <Piece COLOR>
		inline value_t computePawnValueForSparcelyPolulatedBitboards(bitBoard_t pawns, const RankArray_t pawnValue) {
			value_t result = 0;
			const uint32_t CHANGE_SIDE = COLOR == WHITE ? 0 : 0x38;

			for (; pawns != 0; pawns &= pawns - 1) {
				Rank rank = getRank(Square(BitBoardMasks::lsb(pawns) ^ CHANGE_SIDE));
				result += pawnValue[uint32_t(rank)];
			}
			return result;
		}

		/**
		 * Computes the value for passed pawn
		 */
		template <Piece COLOR>
		value_t computePassedPawnValue(MoveGenerator& board, bitBoard_t passedPawns, bool noPieces = false) {
			value_t result = 0;
			uint32_t changeSide = COLOR == WHITE ? 0 : 0x38;
			for (bitBoard_t pawns = passedPawns; pawns != 0; pawns &= pawns - 1) {
				Square pawnPos = BitBoardMasks::lsb(pawns);
				uint32_t rank = uint32_t(getRank(Square(pawnPos ^ changeSide)));
				if (isConnectedPassedPawn(pawnPos, passedPawns)) {
					result += EvalValues::CONNECTED_PASSED_PAWN_VALUE[rank];
				}
				else if (noPieces && isDistantPassedPawn(pawnPos, board.getPieceBB(PAWN + COLOR), 
						board.getPieceBB(PAWN + OPPONENT[COLOR])))
				{
					result += EvalValues::DISTANT_PASSED_PAWN_VALUE[rank];
				}
				else if (isProtectedPassedPawn(pawnPos, board.pawnAttackMask[COLOR])) {
					result += EvalValues::PROTECTED_PASSED_PAWN_VALUE[rank];
				}
				else {
					result += EvalValues::PASSED_PAWN_VALUE[rank];
				}
			}
			return result;
		}

		/**
		 * Compute the passed pawn vaues using precomputed bitboards stored in this class
		 */
		template <Piece COLOR>
		value_t computePassedPawnValue(MoveGenerator& board) {
			bitBoard_t passedPawns = computePassedPawns<COLOR>();
			value_t result = computePassedPawnValue<COLOR>(board, passedPawns);
			return COLOR == WHITE ? result : -result;
		}


		/**
		 * Checks, if a passed pawn is a distant passed pawn (having no opponents further outside)
		 */
		bool isDistantPassedPawn(Square pawnPos, bitBoard_t ownPawns, bitBoard_t opponentPawns) {
			bool noOpponentPawnsFurtherOutside = 
				(opponentPawns & distantPassedPawnCheckNoOpponentPawn[uint32_t(getFile(pawnPos))]) == 0;
			bool ownPawnsOnOtherSideOfBoard = 
				(ownPawns & distantPassedPawnCheckOwnPawn[uint32_t(getFile(pawnPos))]) != 0;
			return noOpponentPawnsFurtherOutside && ownPawnsOnOtherSideOfBoard;
		}

		/**
		 * Checks, if a passed pawn is connected to another passed pawn
		 */
		bool isConnectedPassedPawn(Square pawnPos, bitBoard_t passedPawns) {
			bool connectedPassedPawn = 
				(passedPawns & connectedPassedPawnCheckMap[uint32_t(getFile(pawnPos))]) != 0;
			return connectedPassedPawn;
		}

		/**
		 * Checks, if a passed pawn is protected by another pawn
		 */
		bool isProtectedPassedPawn(Square pawnPos, bitBoard_t pawnAttackMask) {
			bool protectedPassedPawn = (1ULL << pawnPos & pawnAttackMask) != 0;
			return protectedPassedPawn;
		}

		/** 
		 * Computes the isolated pawn penalty
		 */
		template <Piece COLOR>
		value_t computeIsolatedPawnValue() {
			if (COLOR == WHITE) {
				return isolatedPawnAmountLookup[(pawnMoveRay[COLOR] >> 6 * NORTH) & LOOKUP_TABLE_MASK]
					* EvalValues::ISOLATED_PAWN_PENALTY;
			}
			else {
				return -isolatedPawnAmountLookup[(pawnMoveRay[COLOR] >> 1 * NORTH) & LOOKUP_TABLE_MASK]
					* EvalValues::ISOLATED_PAWN_PENALTY;
			}
		}

		/**
		 * Computes the double pawn penalty
		 */
		template <Piece COLOR>
		inline value_t computeDoublePawnValue() {
			value_t result = computeAmountOfDoublePawns(pawns[COLOR], pawnMoveRay[COLOR]) * EvalValues::DOUBLE_PAWN_PENALTY;
			return COLOR == WHITE ? result : -result;
		}

		/**
		 * Computes the advanced pawn bonus
		 */
		template <Piece COLOR>
		value_t computeAdvancedPawnValue() {
			value_t pawnValue = 0;
			bitBoard_t pawnsBB = pawns[COLOR];
			if (COLOR == WHITE) {
				pawnValue += BitBoardMasks::popCountInFirstRank(pawnsBB >> NORTH * 3) * EvalValues::ADVANCED_PAWN_VALUE[3];
				pawnValue += BitBoardMasks::popCountInFirstRank(pawnsBB >> NORTH * 4) * EvalValues::ADVANCED_PAWN_VALUE[4];
				pawnValue += BitBoardMasks::popCountInFirstRank(pawnsBB >> NORTH * 5) * EvalValues::ADVANCED_PAWN_VALUE[5];
				pawnValue += BitBoardMasks::popCountInFirstRank(pawnsBB >> NORTH * 6) * EvalValues::ADVANCED_PAWN_VALUE[6];
			}
			else {
				pawnValue -= BitBoardMasks::popCountInFirstRank(pawnsBB >> NORTH * 4) * EvalValues::ADVANCED_PAWN_VALUE[3];
				pawnValue -= BitBoardMasks::popCountInFirstRank(pawnsBB >> NORTH * 3) * EvalValues::ADVANCED_PAWN_VALUE[4];
				pawnValue -= BitBoardMasks::popCountInFirstRank(pawnsBB >> NORTH * 2) * EvalValues::ADVANCED_PAWN_VALUE[5];
				pawnValue -= BitBoardMasks::popCountInFirstRank(pawnsBB >> NORTH * 1) * EvalValues::ADVANCED_PAWN_VALUE[6];
			}
			return pawnValue;
		}

		/**
		 * Computes the bitboard for passed pawns
		 */
		template<Piece COLOR>
		bitBoard_t computePassedPawns() {
			bitBoard_t opponentPawnMoveRay = pawnMoveRay[OPPONENT[COLOR]];
			bitBoard_t nonPasserMask = opponentPawnMoveRay;
			nonPasserMask |= BitBoardMasks::shift<WEST>(opponentPawnMoveRay);
			nonPasserMask |= BitBoardMasks::shift<EAST>(opponentPawnMoveRay);
			passedPawnFld[COLOR] = pawns[COLOR] & ~nonPasserMask;
			return passedPawnFld[COLOR];
		}

		/**
		 * Computes the amount of double pawns, not counting the first pawn. Triple pawns are counted as two double pawns.
		 * It works for black and white
		 */
		value_t computeAmountOfDoublePawns(bitBoard_t pawns, bitBoard_t pawnMoveRay) {
			bitBoard_t doublePawns = pawns & pawnMoveRay;
			value_t doublePawnAmount = BitBoardMasks::popCount(doublePawns);
			return doublePawnAmount;
		}

		template <Piece COLOR>
		bitBoard_t computePawnMoveRay() {
			pawnMoveRay[COLOR] = 0;
			const auto pawnBB = pawns[COLOR];
			if (COLOR == WHITE && pawnBB != 0) {
				pawnMoveRay[COLOR] = pawnBB << NORTH | pawnBB << 2 * NORTH | pawnBB << 3 * NORTH |
					pawnBB << 4 * NORTH | pawnBB << 5 * NORTH;
			}
			if (COLOR == BLACK && pawnBB != 0) {
				pawnMoveRay[COLOR] = pawnBB >> NORTH | pawnBB >> 2 * NORTH | pawnBB >> 3 * NORTH |
					pawnBB >> 4 * NORTH | pawnBB >> 5 * NORTH;
			}

			return pawnMoveRay[COLOR];
		}

		static struct InitStatics {
			InitStatics();
		} _staticConstructor;

		typedef bool testFunction_t(Square kingPos, Square pawnPos, bool atMove);
		static bool kingSupportsPassedPawn(Square kingPos, Square pawnPos, bool atMove);
		static bool kingReachesPawn(Square kingPos, Square pawnPos, bool atMove);

		static bitBoard_t computeKingInfluence(Square kingPos, bool atMove, testFunction_t testFunction);

		static void computeKingInfluenceTable();
		static void computeKingSupportTable();
		static void computeIsolatedPawnLookupTable();

		static const value_t RUNNER_BONUS = 300;


		static const uint32_t LOOKUP_TABLE_SIZE = 1 << NORTH;
		static const bitBoard_t LOOKUP_TABLE_MASK = LOOKUP_TABLE_SIZE - 1;
		static value_t isolatedPawnAmountLookup[LOOKUP_TABLE_SIZE];
		static bitBoard_t kingInfluenceTable[COLOR_AMOUNT][COLOR_AMOUNT][BOARD_SIZE];
		static bitBoard_t kingSupportPawnTable[COLOR_AMOUNT][BOARD_SIZE];

		static constexpr bitBoard_t distantPassedPawnCheckNoOpponentPawn[NORTH] = {
			0x0101010101010101ULL, 0x0303030303030303ULL, 0x070707070707070FULL, 0x0F0F0F0F0F0F0F0FULL,
			0xF0F0F0F0F0F0F0F0ULL, 0xE0E0E0E0E0E0E0E0ULL, 0xC0C0C0C0C0C0C0C0ULL, 0x8080808080808080ULL
		};

		static constexpr bitBoard_t distantPassedPawnCheckOwnPawn[NORTH] = {
			0xF8F8F8F8F8F8F8F8ULL, 0xF0F0F0F0F0F0F0F0ULL, 0xE0E0E0E0E0E0E0E0ULL, 0xC0C0C0C0C0C0C0C0ULL,
			0x0303030303030303ULL, 0x0707070707070707ULL, 0x0F0F0F0F0F0F0F0FULL, 0x1F1F1F1F1F1F1F1FULL
		};

		static constexpr bitBoard_t connectedPassedPawnCheckMap[NORTH] = {
			0x0202020202020202ULL, 0x0505050505050505ULL, 0x0A0A0A0A0A0A0A0AULL, 0x1414141414141414ULL,
			0x2828282828282828ULL, 0x5050505050505050ULL, 0xA0A0A0A0A0A0A0A0ULL, 0x4040404040404040ULL
		};

	public:
		bitBoard_t pawns[2];
		bitBoard_t pawnMoveRay[2];
		bitBoard_t passedPawnFld[2];

	};

}

#endif // __EVALPAWN_H