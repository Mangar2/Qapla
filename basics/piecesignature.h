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
#include "evalvalue.h"
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

	constexpr pieceSignature_t operator|(Signature a, Signature b) { return pieceSignature_t(a) | pieceSignature_t(b); }
	constexpr pieceSignature_t operator*(Signature a, pieceSignature_t b) { return pieceSignature_t(a) * b; }
	constexpr pieceSignature_t operator+(Signature a, pieceSignature_t b) { return pieceSignature_t(a) + b; }
	constexpr uint32_t operator/(uint32_t a, Signature b) { return a / uint32_t(b); }
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
		PieceSignature(pieceSignature_t signature) : _signature(signature) {}

		PieceSignature(const char* pieces) {
			set(pieces);
		}
		bool operator<(const PieceSignature aSignature) {
			return pieceSignature_t(_signature) < pieceSignature_t(aSignature);
		}
		void clear() { _signature = 0; }

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
		 * Gets a static piece value (Queen = 9, Rook = 5, Bishop & Knight = 3, >= 3 Pawns = 1)
		 * The pawns are not really counted.
		 */
		template <Piece COLOR>
		value_t getStaticPiecesValue() const { return staticPiecesValue[getSignature<COLOR>()]; }

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
			return (signature & SignatureMask::PAWN) || (signature > pieceSignature_t(Signature::BISHOP));
		}

		/**
		 * Checks for draw due to missing material
		 * Tricky but jump free implementation
		 */
		bool drawDueToMissingMaterial() const {
			const pieceSignature_t ONLY_KNIGHT_AND_BISHOP =
				pieceSignature_t(SignatureMask::ALL) & ~(Signature::BISHOP | Signature::KNIGHT);

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
				result = futilityOnCaptureMap[_signature & SignatureMask::ALL];
			}
			else {
				result = futilityOnCaptureMap[_signature >> SIG_SHIFT_BLACK];
			}
			return result;
		}

		/**
		 * Checks for futility pruning for a promotion based on the piece signature
		 */
		bool doFutilityOnPromote() const {
			bool result = true;
			result = futilityOnCaptureMap[_signature & SignatureMask::ALL] && 
				futilityOnCaptureMap[_signature >> SIG_SHIFT_BLACK];
			return result;
		}


		static const pieceSignature_t SIG_SHIFT_BLACK = 10;
		static const pieceSignature_t PIECE_SIGNATURE_SIZE = 1 << (SIG_SHIFT_BLACK * 2);

	private:
		pieceSignature_t _signature;

		inline operator pieceSignature_t() const { return _signature; }


		static array<pieceSignature_t, PIECE_AMOUNT> mapPieceToSignature;

		/**
		 * Returns the amount of pieces found in a signature
		 */
		constexpr static uint32_t getPieceAmount(pieceSignature_t signature)  {
			uint32_t result = 0;
			for (; signature != 0; signature >>= 2) {
				result += signature & 3;
			}
			return result;
		}

		/**
		 * Returns the amount of pieces of a kind found in a signature
		 */
		template <Piece KIND>
		constexpr static uint32_t getPieceAmount(pieceSignature_t signature) {
			switch (KIND) {
			case QUEEN:return (signature & SignatureMask::QUEEN) / Signature::QUEEN;
			case ROOK:return (signature & SignatureMask::ROOK) / Signature::ROOK;
			case BISHOP: return (signature & SignatureMask::BISHOP) / Signature::BISHOP;
			case KNIGHT: return (signature & SignatureMask::KNIGHT) / Signature::KNIGHT;
			case PAWN: return (signature & SignatureMask::PAWN) / Signature::PAWN;
			default: return 0;
			}
		}

		/**
		 * Maps a piece to a signature bit
		 */
		static array<pieceSignature_t, size_t(SignatureMask::ALL)> futilityOnCaptureMap;
		static array<value_t, size_t(SignatureMask::ALL)> staticPiecesValue;
		
		/**
		 * Initializes the static arrays
		 */
		static struct InitStatics {
			InitStatics();
		} _staticConstructur;

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
