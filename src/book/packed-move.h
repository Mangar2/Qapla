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
 * @copyright Copyright (c) 2026 Volker Böhm
 * @Overview
 * Packs a move into 11 bits without needing a move generator, taken from the
 * Spike book format so that the old books stay readable.
 *
 * The bit code of a packed move is
 * (msb) UUUU UPTT TTTT (lsb)
 * where
 * T = destination square
 * P = direction, an index into DIRECTIONS
 * T = promotion flag
 * U = unused
 *
 * Unpacking needs the piece placement of the position, nothing else: the
 * departure square is the first occupied square found when walking from the
 * destination square against the direction of the move. For a slider that
 * square is necessarily the moving piece - if any other piece stood in
 * between, the move would have been blocked. Knights cannot be walked back, so
 * they get eight directions of their own and the departure square follows
 * directly.
 *
 * Castling, en passant and the double pawn push need no special case: they are
 * all walked back like any other move.
 */

#pragma once

#include <array>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <optional>

#include "../../basics/types.h"

namespace QaplaBook {

	using QaplaBasics::Piece;
	using QaplaBasics::Square;

	/**
	 * A move as the book hands it out and takes it in: the two squares and the
	 * promotion piece. It is deliberately not a Qapla Move - the book knows
	 * nothing about capture flags or move actions, and a chess GUI using the
	 * same format has its own move representation.
	 */
	struct BookMove {
		Square from = Square::A1;
		Square to = Square::A1;
		Piece promotion = Piece::NO_PIECE;

		constexpr bool operator==(const BookMove& moveToCompare) const = default;
	};

	/**
	 * A move packed into 11 bits.
	 */
	using PackedMove = uint16_t;

	/**
	 * Everything a book needs to know about a position in order to unpack a
	 * move: whether a square is occupied. There is deliberately no move
	 * generation in this interface - not needing a move generator, and with it
	 * not needing one that agrees with the writer about move ordering, is the
	 * whole point of this format.
	 */
	template <typename POSITION>
	concept BookPosition = requires(const POSITION& position, Square square) {
		{ position.isEmpty(square) } -> std::convertible_to<bool>;
	};

	/**
	 * The 16 directions a move can have. The first eight are the knight
	 * directions, they point from the destination back to the departure square.
	 * The last eight are the ray directions, they point from the departure to
	 * the destination square. The asymmetry comes from the Spike format and is
	 * kept for compatibility.
	 */
	inline constexpr std::array<int32_t, 16> DIRECTIONS = {
		// knight directions
		+2 * 8 - 1, +2 * 8 + 1, +1 * 8 + 2, -1 * 8 + 2,
		+1 * 8 - 2, -1 * 8 - 2, -2 * 8 - 1, -2 * 8 + 1,
		// rook directions
		+0 * 8 + 1, +0 * 8 - 1, +1 * 8 + 0, -1 * 8 + 0,
		// bishop directions
		+1 * 8 + 1, -1 * 8 + 1, +1 * 8 - 1, -1 * 8 - 1
	};

	/**
	 * The promotion piece of a packed promotion, indexed by the three bits that
	 * hold the rank of the destination square in a move that is not a
	 * promotion. Index 0 and 7 both mean queen: those are the values the rank
	 * bits have anyway (rank 1 and rank 8), so a queen promotion leaves them
	 * untouched and books written before sub promotions were coded stay
	 * readable.
	 */
	inline constexpr std::array<Piece, 8> PROMOTION_PIECES = {
		Piece::BLACK_QUEEN, Piece::BLACK_ROOK, Piece::BLACK_BISHOP, Piece::BLACK_KNIGHT,
		Piece::WHITE_KNIGHT, Piece::WHITE_BISHOP, Piece::WHITE_ROOK, Piece::WHITE_QUEEN
	};

	enum packedMoveBits : uint32_t {
		PACKED_TO_MASK = 0x003F,
		PACKED_TO_RANK_MASK = 0x0038,
		PACKED_DIRECTION_SHIFT = 6,
		PACKED_DIRECTION_MASK = 0x03C0,
		PACKED_PROMOTION_FLAG = 0x0400,
		PACKED_MOVE_MASK = 0x07FF,
		PACKED_MOVE_BITS = 11,
		KNIGHT_DIRECTIONS = 8
	};

	/**
	 * The value used for "no move". Zero cannot be the code of a real move: a
	 * ray move always has a direction index of eight or more, and the only
	 * knight code with direction index zero and destination a1 would be a
	 * knight standing on h2, which is not a knight move but a wrap around the
	 * board edge and therefore rejected when packing.
	 */
	inline constexpr PackedMove PACKED_MOVE_NONE = 0;

	namespace detail {

		constexpr int32_t absValue(int32_t value) {
			return value < 0 ? -value : value;
		}

		constexpr int32_t signValue(int32_t value) {
			return value < 0 ? -1 : (value > 0 ? 1 : 0);
		}

		constexpr int32_t fileOf(Square square) {
			return int32_t(square) % 8;
		}

		constexpr int32_t rankOf(Square square) {
			return int32_t(square) / 8;
		}

		/**
		 * Stores the promotion piece in the rank bits of the destination
		 * square. Those bits are redundant for a promotion - the destination is
		 * always the first or the last rank - and a queen keeps them unchanged.
		 */
		constexpr std::optional<PackedMove> addPromotion(PackedMove packed, Piece promotion) {
			const uint32_t rankBits = uint32_t(packed) & PACKED_TO_RANK_MASK;
			const bool isLastRank = rankBits == PACKED_TO_RANK_MASK;
			if (!isLastRank && rankBits != 0) return std::nullopt;
			uint32_t code = 0;
			switch (promotion & ~Piece::COLOR_MASK) {
			case Piece::QUEEN: code = isLastRank ? 7 : 0; break;
			case Piece::ROOK: code = isLastRank ? 6 : 1; break;
			case Piece::BISHOP: code = isLastRank ? 5 : 2; break;
			case Piece::KNIGHT: code = isLastRank ? 4 : 3; break;
			default: return std::nullopt;
			}
			return PackedMove((uint32_t(packed) & ~uint32_t(PACKED_TO_RANK_MASK))
				| (code << 3) | PACKED_PROMOTION_FLAG);
		}
	}

	/**
	 * Packs a move into 11 bits. Returns nothing if the move has no direction
	 * the format can express - a move that is neither a knight move nor along a
	 * ray, or a promotion that does not end on the first or the last rank.
	 */
	constexpr std::optional<PackedMove> packMove(const BookMove& move) {
		if (!QaplaBasics::isInBoard(move.from) || !QaplaBasics::isInBoard(move.to)) {
			return std::nullopt;
		}
		const int32_t fileDelta = detail::fileOf(move.to) - detail::fileOf(move.from);
		const int32_t rankDelta = detail::rankOf(move.to) - detail::rankOf(move.from);
		if (fileDelta == 0 && rankDelta == 0) return std::nullopt;

		PackedMove packed = PACKED_MOVE_NONE;
		if (detail::absValue(fileDelta) * detail::absValue(rankDelta) == 2) {
			// A knight move: the direction is stored pointing back to the
			// departure square, there is nothing to walk back.
			const int32_t backwards = int32_t(move.from) - int32_t(move.to);
			for (uint32_t index = 0; index < KNIGHT_DIRECTIONS; ++index) {
				if (DIRECTIONS[index] == backwards) {
					packed = PackedMove(uint32_t(move.to) | (index << PACKED_DIRECTION_SHIFT));
					break;
				}
			}
		}
		else if (fileDelta == 0 || rankDelta == 0
			|| detail::absValue(fileDelta) == detail::absValue(rankDelta)) {
			// Anything else - queen, rook, bishop, king and pawn - moves along
			// one of eight rays, and one step of that ray is the direction.
			const int32_t step = detail::signValue(rankDelta) * 8 + detail::signValue(fileDelta);
			for (uint32_t index = KNIGHT_DIRECTIONS; index < DIRECTIONS.size(); ++index) {
				if (DIRECTIONS[index] == step) {
					packed = PackedMove(uint32_t(move.to) | (index << PACKED_DIRECTION_SHIFT));
					break;
				}
			}
		}
		if (packed == PACKED_MOVE_NONE) return std::nullopt;
		if (move.promotion == Piece::NO_PIECE) return packed;
		return detail::addPromotion(packed, move.promotion);
	}

	/**
	 * Unpacks a move, using the piece placement of the position it was played
	 * in. The position must be the one the move is played from, otherwise the
	 * departure square found is not the one that was packed.
	 */
	template <BookPosition POSITION>
	constexpr BookMove unpackMove(PackedMove packed, const POSITION& position) {
		uint32_t bits = uint32_t(packed);
		Piece promotion = Piece::NO_PIECE;
		if ((bits & PACKED_PROMOTION_FLAG) != 0) {
			const uint32_t code = (bits & PACKED_TO_RANK_MASK) >> 3;
			promotion = PROMOTION_PIECES[code];
			// Restore the rank of the destination square, which the promotion
			// piece was stored in.
			bits = code > 3 ? (bits | PACKED_TO_RANK_MASK) : (bits & ~uint32_t(PACKED_TO_RANK_MASK));
		}
		const Square to = Square(bits & PACKED_TO_MASK);
		const uint32_t index = (bits & PACKED_DIRECTION_MASK) >> PACKED_DIRECTION_SHIFT;

		Square from = Square(int32_t(to) + DIRECTIONS[index]);
		if (index >= KNIGHT_DIRECTIONS) {
			const int32_t step = DIRECTIONS[index];
			for (from = Square(int32_t(to) - step);
				QaplaBasics::isInBoard(from) && position.isEmpty(from);
				from = Square(int32_t(from) - step));
		}
		assert(QaplaBasics::isInBoard(from));
		return BookMove{ .from = from, .to = to, .promotion = promotion };
	}
}
