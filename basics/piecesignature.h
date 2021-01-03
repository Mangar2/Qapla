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
 * Defines a bitmap representing the available pieces at the board
 */

#ifndef __PIECESIGNATURE_H
#define __PIECESIGNATURE_H

#include <array>
#include "move.h"

using namespace std;

namespace ChessBasics {

	typedef uint32_t pieceSignature_t;

	/**
	 * Bit of a piece in the signature field
	 */
	enum class Signature {
		EMPTY = 0,
		PAWN = 0x00001,
		KNIGHT = 0x00004,
		BISHOP = 0x00010,
		ROOK = 0x00040,
		QUEEN = 0x00100
	};

	constexpr pieceSignature_t operator*(Signature a, pieceSignature_t b) { return pieceSignature_t(a) * b; }
	constexpr pieceSignature_t operator+(Signature a, pieceSignature_t b) { return pieceSignature_t(a) + b; }
	// constexpr pieceSignature_t operator+(Signature a, Signature b) { return pieceSignature_t(a) + pieceSignature_t(b); }

	/**
	 * Mask to extract a dedicated piece type form the bit field
	 */
	enum class SignatureMask {
		PAWN = Signature::PAWN * 3,
		KNIGHT = Signature::KNIGHT * 3,
		BISHOP = Signature::BISHOP * 3,
		ROOK = Signature::ROOK * 3,
		QUEEN = Signature::QUEEN * 3,
		ALL = PAWN + KNIGHT + BISHOP + ROOK + QUEEN,
		SIZE = ALL + 1
	};
	constexpr pieceSignature_t operator|(SignatureMask a, SignatureMask b) { return pieceSignature_t(a) | pieceSignature_t(b); }
	constexpr pieceSignature_t operator|(pieceSignature_t a, SignatureMask b) { return a | pieceSignature_t(b); }
	constexpr pieceSignature_t operator&(SignatureMask a, SignatureMask b) { return pieceSignature_t(a) & pieceSignature_t(b); }
	constexpr pieceSignature_t operator&(pieceSignature_t a, SignatureMask b) { return a & pieceSignature_t(b); }
	constexpr pieceSignature_t operator~(SignatureMask a) { return ~pieceSignature_t(a); }


	class PieceSignature {
	public:

		PieceSignature() : _signature(0) {}
		PieceSignature(const PieceSignature& _signature) {
			operator=(_signature);
		}
		PieceSignature(const char* pieces) {
			set(pieces);
		}
		bool operator<(const PieceSignature aSignature) {
			return pieceSignature_t(_signature) < pieceSignature_t(aSignature);
		}
		void clear() { _signature = 0; }

		PieceSignature& operator=(const PieceSignature& pieceSignature) { 
			_signature = pieceSignature._signature; return *this; 
		}

		/**
		 * Checks, if a piece is available more than twice
		 */
		inline bool moreThanTwoPiecesInBitBoard(bitBoard_t bitBoard) const {
			bitBoard &= bitBoard - 1;
			bitBoard &= bitBoard - 1;
			return bitBoard != 0;
		}

		/**
		 * Checks, if a piece is available more than once
		 */
		inline bool moreThanOnePieceInBitBoard(bitBoard_t bitBoard) const {
			bitBoard &= bitBoard - 1;
			return bitBoard != 0;
		}

		/**
		 * Adds a piece to the signature
		 */
		void addPiece(Piece piece, bitBoard_t pieceBitboardBeforeAddingPiece) {
			if (!moreThanTwoPiecesInBitBoard(pieceBitboardBeforeAddingPiece)) {
				_signature += mapPieceToSignature[piece];
			}
		}

		/**
		 * Removes a piece from the signature
		 */
		void removePiece(Piece piece, bitBoard_t pieceBitboardAfterRemovingPiece) {
			if (!moreThanTwoPiecesInBitBoard(pieceBitboardAfterRemovingPiece)) {
				_signature -= mapPieceToSignature[piece];
			}
		}

		/**
		 * Gets the piece signature of a color
		 */
		template <Piece COLOR> 
		constexpr pieceSignature_t getSignature() const {
			return (COLOR == WHITE) ? (_signature & pieceSignature_t(SignatureMask::ALL)) : _signature >> SIG_SHIFT_BLACK;
		}

		/**
		 * Gets the signature of all pieces
		 */
		inline pieceSignature_t getPiecesSignature() const {
			return _signature;
		}

		/**
		 * Checks if a side has a range piece
		 */
		template <Piece COLOR>
		inline bool hasQueenOrRookOrBishop() const {
			constexpr pieceSignature_t mask = SignatureMask::QUEEN | SignatureMask::ROOK | SignatureMask::BISHOP;
			return (getSignature<COLOR>() & mask) != 0;
		}

		/**
		 * Checks, if the side to move has a range piece
		 */
		inline bool sideToMoveHasQueenRookBishop(bool whiteToMove) const {
			return whiteToMove ? hasQueenOrRookOrBishop<WHITE>() : hasQueenOrRookOrBishop<BLACK>();
		}

		/**
		 * Checks, if a side has enough material to mate
		 */
		template <Piece COLOR>
		bool hasEnoughMaterialToMate() const {
			pieceSignature_t signature = getSignature<COLOR>();
			return (signature & SignatureMask::PAWN) || (signature > Signature::BISHOP);
		}

		/**
		 * Checks for draw due to missing material
		 * Tricky but jump free implementation
		 */
		bool drawDueToMissingMaterial() const {
			const pieceSignature_t ONLY_KNIGHT_AND_BISHOP =
				pieceSignature_t(SignatureMask::ALL) & ~(SignatureMask::BISHOP | SignatureMask::KNIGHT);
			// Checks that no other bit is set than "one bishop" and "one knight"
			bool anyColorNotMoreThanOneNightAndOneBishop = 
				(_signature & (ONLY_KNIGHT_AND_BISHOP | (ONLY_KNIGHT_AND_BISHOP << SIG_SHIFT_BLACK))) == 0;
			// Checks that "one bishop" and "one knight" is not set at the same time
			bool hasEitherKnightOrBishop = (_signature & (_signature >> 2)) == 0;
			return anyColorNotMoreThanOneNightAndOneBishop && hasEitherKnightOrBishop;
		}

		/**
		 * Tansforms a string constant in a piece signature (like KKN = one Knight)
		 */
		bool set(string pieces, uint32_t iteration = 0) {
			_signature = 0;
			pieceSignature_t shift = 0;
			pieceSignature_t lastSignature = 0;

			for (int pos = 0; pos < 10 && pieces[pos] != 0; pos++) {
				switch (pieces[pos]) {
				case 'K':
					if (pos > 0) {
						shift = SIG_SHIFT_BLACK;
					}
					break;
				case '+':
					_signature += lastSignature * (iteration % 3);
					iteration /= 3;
					break;
				case '*':
					_signature -= lastSignature;
					_signature += lastSignature * (iteration % 4);
					iteration /= 4;
					break;
				default:
					lastSignature = charToSignature(pieces[pos]) << shift;
					_signature += lastSignature;
					break;
				}
			}
			return iteration == 0;
		}

		/**
		 * Debugging functionality: swap white and black signature
		 */
		void changeSide() {
			_signature = (getSignature<WHITE>() << SIG_SHIFT_BLACK) + getSignature<BLACK>();
		}

		/**
		 * Checks for futility pruning for a capture
		 */
		bool doFutilityOnCapture(Piece capturedPiece) const {
			bool result = true;
			if (getPieceColor(capturedPiece) == WHITE) {
				result = futilityOnCaptureMap()[_signature & SignatureMask::ALL];
			}
			else {
				result = futilityOnCaptureMap()[_signature >> SIG_SHIFT_BLACK];
			}
			return result;
		}

		/**
		 * Checks for futility pruning for a promotion based on the piece signature
		 */
		bool doFutilityOnPromote() const {
			bool result = true;
			result = futilityOnCaptureMap()[_signature & SignatureMask::ALL] && 
				futilityOnCaptureMap()[_signature >> SIG_SHIFT_BLACK];
			return result;
		}

		static const pieceSignature_t SIG_SHIFT_BLACK = 10;
		static const pieceSignature_t PIECE_SIGNATURE_SIZE = 1 << (SIG_SHIFT_BLACK * 2);

	private:
		pieceSignature_t _signature;

		inline operator pieceSignature_t() const { return _signature; }

		struct PieceToSignature {
			PieceToSignature() {
				map.fill(pieceSignature_t(Signature::EMPTY));
				map[WHITE_PAWN] = pieceSignature_t(Signature::PAWN);
				map[WHITE_KNIGHT] = pieceSignature_t(Signature::KNIGHT);
				map[WHITE_BISHOP] = pieceSignature_t(Signature::BISHOP);
				map[WHITE_ROOK] = pieceSignature_t(Signature::ROOK);
				map[WHITE_QUEEN] = pieceSignature_t(Signature::QUEEN);
				map[BLACK_PAWN] = pieceSignature_t(Signature::PAWN) << SIG_SHIFT_BLACK;
				map[BLACK_KNIGHT] = pieceSignature_t(Signature::KNIGHT) << SIG_SHIFT_BLACK;
				map[BLACK_BISHOP] = pieceSignature_t(Signature::BISHOP) << SIG_SHIFT_BLACK;
				map[BLACK_ROOK] = pieceSignature_t(Signature::ROOK) << SIG_SHIFT_BLACK;
				map[BLACK_QUEEN] = pieceSignature_t(Signature::QUEEN) << SIG_SHIFT_BLACK;
			}
			pieceSignature_t operator[](Piece piece) const {
				return pieceSignature_t(map[pieceSignature_t(piece)]);
			}
			array<pieceSignature_t, PIECE_AMOUNT> map;
		};

		static PieceToSignature mapPieceToSignature;

		/**
		 * Returns the amount of pieces found in a signature
		 */
		constexpr uint32_t getPieceAmount(pieceSignature_t signature) const {
			uint32_t result = 0;
			for (; signature != 0; signature >>= 2) {
				result += signature & 3;
			}
			return result;
		}


		/**
		 * Maps a piece to a signature bit
		 */
		constexpr array<pieceSignature_t, PIECE_AMOUNT> futilityOnCaptureMap() const {
			array<pieceSignature_t, PIECE_AMOUNT> map{};
			for (uint32_t index = 0; index <= uint32_t(SignatureMask::ALL); index++) {
				map[index] = true;
				if (getPieceAmount(index) <= 2) {
					map[index] = false;
				}
			}
			return map;
		}

		/**
		 * Maps a piece char to a piece signature bit
		 */
		pieceSignature_t charToSignature(char piece) {
			Signature result = Signature::EMPTY;
			switch (piece) {
			case 'Q': result = Signature::QUEEN; break;
			case 'R': result = Signature::ROOK; break;
			case 'B': result = Signature::BISHOP; break;
			case 'N': result = Signature::KNIGHT; break;
			case 'P': result = Signature::PAWN; break;
			}
			return pieceSignature_t(result);
		}
	};

}

#endif // __PIECESIGNATURE_H
