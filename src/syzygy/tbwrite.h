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
 * Writes win/draw/loss tables in the Syzygy format.
 *
 * The counterpart of tbprobe.h. It knows nothing about where the values come
 * from - the caller asks where a position's value belongs, fills the tables and
 * hands them over. The index both directions use is the shared one in tbindex.h,
 * so a file written here is read by the prober without either side knowing about
 * the other.
 *
 * Compression is Huffman over the stored values, without the recursive pairing
 * grammar de Man's generator builds on top of it. That is a valid file - the
 * grammar is optional, a symbol without children is a leaf - and it costs size,
 * not correctness.
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "tbprobe.h"

namespace QaplaSyzygy {

	/** Marks a slot no legal position reaches. Such slots may hold anything. */
	constexpr uint8_t TB_UNREACHED = 0xFF;

	/**
	 * Added to a value it says: the entry may be stored lower than this, down to a
	 * loss, because a capture already reaches the true value and the reader takes the
	 * better of the two.
	 *
	 * This is where the size of a table is decided. Nearly every entry of a won
	 * endgame carries it, and letting the writer lower them all to the same value
	 * turns the table into one long run - de Man's KRvK holds nine entries that are
	 * not a loss, out of 31332.
	 */
	constexpr uint8_t TB_REDUCIBLE = 0x80;

	/** The stored value set, from the side to move. */
	enum TbStoredWdl : uint8_t {
		StoredLoss = 0, StoredBlessedLoss = 1, StoredDraw = 2, StoredCursedWin = 3, StoredWin = 4
	};

	/** Where the value of one position belongs. */
	struct WdlSlot {
		int      side = 0;    // side to move table
		int      file = 0;    // file table of the leading pawn, 0 without pawns
		uint64_t index = 0;
	};

	/**
	 * Recomputes the check bytes of a table file and compares them against the ones it
	 * carries. de Man puts a checksum in the last sixteen bytes and no probing code
	 * looks at it; this verifies the ones written here.
	 *
	 * @param reason filled with what is wrong when the answer is false
	 */
	bool verifyChecksum(const std::string& filePath, std::string& reason);

	class WdlWriter {
	public:
		/**
		 * @param code material of the table, in the shape of its file name ("KRvK")
		 */
		explicit WdlWriter(const std::string& code);
		~WdlWriter();

		WdlWriter(const WdlWriter&) = delete;
		WdlWriter& operator=(const WdlWriter&) = delete;

		/**
		 * Whether this material can be written at all. Pawn tables are split by file
		 * and ordered by the leading pawn, which is not built yet - so they are
		 * refused rather than written wrongly.
		 */
		bool isSupported(std::string& reason) const;

		/** Number of side-to-move tables: 2, or 1 when both sides hold the same material. */
		int sideCount() const { return _sideCount; }

		/** Number of file tables: 1 without pawns, 4 with them. */
		int fileCount() const { return _fileCount; }

		/** Number of values one table holds. */
		uint64_t tableSize(int side, int file) const;

		/** The slot the value of this position belongs to. */
		WdlSlot slotOf(const TbPosition& pos) const;

		/**
		 * Writes the file.
		 *
		 * @param filePath destination, ending in .rtbw
		 * @param values   values[side][file], each of tableSize() entries, in the
		 *                 stored value set; TB_UNREACHED where nothing reached the slot
		 * @throws std::runtime_error on a size mismatch or an I/O failure
		 */
		void write(const std::string& filePath,
			const std::vector<uint8_t> values[2][4]) const;

	private:
		struct Layout;

		std::string              _code;
		int                      _sideCount = 2;
		int                      _fileCount = 1;
		std::string              _unsupported;
		std::unique_ptr<Layout>  _layout;
	};

}
