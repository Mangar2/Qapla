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
 * Reads and writes the header of a book file, see book-io.h
 */

#include <charconv>
#include <vector>

#include "book-io.h"

using namespace QaplaBook;

namespace {

	/** Longest string a header record may hold, a corrupt file must not allocate. */
	constexpr int32_t MAX_RECORD_LENGTH = 1 << 16;

	int32_t readInt32(std::istream& stream) {
		int32_t value = 0;
		stream.read(reinterpret_cast<char*>(&value), sizeof(value));
		if (!stream) throw BookFormatError("file ends inside the header");
		return value;
	}

	void writeInt32(std::ostream& stream, int32_t value) {
		stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
	}

	/**
	 * Reads a record string: a 32 bit length followed by that many characters,
	 * the terminating zero included in the length.
	 */
	std::string readString(std::istream& stream) {
		const int32_t length = readInt32(stream);
		if (length < 0 || length > MAX_RECORD_LENGTH) {
			throw BookFormatError("header record with an implausible length");
		}
		if (length == 0) return {};
		std::vector<char> buffer(static_cast<size_t>(length));
		stream.read(buffer.data(), length);
		if (!stream) throw BookFormatError("file ends inside a header record");
		// The stored string is zero terminated, the terminator is not part of it.
		const size_t used = buffer.back() == '\0' ? size_t(length - 1) : size_t(length);
		return std::string(buffer.data(), used);
	}

	void writeString(std::ostream& stream, BookHeaderTag tag, const std::string& value) {
		writeInt32(stream, int32_t(tag));
		writeInt32(stream, int32_t(value.size() + 1));
		stream.write(value.c_str(), std::streamsize(value.size() + 1));
	}
}

uint32_t QaplaBook::readBookHeader(std::istream& stream, BookHeader& header) {
	header = BookHeader{};
	// A file without a version record is a book of the first generation.
	header.fileVersion = "1";
	header.nodeSize = 2;
	bool nodeSizeSeen = false;

	for (auto tag = BookHeaderTag(readInt32(stream));
		tag != BookHeaderTag::END_OF_HEADER;
		tag = BookHeaderTag(readInt32(stream))) {
		const std::string value = readString(stream);
		switch (tag) {
		case BookHeaderTag::AUTHOR: header.author = value; break;
		case BookHeaderTag::NAME: header.name = value; break;
		case BookHeaderTag::BOOK_VERSION: header.version = value; break;
		case BookHeaderTag::FILE_VERSION: header.fileVersion = value; break;
		case BookHeaderTag::PAYLOAD_ID: header.payloadId = value; break;
		case BookHeaderTag::NODE_SIZE: {
			uint32_t nodeSize = 0;
			const auto result = std::from_chars(value.data(), value.data() + value.size(), nodeSize);
			if (result.ec != std::errc() || nodeSize == 0) {
				throw BookFormatError("node size record is not a number");
			}
			header.nodeSize = nodeSize;
			nodeSizeSeen = true;
			break;
		}
		default:
			// An unknown record is skipped, that is what keeps the format
			// extensible in both directions.
			break;
		}
	}
	if (!nodeSizeSeen) header.nodeSize = 2;

	const int32_t nodeCount = readInt32(stream);
	if (nodeCount < 0) throw BookFormatError("negative node count");
	return uint32_t(nodeCount);
}

void QaplaBook::writeBookHeader(std::ostream& stream, const BookHeader& header, uint32_t nodeCount) {
	writeString(stream, BookHeaderTag::AUTHOR, header.author);
	writeString(stream, BookHeaderTag::NAME, header.name);
	writeString(stream, BookHeaderTag::BOOK_VERSION, header.version);
	writeString(stream, BookHeaderTag::FILE_VERSION, header.fileVersion);
	writeString(stream, BookHeaderTag::PAYLOAD_ID, header.payloadId);
	writeString(stream, BookHeaderTag::NODE_SIZE, std::to_string(header.nodeSize));
	writeInt32(stream, int32_t(BookHeaderTag::END_OF_HEADER));
	writeInt32(stream, int32_t(nodeCount));
}
