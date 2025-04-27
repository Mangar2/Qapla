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
 * Adds additional state information of a board in a compressed form:
 * - Amount of moves without pawn move or capture
 * - board hash
 * - pawn hash
 * - en passant square
 * - castling rights 
 * En passant and castling rights are coded in a 32 bit value:
 * kqKQEEEEEEEE
 * E = En passant square
 * Q = White Queen side castleing allowed
 * K = White King side castleing allowed
 * q = Black Queen side castleing allowed
 * k = Black King side castleing allowed
 */

#ifndef __BOARDSTATE_H
#define __BOARDSTATE_H

#include "types.h"
#include "hashconstants.h"

namespace QaplaBasics {

	class BoardState {
	public:

		BoardState() { initialize(); };

		bool operator==(const BoardState& boardInfo) const {
			return _info == boardInfo._info;
		}

		/**
		 * Checks, if castling king side is allowed
		 */
		template <Piece COLOR>
		bool isKingSideCastleAllowed() {
			return (_info & (COLOR == WHITE ? WHITE_KING_SIDE_CASTLE_BIT : BLACK_KING_SIDE_CASTLE_BIT)) != 0;
		}

		/**
		 * Checks, if castling queen side is allowed
		 */
		template <Piece COLOR>
		bool isQueenSideCastleAllowed() {
			return (_info & (COLOR == WHITE ? WHITE_QUEEN_SIDE_CASTLE_BIT : BLACK_QUEEN_SIDE_CASTLE_BIT)) != 0;
		}

		/**
		 * sets the status white king moved
		 */
		void setWhiteKingMoved() {
			_info &= ~(WHITE_QUEEN_SIDE_CASTLE_BIT | WHITE_KING_SIDE_CASTLE_BIT);
		}

		/**
		 * sets the status black king moved
		 */
		void setBlackKingMoved() {
			_info &= ~(BLACK_QUEEN_SIDE_CASTLE_BIT | BLACK_KING_SIDE_CASTLE_BIT);
		}

		/**
		 * sets the status white queen side rook moved
		 */
		void setWhiteQueenSideRookMoved() {
			_info &= ~(WHITE_QUEEN_SIDE_CASTLE_BIT);
		}

		/**
		 * sets the status white king side rook moved
		 */
		void setWhiteKingSideRookMoved() {
			_info &= ~(WHITE_KING_SIDE_CASTLE_BIT);
		}

		/**
		 * sets the status black queen side rook moved
		 */
		void setBlackQueenSideRookMoved() {
			_info &= ~(BLACK_QUEEN_SIDE_CASTLE_BIT);
		}

		/**
		 * sets the status black king side rook moved
		 */
		void setBlackKingSideRookMoved() {
			_info &= ~(BLACK_KING_SIDE_CASTLE_BIT);
		}

		/**
		 * * Enable/Disable castling right
		 */
		void setCastlingRight(Piece color, bool kingSide, bool allow) {
			uint16_t bit = 0;
			if (color == WHITE) {
				bit = kingSide ? WHITE_KING_SIDE_CASTLE_BIT : WHITE_QUEEN_SIDE_CASTLE_BIT;
			}
			if (color == BLACK) {
				bit = kingSide ? BLACK_KING_SIDE_CASTLE_BIT : BLACK_QUEEN_SIDE_CASTLE_BIT;
			}
			if (allow) {
				_info |= bit;
			}
			else {
				_info &= ~bit;
			}
		}

		/**
		 * Disable any castling right using a mask
		 */
		void disableCastlingRightsByMask(uint32_t mask) {
			_info &= mask;
		}

		/**
		 * Gets the castling right mask
		 */
		uint16_t getCastlingRightsMask() const {
			return (_info & CASTLE_MASK) >> CASTLE_SHIFT;
		}

		/**
		 * Retrieves the EP square = the square the opponent pawn moved to
		 */
		Square getEP() const { return Square(_info & EP_MASK); }
		void setEP(Square epSquare) { clearEP(); _info |= uint32_t(epSquare); }

		bool hasEP() const { return (_info & EP_MASK) != 0; }
		void clearEP() { _info &= ~EP_MASK; }

		/**
		 * computes the curren board hash
		 */
		inline hash_t computeBoardHash() const {
			hash_t result = boardHash
				^ HashConstants::EP_RANDOMS[getEP()]
				^ HashConstants::CASTLE_RANDOMS[getCastlingRightsMask()];
			return result;
		}

		/**
		 * Updates the hashes for a piece
		 */
		inline void updateHash(Square square, Piece piece) {
			boardHash ^= HashConstants::cHashBoardRandoms[square][piece];
			if (isPawn(piece)) {
				pawnHash ^= HashConstants::cHashBoardRandoms[square][piece];
			}
		}

		/**
		 * Initializes all members
		 */
		void initialize() {
			clearEP();
			_info = 0;
			setCastlingRight(WHITE, true, false);
			setCastlingRight(WHITE, false, false);
			setCastlingRight(BLACK, true, false);
			setCastlingRight(BLACK, false, false);
			halfmovesWithoutPawnMoveOrCapture = 0;
			fenHalfmovesWithoutPawnMoveOrCapture = 0;
			pawnHash = 0;
			boardHash = 0;
		}

		static uint16_t const WHITE_QUEEN_SIDE_CASTLE_BIT = 0x0100;
		static uint16_t const WHITE_KING_SIDE_CASTLE_BIT = 0x0200;
		static uint16_t const BLACK_QUEEN_SIDE_CASTLE_BIT = 0x0400;
		static uint16_t const BLACK_KING_SIDE_CASTLE_BIT = 0x0800;

		uint16_t halfmovesWithoutPawnMoveOrCapture;
		uint16_t fenHalfmovesWithoutPawnMoveOrCapture;
		uint64_t boardHash;
		uint64_t pawnHash;

	private:

		uint16_t _info;
		static uint16_t const CASTLE_MASK = 0xF00;
		static uint16_t const CASTLE_SHIFT = 8;
		static uint16_t const EP_MASK = 0x00FF;
	};

}

#endif // __BOARDSTATE_H

