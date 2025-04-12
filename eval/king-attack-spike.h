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
 * Implements the evaluation of a king attack based on attack bitboards
 * The king attack area is two rows to north, one row to south and one file east and west,
 * thus a rectangle of 3x4 fields
 */

#pragma once

 // Idee 1: Evaluierung in der Ruhesuche ohne positionelle Details
 // Idee 2: Zugsortierung nach lookup Tabelle aus reduziertem Board-Hash
 // Idee 3: Beweglichkeit einer Figur aus der Suche evaluieren. Speichern, wie oft eine Figur von einem Startpunkt erfolgreich bewegt wurde.

#include <map>
#include <algorithm>
#include "../basics/types.h"
#include "../basics/square-table.h"
#include "../movegenerator/bitboardmasks.h"
#include "../movegenerator/movegenerator.h"
#include "evalresults.h"
#include "evalendgame.h"

using namespace std;
using namespace QaplaMoveGenerator;

namespace ChessEval {

	class KingAttack2 {

	public:

		/**
		 * Calculates an evaluation for the current position position
		 */
		static EvalValue eval(const MoveGenerator& position, EvalResults& results) {
			computeAttacks<WHITE>(position, results);
			computeAttacks<BLACK>(position, results);
			return computeAttackValue<WHITE, false>(position, results, nullptr) - computeAttackValue<BLACK, false>(position, results, nullptr);
		}

		static EvalValue evalWithDetails(const MoveGenerator& position, EvalResults& results, std::vector<PieceInfo>& details) {
			computeAttacks<WHITE>(position, results);
			computeAttacks<BLACK>(position, results);
			return computeAttackValue<WHITE, true>(position, results, &details) - computeAttackValue<BLACK, true>(position, results, &details);
		}


	private:

		/**
		 * Counts the undefended or under-defended attacks for squares near king
		 * King itself is not counted as defending piece
		 * 38,5% Only for the initialKingThreat lookup
		 * 40% with counting attacks adjacent to the king
		 */
		template <Piece COLOR, bool STORE_DETAILS>
		inline static EvalValue computeAttackValue(const MoveGenerator& position, EvalResults& results, std::vector<PieceInfo>* details) {
			const Piece OPPONENT = opponentColor<COLOR>();
			const auto kingSquare = position.getKingSquare<COLOR>();
			const auto opponentKingSquare = position.getKingSquare<OPPONENT>();
			bitBoard_t myPawnBB = position.getPieceBB(PAWN + COLOR);
			bitBoard_t attackArea = _kingAttackBB2[kingSquare];
			auto singleAttacks = results.piecesAttack[OPPONENT];
			auto doubleAttacks = results.piecesDoubleAttack[OPPONENT];
			auto opponentKingAttack = BitBoardMasks::kingMoves[opponentKingSquare];
			doubleAttacks |= singleAttacks & opponentKingAttack;
			singleAttacks |= opponentKingAttack;
			auto singleDefends = results.piecesAttack[COLOR];
			auto doubleDefends = results.piecesDoubleAttack[COLOR];
			bitBoard_t emptyKingFrontAttacks = _kingFront[kingSquare] & singleAttacks & ~position.getPiecesOfOneColorBB<COLOR>();
			uint32_t attackIndex = initialKingThreat.map<COLOR>(kingSquare);
			// 56,8%
			attackIndex += popCountForSparcelyPopulatedBitBoards(singleAttacks & attackArea);
			// 55,5%
			attackIndex += popCountForSparcelyPopulatedBitBoards(doubleAttacks & attackArea);
			// 54,8%
			attackIndex += popCountForSparcelyPopulatedBitBoards(emptyKingFrontAttacks);
			// 54,5%
			attackIndex += popCountForSparcelyPopulatedBitBoards(singleAttacks & ~singleDefends & attackArea);
			// 53,3%
			attackIndex += popCountForSparcelyPopulatedBitBoards(doubleAttacks & ~singleDefends & attackArea);
			// 52,7%
			attackIndex += popCountForSparcelyPopulatedBitBoards(doubleAttacks & ~doubleDefends & attackArea);
			// 52,9%
			uint32_t pieceIndex = 
				((results.queenAttack[OPPONENT] & attackArea) != 0) * 0x1 +
				((results.rookAttack[OPPONENT] & attackArea) != 0) * 0x2 +
				((results.bishopAttack[OPPONENT] & attackArea) != 0) * 0x4 +
				((results.knightAttack[OPPONENT] & attackArea) != 0) * 0x8 +
				((opponentKingAttack & attackArea) != 0) * 0x10 +
				((position.pawnAttack[OPPONENT] & attackArea) != 0) * 0x20;

			attackIndex += pieceMap[pieceIndex];

			value_t attackValue = -attackWeight[std::min(MAX_WEIGHT_COUNT - 1, attackIndex)];
			if constexpr (STORE_DETAILS) {
				const IndexVector indexVector{ { "kingAttack", attackIndex, COLOR } };
				details->push_back({ KING + COLOR, kingSquare, indexVector, "a<" + std::to_string(attackIndex) + ">", COLOR == WHITE ? attackValue : -attackValue});
			}
			return EvalValue(attackValue, 0);
		}

		/**
		 * Computes attack bitboards for all pieces of one color - single and double attacks
		 */
		template <Piece COLOR>
		inline static void computeAttacks(const MoveGenerator& position, EvalResults& results) {
			const auto OPPONENT = opponentColor<COLOR>();
			
			bitBoard_t singleAttacks = results.piecesAttack[OPPONENT];
			bitBoard_t doubleAttacks = results.piecesDoubleAttack[OPPONENT];
			bitBoard_t pawnAttack = position.pawnAttack[OPPONENT];
			doubleAttacks |= singleAttacks & pawnAttack;
			singleAttacks |= pawnAttack;
			results.piecesAttack[OPPONENT] = singleAttacks;
			results.piecesDoubleAttack[OPPONENT] = doubleAttacks;
		}

		static const uint32_t MAX_WEIGHT_COUNT = 36;
		// 100 cp = 67% winning propability. 300 cp = 85% winning propability
		static constexpr array<value_t, MAX_WEIGHT_COUNT + 1> attackWeight =
		{
			0, 2, 3, 6, 12, 18, 25, 37, 50, 75, 100, 125, 150, 175, 200, 225, 250, 275, 300, 
			325, 350, 375, 400, 425, 450, 475, 500, 525, 550, 575, 600, 600, 600, 600, 600, 600, 600
		};

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

		inline static array<uint32_t, 64> pieceMap = [] {
			array<uint32_t, 64> pieceMap;
			for (uint32_t index = 0; index < 64; ++index) {
				bool queen = (index & 0x01) != 0;
				bool rook = (index & 0x02) != 0;
				bool bishop = (index & 0x04) != 0;
				bool knight = (index & 0x08) != 0;
				bool king = (index & 0x10) != 0;
				bool pawn = (index & 0x20) != 0;
				uint32_t value = 0;
				if (rook + bishop + knight >= 2) value++;
				if (queen && (rook + knight + bishop + pawn > 0)) {
					value++;
					if (rook + king + knight + bishop) {
						value++;
					}
					if (king && (rook + knight + bishop + pawn > 0)) {
						value++;
					}
				}
				if (king + rook) {
					value++;
				}
				pieceMap[index] = value;
			}
			return pieceMap;
		} ();

		inline static array<bitBoard_t, BOARD_SIZE> _kingAttackBB2 = [] {
			array<bitBoard_t, BOARD_SIZE> kingAttackBB;
			for (Square square = A1; square <= H8; ++square) {
				bitBoard_t attackArea = squareToBB(square);
				attackArea |= BitBoardMasks::shift<WEST>(attackArea);
				attackArea |= BitBoardMasks::shift<EAST>(attackArea);
				attackArea |= BitBoardMasks::shift<SOUTH>(attackArea);
				attackArea |= BitBoardMasks::shift<NORTH>(attackArea);
				// Ensure the attackArea is always 3x3 fields
				/*
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
				*/
				kingAttackBB[square] = attackArea;
			}
			return kingAttackBB;
		} ();

		inline static array<bitBoard_t, BOARD_SIZE> _kingFront = [] {
			array<bitBoard_t, BOARD_SIZE> kingAttackBB;
			for (Square square = A1; square <= H8; ++square) {
				bitBoard_t attackArea = BitBoardMasks::shift<NORTH>(squareToBB(square));
				attackArea |= BitBoardMasks::shift<WEST>(attackArea);
				attackArea |= BitBoardMasks::shift<EAST>(attackArea);
				// Ensure the attackArea is always 3x3 fields
				/*
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
				*/
				kingAttackBB[square] = attackArea;
			}
			return kingAttackBB;
			} ();
		
	};
}

