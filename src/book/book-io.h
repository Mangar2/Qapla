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
 * The header of a book file. It is a list of typed records, every record a
 * length prefixed string, terminated by a record of type zero and followed by
 * the number of nodes.
 *
 * A reader skips records it does not know, so the format can be extended
 * without breaking older readers - which is how the payload records below can be
 * added while a Spike build still reads the same file. The node size is stored
 * for exactly one reason: a book written with one payload and read with another
 * has to fail loudly instead of reinterpreting the bytes.
 *
 * The records hold native little endian integers, as the Spike books do. All
 * three target platforms are little endian; a big endian one would need to swap.
 */

#pragma once

#include <cstdint>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>

namespace QaplaBook {

	/**
	 * Thrown when a file is not a book, is truncated, or holds nodes of a size
	 * the reader cannot use.
	 */
	class BookFormatError : public std::runtime_error {
	public:
		explicit BookFormatError(const std::string& message)
			: std::runtime_error("book format: " + message) {
		}
	};

	/**
	 * The types of the header records. Everything up to FILE_VERSION is written
	 * by Spike as well, the two payload records are new and are skipped by a
	 * reader that does not know them.
	 */
	enum class BookHeaderTag : int32_t {
		END_OF_HEADER = 0,
		AUTHOR = 1,
		NAME = 2,
		BOOK_VERSION = 3,
		FILE_VERSION = 4,
		PAYLOAD_ID = 5,
		NODE_SIZE = 6
	};

	/**
	 * The file version written by this code. Versions "1" and "2" are the Spike
	 * books, they have no payload and two byte nodes.
	 */
	inline constexpr const char* BOOK_FILE_VERSION = "3";

	/**
	 * The header of a book file.
	 */
	struct BookHeader {
		std::string author;
		std::string name;
		std::string version;
		std::string fileVersion = BOOK_FILE_VERSION;
		/** Names what the payload of a node means, for example "eval-cp-v1". */
		std::string payloadId;
		/** Size of one node in the file, the 16 bit part included. */
		uint32_t nodeSize = 2;
	};

	/**
	 * Reads the header records and the node count that follows them.
	 * Returns the number of nodes in the file.
	 */
	uint32_t readBookHeader(std::istream& stream, BookHeader& header);

	/**
	 * Writes the header records and the node count.
	 */
	void writeBookHeader(std::ostream& stream, const BookHeader& header, uint32_t nodeCount);
}
