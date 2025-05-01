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
 * Defines a bitmap representing the available pieces at the board
 */

#pragma once

#include <string.h>
#include <assert.h>
#include <array>
#include <vector>
#include <tuple>
#include "evalvalue.h"
#include "bits.h"

namespace QaplaBasics
{

	typedef uint32_t pieceSignature_t;

	/**
	 * Bit of a piece in the signature field
	 */
	enum class Signature
	{
		EMPTY = 0,
		PAWN = 1 << 0,
		KNIGHT = 1 << 4,
		BISHOP = 1 << 6,
		ROOK = 1 << 8,
		QUEEN = 1 << 10
	};

	constexpr pieceSignature_t operator|(Signature a, Signature b) { return pieceSignature_t(a) | pieceSignature_t(b); }
	constexpr pieceSignature_t operator*(Signature a, pieceSignature_t b) { return pieceSignature_t(a) * b; }
	constexpr pieceSignature_t operator+(Signature a, pieceSignature_t b) { return pieceSignature_t(a) + b; }
	constexpr uint32_t operator/(uint32_t a, Signature b) { return a / uint32_t(b); }
	// constexpr pieceSignature_t operator+(Signature a, Signature b) { return pieceSignature_t(a) + pieceSignature_t(b); }

	/**
	 * Mask to extract a dedicated piece type form the bit field
	 */
	enum class SignatureMask
	{
		PAWN = Signature::PAWN * 15,
		KNIGHT = Signature::KNIGHT * 3,
		BISHOP = Signature::BISHOP * 3,
		ROOK = Signature::ROOK * 3,
		QUEEN = Signature::QUEEN * 3,
		ALL = PAWN | KNIGHT | BISHOP | ROOK | QUEEN,
		SIZE = ALL + 1
	};
	constexpr pieceSignature_t operator|(SignatureMask a, SignatureMask b) { return pieceSignature_t(a) | pieceSignature_t(b); }
	constexpr pieceSignature_t operator|(pieceSignature_t a, SignatureMask b) { return a | pieceSignature_t(b); }
	constexpr pieceSignature_t operator&(SignatureMask a, SignatureMask b) { return pieceSignature_t(a) & pieceSignature_t(b); }
	constexpr pieceSignature_t operator&(pieceSignature_t a, SignatureMask b) { return a & pieceSignature_t(b); }
	constexpr pieceSignature_t operator~(SignatureMask a) { return ~pieceSignature_t(a); }

	class PieceSignature
	{
	public:
		static const pieceSignature_t SIG_SHIFT_BLACK = 12;
		static const pieceSignature_t SIG_SIZE = 1 << (SIG_SHIFT_BLACK * 2);
		static const pieceSignature_t SIG_SIZE_PER_SIDE = 1 << SIG_SHIFT_BLACK;

		PieceSignature() : _signature(0) {}
		PieceSignature(pieceSignature_t signature) : _signature(signature) {}

		PieceSignature(const char *pieces)
		{
			set(pieces);
		}
		bool operator<(const PieceSignature aSignature)
		{
			return pieceSignature_t(_signature) < pieceSignature_t(aSignature);
		}
		void clear() { _signature = 0; }

		std::string toString() const
		{
			return toString<WHITE>() + toString<BLACK>();
		}

		template <Piece COLOR>
		std::string toString() const
		{
			pieceSignature_t colorSignature = getSignature<COLOR>();
			std::string result = "K";
			for (uint32_t i = 0; i < getPieceAmount<QUEEN>(colorSignature); i++)
				result += "Q";
			for (uint32_t i = 0; i < getPieceAmount<ROOK>(colorSignature); i++)
				result += "R";
			for (uint32_t i = 0; i < getPieceAmount<BISHOP>(colorSignature); i++)
				result += "B";
			for (uint32_t i = 0; i < getPieceAmount<KNIGHT>(colorSignature); i++)
				result += "N";
			for (uint32_t i = 0; i < getPieceAmount<PAWN>(colorSignature); i++)
				result += "P";
			return result;
		}

		/**
		 * Computes the classic value difference for the pieces - without pawn, as the signature
		 * does not include more than 3 pawns per side
		 */
		value_t toValueNP() const
		{
			return toValueNP<WHITE>() - toValueNP<BLACK>();
		}

		template <Piece COLOR>
		value_t toValueNP() const
		{
			pieceSignature_t colorSignature = getSignature<COLOR>();
			value_t value = 0;
			value += getPieceAmount<QUEEN>(colorSignature) * 9;
			value += getPieceAmount<ROOK>(colorSignature) * 5;
			value += getPieceAmount<BISHOP>(colorSignature) * 3;
			value += getPieceAmount<KNIGHT>(colorSignature) * 3;
			return value;
		}

		/**
		 * Adds a piece to the signature
		 */
		void addPiece(Piece piece)
		{
			const pieceSignature_t inc = pieceToSignature[piece];
			const pieceSignature_t mask = pieceToMask[piece];
			_signature += ((_signature & mask) < mask) * inc;
			assert(_signature < SIG_SIZE);
		}

		/**
		 * Removes a piece from the signature
		 */
		void removePiece(Piece piece, bitBoard_t pieceBitboardAfterRemovingPiece)
		{
			const pieceSignature_t dec = pieceToSignature[piece];
			const pieceSignature_t mask = pieceToMask[piece];
			const pieceSignature_t pieces = popCount(pieceBitboardAfterRemovingPiece) * dec;
			assert(mask == 0 || (_signature & mask) != 0); // king results in mask = 0
			_signature -= (pieces < mask) * dec;
			assert(_signature < SIG_SIZE);
		}

		/**
		 * Gets the piece signature of a color
		 */
		template <Piece COLOR>
		constexpr pieceSignature_t getSignature() const
		{
			return (COLOR == WHITE) ? (_signature & pieceSignature_t(SignatureMask::ALL)) : _signature >> SIG_SHIFT_BLACK;
		}

		/**
		 * Gets the signature of all pieces
		 */
		inline pieceSignature_t getPiecesSignature() const
		{
			return _signature;
		}

		/**
		 * Gets a static piece value (Queen = 9, Rook = 5, Bishop & Knight = 3, >= 3 Pawns = 1)
		 * The pawns are counted as one, if there are 3 or more pawns
		 */
		template <Piece COLOR>
		value_t getStaticPiecesValue() const
		{
			const auto signature = getSignature<COLOR>();
			return staticPiecesValue[signature];
		}

		/**
		 * Checks, if a side has any material
		 */
		template <Piece COLOR>
		bool hasAnyMaterial() const
		{
			pieceSignature_t signature = getSignature<COLOR>();
			return (signature > 0);
		}

		/**
		 * Checks if a side has a range piece
		 */
		template <Piece COLOR>
		inline bool hasQueenOrRookOrBishop() const
		{
			constexpr pieceSignature_t mask = SignatureMask::QUEEN | SignatureMask::ROOK | SignatureMask::BISHOP;
			return (getSignature<COLOR>() & mask) != 0;
		}

		template <Piece COLOR>
		bool hasMoreThanPawns() const
		{
			constexpr pieceSignature_t mask = SignatureMask::QUEEN | SignatureMask::ROOK | SignatureMask::BISHOP | SignatureMask::KNIGHT;
			return (getSignature<COLOR>() & mask) != 0;
		}

		/**
		 * Checks, if the side to move has a range piece
		 */
		inline bool sideToMoveHasQueenRookBishop(bool whiteToMove) const
		{
			return whiteToMove ? hasQueenOrRookOrBishop<WHITE>() : hasQueenOrRookOrBishop<BLACK>();
		}

		/**
		 * Checks, if a side has enough material to mate
		 */
		template <Piece COLOR>
		bool hasEnoughMaterialToMate() const
		{
			pieceSignature_t signature = getSignature<COLOR>();
			return (signature & SignatureMask::PAWN) || (signature > pieceSignature_t(Signature::BISHOP)) || (signature & SignatureMask::KNIGHT) == pieceSignature_t(SignatureMask::KNIGHT);
		}

		/**
		 * Checks for draw due to missing material
		 * Tricky but jump free implementation
		 */
		bool drawDueToMissingMaterial() const
		{
			constexpr pieceSignature_t ONLY_KNIGHT_AND_BISHOP =
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
		void set(string pieces);

		/**
		 * Generates a list of signatures from a pattern. It supports "+" (one or more pieces),
		 * "*" (zero or more pieces)
		 * Example: KQP+KRP* generates: all KQKR combinations with one or more whites pawn and zero or more black pawns
		 * @param pattern The pattern to generate the signatures from
		 * @param out The output vector to store the generated signatures
		 *
		 */
		void generateSignatures(const std::string &pattern, std::vector<pieceSignature_t> &out);

		/**
		 * Checks if the current signature matches a pattern
		 * @param pattern The pattern to check against
		 * @return true if the signature matches the pattern, false otherwise
		 */
		bool matchesPattern(const std::string &pattern) const;

		/**
		 * Debugging functionality: swap white and black signature
		 */
		void changeSide()
		{
			_signature = (getSignature<WHITE>() << SIG_SHIFT_BLACK) + getSignature<BLACK>();
		}

		/**
		 * Checks for futility pruning for a capture
		 */
		bool doFutilityOnCapture(Piece capturedPiece) const
		{
			bool result = true;
			if (getPieceColor(capturedPiece) == WHITE)
			{
				result = futilityOnCaptureMap[_signature & SignatureMask::ALL];
			}
			else
			{
				result = futilityOnCaptureMap[_signature >> SIG_SHIFT_BLACK];
			}
			return result;
		}

		/**
		 * Checks for futility pruning for a promotion based on the piece signature
		 */
		bool doFutilityOnPromote() const
		{
			bool result = true;
			result = futilityOnCaptureMap[_signature & SignatureMask::ALL] &&
					 futilityOnCaptureMap[_signature >> SIG_SHIFT_BLACK];
			return result;
		}

	private:
		pieceSignature_t _signature;

		inline operator pieceSignature_t() const { return _signature; }

		/**
		 * Checks, if a piece is available more than twice
		 */
		inline bool moreThanTwoPiecesInBitBoard(bitBoard_t bitBoard) const
		{
			bitBoard &= bitBoard - 1;
			bitBoard &= bitBoard - 1;
			return bitBoard != 0;
		}

		/**
		 * Checks, if a piece is available more than once
		 */
		inline bool moreThanOnePieceInBitBoard(bitBoard_t bitBoard) const
		{
			bitBoard &= bitBoard - 1;
			return bitBoard != 0;
		}

		/**
		 * Returns the amount of pieces found in a signature
		 */
		constexpr static uint32_t getPieceAmount(pieceSignature_t signature)
		{
			return getPieceAmount<PAWN>(signature) +
				   getPieceAmount<KNIGHT>(signature) +
				   getPieceAmount<BISHOP>(signature) +
				   getPieceAmount<ROOK>(signature) +
				   getPieceAmount<QUEEN>(signature);
		}

		/**
		 * Returns the amount of pieces of a kind found in a signature
		 */
		template <Piece KIND>
		constexpr static uint32_t getPieceAmount(pieceSignature_t signature)
		{
			if (KIND == Piece::QUEEN)
				return (signature & SignatureMask::QUEEN) / Signature::QUEEN;
			else if (KIND == Piece::ROOK)
				return (signature & SignatureMask::ROOK) / Signature::ROOK;
			else if (KIND == Piece::BISHOP)
				return (signature & SignatureMask::BISHOP) / Signature::BISHOP;
			else if (KIND == Piece::KNIGHT)
				return (signature & SignatureMask::KNIGHT) / Signature::KNIGHT;
			else if (KIND == Piece::PAWN)
				return (signature & SignatureMask::PAWN) / Signature::PAWN;
			else
				return 0;
		}

		static constexpr array<pieceSignature_t, PIECE_AMOUNT> pieceToSignature = []()
		{
			array<pieceSignature_t, PIECE_AMOUNT> result{};
			for (size_t i = 0; i < PIECE_AMOUNT; ++i)
				result[i] = pieceSignature_t(Signature::EMPTY);
			result[WHITE_PAWN] = pieceSignature_t(Signature::PAWN);
			result[WHITE_KNIGHT] = pieceSignature_t(Signature::KNIGHT);
			result[WHITE_BISHOP] = pieceSignature_t(Signature::BISHOP);
			result[WHITE_ROOK] = pieceSignature_t(Signature::ROOK);
			result[WHITE_QUEEN] = pieceSignature_t(Signature::QUEEN);

			result[BLACK_PAWN] = pieceSignature_t(Signature::PAWN) << SIG_SHIFT_BLACK;
			result[BLACK_KNIGHT] = pieceSignature_t(Signature::KNIGHT) << SIG_SHIFT_BLACK;
			result[BLACK_BISHOP] = pieceSignature_t(Signature::BISHOP) << SIG_SHIFT_BLACK;
			result[BLACK_ROOK] = pieceSignature_t(Signature::ROOK) << SIG_SHIFT_BLACK;
			result[BLACK_QUEEN] = pieceSignature_t(Signature::QUEEN) << SIG_SHIFT_BLACK;
			return result;
		}();

		static constexpr std::array<pieceSignature_t, PIECE_AMOUNT> pieceToMask = []()
		{
			std::array<pieceSignature_t, PIECE_AMOUNT> result{};
			result[WHITE_PAWN] = pieceSignature_t(SignatureMask::PAWN);
			result[WHITE_KNIGHT] = pieceSignature_t(SignatureMask::KNIGHT);
			result[WHITE_BISHOP] = pieceSignature_t(SignatureMask::BISHOP);
			result[WHITE_ROOK] = pieceSignature_t(SignatureMask::ROOK);
			result[WHITE_QUEEN] = pieceSignature_t(SignatureMask::QUEEN);

			result[BLACK_PAWN] = static_cast<pieceSignature_t>(SignatureMask::PAWN) << SIG_SHIFT_BLACK;
			result[BLACK_KNIGHT] = static_cast<pieceSignature_t>(SignatureMask::KNIGHT) << SIG_SHIFT_BLACK;
			result[BLACK_BISHOP] = static_cast<pieceSignature_t>(SignatureMask::BISHOP) << SIG_SHIFT_BLACK;
			result[BLACK_ROOK] = static_cast<pieceSignature_t>(SignatureMask::ROOK) << SIG_SHIFT_BLACK;
			result[BLACK_QUEEN] = static_cast<pieceSignature_t>(SignatureMask::QUEEN) << SIG_SHIFT_BLACK;
			return result;
		}();

		/**
		 * Initializes a lookup table used for futility pruning.
		 *
		 * Prevents futility pruning when the remaining material after a capture
		 * consists of fewer than two pieces.
		 */
		static inline array<pieceSignature_t, size_t(SignatureMask::SIZE)> futilityOnCaptureMap = []()
		{
			array<pieceSignature_t, static_cast<size_t>(SignatureMask::SIZE)> result{};
			for (uint32_t index = 0; index < static_cast<uint32_t>(SignatureMask::SIZE); index++)
			{
				result[index] = true;
				if (getPieceAmount(index) <= 2)
				{
					result[index] = false;
				}
			}
			return result;
		}();

		/**
		 * Initializes a lookup table for static piece values.
		 *
		 * Queen = 9, Rook = 5, Bishop = 3, Knight = 3.
		 * Pawns contribute exactly 1 point if there are at least 3.
		 *
		 * This table is used for fast material evaluation in search heuristics.
		 */
		static inline array<value_t, size_t(SignatureMask::SIZE)> staticPiecesValue = []()
		{
			array<value_t, static_cast<size_t>(SignatureMask::SIZE)> result{};
			for (uint32_t index = 0; index < static_cast<uint32_t>(SignatureMask::ALL); index++)
			{
				result[index] =
					getPieceAmount<QUEEN>(index) * 9 +
					getPieceAmount<ROOK>(index) * 5 +
					getPieceAmount<BISHOP>(index) * 3 +
					getPieceAmount<KNIGHT>(index) * 3 +
					(getPieceAmount<PAWN>(index) >= 3 ? 1 : 0);
			}
			return result;
		}();

		/**
		 * Maps a piece char to a piece signature bit
		 */
		std::tuple<pieceSignature_t, pieceSignature_t> charToSignature(char piece) const;
	};
}
