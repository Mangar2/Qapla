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
 * Provides an access to the chess position - adapter to different chess boards
 * of different chess engines
 */


#ifndef __BOARDACCESS_H
#define __BOARDACCESS_H

#include <vector>
#include <fstream>
#include "../basics/move.h"
#include "bitbaseindex.h"
#include "../movegenerator/bitboardmasks.h"
#include "../movegenerator/movegenerator.h"

using namespace ChessMoveGenerator;

namespace QaplaBitbase {

	class BoardAccess {
	public:
		/**
		 * Computes a _bitbase index from a position position
		 */
		template<bool SYMETRIC>
		static uint64_t getIndex(const MoveGenerator& position) {
			PieceList pieceList(position);
			bool wtm = position.isWhiteToMove() ^ SYMETRIC;
			if (SYMETRIC) pieceList.toSymetric();
			BitbaseIndex bitBaseIndex(pieceList, wtm);
			uint64_t index = bitBaseIndex.getIndex();
			return index;
		}

		/**
		 * Computes a _bitbase index from piece list and a move (applied to the piece list)
		 * The move must not change the amount or type of the pieces in the list (no capture, no promote)
		 */
		static uint64_t getIndex(bool wtm, const PieceList& pieceList, Move move) {
			assert(!move.isCapture());
			assert(!move.isPromote());
			PieceList pieceListAfterMove(pieceList);
			setMoveToPieceList(pieceListAfterMove, move);
			BitbaseIndex bitBaseIndex(pieceListAfterMove, wtm);
			uint64_t index = bitBaseIndex.getIndex();
			return index;
		}


	private:

		/**
		 * Sets a bitbase index having a piece list with pieces and squares
		 */
		static void setMoveToPieceList(PieceList& pieceList, Move move) {
			Square departure = move.getDeparture();
			for (uint32_t index = 0; index < pieceList.getNumberOfPieces(); index++) {
				if (pieceList.getSquare(index) == departure) {
					pieceList.setSquare(index, move.getDestination());
				}
			}
		}
	};

}

#endif // __BOARDACCESS_H