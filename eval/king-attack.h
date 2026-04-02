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
 * Implements the evaluation of a king attack based on attack bitboards
 * The king attack area is two rows to north, one row to south and one file east and west,
 * thus a rectangle of 3x4 fields
 */

#pragma once

#include <map>
#include <algorithm>
#include "../basics/types.h"
#include "../basics/square-table.h"
#include "../movegenerator/bitboardmasks.h"
#include "../movegenerator/movegenerator.h"
#include "evalresults.h"
#include "array-generator.h"
#include "../interface/uci-parameter-provider.h"
#include "../movegenerator/magics.h"

#ifdef PARAM_OPTIMIZE
#define PARAM_OPTIMIZE_KING_ATTACK
#endif


using namespace std;
using namespace QaplaMoveGenerator;

namespace ChessEval {

	class KingAttack {

	public:
		friend class KingAttackUciAccess;

		/**
		 * Get UCI parameter access interface
		 * @return Reference to UCI parameter provider
		 */
		static UciParameterProvider& getUciAccess();

		static IndexLookupMap getIndexLookup() {
			IndexLookupMap indexLookup;
			std::vector<EvalValue> attack;
			for (auto weight : attackWeight) {
				attack.push_back(EvalValue(weight, 0));
			}
			indexLookup["kAttack"] = attack;
			std::vector<EvalValue> shield;
			for (auto weight : pawnIndexFactor) {
				shield.push_back(EvalValue(weight, 0));
			}
			indexLookup["kShield"] = shield;
			return indexLookup;
		}

		static void addToIndexVector(const EvalResults& results, IndexVector& indexVector) {
		}

		/**
		 * Calculates an evaluation for the current position position
		 */
		static EvalValue eval(MoveGenerator& position, EvalResults& results) {
			computeAttacks<WHITE>(position, results);
			computeAttacks<BLACK>(position, results);

			auto attackValue =  computeAttackValue<WHITE, false>(position, results, nullptr) - computeAttackValue<BLACK, false>(position, results, nullptr);
			return attackValue;
		}

		static EvalValue evalWithDetails(const MoveGenerator& position, EvalResults& results, std::vector<PieceInfo>& details) {
			computeAttacks<WHITE>(position, results);
			computeAttacks<BLACK>(position, results);
			return computeAttackValue<WHITE, true>(position, results, &details) - computeAttackValue<BLACK, true>(position, results, &details);
		}
		static constexpr uint32_t MAX_WEIGHT_COUNT = 32;

	private:

		/**
		 * Computes an index for the pawn shield
		 * Bit FWE F = Front pawn exists, W = West pawn Exists, E = East pawn exists
		 */
		template <Piece COLOR>
		inline static uint32_t computePawnShieldIndex(Square kingSquare, bitBoard_t myPawnBB) {
			uint32_t index = 0;
			bitBoard_t kingBB = 1ULL << kingSquare;
			bitBoard_t kingNorth = kingBB |
				BitBoardMasks::shiftColor<COLOR, NORTH>(kingBB) |
				BitBoardMasks::shiftColor<COLOR, NORTH + NORTH>(kingBB);
			bitBoard_t kingFront = myPawnBB & kingNorth;
			bitBoard_t kingWest = myPawnBB & BitBoardMasks::shiftColor<COLOR, WEST>(kingNorth);
			bitBoard_t kingEast = myPawnBB & BitBoardMasks::shiftColor<COLOR, EAST>(kingNorth);
			index += (kingFront != 0) * 4;
			index += (kingWest != 0 || (kingBB & BitBoardMasks::FILE_A_BITMASK) != 0) * 2;
			index += kingEast != 0 || (kingBB & BitBoardMasks::FILE_H_BITMASK) != 0;
			return index;
		}

		template <Piece COLOR>
		inline static value_t computePawnShieldValue(MoveGenerator& position, const EvalResults& results) {
			Square kingSquare = position.getKingSquare<COLOR>();
			bitBoard_t myPawnBB = position.getPieceBB(PAWN + COLOR);
			uint32_t index = computePawnShieldIndex<COLOR>(kingSquare, myPawnBB);
			value_t pawnShieldValue = (pawnIndexFactor[index] * results.midgameInPercentV2) / 100;
			return pawnShieldValue;
		}

		/**
		 * Compute the amount of moves checking the king
		 */
		template <Piece COLOR>
		inline static value_t computeCheckMoves(const MoveGenerator& position, EvalResults& results) {
			const Piece OPPONENT = switchColor(COLOR);
			Square kingSquare = position.getKingSquare<COLOR>();
			bitBoard_t allPieces = position.getAllPiecesBB();
			bitBoard_t kingAttack = BitBoardMasks::kingMoves[kingSquare];
			bitBoard_t bishopChecks = Magics::genBishopAttackMask(kingSquare, allPieces);
			bitBoard_t rookChecks = Magics::genRookAttackMask(kingSquare, allPieces);
			bitBoard_t knightChecks = BitBoardMasks::knightMoves[kingSquare];

			bishopChecks &= results.queenAttack[OPPONENT] | results.bishopAttack[OPPONENT];
			rookChecks &= results.queenAttack[OPPONENT] | results.rookAttack[OPPONENT];
			knightChecks &= results.knightAttack[OPPONENT];

			bitBoard_t checks = (bishopChecks | rookChecks | knightChecks) & ~position.getPiecesOfOneColorBB<OPPONENT>();
			bitBoard_t undefended = ~results.piecesAttack[COLOR];
			bitBoard_t safeChecks = checks & ((undefended & ~kingAttack) | (undefended & results.piecesDoubleAttack[OPPONENT]));
			return popCount(checks) + popCount(safeChecks) * 2;
		}


		/**
		 * Counts the undefended or under-defended attacks for squares near king
		 * King itself is not counted as defending piece
		 */
		template <Piece COLOR, bool STORE_DETAILS>
		inline static value_t computeAttackValue(const MoveGenerator& position, EvalResults& results, std::vector<PieceInfo>* details) {
			Square kingSquare = position.getKingSquare<COLOR>();
			const Piece OPPONENT = opponentColor<COLOR>();
			bitBoard_t attackArea = _kingAttackBB[COLOR][kingSquare];

			bitBoard_t kingAttacks = attackArea & results.piecesAttack[OPPONENT];
			bitBoard_t kingAttacksNotDefendedByPawns = kingAttacks & ~position.pawnAttack[COLOR];

			bitBoard_t kingDoubleAttacks = attackArea & results.piecesDoubleAttack[OPPONENT];
			bitBoard_t kingDoubleAttacksDefended = kingDoubleAttacks & results.piecesAttack[COLOR];
			bitBoard_t kingDoubleAttacksUndefended = kingDoubleAttacks & ~results.piecesAttack[COLOR];
			uint32_t attackIndex =
				popCountForSparcelyPopulatedBitBoards(kingAttacksNotDefendedByPawns) +
				popCountForSparcelyPopulatedBitBoards(kingDoubleAttacksDefended) +
				popCountForSparcelyPopulatedBitBoards(kingDoubleAttacksUndefended) * 2 +
				computeCheckMoves<COLOR>(position, results) +
				(position.getPieceBB(QUEEN + COLOR) != 0) * queenFactor;

			attackIndex = std::min(MAX_WEIGHT_COUNT, attackIndex);
			value_t attackValue = 0;
			attackValue = attackWeight[attackIndex];
			attackValue = (attackValue * results.midgameInPercentV2) / 100;
			
			if constexpr (STORE_DETAILS) {
				const IndexVector indexVector{ { "kingAttack", attackIndex, COLOR } };
				details->push_back({ KING + COLOR, kingSquare, indexVector, "a<" + std::to_string(attackIndex) + ">", COLOR == WHITE ? attackValue : -attackValue });
			}
			
			return attackValue;
		}

		/**
		 * Computes attack bitboards for all pieces of one color - single and double attacks
		 */
		template <Piece COLOR>
		inline static void computeAttacks(const MoveGenerator& position, EvalResults& results) {
			bitBoard_t pawnAttack = position.pawnAttack[COLOR];
			results.piecesDoubleAttack[COLOR] |= results.piecesAttack[COLOR] & pawnAttack;
			results.piecesAttack[COLOR] |= pawnAttack;
		}

		static constexpr uint32_t MAX_ATTACK_COUNT = 0x0F;
		static constexpr uint32_t QUEEN_AVAILABLE_INDEX = 0x10;

		
		static constexpr uint32_t QUEEN_INDEX = 0x01;
		static constexpr uint32_t PRESSURE_INDEX = 0x2;
		static constexpr uint32_t PRESSURE_MASK = 0x1F;
		static constexpr uint32_t INDEX_SIZE = 0x40;
		static constexpr uint32_t QUEEN_FACTOR_DEFAULT = 2;

		static constexpr array<value_t, MAX_WEIGHT_COUNT + 1> ATTACK_WEIGHT_DEFAULT =
		
		{ 0, -8, -16, -19, -22, -25, -30, -42, -59, -79, -102, -126, -151, -177, -207, -239, -274, -313, -353, -396, -439, -482, -526, -568, -608, -647,
		  -682, -713, -741, -763, -780, -791, -794 };
		
		/*
		{ 0, 0, -5, -10, -15, -25, -35, -50, -65, -85, -105, -140, -165, -190, -215, -230, -255, -280, -305, -330, -355, -380, -410, -440, -470, -500,
		  -530, -560, -590, -620, -650, -680, -710 };

		{ 0, 0, -3, -7, -13, -20, -29, -41, -54, -69, -86, -105, -125, -147, -171, -195, -221, -247, -273, -300, -327, 
			-354, -380, -406, -432, -456, -480, -504, -526, -547, -568, -587, -605 };
		*/

#ifndef PARAM_OPTIMIZE_KING_ATTACK
		static constexpr array<value_t, MAX_WEIGHT_COUNT + 1> attackWeight = ATTACK_WEIGHT_DEFAULT;
		static constexpr uint32_t queenFactor = QUEEN_FACTOR_DEFAULT;
#else
		inline static array<value_t, MAX_WEIGHT_COUNT + 1> attackWeight = ATTACK_WEIGHT_DEFAULT;
		inline static uint32_t queenFactor = QUEEN_FACTOR_DEFAULT;
#endif

		static constexpr SquareTable<value_t> initialKingThreat = SquareTable<value_t>(
			std::array<value_t, 64>{
			5, 5, 5, 5, 5, 5, 5, 5,
				5, 5, 5, 5, 5, 5, 5, 5,
				5, 5, 5, 5, 5, 5, 5, 5,
				5, 5, 5, 5, 5, 5, 5, 5,
				5, 5, 5, 5, 5, 5, 5, 5,
				3, 3, 3, 3, 3, 3, 3, 3,
				1, 1, 2, 2, 2, 2, 1, 1,
				1, 0, 0, 1, 1, 0, 0, 1

		});

		static constexpr array<array<bitBoard_t, BOARD_SIZE>, 2> _kingAttackBB = [] {
			array<array<bitBoard_t, BOARD_SIZE>, 2> kingAttackBB{};
			for (Square kingSquare = A1; kingSquare <= H8; ++kingSquare) {
				bitBoard_t attackArea = 1ULL << kingSquare;
				attackArea |= BitBoardMasks::shift<WEST>(attackArea);
				attackArea |= BitBoardMasks::shift<EAST>(attackArea);
				attackArea |= BitBoardMasks::shift<SOUTH>(attackArea);
				attackArea |= BitBoardMasks::shift<NORTH>(attackArea);
				// The king shall not have a smaller attack area, if at the edges of the board
				if (getFile(kingSquare) == File::A) {
					attackArea |= BitBoardMasks::shift<EAST>(attackArea);
				}
				if (getFile(kingSquare) == File::H) {
					attackArea |= BitBoardMasks::shift<WEST>(attackArea);
				}
				kingAttackBB[WHITE][kingSquare] = attackArea | BitBoardMasks::shift<NORTH>(attackArea);
				kingAttackBB[BLACK][kingSquare] = attackArea | BitBoardMasks::shift<SOUTH>(attackArea);
			}
			return kingAttackBB;
			} ();

		static constexpr array<bitBoard_t, BOARD_SIZE> _kingAttackBB2 = [] {
			array<bitBoard_t, BOARD_SIZE> kingAttackBB{};
			for (Square square = A1; square <= H8; ++square) {
				bitBoard_t attackArea = 1ULL << square;
				attackArea |= BitBoardMasks::shift<WEST>(attackArea);
				attackArea |= BitBoardMasks::shift<EAST>(attackArea);
				attackArea |= BitBoardMasks::shift<SOUTH>(attackArea);
				attackArea |= BitBoardMasks::shift<NORTH>(attackArea);
				// Ensure the attackArea is always 3x3 fields
				if (getFile(square) == File::A) {
					attackArea |= BitBoardMasks::shift<EAST>(attackArea);
				}
				if (getFile(square) == File::H) {
					attackArea |= BitBoardMasks::shift<WEST>(attackArea);
				}
				if (getRank(square) == Rank::R1) {
					attackArea |= BitBoardMasks::shift<NORTH>(attackArea);
				}
				if (getRank(square) == Rank::R8) {
					attackArea |= BitBoardMasks::shift<SOUTH>(attackArea);
				}
				kingAttackBB[square] = attackArea;
			}
			return kingAttackBB;
		} ();

		static constexpr array<value_t, 8> pawnIndexFactor = { -8, -9, -9, -5, -9, -4, 5, 10 };

	};
}
