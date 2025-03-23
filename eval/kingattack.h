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

#ifndef __KINGATTACK_H
#define __KINGATTACK_H

 // Idee 1: Evaluierung in der Ruhesuche ohne positionelle Details
 // Idee 2: Zugsortierung nach lookup Tabelle aus reduziertem Board-Hash
 // Idee 3: Beweglichkeit einer Figur aus der Suche evaluieren. Speichern, wie oft eine Figur von einem Startpunkt erfolgreich bewegt wurde.

#include <map>
#include "../basics/types.h"
#include "../movegenerator/bitboardmasks.h"
#include "../movegenerator/movegenerator.h"
#include "evalresults.h"
#include "evalendgame.h"

using namespace std;
using namespace QaplaMoveGenerator;

namespace ChessEval {

	struct KingAttackValues {
		static const uint32_t MAX_WEIGHT_COUNT = 25;
		// 100 cp = 67% winning propability. 300 cp = 85% winning propability
		static constexpr array<value_t, MAX_WEIGHT_COUNT + 1> attackWeight =
		{ 0,  0, 0, 0, -5, -20, -35, -50, -65, -80, -100, -120, -140, -160, -180, -200, -250, -300, -350, -400, -450, -500, -600, -700, -800, -900 };
		static constexpr value_t pawnIndexFactor[8] = { 100, 100, 100, 100, 100, 100, 100, 100 };
	};

	class KingAttack {

	public:

		static IndexLookupMap getIndexLookup() {
			IndexLookupMap indexLookup;
			std::vector<EvalValue> attack;
			for (auto weight: KingAttackValues::attackWeight) {
				attack.push_back(EvalValue(weight, 0));
			}
			indexLookup["kAttack"] = attack;
			std::vector<EvalValue> shield;
			for (auto weight : KingAttackValues::pawnIndexFactor) {
				shield.push_back(EvalValue(weight, 0));
			}
			indexLookup["kShield"] = shield;
			return indexLookup;
		}

		static void addToIndexVector(const EvalResults& results, IndexVector& indexVector) {
			if (results.kingPressureCount[WHITE]) {
				indexVector.push_back(IndexInfo{ "kAttack", uint32_t(results.kingPressureCount[WHITE]), WHITE });
			}
			if (results.kingPressureCount[BLACK]) {
				indexVector.push_back(IndexInfo{ "kAttack", uint32_t(results.kingPressureCount[BLACK]), BLACK });
			}
		}

		static void printBB(bitBoard_t bb) {
			uint32_t lineBreak = 8;
			for (uint64_t i = 1ULL << 63; i > 0; i /= 2) {
				cout << ((bb & i) ? "X " : ". ");
				lineBreak--;
				if (lineBreak == 0) {
					cout << endl;
					lineBreak = 8;
				}
			}
			cout << endl;
		}

		/**
		 * Calculates an evaluation for the current position position
		 */
		static value_t eval(MoveGenerator& position, EvalResults& results) {
			computeAttacks<WHITE>(position, results);
			computeAttacks<BLACK>(position, results);
			value_t result = computeAttackValue<WHITE>(position, results) -
				computeAttackValue<BLACK>(position, results);
			return result;
		}

		/**
		 * Prints the evaluation results
		 */
		static void print(MoveGenerator& position, EvalResults& results) {
			eval(position, results);
			cout << "King attack" << endl;
			cout << "White pawn shield:" << std::right << std::setw(18) << 
				KingAttackValues::pawnIndexFactor[
					computePawnShieldIndex<WHITE>(position.getKingSquare<WHITE>(), position.getPieceBB(WHITE_PAWN))] 
				<< endl;
			cout << "Black pawn shield:" << std::right << std::setw(18) <<
				KingAttackValues::pawnIndexFactor[
					computePawnShieldIndex<BLACK>(position.getKingSquare<BLACK>(), position.getPieceBB(BLACK_PAWN))]
				<< endl;
			cout << "White (pressure " << results.kingPressureCount[WHITE] << "):" << std::right << std::setw(17) << 
				results.kingAttackValue[WHITE] << endl;
			cout << "Black (pressure " << results.kingPressureCount[BLACK] << "):" << std::right << std::setw(17) <<
				results.kingAttackValue[BLACK] << endl;
		}

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

		/**
		 * Examine fields to attack the king
		 */
		template <Piece COLOR>
		inline static int32_t computeCheckMoves(MoveGenerator& position, EvalResults& results) {
			const Piece us = COLOR;
			const Piece them = switchColor(COLOR);
			Square kingSquare = position.getKingSquare<us>();
			bitBoard_t allPieces = position.getAllPiecesBB();
			bitBoard_t kingAttack = BitBoardMasks::kingMoves[kingSquare];
			bitBoard_t bishopChecks = Magics::genBishopAttackMask(kingSquare, allPieces);
			bitBoard_t rookChecks = Magics::genRookAttackMask(kingSquare, allPieces);
			bitBoard_t knightChecks = BitBoardMasks::knightMoves[kingSquare];

			bishopChecks &= results.queenAttack[them] | results.bishopAttack[them];
			rookChecks &= results.queenAttack[them] | results.rookAttack[them];
			knightChecks &= results.knightAttack[them];

			bitBoard_t checks = (bishopChecks | rookChecks | knightChecks) & ~position.getPiecesOfOneColorBB<them>();
			bitBoard_t undefended = ~results.piecesAttack[us];
			bitBoard_t safeChecks = checks & ((undefended & ~kingAttack) | (undefended & results.piecesDoubleAttack[them]));
			return popCount(checks) + popCount(safeChecks) * 2;
		}


		/**
		 * Counts the undefended or under-defended attacks for squares near king
		 * King itself is not counted as defending piece
		 */
		template <Piece COLOR>
		inline static value_t computeAttackValue(MoveGenerator& position, EvalResults& results) {
			Square kingSquare = position.getKingSquare<COLOR>();
			bitBoard_t myPawnBB = position.getPieceBB(PAWN + COLOR);
			const Piece OPPONENT = COLOR == WHITE ? BLACK : WHITE;
			bitBoard_t attackArea = _kingAttackBB[COLOR][kingSquare];

			bitBoard_t kingAttacks = attackArea & results.piecesAttack[OPPONENT];
			bitBoard_t kingAttacksNotDefendedByPawns = kingAttacks & ~position.pawnAttack[COLOR];

			bitBoard_t kingDoubleAttacks = attackArea & results.piecesDoubleAttack[OPPONENT];
			bitBoard_t kingDoubleAttacksDefended = kingDoubleAttacks & results.piecesAttack[COLOR];
			bitBoard_t kingDoubleAttacksUndefended = kingDoubleAttacks & ~results.piecesAttack[COLOR];

			results.kingPressureCount[COLOR] =
				popCountForSparcelyPopulatedBitBoards(kingAttacksNotDefendedByPawns) +
				popCountForSparcelyPopulatedBitBoards(kingDoubleAttacksDefended) +
				popCountForSparcelyPopulatedBitBoards(kingDoubleAttacksUndefended) * 2 +
				computeCheckMoves<COLOR>(position, results) +
				(position.getPieceBB(QUEEN + COLOR) != 0) * 3;

			if (results.kingPressureCount[COLOR] > KingAttackValues::MAX_WEIGHT_COUNT) {
				results.kingPressureCount[COLOR] = KingAttackValues::MAX_WEIGHT_COUNT;
			}
			value_t attackValue =
				(KingAttackValues::attackWeight[results.kingPressureCount[COLOR]] * results.midgameInPercentV2) / 100;
			results.kingAttackValue[COLOR] = attackValue;
			return results.kingAttackValue[COLOR];
		}

		/**
		 * Computes attack bitboards for all pieces of one color - single and double attacks
		 */
		template <Piece COLOR>
		inline static void computeAttacks(MoveGenerator& position, EvalResults& results) {
			const Piece OPPONENT = COLOR == WHITE ? BLACK : WHITE;
			bitBoard_t pawnAttack = position.pawnAttack[COLOR];
			results.piecesDoubleAttack[COLOR] |= results.piecesAttack[COLOR] & pawnAttack;
			results.piecesAttack[COLOR] |= pawnAttack;
		}


		static struct InitStatics {
			InitStatics();
		} _staticConstructor;

		static array<bitBoard_t, BOARD_SIZE> _kingAttackBB[2];

	};
}

#endif // __KINGATTACK_H