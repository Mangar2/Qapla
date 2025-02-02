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
 * Implements a static class to evaluate knights 
 */

#ifndef __KNIGHT_H
#define __KNIGHT_H

#include <cstdint>
#include "../basics/types.h"
#include "../movegenerator/bitboardmasks.h"
#include "../movegenerator/movegenerator.h"
#include "../basics/evalvalue.h"
#include "evalresults.h"

using namespace QaplaBasics;
using namespace QaplaMoveGenerator;

namespace ChessEval {
	class Knight {
	public:
		template <bool PRINT>
		static EvalValue eval(const MoveGenerator& position, EvalResults& results) {
			return eval<WHITE, PRINT>(position, results) - eval<BLACK, PRINT>(position, results);
		}
	private:

		/**
		 * Evaluates Knights
		 */
		template<Piece COLOR, bool PRINT>
		static EvalValue eval(const MoveGenerator& position, EvalResults& results) {
			EvalValue value;
			constexpr Piece OPPONENT = COLOR == WHITE ? BLACK : WHITE;
			results.knightAttack[COLOR] = 0;
			bitBoard_t knights = position.getPieceBB(KNIGHT + COLOR);
			bitBoard_t removeBB = ~position.getPiecesOfOneColorBB<COLOR>() & ~position.pawnAttack[OPPONENT];

			while (knights)
			{

				const Square knightSquare = lsb(knights);
				knights &= knights - 1;
				value += calcMobility<COLOR, PRINT>(results, knightSquare, removeBB);
				value += calcPropertyValue<COLOR, PRINT>(position, knightSquare);
				if (PRINT) cout << endl;
			}

			if (PRINT) cout  
				<< colorToString(COLOR) << " knights: " 
				<< std::right << std::setw(18) << value << endl;
			return value;
		}

		/**
		 * Calculates the mobility of a knight
		 */
		template<Piece COLOR, bool PRINT>
		static EvalValue calcMobility(
			EvalResults& results, Square square, bitBoard_t removeBB)
		{
			bitBoard_t attackBB = BitBoardMasks::knightMoves[square];
			results.knightAttack[COLOR] |= attackBB;
			results.piecesDoubleAttack[COLOR] |= results.piecesAttack[COLOR] & attackBB;
			results.piecesAttack[COLOR] |= attackBB;

			attackBB &= removeBB;
			const EvalValue value = KNIGHT_MOBILITY_MAP[popCountForSparcelyPopulatedBitBoards(attackBB)];
			if (PRINT) cout << colorToString(COLOR)
				<< " knight (" << squareToString(square) << ") mobility: "
				<< std::right << std::setw(5) << value;
			return value;
		}

		/**
		 * Checks, if a knight is an outpost - i.e. a Knight in the enemy teretory protected by a pawn
		 * and not attackable by a pawn
		 */
		template<Piece COLOR, bool PRINT>
		static inline bool isOutpost(Square square, bitBoard_t opponentPawnsBB, const MoveGenerator& position) {
			bool result = false;
			bitBoard_t knightBB = 1ULL << square;
			bool isProtectedByPawnAndInOpponentArea =
				(knightBB & OUTPOST_BB[COLOR] & position.pawnAttack[COLOR]) != 0;
			if (isProtectedByPawnAndInOpponentArea) {
				bitBoard_t shift = COLOR == WHITE ? (square + NW) : (square + SOUTH + SW);
				bitBoard_t opponentPawnCheckBB = 0x505ULL << shift;
				result = (opponentPawnCheckBB & opponentPawnsBB) == 0;
			}
			if (PRINT && result) cout << "<otp>";
			return result;
		}

		/**
		 * Returns true, if the knight is pinned
		 */
		template<bool PRINT>
		static inline bool isPinned(bitBoard_t pinnedBB, Square square) {
			bool result = (pinnedBB & (1ULL << square)) != 0;
			if (PRINT && result) cout << "<pin>";
			return result;
		}

		/**
		 * Calculates properties and their Values for Knights
		 */
		template<Piece COLOR, bool PRINT>
		static inline EvalValue calcPropertyValue(const MoveGenerator& position, Square knightSquare)
		{
			uint16_t knightIndex = 0;
			constexpr Piece OPPONENT = COLOR == WHITE ? BLACK : WHITE;
			const bitBoard_t opponentPawnBB = position.getPieceBB(PAWN + OPPONENT);
			knightIndex += isOutpost<COLOR, PRINT>(knightSquare, opponentPawnBB, position) * OUTPOST;
			knightIndex += isPinned<PRINT>(position.pinnedMask[COLOR], knightSquare) * PINNED;
			return getFromIndexMap(knightIndex);
		}

		/**
		 * Gets a value pair from the index map
		 */
		static inline EvalValue getFromIndexMap(uint32_t index) {
			return EvalValue(_indexToValue[index * 2], _indexToValue[index * 2 + 1]);
		}

		// Mobility Map for knights
		static constexpr value_t KNIGHT_MOBILITY_MAP[9][2] = { 
			{ -30, -30 }, { -20, -20 }, { -10, -10 }, { 0, 0 }, { 10, 10 }, { 20, 20 }, 
			{ 25, 25 }, { 25, 25 }, { 25, 25 }
		};

		static const uint32_t OUTPOST = 1;
		static const uint32_t PINNED = 2;
		
		static constexpr value_t _outpost[2] = { 20, 0 };
		static constexpr value_t _pinned[2] = { 0, 0 };
		static constexpr value_t _indexToValue[8] = { 
			0, 0, 
			_outpost[0], _outpost[1],
			_pinned[0], _pinned[1], 
			_pinned[0] + _outpost[0], _pinned[1] + _outpost[1]
		};

		static constexpr bitBoard_t OUTPOST_BB[2] = { 0x003C3C3C00000000, 0x000000003C3C3C00 };
	};
}


#endif

