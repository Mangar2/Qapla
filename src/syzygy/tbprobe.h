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
 * @Overview
 * Syzygy tablebase format layer: turns a position into a raw table entry.
 *
 * This is the complete public interface of src/syzygy. Nothing in this directory
 * knows about the engine - no board, no move generator, no search. The caller
 * fills a TbPosition and gets the stored entry back, unchanged.
 *
 * The corrections the format demands on top of a raw entry - resolving captures
 * for a win/draw/loss answer, zeroing moves and the side switch for a distance
 * answer - need moves and are therefore engine code, not part of this layer.
 *
 * See src/syzygy/README.md for origin and licence of the ported code.
 */

#pragma once

#include <cstdint>
#include <string>

namespace QaplaSyzygy {

	/** Maximum number of pieces the Syzygy format supports. */
	constexpr int TB_MAX_PIECES = 7;

	/**
	 * Piece encoding of the handover structure. This is the interface's own
	 * encoding - neither the engine's nor the one used inside the format layer.
	 */
	enum TbPieceCode : uint8_t {
		WhitePawn = 0, WhiteKnight, WhiteBishop, WhiteRook, WhiteQueen, WhiteKing,
		BlackPawn, BlackKnight, BlackBishop, BlackRook, BlackQueen, BlackKing,
		TbPieceCodeAmount
	};

	/**
	 * Everything the index computation needs, and nothing else.
	 * The list must be sorted ascending by piece; squares of equal pieces
	 * ascending is what the natural bitboard walk produces and is not required.
	 */
	struct TbPosition {
		uint8_t square[TB_MAX_PIECES];   // A1 = 0 ... H8 = 63
		uint8_t piece[TB_MAX_PIECES];    // TbPieceCode, ascending
		uint8_t pieceCount;              // 2 ... TB_MAX_PIECES
		bool    whiteToMove;
	};

	/** Result of a win/draw/loss lookup, from the point of view of the side to move. */
	enum class Wdl : int8_t {
		Loss = -2,          // loss
		BlessedLoss = -1,   // loss, but draw under the 50 move rule
		Draw = 0,           // draw
		CursedWin = 1,      // win, but draw under the 50 move rule
		Win = 2             // win
	};

	enum class Status : uint8_t {
		Ok = 0,             // the entry is valid
		NoTable,            // no table file for this material
		OtherSideToMove     // distance tables only: the entry answers for the other side
	};

	struct WdlEntry {
		Status status;
		Wdl    value;
	};

	struct DtzEntry {
		Status  status;
		int32_t distance;
	};

	/**
	 * Reads the stored win/draw/loss entry. The value is only the true value of the
	 * position when no capture is available - where one is, the format stores a
	 * "don't care" and the caller has to resolve the captures itself.
	 */
	WdlEntry probeWdlEntry(const TbPosition& pos);

	/**
	 * Reads the stored distance to zero entry. Needs the win/draw/loss value of the
	 * same position, because the stored value is remapped per outcome.
	 */
	DtzEntry probeDtzEntry(const TbPosition& pos, Wdl wdl);

	struct LoadResult {
		uint32_t wdlFiles = 0;
		uint32_t dtzFiles = 0;
		uint32_t maxCardinality = 0;
	};

	/**
	 * Scans the given directories for table files. Multiple directories are
	 * separated by ';' on Windows and by ':' elsewhere. An empty path releases
	 * everything and leaves no table loaded.
	 */
	LoadResult setPath(const std::string& paths);

	/** Releases all mapped files. */
	void release();

	/** Highest number of pieces any loaded table covers, 0 if none are loaded. */
	uint32_t maxCardinality();

}
