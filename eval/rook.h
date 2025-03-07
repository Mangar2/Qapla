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
 * Implements a static class to evaluate rooks 
 */

#ifndef __ROOK_H
#define __ROOK_H

#include <cstdint>
#include "../basics/types.h"
#include "../movegenerator/bitboardmasks.h"
#include "../movegenerator/movegenerator.h"
#include "../basics/evalvalue.h"
#include "print-eval.h"
#include "eval-map.h"
#include "evalresults.h"

using namespace QaplaBasics;
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
				int32_t rookIndex = 
					calcPropertyValue<COLOR, PRINT>(position, results, rookSquare) +
					calcMobility<COLOR, PRINT>(results, rookSquare, occupiedBB, removeMask) * 32;

				// if (PRINT) printIndex(rookSquare, rookIndex);
				const EvalValue rookValue = evalMap.getValue(rookIndex);
				if (PRINT) printValue("rook", COLOR, rookValue, rookSquare);
				value += rookValue;
			}
			if (PRINT) printValue("rooks", COLOR, value);
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
		static inline uint32_t calcPropertyValue(const MoveGenerator& position, EvalResults& results,
			Square rookSquare)
		{
			uint32_t rookIndex = 0;
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
			return rookIndex;
		}

		/**
		 * Calculates the results of a rook
		 */
		template<Piece COLOR, bool PRINT>
		static uint32_t calcMobility(
			EvalResults& results, Square square, bitBoard_t occupiedBB, bitBoard_t removeBB)
		{
			bitBoard_t attackBB = Magics::genRookAttackMask(square, occupiedBB);
			results.rookAttack[COLOR] |= attackBB;
			results.piecesDoubleAttack[COLOR] |= results.piecesAttack[COLOR] & attackBB;
			results.piecesAttack[COLOR] |= attackBB;

			attackBB &= removeBB;
			return popCount(attackBB);
		}

		/**
		 * Prints a rook index
		 */
		static string getIndexStr(Square square, uint16_t rookIndex) {
			string result = "Mob: " + std::to_string(rookIndex / 32);
			if ((rookIndex & OPEN_FILE) != 0) { result += " <of>"; }
			if ((rookIndex & HALF_OPEN_FILE) != 0) { result += " <hof>"; }
			if ((rookIndex & TRAPPED) != 0) { result += "<tbk>"; }
			if ((rookIndex & PROTECTS_PP) != 0) { result += " <ppp>"; }
			if ((rookIndex & PINNED) != 0) { result += " <pin>"; }
			return result;
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
			if (PRINT && amountOnRow7 > 0) {
				cout << colorToString(COLOR)
					<< " rook row 7:"
					<< std::right << std::setw(16) << valueForRow7
					<< endl;
			}
			return valueForRow7;
		}


		static constexpr bitBoard_t ROW_7[COLOR_COUNT] = { 0x00FF000000000000, 0x000000000000FF00 };
		// Row 7 map
		static constexpr value_t ROOK_ROW_7_MAP[8][2] = {
			{ 0, 0 }, { 10, 0 }, { 20, 0 }, { 40, 0 }, { 40, 0 }, { 40, 0 }, { 40, 0 }, { 40, 0 }
		};

		static const uint32_t INDEX_SIZE = 32 * 16;
		static const uint32_t TRAPPED = 1;
		static const uint32_t OPEN_FILE = 2;
		static const uint32_t HALF_OPEN_FILE = 4;
		static const uint32_t PROTECTS_PP = 8;
		static const uint32_t PINNED = 16;
		
		inline static EvalMap<INDEX_SIZE, 2> evalMap = [] {
			const value_t _trapped[2] = { -50, 0 };
			const value_t _openFile[2] = { 10, 0 };
			const value_t _halfOpenFile[2] = { 10, 0 };
			const value_t _protectsPP[2] = { 20, 0 };
			const value_t _pinned[2] = { 0, 0 };

			// Mobility Map
			const value_t ROOK_MOBILITY_MAP[15][2] = {
				{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 8, 8 }, { 12, 12 }, { 16, 16 },
				{ 20, 20 }, { 25, 25 }, { 25, 25 }, { 25, 25 }, { 25, 25 }, { 25, 25 }, { 25, 25 } };

			EvalMap<INDEX_SIZE, 2> map;
			for (uint32_t mobility = 0; mobility < 16; ++mobility) {
				for (uint32_t bitmask = 0; bitmask < 32; ++bitmask) {
					EvalValue value;
					if (bitmask & TRAPPED) { value += _trapped; }
					if (bitmask & OPEN_FILE) { value += _openFile; }
					if (bitmask & HALF_OPEN_FILE) { value += _halfOpenFile; }
					if (bitmask & PROTECTS_PP) { value += _protectsPP; }
					if (bitmask & PINNED) { value += _pinned; }
					value += ROOK_MOBILITY_MAP[mobility];
					map.setValue(mobility * 32 + bitmask, value.getValue());
				}
			}
			return map;
		} ();
	};
}


#endif

