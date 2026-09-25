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
 * The file of played games, the training data of the net.
 *
 * A game is a length byte followed by that many move records of three bytes:
 *
 *   byte  0        : number of half moves of the game, 1 to 255
 *   then per move  : (msb) VVVV VVVV VVVW WMMM MMMM MMMM (lsb)
 *
 * where
 *   M = the move, packed as in a book, see src/book/packed-move.h
 *   W = the result of the game, seen from the side to move in this position
 *   V = the value of the position the move is played from, from the search of
 *       the game, seen from the side to move, two's complement
 *
 * Three bytes hold 24 bits, and move plus value need 22 of them, so the result
 * costs nothing where it stands. It is the same result in every record of a
 * game, only turned around every ply - which saves the reader from turning it.
 *
 * The value is eleven bits, -1024 to 1023, in the internal unit of the engine
 * where a pawn is 80 to 95. A game ends as soon as a value does not fit, so
 * -1024 never occurs as a value and marks a move that has none: the moves of the
 * book line the game starts from. They are in the file because a packed move has
 * no departure square - the reader replays the game from the start position, and
 * it can only do that if the file starts there too.
 *
 * A reader therefore needs a board and no move generator, and it can seek to a
 * game only by walking the games before it. Shuffling happens over whole games,
 * see the note on the format in the book.
 */

#pragma once

#include <cstdint>
#include <istream>
#include <ostream>
#include <vector>

#include "../book/packed-move.h"
#include "../../basics/evalvalue.h"

namespace QaplaNnueData {

	/**
	 * The result of a game, seen from the side to move of the position it is
	 * stored with.
	 */
	enum class GameValue : uint8_t {
		LOSS = 0,
		DRAW = 1,
		WIN = 2
	};

	enum gameRecordBits : uint32_t {
		RECORD_MOVE_MASK = 0x0007FF,
		RECORD_RESULT_SHIFT = 11,
		RECORD_RESULT_MASK = 0x001800,
		RECORD_VALUE_SHIFT = 13,
		RECORD_VALUE_BITS = 11,
		RECORD_BYTES = 3,
		/** Longest game the length byte can hold. */
		MAX_HALF_MOVES = 255
	};

	/** The value that means "this move has none", the one the range leaves over. */
	inline constexpr QaplaBasics::value_t NO_GAME_VALUE = -1024;
	inline constexpr QaplaBasics::value_t MIN_GAME_VALUE = -1023;
	inline constexpr QaplaBasics::value_t MAX_GAME_VALUE = 1023;

	/** True if a value can be stored in a record. */
	constexpr bool fitsInRecord(QaplaBasics::value_t value) {
		return value >= MIN_GAME_VALUE && value <= MAX_GAME_VALUE;
	}

	/**
	 * One move of a game as it is stored.
	 */
	struct GameMove {
		QaplaBook::PackedMove move = QaplaBook::PACKED_MOVE_NONE;
		QaplaBasics::value_t value = NO_GAME_VALUE;
		GameValue result = GameValue::DRAW;
	};

	/**
	 * Packs a move record into its 24 bits.
	 */
	constexpr uint32_t packGameMove(const GameMove& gameMove) {
		const uint32_t value = uint32_t(gameMove.value) & ((1u << RECORD_VALUE_BITS) - 1);
		return (uint32_t(gameMove.move) & RECORD_MOVE_MASK)
			| (uint32_t(gameMove.result) << RECORD_RESULT_SHIFT)
			| (value << RECORD_VALUE_SHIFT);
	}

	/**
	 * Unpacks a move record.
	 */
	constexpr GameMove unpackGameMove(uint32_t record) {
		const uint32_t valueBits = record >> RECORD_VALUE_SHIFT;
		// Sign extend the eleven bit value.
		const QaplaBasics::value_t value = QaplaBasics::value_t(
			valueBits >= (1u << (RECORD_VALUE_BITS - 1))
			? int32_t(valueBits) - (1 << RECORD_VALUE_BITS)
			: int32_t(valueBits));
		return GameMove{
			.move = QaplaBook::PackedMove(record & RECORD_MOVE_MASK),
			.value = value,
			.result = GameValue((record >> RECORD_RESULT_SHIFT) & 0x3) };
	}

	/**
	 * Writes a game: the length byte and the move records.
	 */
	inline void writeGame(std::ostream& stream, const std::vector<GameMove>& moves) {
		const uint8_t halfMoves = uint8_t(moves.size());
		stream.write(reinterpret_cast<const char*>(&halfMoves), 1);
		for (const GameMove& gameMove : moves) {
			const uint32_t record = packGameMove(gameMove);
			const char bytes[RECORD_BYTES] = { char(record & 0xFF),
				char((record >> 8) & 0xFF), char((record >> 16) & 0xFF) };
			stream.write(bytes, RECORD_BYTES);
		}
	}

	/**
	 * Reads one game. Returns false at the end of the file.
	 */
	inline bool readGame(std::istream& stream, std::vector<GameMove>& moves) {
		uint8_t halfMoves = 0;
		stream.read(reinterpret_cast<char*>(&halfMoves), 1);
		if (!stream || halfMoves == 0) return false;
		moves.clear();
		moves.reserve(halfMoves);
		for (uint32_t index = 0; index < halfMoves; index++) {
			unsigned char bytes[RECORD_BYTES] = { 0, 0, 0 };
			stream.read(reinterpret_cast<char*>(bytes), RECORD_BYTES);
			if (!stream) return false;
			moves.push_back(unpackGameMove(uint32_t(bytes[0])
				| (uint32_t(bytes[1]) << 8) | (uint32_t(bytes[2]) << 16)));
		}
		return true;
	}
}
