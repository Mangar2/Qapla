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
 * Packs a move into 9 bits without needing a move generator.
 *
 * The bit code of a packed move is
 * (msb) IIII CCCC C (lsb)
 * where
 * I = index of the moving piece among the pieces of the side to move, counted
 *     from a1 to h8
 * C = move code, its meaning depends on the type of the moving piece
 *
 * Queen, rook, bishop and king share one code: the line the move runs along
 * (two bits) and the position of the destination square on that line (three
 * bits). The lines are absolute, not relative to the moving piece:
 *   0 = the rank of the piece, position = file of the destination
 *   1 = the file of the piece, position = rank of the destination
 *   2 = the diagonal (a1-h8 direction), position = file of the destination
 *   3 = the anti diagonal (a8-h1 direction), position = file of the destination
 * A rook never uses the diagonals and a king only reaches its neighbours, the
 * codes are nevertheless the same for all four - one decoder serves them all.
 *
 * Castling is the king moving onto its own rook, the rank code holds the file
 * of the rook. That covers Chess960 without any special case.
 *
 * A knight uses the codes 0 to 7, one per direction, see KNIGHT_OFFSETS.
 *
 * A pawn uses the codes 0 to 15:
 *   0 = one square forward, 1 = capture towards the a file,
 *   2 = capture towards the h file, 3 = two squares forward,
 *   4 + 4 * direction + piece = promotion, direction 0 to 2 as the codes 0 to 2,
 *   piece 0 to 3 = queen, rook, bishop, knight.
 * En passant needs no code of its own: it is the capture code, and the reader
 * recognises it by the empty destination square.
 *
 * Unpacking needs the piece placement and the side to move, no move
 * generator: find the n-th piece of the side to move, look at its type, apply
 * the code. Packing needs the same, the index of the piece depends on the
 * position.
 */

#pragma once

#include <array>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <optional>

#include "../../basics/types.h"
#include "../book/packed-move.h"

namespace QaplaBook9 {

	using QaplaBasics::Piece;
	using QaplaBasics::Square;

	/**
	 * The move as the book takes it in and hands it out, the same struct as the
	 * 11 bit book uses. A castling move is handed out as the king moving onto its
	 * own rook; packing takes the king moving to the c or g file as well.
	 */
	using QaplaBook::BookMove;

	/**
	 * A move packed into 9 bits.
	 */
	using PackedMove = uint16_t;

	/**
	 * Everything a book needs to know about a position in order to pack and
	 * unpack a move: the piece on a square and the side to move. There is
	 * deliberately no move generation in this interface.
	 */
	template <typename POSITION>
	concept BookPosition = requires(const POSITION& position, Square square) {
		{ position.pieceAt(square) } -> std::convertible_to<Piece>;
		{ position.isWhiteToMove() } -> std::convertible_to<bool>;
	};

	enum packedMoveBits : uint32_t {
		PACKED_CODE_MASK = 0x001F,
		PACKED_PIECE_SHIFT = 5,
		PACKED_PIECE_MASK = 0x01E0,
		PACKED_MOVE_MASK = 0x01FF,
		PACKED_MOVE_BITS = 9,
		MAX_PIECES_PER_COLOR = 16,
		LINE_SHIFT = 3,
		LINE_POSITION_MASK = 0x0007
	};

	enum moveLine : uint32_t {
		RANK_LINE = 0,
		FILE_LINE = 1,
		DIAGONAL_LINE = 2,
		ANTI_DIAGONAL_LINE = 3
	};

	enum pawnCode : uint32_t {
		PAWN_PUSH = 0,
		PAWN_CAPTURE_TOWARDS_A = 1,
		PAWN_CAPTURE_TOWARDS_H = 2,
		PAWN_DOUBLE_PUSH = 3,
		PAWN_PROMOTION = 4,
		PAWN_CODE_COUNT = 16
	};

	/**
	 * The value used for "no move", the move of the root. Every 9 bit value can
	 * be a real move - piece 0 with code 0 is a rook stepping from b1 to a1 - so
	 * the sentinel lies outside the 9 bits. It never goes into a file, the root
	 * is not stored.
	 */
	inline constexpr PackedMove PACKED_MOVE_NONE = 0xFFFF;

	struct BoardOffset {
		int32_t file;
		int32_t rank;
	};

	/** The eight knight directions, indexed by the knight code. */
	inline constexpr std::array<BoardOffset, 8> KNIGHT_OFFSETS = { {
		{ -1, +2 }, { +1, +2 }, { +2, +1 }, { +2, -1 },
		{ -2, +1 }, { -2, -1 }, { -1, -2 }, { +1, -2 }
	} };

	/** The promotion pieces, indexed by the piece part of a promotion code. */
	inline constexpr std::array<Piece, 4> PROMOTION_TYPES = {
		Piece::QUEEN, Piece::ROOK, Piece::BISHOP, Piece::KNIGHT
	};

	namespace detail {

		constexpr int32_t fileOf(Square square) {
			return int32_t(square) % 8;
		}

		constexpr int32_t rankOf(Square square) {
			return int32_t(square) / 8;
		}

		constexpr bool isOnBoard(int32_t file, int32_t rank) {
			return file >= 0 && file < 8 && rank >= 0 && rank < 8;
		}

		constexpr Square squareOf(int32_t file, int32_t rank) {
			return Square(rank * 8 + file);
		}

		template <BookPosition POSITION>
		constexpr Piece colorToMove(const POSITION& position) {
			return position.isWhiteToMove() ? Piece::WHITE : Piece::BLACK;
		}

		template <BookPosition POSITION>
		constexpr bool isPieceOf(const POSITION& position, Square square, Piece color) {
			const Piece piece = position.pieceAt(square);
			return piece != Piece::NO_PIECE && QaplaBasics::getPieceColor(piece) == color;
		}

		/**
		 * The number of pieces of a color standing on squares below a square.
		 */
		template <BookPosition POSITION>
		constexpr uint32_t pieceIndex(const POSITION& position, Square square, Piece color) {
			uint32_t index = 0;
			for (int32_t below = 0; below < int32_t(square); ++below) {
				if (isPieceOf(position, Square(below), color)) ++index;
			}
			return index;
		}

		/**
		 * The square of the n-th piece of a color, counted from a1, or
		 * NO_SQUARE if the color has fewer pieces.
		 */
		template <BookPosition POSITION>
		constexpr Square nthPiece(const POSITION& position, uint32_t index, Piece color) {
			for (int32_t square = 0; square < 64; ++square) {
				if (!isPieceOf(position, Square(square), color)) continue;
				if (index == 0) return Square(square);
				--index;
			}
			return Square::NO_SQUARE;
		}

		/**
		 * The code of a move along a rank, a file or a diagonal.
		 */
		constexpr std::optional<uint32_t> lineCode(Square from, Square to) {
			const int32_t fileDelta = fileOf(to) - fileOf(from);
			const int32_t rankDelta = rankOf(to) - rankOf(from);
			if (fileDelta == 0 && rankDelta == 0) return std::nullopt;
			if (rankDelta == 0) return (RANK_LINE << LINE_SHIFT) | uint32_t(fileOf(to));
			if (fileDelta == 0) return (FILE_LINE << LINE_SHIFT) | uint32_t(rankOf(to));
			if (fileDelta == rankDelta) return (DIAGONAL_LINE << LINE_SHIFT) | uint32_t(fileOf(to));
			if (fileDelta == -rankDelta) return (ANTI_DIAGONAL_LINE << LINE_SHIFT) | uint32_t(fileOf(to));
			return std::nullopt;
		}

		/**
		 * The destination square of a line code, or NO_SQUARE if the code
		 * points off the board.
		 */
		constexpr Square lineTarget(Square from, uint32_t code) {
			const int32_t position = int32_t(code & LINE_POSITION_MASK);
			const int32_t file = fileOf(from);
			const int32_t rank = rankOf(from);
			int32_t targetFile = position;
			int32_t targetRank = rank;
			switch (code >> LINE_SHIFT) {
			case RANK_LINE: break;
			case FILE_LINE: targetFile = file; targetRank = position; break;
			case DIAGONAL_LINE: targetRank = rank + (position - file); break;
			case ANTI_DIAGONAL_LINE: targetRank = rank - (position - file); break;
			}
			if (!isOnBoard(targetFile, targetRank)) return Square::NO_SQUARE;
			return squareOf(targetFile, targetRank);
		}

		/**
		 * Turns a castling move given as the king moving to the c or the g file
		 * into the king moving onto its rook. A king moving onto its own rook
		 * is already in that form and stays as it is, as does every other
		 * king move.
		 *
		 * A king move to the c or g file is taken as castling if it covers
		 * more than one file or none at all - the latter being a Chess960 king
		 * that stands on its castling square already. A Chess960 castling in
		 * which the king moves a single file cannot be told from a king step
		 * by the squares alone; for Chess960 pass the rook square.
		 */
		template <BookPosition POSITION>
		constexpr std::optional<Square> kingDestination(const BookMove& move,
			const POSITION& position, Piece color) {
			const int32_t fileDelta = fileOf(move.to) - fileOf(move.from);
			const bool sameRank = rankOf(move.to) == rankOf(move.from);
			const bool isStep = fileDelta >= -1 && fileDelta <= 1 && fileDelta != 0;
			const bool ontoOwnPiece = move.to != move.from && isPieceOf(position, move.to, color);
			if (!sameRank || isStep || ontoOwnPiece) return move.to;
			const int32_t targetFile = fileOf(move.to);
			if (targetFile != 2 && targetFile != 6) return std::nullopt;
			const int32_t step = targetFile == 6 ? 1 : -1;
			const int32_t rank = rankOf(move.from);
			for (int32_t file = fileOf(move.from) + step; file >= 0 && file < 8; file += step) {
				const Piece piece = position.pieceAt(squareOf(file, rank));
				if (piece == Piece::NO_PIECE) continue;
				if (piece != Piece(Piece::ROOK | color)) return std::nullopt;
				return squareOf(file, rank);
			}
			return std::nullopt;
		}

		constexpr std::optional<uint32_t> pawnCode(const BookMove& move, Piece color) {
			const int32_t forward = color == Piece::WHITE ? 1 : -1;
			const int32_t fileDelta = fileOf(move.to) - fileOf(move.from);
			const int32_t steps = (rankOf(move.to) - rankOf(move.from)) * forward;
			if (steps == 2 && fileDelta == 0 && move.promotion == Piece::NO_PIECE) {
				return PAWN_DOUBLE_PUSH;
			}
			if (steps != 1 || fileDelta < -1 || fileDelta > 1) return std::nullopt;
			const uint32_t direction = fileDelta == 0 ? PAWN_PUSH
				: (fileDelta < 0 ? PAWN_CAPTURE_TOWARDS_A : PAWN_CAPTURE_TOWARDS_H);
			if (move.promotion == Piece::NO_PIECE) return direction;
			const Piece type = QaplaBasics::getPieceType(move.promotion);
			for (uint32_t index = 0; index < PROMOTION_TYPES.size(); ++index) {
				if (PROMOTION_TYPES[index] == type) {
					return PAWN_PROMOTION + direction * uint32_t(PROMOTION_TYPES.size()) + index;
				}
			}
			return std::nullopt;
		}

		constexpr BookMove pawnMove(Square from, uint32_t code, Piece color) {
			const int32_t forward = color == Piece::WHITE ? 1 : -1;
			const int32_t file = fileOf(from);
			const int32_t rank = rankOf(from);
			if (code == PAWN_DOUBLE_PUSH) {
				return BookMove{ .from = from, .to = squareOf(file, rank + 2 * forward) };
			}
			Piece promotion = Piece::NO_PIECE;
			uint32_t direction = code;
			if (code >= PAWN_PROMOTION) {
				const uint32_t promotionCode = code - PAWN_PROMOTION;
				direction = promotionCode / uint32_t(PROMOTION_TYPES.size());
				promotion = Piece(PROMOTION_TYPES[promotionCode % PROMOTION_TYPES.size()] | color);
			}
			const int32_t fileDelta = direction == PAWN_PUSH ? 0
				: (direction == PAWN_CAPTURE_TOWARDS_A ? -1 : 1);
			assert(isOnBoard(file + fileDelta, rank + forward));
			return BookMove{ .from = from, .to = squareOf(file + fileDelta, rank + forward),
				.promotion = promotion };
		}
	}

	/**
	 * Packs a move into 9 bits, using the position it is played from. Returns
	 * nothing if the move cannot be expressed: no piece of the side to move on
	 * the departure square, a destination the piece type has no code for, or a
	 * side with more than 16 pieces. The move is not checked for legality.
	 */
	template <BookPosition POSITION>
	constexpr std::optional<PackedMove> packMove(const BookMove& move, const POSITION& position) {
		if (!QaplaBasics::isInBoard(move.from) || !QaplaBasics::isInBoard(move.to)) {
			return std::nullopt;
		}
		const Piece color = detail::colorToMove(position);
		if (!detail::isPieceOf(position, move.from, color)) return std::nullopt;
		const uint32_t index = detail::pieceIndex(position, move.from, color);
		if (index >= MAX_PIECES_PER_COLOR) return std::nullopt;

		std::optional<uint32_t> code;
		switch (QaplaBasics::getPieceType(position.pieceAt(move.from))) {
		case Piece::PAWN:
			code = detail::pawnCode(move, color);
			break;
		case Piece::KNIGHT: {
			const int32_t fileDelta = detail::fileOf(move.to) - detail::fileOf(move.from);
			const int32_t rankDelta = detail::rankOf(move.to) - detail::rankOf(move.from);
			for (uint32_t direction = 0; direction < KNIGHT_OFFSETS.size(); ++direction) {
				if (KNIGHT_OFFSETS[direction].file == fileDelta
					&& KNIGHT_OFFSETS[direction].rank == rankDelta) {
					code = direction;
				}
			}
			break;
		}
		case Piece::KING: {
			const auto destination = detail::kingDestination(move, position, color);
			if (destination) code = detail::lineCode(move.from, *destination);
			break;
		}
		default:
			code = detail::lineCode(move.from, move.to);
			break;
		}
		if (!code) return std::nullopt;
		if (move.promotion != Piece::NO_PIECE && !QaplaBasics::isPawn(position.pieceAt(move.from))) {
			return std::nullopt;
		}
		return PackedMove((index << PACKED_PIECE_SHIFT) | *code);
	}

	/**
	 * Unpacks a move, using the position it is played from. The position must
	 * be the one the move was packed in, otherwise the piece found is not the
	 * one that was packed. A castling move comes back as the king moving onto
	 * its own rook.
	 */
	template <BookPosition POSITION>
	constexpr BookMove unpackMove(PackedMove packed, const POSITION& position) {
		const Piece color = detail::colorToMove(position);
		const uint32_t index = (uint32_t(packed) & PACKED_PIECE_MASK) >> PACKED_PIECE_SHIFT;
		const uint32_t code = uint32_t(packed) & PACKED_CODE_MASK;
		const Square from = detail::nthPiece(position, index, color);
		assert(from != Square::NO_SQUARE);

		switch (QaplaBasics::getPieceType(position.pieceAt(from))) {
		case Piece::PAWN:
			assert(code < PAWN_CODE_COUNT);
			return detail::pawnMove(from, code, color);
		case Piece::KNIGHT: {
			assert(code < KNIGHT_OFFSETS.size());
			const BoardOffset offset = KNIGHT_OFFSETS[code];
			assert(detail::isOnBoard(detail::fileOf(from) + offset.file,
				detail::rankOf(from) + offset.rank));
			return BookMove{ .from = from, .to = detail::squareOf(
				detail::fileOf(from) + offset.file, detail::rankOf(from) + offset.rank) };
		}
		default: {
			const Square to = detail::lineTarget(from, code);
			assert(to != Square::NO_SQUARE);
			return BookMove{ .from = from, .to = to };
		}
		}
	}

	/**
	 * True, if an unpacked move is a castling move: the king moving onto a
	 * piece of its own color.
	 */
	template <BookPosition POSITION>
	constexpr bool isCastling(const BookMove& move, const POSITION& position) {
		const Piece piece = position.pieceAt(move.from);
		const Piece target = position.pieceAt(move.to);
		return QaplaBasics::isKing(piece) && target != Piece::NO_PIECE
			&& QaplaBasics::getPieceColor(target) == QaplaBasics::getPieceColor(piece);
	}

	/**
	 * A castling move as the king moving to the c or the g file, the form Qapla
	 * uses. Every other move is returned unchanged.
	 */
	template <BookPosition POSITION>
	constexpr BookMove toKingTargetCastling(const BookMove& move, const POSITION& position) {
		if (!isCastling(move, position)) return move;
		const bool kingSide = detail::fileOf(move.to) > detail::fileOf(move.from);
		return BookMove{ .from = move.from,
			.to = detail::squareOf(kingSide ? 6 : 2, detail::rankOf(move.from)) };
	}
}
