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
 * Tells whether a move pushes a passed pawn, for use inside the search. The eval knows its
 * passed pawns from the pawn hash, but that information does not reach the move loop.
 */

#ifndef __PASSEDPAWN_H
#define __PASSEDPAWN_H

#include <array>
#include "../basics/types.h"
#include "../movegenerator/movegenerator.h"

using namespace QaplaBasics;
using namespace QaplaMoveGenerator;

namespace QaplaSearch {

	namespace PassedPawnDetail {
		/**
		 * Every square ahead of a square on its own file and on both adjacent files. An opponent
		 * pawn on one of them either blocks the pawn or can capture it on its way.
		 */
		constexpr std::array<std::array<bitBoard_t, 64>, 2> computeFrontSpans() {
			constexpr bitBoard_t FILE_A = 0x0101010101010101ULL;
			std::array<std::array<bitBoard_t, 64>, 2> result{};
			for (uint32_t square = 0; square < 64; ++square) {
				const uint32_t file = square % 8;
				const uint32_t rank = square / 8;
				bitBoard_t files = FILE_A << file;
				if (file > 0) files |= FILE_A << (file - 1);
				if (file < 7) files |= FILE_A << (file + 1);
				const bitBoard_t ahead = rank >= 7 ? 0ULL : ~0ULL << ((rank + 1) * 8);
				const bitBoard_t behind = rank == 0 ? 0ULL : (1ULL << (rank * 8)) - 1;
				result[WHITE][square] = files & ahead;
				result[BLACK][square] = files & behind;
			}
			return result;
		}

		constexpr std::array<std::array<bitBoard_t, 64>, 2> FRONT_SPAN = computeFrontSpans();
	}

	class PassedPawn {
	public:
		/**
		 * Checks whether the move pushes a pawn that no opponent pawn can stop on its way. A
		 * promotion always counts, it is the last push of a pawn that already got through.
		 */
		static bool isPassedPawnPush(const MoveGenerator& board, Move move) {
			const Piece movingPiece = move.getMovingPiece();
			if (!isPawn(movingPiece)) return false;
			if (move.isPromote()) return true;
			const uint32_t destination = uint32_t(move.getDestination());
			return getPieceColor(movingPiece) == WHITE
				? (FRONT_SPAN[WHITE][destination] & board.getPieceBB(BLACK_PAWN)) == 0
				: (FRONT_SPAN[BLACK][destination] & board.getPieceBB(WHITE_PAWN)) == 0;
		}

	private:
		static constexpr auto& FRONT_SPAN = PassedPawnDetail::FRONT_SPAN;
	};
}

#endif // __PASSEDPAWN_H
