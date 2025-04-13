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
 * Implements compression for bitbases
 * The first byte contains an information about following bytes:
 * C NNNNN TT 
 * TT two bit compression type, either "COPY" or "REFERENCE"
 * NNNNN five bit number of bytes this referes to 
 * C continuation flag - if 1 the next byte has more bits of for the number of bytes
 * 
 * A continuation byte has the format
 * C NNNNNNN
 * C continuation flag - more bits following ...
 * NNNNNNN lower significant bits of the number
 * 
 * The uncompressed bytes to copy follows a  copy compression information
 * The number of bytes to move backward follows the REFERENCE compression type
 * The number is again coded with a flexibel amount of bytes by using the 
 * continuation byte structure explained above.
 */

#ifndef _QAPLA_COMPRESS_COMPRESS_H
#define _QAPLA_COMPRESS_COMPRESS_H

#include <cstdint>
#include <vector>

namespace QaplaCompress {
	using bbt_t = uint8_t;

	enum class Compression {
		uncompresed = 0,
		inflated = 1,
		huffman = 2,
		lz4 = 3
	};

	/**
	 * Compressed a vector
	 */
	std::vector<bbt_t> compress(const std::vector<bbt_t>& in, bool verbose = false);

	/**
	 * Uncompresses a vector
	 */
	std::vector<bbt_t> uncompress(const std::vector<bbt_t>&in, uint64_t outSize);

	/**
	 * Compresses a vector using LZ4
	 */
	std::vector<bbt_t> lz4Compress(const std::vector<bbt_t>& input);

	/**
	 * Decompresses a vector using LZ4
	 */
	std::vector<bbt_t> lz4Uncompress(const std::vector<bbt_t>& compressed, size_t decompressedSize);
}


#endif		// _QAPLA_COMPRESS_COMPRESS_H