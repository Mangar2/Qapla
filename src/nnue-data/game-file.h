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
 * A file begins with a magic and a version, and then holds game after game: a length
 * byte followed by that many move records of three bytes.
 *
 *   bytes 0..7     : "QAPLAGM2"
 *   bytes 8..11    : the version, little endian
 *   then per game  : one byte, the number of half moves, 1 to 255
 *   then per move  : (msb) PPPP PPPP PPPW WMMM MMMM MMMM (lsb)
 *
 * where
 *   M = the move, packed as in a book, see src/book/packed-move.h
 *   W = the result of the game, seen from the side to move, see GameValue
 *   P = the value of the position the move is played from, as a win probability for
 *       the side to move
 *
 * Three bytes hold 24 bits, and move plus value need 22 of them, so the result costs
 * nothing where it stands. It is the same result in every record of a game, only turned
 * around every ply - which saves the reader from turning it.
 *
 * The value is a **win probability** and not a number of pawns, which is what removes
 * the one real limit the format used to have. As pawns it was eleven bits of centipawns,
 * so about twelve pawns, and a game had to end when it left them - in exactly the
 * positions the net has the least idea of. A probability has no such range: a queen and
 * a rook down, a mate in three and a dead draw all fit, and the eleven bits give a
 * resolution of one in 2046, forty times finer than the error a trained net makes.
 *
 * It is also the number the loss is computed in, so the trainer reads it as it stands.
 * The engine keeps its own unit: pawns go in, a probability comes out, through
 * sigmoid(value / NET_VALUE_SCALE) - the same constant the net is quantized with, which
 * is why this file takes it from there instead of writing 400 down twice.
 *
 * The code 0 means the move has no value of its own. It happens for the moves of an
 * opening line that were played without a search.
 *
 * A reader needs a board and no move generator, and it can only seek to a game by
 * walking the games before it. Shuffling happens over whole games.
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <istream>
#include <ostream>
#include <string>
#include <vector>

#include "../book/packed-move.h"
#include "../nnue/nnue-arch.h"
#include "../../basics/evalvalue.h"

namespace QaplaNnueData {

	/**
	 * The result of a game, seen from the side to move of the position it is stored
	 * with. NONE is for a game whose result says nothing about its positions - two
	 * players of different strength, where the result is about the player.
	 */
	enum class GameValue : uint8_t {
		LOSS = 0,
		DRAW = 1,
		WIN = 2,
		NONE = 3
	};

	/** What the result is worth as a number, for a loss that is told from a draw. */
	constexpr double resultAsProbability(GameValue result) {
		switch (result) {
		case GameValue::LOSS: return 0.0;
		case GameValue::DRAW: return 0.5;
		case GameValue::WIN: return 1.0;
		default: return 0.5;
		}
	}

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

	/** The code that means "this move has no value". */
	inline constexpr uint32_t NO_GAME_VALUE = 0;
	/** The codes a probability uses, 1 for a certain loss and 2047 for a certain win. */
	inline constexpr uint32_t MIN_VALUE_CODE = 1;
	inline constexpr uint32_t MAX_VALUE_CODE = (1u << RECORD_VALUE_BITS) - 1;

	inline constexpr char GAME_FILE_MAGIC[8] = { 'Q', 'A', 'P', 'L', 'A', 'G', 'M', '2' };
	inline constexpr uint32_t GAME_FILE_VERSION = 2;

	/** The probability of a code, or a negative number for "no value". */
	constexpr double probabilityOfCode(uint32_t code) {
		if (code == NO_GAME_VALUE) return -1.0;
		return double(code - MIN_VALUE_CODE) / double(MAX_VALUE_CODE - MIN_VALUE_CODE);
	}

	/** The code of a probability. */
	inline uint32_t codeOfProbability(double probability) {
		const double clamped = probability < 0.0 ? 0.0 : (probability > 1.0 ? 1.0 : probability);
		return MIN_VALUE_CODE + uint32_t(std::lround(clamped
			* double(MAX_VALUE_CODE - MIN_VALUE_CODE)));
	}

	/**
	 * The code of a value of the engine, in its own unit where a pawn is 80 to 95. A
	 * mate value lands at a probability of one, which is where it belongs and which the
	 * old range of pawns could not hold.
	 */
	inline uint32_t codeOfValue(QaplaBasics::value_t value) {
		return codeOfProbability(1.0 / (1.0 + std::exp(-double(value)
			/ double(QaplaNnue::NET_VALUE_SCALE))));
	}

	/**
	 * One move of a game as it is stored.
	 */
	struct GameMove {
		QaplaBook::PackedMove move = QaplaBook::PACKED_MOVE_NONE;
		/** The win probability as its code, NO_GAME_VALUE for a move without one. */
		uint32_t value = NO_GAME_VALUE;
		GameValue result = GameValue::NONE;
	};

	/**
	 * Packs a move record into its 24 bits.
	 */
	constexpr uint32_t packGameMove(const GameMove& gameMove) {
		return (uint32_t(gameMove.move) & RECORD_MOVE_MASK)
			| (uint32_t(gameMove.result) << RECORD_RESULT_SHIFT)
			| ((gameMove.value & MAX_VALUE_CODE) << RECORD_VALUE_SHIFT);
	}

	/**
	 * Unpacks a move record.
	 */
	constexpr GameMove unpackGameMove(uint32_t record) {
		return GameMove{
			.move = QaplaBook::PackedMove(record & RECORD_MOVE_MASK),
			.value = (record >> RECORD_VALUE_SHIFT) & MAX_VALUE_CODE,
			.result = GameValue((record >> RECORD_RESULT_SHIFT) & 0x3) };
	}

	/**
	 * Writes a game: the length byte and the move records.
	 */
	inline void writeGameFileHeader(std::ostream& stream) {
		stream.write(GAME_FILE_MAGIC, sizeof(GAME_FILE_MAGIC));
		const uint32_t version = GAME_FILE_VERSION;
		stream.write(reinterpret_cast<const char*>(&version), sizeof(version));
	}

	/**
	 * Reads the header. False if the file is not one of games of this version - which is
	 * what keeps a file of the older format, whose values were pawns, from being read as
	 * probabilities without anyone noticing.
	 */
	inline bool readGameFileHeader(std::istream& stream) {
		char magic[sizeof(GAME_FILE_MAGIC)] = {};
		uint32_t version = 0;
		stream.read(magic, sizeof(magic));
		stream.read(reinterpret_cast<char*>(&version), sizeof(version));
		if (!stream) return false;
		return std::string(magic, sizeof(magic))
			== std::string(GAME_FILE_MAGIC, sizeof(GAME_FILE_MAGIC))
			&& version == GAME_FILE_VERSION;
	}

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
