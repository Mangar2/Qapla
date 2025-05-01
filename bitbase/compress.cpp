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
namespace QaplaCompress {

	std::vector<uint8_t> noneCompressFn(const uint8_t* in, size_t inSize) {
		std::vector<uint8_t> out(inSize);
		std::memcpy(out.data(), in, inSize);
		return out;
	}

	std::vector<uint8_t> noneDecompressFn(const uint8_t* in, size_t inSize, size_t expectedSize) {
		if (inSize != expectedSize) {
			throw std::runtime_error("None decompression size mismatch");
		}
		std::vector<uint8_t> out(expectedSize);
		std::memcpy(out.data(), in, expectedSize);
		return out;
	}

	std::vector<uint8_t> lz4CompressFn(const uint8_t* in, size_t inSize) {
		const int maxSize = LZ4_compressBound(static_cast<int>(inSize));
		std::vector<uint8_t> out(maxSize);

		const int compressedSize = LZ4_compress_default(
			reinterpret_cast<const char*>(in),
			reinterpret_cast<char*>(out.data()),
			static_cast<int>(inSize),
			maxSize
		);

		if (compressedSize <= 0) {
			throw std::runtime_error("LZ4 compression failed");
		}

		out.resize(compressedSize);
		return out;
	}

	std::vector<uint8_t> lz4DecompressFn(const uint8_t* in, size_t inSize, size_t expectedSize) {
		std::vector<uint8_t> out(expectedSize);

		const int actualSize = LZ4_decompress_safe(
			reinterpret_cast<const char*>(in),
			reinterpret_cast<char*>(out.data()),
			static_cast<int>(inSize),
			static_cast<int>(expectedSize)
		);

		if (actualSize < 0) {
			throw std::runtime_error("LZ4 decompression failed");
		}

		return out;
	}


	CompressFn Compress::getCompressor(CompressionType type) {
		switch (type) {
		case CompressionType::None:  return noneCompressFn;
		case CompressionType::LZ4:   return lz4CompressFn;
		case CompressionType::Miniz: throw std::runtime_error("Miniz compression not implemented");
		default:                     throw std::runtime_error("Unknown compression type");
		}
	}

	DecompressFn Compress::getDecompressor(CompressionType type) {
		switch (type) {
		case CompressionType::None:  return noneDecompressFn;
		case CompressionType::LZ4:   return lz4DecompressFn;
		case CompressionType::Miniz: throw std::runtime_error("Miniz decompression not implemented");
		default:                     throw std::runtime_error("Unknown compression type");
		}
	}

	/**
	 * Compresses a vector using the best available method (default: LZ4)
	 */
	std::vector<QaplaCompress::bbt_t> QaplaCompress::compress(const std::vector<bbt_t>& in, bool verbose) {
		std::vector<bbt_t> out;
		/*
		// Default: use LZ4
		auto compressed = lz4Compress(in);
		out.push_back(static_cast<bbt_t>(Compression::lz4));
		out.insert(out.end(), compressed.begin(), compressed.end());

		if (verbose) {
			std::cout << "Original: " << in.size() << " compressed (LZ4): " << compressed.size()
				<< " total with header: " << out.size() << std::endl;
		}
		*/
		// Use miniz
		auto compressed = minizCompress(in);
		out.push_back(static_cast<bbt_t>(Compression::miniz));
		out.insert(out.end(), compressed.begin(), compressed.end());
		if (verbose) {
			std::cout << "Original: " << in.size() << " compressed (miniz): " << compressed.size()
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
		else if (compression == Compression::miniz) {
			std::vector<bbt_t> compressed(in.begin() + 1, in.end());
			out = minizUncompress(compressed, outSize);
		}
		else {
			// uncompressed fallback
			out.resize(outSize);
			std::copy(in.begin() + 1, in.end(), out.begin());
		}

		return out;
	}
}

