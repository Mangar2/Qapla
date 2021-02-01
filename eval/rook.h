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
 * Implements a static class to evaluate rooks 
 */

#ifndef __ROOK_H
#define __ROOK_H

#include <cstdint>
#include "../basics/types.h"
#include "../movegenerator/bitboardmasks.h"
#include "../movegenerator/movegenerator.h"
#include "../basics/evalvalue.h"
#include "evalresults.h"

using namespace ChessBasics;
using namespace ChessMoveGenerator;

namespace ChessEval {
	class Rook {
	public:
		template <bool PRINT>
		static EvalValue eval(const MoveGenerator& position, EvalResults& results) {
			return eval<WHITE, PRINT>(position, results) - eval<BLACK, PRINT>(position, results);
		}
	private:

		/**
		 * Evaluates Rooks
		 */
		template<Piece COLOR, bool PRINT>
		static EvalValue eval(const MoveGenerator& position, EvalResults& results) {
			EvalValue value;
			constexpr Piece OPPONENT = COLOR == WHITE ? BLACK : WHITE;
			bitBoard_t rooks = position.getPieceBB(ROOK + COLOR);
			const bitBoard_t ourPawnBB = position.getPieceBB(PAWN + COLOR);
			const bitBoard_t theirPawnBB = position.getPieceBB(PAWN + OPPONENT);
			while (rooks)
			{
				uint16_t rookIndex = 0;
				const Square rookSquare = BitBoardMasks::lsb(rooks);
				rooks &= rooks - 1;
				const Square rank8Destination = (COLOR == WHITE ? A8 : A1) + Square(getFile(rookSquare));
				const bitBoard_t moveRay = BitBoardMasks::fileBB[int(getFile(rookSquare))];
				rookIndex += isOnOpenFile(results.pawnsBB, moveRay) * OPEN_FILE;
				rookIndex += isOnHalfOpenFile(ourPawnBB, moveRay) * HALF_OPEN_FILE;
				rookIndex += trappedByKing<COLOR>(rookSquare, position.getKingSquare<COLOR>()) * TRAPPED;
				rookIndex +=
					protectsPassedPawnFromBehind<COLOR>(results.passedPawns[COLOR], rookSquare, moveRay) * PROTECTS_PP;
				value += getFromIndexMap(rookIndex);
				if (PRINT) printIndex(rookSquare, rookIndex);
			}
			if (PRINT) cout << (COLOR == WHITE ? "White" : "Black") << " rook: " << value << endl;
			return value;
		}

		/**
		 * Calculates the mobility of a rook
		 */
		template<Piece COLOR>
		static EvalValue calcMobility(
			Square square, bitBoard_t occupiedBB, bitBoard_t removeBB, EvalResults& results) 
		{
			bitBoard_t attackBB = Magics::genRookAttackMask(square, occupiedBB);
			results.doubleRookAttack[COLOR] |= results.rookAttack[COLOR] & attackBB;
			results.rookAttack[COLOR] |= attackBB;
			attackBB &= removeBB;
			return ROOK_MOBILITY_MAP[BitBoardMasks::popCount(attackBB)];
		}

		/**
		 * Prints a rook index
		 */
		static void printIndex(Square square, uint16_t rookIndex) {
			cout << "Rook properties: " << (char(getFile(square) + 'A')) << int(getRank(square) + 1);
			if ((rookIndex & OPEN_FILE) != 0) { cout << " <open file>"; }
			if ((rookIndex & HALF_OPEN_FILE) != 0) { cout << " <half open file>"; }
			if ((rookIndex & TRAPPED) != 0) { cout << " <trapped by king>"; }
			if ((rookIndex & PROTECTS_PP) != 0) { cout << " <protects pp>"; }
			cout << endl;
		}

		/**
		 * Returns true, if the rook is on an open file
		 */
		static inline bool isOnOpenFile(bitBoard_t pawnsBB, bitBoard_t moveRay) {
			return (moveRay & pawnsBB) == 0;
		}

		/**
		 * Returns true, if the rook is on an half open file
		 */
		static inline bool isOnHalfOpenFile(bitBoard_t ourPawnBB, bitBoard_t moveRay) {
			return (moveRay & ourPawnBB) == 0;
		}
		
		/**
		 * Returns true, if the rook protects a passed pawn from behind
		 */
		template <Piece COLOR>
		static inline bool protectsPassedPawnFromBehind(bitBoard_t passedPawns, Square rookSquare, bitBoard_t moveRay) {
			bitBoard_t protectBB = (moveRay & passedPawns);
			return COLOR == WHITE ? 
				((1ULL << rookSquare) < protectBB) :
				(protectBB != 0 && (1ULL << rookSquare) > protectBB);
		}

		/**
		 * Returns true, if the rook is trapped by a king
		 */
		template <Piece COLOR>
		static inline bool trappedByKing(Square rookSquare, Square kingSquare) {
			static constexpr bitBoard_t kingSide[2] = { 0x00000000000000E0, 0xE000000000000000 };
			static constexpr bitBoard_t queenSide[2] = { 0x000000000000000F, 0x0F00000000000000 };
			const bitBoard_t rookAndKingBB = (1ULL << rookSquare) | (1ULL << kingSquare);
			const bitBoard_t kingSideBB = kingSide[COLOR] & rookAndKingBB;
			const bitBoard_t queenSideBB = queenSide[COLOR] & rookAndKingBB;
			bool trappedKingSide = (kingSquare < rookSquare) && (kingSideBB == rookAndKingBB);
			bool trappedQueenSide = (kingSquare > rookSquare) && (queenSideBB == rookAndKingBB);
			return trappedKingSide || trappedQueenSide;
		}

		static void addToIndexMap(uint32_t index, const value_t data[2]) {
			indexToValue[index * 2] += data[0];
			indexToValue[index * 2 + 1] += data[1];
		}

		static inline EvalValue getFromIndexMap(uint32_t index) {
			return EvalValue(indexToValue[index * 2], indexToValue[index * 2 + 1]);
		}

		static struct InitStatics {
			InitStatics();
		} _staticConstructor;

		static const uint32_t INDEX_SIZE = 16;
		static const uint32_t TRAPPED = 1;
		static const uint32_t OPEN_FILE = 2;
		static const uint32_t HALF_OPEN_FILE = 4;
		static const uint32_t PROTECTS_PP = 8;
		
		static constexpr value_t _trapped[2] = { -50, 0 }; 
		static constexpr value_t _openFile[2] = { 20, 0 };
		static constexpr value_t _halfOpenFile[2] = { 10, 0 };
		static constexpr value_t _protectsPP[2] = { 20, 0 };
		// 20:20 50.30; 40:0 51.45
		static array<value_t, INDEX_SIZE * 2> indexToValue;

		static constexpr value_t ROOK_MOBILITY_MAP[15][2] = { 
			{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 10, 10 }, { 15, 15 }, { 20, 20 },
			{ 25, 25 }, { 30, 30 }, { 30, 30 }, { 30, 30 }, { 30, 30 }, { 30, 30 }, { 30, 30 } };


#ifdef _TEST0 
		static constexpr value_t _protectsPP[2] = { 10, 0 };
#endif
#ifdef _TEST1 
		static constexpr value_t _protectsPP[2] = { 15, 5 };
#endif
#ifdef _TEST2 
		static constexpr value_t _protectsPP[2] = { 20, 10 };
#endif
#ifdef _T3 
		static constexpr value_t _protectsPP[2] = { 25, 10 };
#endif
#ifdef _T4 
		static constexpr value_t _protectsPP[2] = { 25, 15 };
#endif
#ifdef _T5
		static constexpr value_t _protectsPP[2] = { 30, 15 };
#endif
		

	};
}


#endif

