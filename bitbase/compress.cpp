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
 */

#include <iostream>
#include <queue>
#include <array>
#include <vector>
#include <unordered_map>
#include "lz4.h"
#include "try.h"
#include "HuffmanCode.h"
#include "deflate.h"
#include "compress.h"

using namespace std;

/**
 * Compresses a vector using LZ4
 */
std::vector<QaplaCompress::bbt_t> QaplaCompress::lz4Compress(const std::vector<bbt_t>& input) {
	int maxCompressedSize = LZ4_compressBound(static_cast<int>(input.size()));
	std::vector<bbt_t> compressed(maxCompressedSize);

	int compressedSize = LZ4_compress_default(
		reinterpret_cast<const char*>(input.data()),
		reinterpret_cast<char*>(compressed.data()),
		static_cast<int>(input.size()),
		maxCompressedSize
	);

	if (compressedSize <= 0) {
		throw std::runtime_error("LZ4 compression failed");
	}

	compressed.resize(compressedSize);
	return compressed;
}

/**
 * Decompresses a vector using LZ4
 */
std::vector<QaplaCompress::bbt_t> QaplaCompress::lz4Uncompress(const std::vector<bbt_t>& compressed, size_t decompressedSize) {
	std::vector<bbt_t> decompressed(decompressedSize);

	int decompressedSizeActual = LZ4_decompress_safe(
		reinterpret_cast<const char*>(compressed.data()),
		reinterpret_cast<char*>(decompressed.data()),
		static_cast<int>(compressed.size()),
		static_cast<int>(decompressedSize)
	);

	if (decompressedSizeActual < 0) {
		throw std::runtime_error("LZ4 decompression failed");
	}

	return decompressed;
}

 /**
  * Compresses a vector using the best available method (default: LZ4)
  */
std::vector<QaplaCompress::bbt_t> QaplaCompress::compress(const std::vector<bbt_t>& in, bool verbose) {
	std::vector<bbt_t> out;

	// Default: use LZ4
	auto compressed = lz4Compress(in);
	out.push_back(static_cast<bbt_t>(Compression::lz4));
	out.insert(out.end(), compressed.begin(), compressed.end());

	if (verbose) {
		std::cout << "Original: " << in.size() << " compressed (LZ4): " << compressed.size()
			<< " total with header: " << out.size() << std::endl;
	}

	return out;
}


/**
 * Decompresses a vector
 */
std::vector<QaplaCompress::bbt_t> QaplaCompress::uncompress(const std::vector<bbt_t>& in, uint64_t outSize) {
	Compression compression = Compression(in[0]);
	std::vector<bbt_t> out;

	if (compression == Compression::huffman) {
		std::vector<bbt_t> tmp;
		HuffmanCode huffman;
		uint32_t codeSize = huffman.retrieveCode(in.begin() + 1);
		huffman.uncompress(in.begin() + codeSize + 1, tmp);
		out.resize(outSize);
		Deflate::uncompress(tmp.begin(), tmp.end(), out.begin(), outSize);
	}
	else if (compression == Compression::inflated) {
		out.resize(outSize);
		Deflate::uncompress(in.begin() + 1, in.end(), out.begin(), outSize);
	}
	else if (compression == Compression::lz4) {
		std::vector<bbt_t> compressed(in.begin() + 1, in.end());
		out = lz4Uncompress(compressed, outSize);
	}
	else {
		// uncompressed fallback
		out.resize(outSize);
		std::copy(in.begin() + 1, in.end(), out.begin());
	}

	return out;
}

