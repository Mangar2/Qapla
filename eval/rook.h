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
using namespace QaplaMoveGenerator;

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
			constexpr Piece OPPONENT = COLOR == WHITE ? BLACK : WHITE;
			EvalValue value;
			results.rookAttack[COLOR] = 0;
			bitBoard_t rooks = position.getPieceBB(ROOK + COLOR);
			if (rooks == 0) return EvalValue(0, 0);

			bitBoard_t passThrough = results.queensBB | rooks;
			bitBoard_t occupiedBB = position.getAllPiecesBB();
			bitBoard_t removeMask = (~position.getPiecesOfOneColorBB<COLOR>() | passThrough) &
				~position.pawnAttack[OPPONENT];
			occupiedBB &= ~passThrough;
			value += rooksOnRow7<COLOR, PRINT>(rooks, position.getPieceBB(QUEEN + COLOR));

			while (rooks)
			{
				const Square rookSquare = lsb(rooks);
				rooks &= rooks - 1;
				value += calcMobility<COLOR, PRINT>(results, rookSquare, occupiedBB, removeMask);
				value += calcPropertyValue<COLOR, PRINT>(position, results, rookSquare);
			}
			if (PRINT) cout << colorToString(COLOR) << " rooks: "
				<< std::right << std::setw(20) << value << endl;
			return value;
		}

		/**
		 * Calculates several properties for a rook and return their value
		 * Is on open file
		 * Is on half open file
		 * Is protecting a passed pawn from behind
		 * Is trapped by king
		 */
		template<Piece COLOR, bool PRINT>
		static inline EvalValue calcPropertyValue(const MoveGenerator& position, EvalResults& results,
			Square rookSquare)
		{
			uint16_t rookIndex = 0;
			constexpr Piece OPPONENT = COLOR == WHITE ? BLACK : WHITE;
			const bitBoard_t ourPawnBB = position.getPieceBB(PAWN + COLOR);
			const bitBoard_t theirPawnBB = position.getPieceBB(PAWN + OPPONENT);
			const Square rank8Destination = (COLOR == WHITE ? A8 : A1) + Square(getFile(rookSquare));
			const bitBoard_t moveRay = BitBoardMasks::fileBB[int(getFile(rookSquare))];
			rookIndex += isOnOpenFile(results.pawnsBB, moveRay) * OPEN_FILE;
			rookIndex += isOnHalfOpenFile(ourPawnBB, moveRay) * HALF_OPEN_FILE;
			rookIndex += trappedByKing<COLOR>(rookSquare, position.getKingSquare<COLOR>()) * TRAPPED;
			rookIndex +=
				protectsPassedPawnFromBehind<COLOR>(results.passedPawns[COLOR], rookSquare, moveRay) * PROTECTS_PP;
			rookIndex += isPinned(position.pinnedMask[COLOR], rookSquare) * PINNED;
			if (PRINT) printIndex(rookSquare, rookIndex);
			return getFromIndexMap(rookIndex);
		}

		/**
		 * Calculates the results of a rook
		 */
		template<Piece COLOR, bool PRINT>
		static EvalValue calcMobility(
			EvalResults& results, Square square, bitBoard_t occupiedBB, bitBoard_t removeBB)
		{
			bitBoard_t attackBB = Magics::genRookAttackMask(square, occupiedBB);
			results.rookAttack[COLOR] |= attackBB;
			results.piecesDoubleAttack[COLOR] |= results.piecesAttack[COLOR] & attackBB;
			results.piecesAttack[COLOR] |= attackBB;

			attackBB &= removeBB;
			const EvalValue value = ROOK_MOBILITY_MAP[popCount(attackBB)];
			if (PRINT) cout << colorToString(COLOR)
				<< " rook (" << squareToString(square) << ") mobility: "
				<< std::right << std::setw(7) << value;
			return value;
		}

		/**
		 * Prints a rook index
		 */
		static void printIndex(Square square, uint16_t rookIndex) {
			if ((rookIndex & OPEN_FILE) != 0) { cout << " <of>"; }
			if ((rookIndex & HALF_OPEN_FILE) != 0) { cout << " <hof>"; }
			if ((rookIndex & TRAPPED) != 0) { cout << " <tbk>"; }
			if ((rookIndex & PROTECTS_PP) != 0) { cout << " <ppp>"; }
			if ((rookIndex & PINNED) != 0) { cout << " <pin>"; }
			cout << endl;
		}

		/**
		 * Returns true, if the rook is pinned
		 */
		static inline bool isPinned(bitBoard_t pinnedBB, Square square) {
			return (pinnedBB & (1ULL << square)) != 0;
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

		template <Piece COLOR, bool PRINT>
		static inline EvalValue rooksOnRow7(bitBoard_t rookBB, bitBoard_t queenBB) {
			const auto rooksRow7BB = (rookBB | queenBB) & ROW_7[COLOR];
			const auto amountOnRow7 = popCountBrianKernighan(rooksRow7BB);
			const EvalValue valueForRow7 =  ROOK_ROW_7_MAP[amountOnRow7];
			if (PRINT && amountOnRow7 > 0) cout << colorToString(COLOR)
				<< " rook row 7:"
				<< std::right << std::setw(16) << valueForRow7
				<< endl;
			return valueForRow7;
		}

		/**
		 * Adds a value pair to the index map
		 */
		static void addToIndexMap(uint32_t index, const value_t data[2]) {
			_indexToValue[index * 2] += data[0];
			_indexToValue[index * 2 + 1] += data[1];
		}

		/**
		 * Gets a value pair from the index map
		 */
		static inline EvalValue getFromIndexMap(uint32_t index) {
			return EvalValue(_indexToValue[index * 2], _indexToValue[index * 2 + 1]);
		}

		/**
		 * Initializes the index map on program start
		 */
		static struct InitStatics {
			InitStatics();
		} _staticConstructor;

		static constexpr bitBoard_t ROW_7[COLOR_COUNT] = { 0x00FF000000000000, 0x000000000000FF00 };

		static const uint32_t INDEX_SIZE = 32;
		static const uint32_t TRAPPED = 1;
		static const uint32_t OPEN_FILE = 2;
		static const uint32_t HALF_OPEN_FILE = 4;
		static const uint32_t PROTECTS_PP = 8;
		static const uint32_t PINNED = 16;
		
		static constexpr value_t _trapped[2] = { -50, 0 }; 
		static constexpr value_t _openFile[2] = { 10, 0 };
		static constexpr value_t _halfOpenFile[2] = { 10, 0 };
		static constexpr value_t _protectsPP[2] = { 20, 0 };
		static constexpr value_t _pinned[2] = { 0, 0 };
		
		// Lookup table to get an evaluation for all topics in one access via. bitmask. Saves lots of if/else
		static array<value_t, INDEX_SIZE * 2> _indexToValue;

		// Row 7 map
		static constexpr value_t ROOK_ROW_7_MAP[8][2] = {
			{ 0, 0 }, { 10, 0 }, { 20, 0 }, { 40, 0 }, { 40, 0 }, { 40, 0 }, { 40, 0 }, { 40, 0 }
		};

		// Mobility Map
		static constexpr value_t ROOK_MOBILITY_MAP[15][2] = {
			{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 8, 8 }, { 12, 12 }, { 16, 16 },
			{ 20, 20 }, { 25, 25 }, { 25, 25 }, { 25, 25 }, { 25, 25 }, { 25, 25 }, { 25, 25 } };
	};
}


#endif

