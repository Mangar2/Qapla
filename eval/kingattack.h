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
#include <algorithm>
#include "../basics/types.h"
#include "../movegenerator/bitboardmasks.h"
#include "../movegenerator/movegenerator.h"
#include "evalresults.h"
#include "evalendgame.h"

using namespace std;
using namespace QaplaMoveGenerator;

namespace ChessEval {

	class KingAttack {

	public:

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
		static EvalValue eval(MoveGenerator& position, EvalResults& results) {
			computeAttacks<WHITE>(position, results);
			computeAttacks<BLACK>(position, results);
			/*
			if (position.getEvalVersion() == 1) {
				auto result = computeAttackValue<WHITE>(position, results) - computeAttackValue<BLACK>(position, results);
				result += computePawnShieldValue<WHITE>(position, results) - computePawnShieldValue<BLACK>(position, results);
				return result;
			}
			*/
			return computeAttackValue<WHITE>(position, results) - computeAttackValue<BLACK>(position, results);
		}

		/**
		 * Prints the evaluation results
		 */
		static void print(MoveGenerator& position, EvalResults& results) {
			eval(position, results);
			cout << "King attack" << endl;
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
		inline static value_t computeCheckMoves(MoveGenerator& position, EvalResults& results) {
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

			if (results.kingPressureCount[COLOR] > MAX_WEIGHT_COUNT) {
				results.kingPressureCount[COLOR] = MAX_WEIGHT_COUNT;
			}
			value_t attackValue =
				(attackWeight[results.kingPressureCount[COLOR]] * results.midgameInPercentV2) / 100;
			results.kingAttackValue[COLOR] = attackValue;

			//uint32_t pawnShieldIndex = computePawnShieldIndex<COLOR>(kingSquare, myPawnBB);
			//attackValue += (pawnIndexFactor[pawnShieldIndex] * results.midgameInPercentV2) / 100;
			return attackValue;
		}

		template <Piece COLOR>
		inline static EvalValue computeAttackValue2(MoveGenerator& position, EvalResults& results) {
			Square kingSquare = position.getKingSquare<COLOR>();
			bitBoard_t myPawnBB = position.getPieceBB(PAWN + COLOR);
			const Piece OPPONENT = switchColor(COLOR);
			bitBoard_t attackArea = _kingAttackBB2[kingSquare];

			bitBoard_t kingAttacks = attackArea & results.piecesAttack[OPPONENT];
			bitBoard_t kingAttacksNotDefendedByPawns = kingAttacks & ~position.pawnAttack[COLOR];

			bitBoard_t kingDoubleAttacks = attackArea & results.piecesDoubleAttack[OPPONENT];
			bitBoard_t kingDoubleAttacksDefended = kingDoubleAttacks & results.piecesAttack[COLOR];
			bitBoard_t kingDoubleAttacksUndefended = kingDoubleAttacks & ~results.piecesAttack[COLOR];
			auto pieceSignature = position.getPiecesSignature<OPPONENT>();
			auto checkMoves = computeCheckMoves<COLOR>(position, results);
			bool hasQueen = (pieceSignature & SignatureMask::QUEEN) != 0;

			uint32_t pressure =
				popCountForSparcelyPopulatedBitBoards(kingAttacksNotDefendedByPawns) +
				popCountForSparcelyPopulatedBitBoards(kingDoubleAttacksDefended) +
				popCountForSparcelyPopulatedBitBoards(kingDoubleAttacksUndefended) * 2;
			uint32_t pieceIndex = std::min(MAX_ATTACK_COUNT, pressure) + hasQueen * QUEEN_AVAILABLE_INDEX;
			/*
			uint32_t attackSignature = 
				((results.queenAttack[OPPONENT] & attackArea) > 0) * 1 +
				((results.rookAttack[OPPONENT] & attackArea) > 0) * 2 +
				((results.bishopAttack[OPPONENT] & attackArea) > 0) * 4 +
				((results.knightAttack[OPPONENT] & attackArea) > 0) * 8;
				attackValue = (attackValue * CandidateTrainer::getCurrentCandidate().getWeightVector(5)[attackSignature].midgame()) / 100;
			*/

			uint32_t index = hasQueen * QUEEN_INDEX + std::min(PRESSURE_MASK, pressure + checkMoves) * PRESSURE_INDEX;
			//value_t attackValue = CandidateTrainer::getCurrentCandidate().getWeightVector(0)[index].midgame();
			//attackValue = (attackValue * results.midgameInPercentV2) / 100;

			value_t attackValue = attackValueMap[index];
			if (attackValue > -10) { attackValue = 0; }
			results.kingPressureCount[COLOR] = index;
			results.kingAttackValue[COLOR] = attackValue;
			return EvalValue(attackValue, 0);
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

		static const uint32_t MAX_ATTACK_COUNT = 0x0F;
		static const uint32_t QUEEN_AVAILABLE_INDEX = 0x10;

		static constexpr array<value_t, QUEEN_AVAILABLE_INDEX * 2> queenAttackWeight = {
			0, 0, 0, 0, -1, -3, -5, -8, -11, -15, -19, -24, -29, -35, -41, -48, 0, 0, 0, 0, -1, -3, -5, -8, -11, -15, -19, -24, -29, -35, -41, -48
		};

		static constexpr array<value_t, QUEEN_AVAILABLE_INDEX * 2> rookAttackWeight = {
			0, 0, 0, -1, -1, -2, -3, -3, -4, -5, -7, -11, -14, -18, -23, -28, 0, -1, -1, -2, -3, -4, -5, -7, -9, -11, -16, -21, -27, -34, -41, -49
		};

		static constexpr array<value_t, QUEEN_AVAILABLE_INDEX * 2> bishopAttackWeight = {
			0, 0, 0, -1, -1, -3, -4, -4, -6, -7, -10, -15, -19, -25, -31, -38, 0, -1, -1, -2, -3, -4, -5, -8, -10, -12, -18, -24, -31, -39, -48, -57
		};

		static constexpr array<value_t, QUEEN_AVAILABLE_INDEX * 2> knightAttackWeight = {
			0, -1, -1, -2, -2, -4, -5, -5, -7, -9, -12, -18, -23, -30, -38, -46, 0, -1, -1, -2, -4, -5, -7, -9, -12, -15, -22, -29, -37, -47, -57, -68
		};

		static constexpr array<value_t, 0x10> SIGNATURE_FACTOR = {
			237, 90, 117, 136, 110, 114, 171, 140, 56, 113, 100, 152, 79, 92, 122, 70
		};



		static const uint32_t QUEEN_INDEX = 0x01;
		static const uint32_t PRESSURE_INDEX = 0x2;
		static const uint32_t PRESSURE_MASK = 0x1F;
		static const uint32_t INDEX_SIZE = 0x40;

		static struct InitStatics {
			InitStatics();
		} _staticConstructor;

		static array<bitBoard_t, BOARD_SIZE> _kingAttackBB[2];
		static const uint32_t MAX_WEIGHT_COUNT = 25;
		// 100 cp = 67% winning propability. 300 cp = 85% winning propability
		static constexpr array<value_t, MAX_WEIGHT_COUNT + 1> attackWeight =
		{ 0,  0, 0, 0, -5, -20, -35, -50, -65, -80, -100, -120, -140, -160, -180, -200, -250, -300, -350, -400, -450, -500, -600, -700, -800, -900 };
		static constexpr array<value_t, 8> pawnIndexFactor = { -8, -9, -9, -5, -9, -4, 5, 10 };

		constexpr static array<value_t, INDEX_SIZE> attackValueMap = {
			   0,   0,   0,   0,   0,  -2,   0,  -7,  -2, -15,  -9, -26, -20, -39, -34, -55,
			 -52, -73, -73, -93, -96,-115,-121,-137,-148,-161,-176,-186,-205,-211,-235,-237,
			-265,-263,-295,-289,-324,-314,-352,-339,-379,-363,-404,-385,-427,-407,-448,-427,
			-466,-445,-480,-461,-491,-474,-498,-485,-500,-493,-500,-498,-500,-500,-500,-500
		};

		inline static array<bitBoard_t, BOARD_SIZE> _kingAttackBB2 = [] {
			array<bitBoard_t, BOARD_SIZE> kingAttackBB;
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
	
		/*
		inline static array<value_t, INDEX_SIZE> attackValueMap = [] {
			const auto PRINT = false;
			if (PRINT) std::cout << "{";
			std::string separator = "";
			array<value_t, INDEX_SIZE> map;
			for (uint32_t bitmask = 0; bitmask < INDEX_SIZE; ++bitmask) {
				if (PRINT) if (bitmask % 16 == 0) std::cout << std::endl << "    ";
				uint32_t index = 0;
				if (bitmask & QUEEN_INDEX) index += 3;
				index += (bitmask & (PRESSURE_INDEX * PRESSURE_MASK)) / PRESSURE_INDEX;
				map[bitmask] = attackWeight[std::min(index, MAX_WEIGHT_COUNT)];
				if (PRINT) std::cout << separator << map[bitmask];
				separator = ", ";
			}
			if (PRINT) std::cout << "}" << std::endl;
			return map;
		} ();
		*/
	};
}

#endif // __KINGATTACK_H