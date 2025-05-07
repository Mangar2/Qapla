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
#include <cstring>
#include "lz4.h"
#include "miniz.h"
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

	const char* minizErrorString(int code) {
		switch (code) {
		case Z_OK:             return "Z_OK";
		case Z_STREAM_END:     return "Z_STREAM_END";
		case Z_NEED_DICT:      return "Z_NEED_DICT";
		case Z_ERRNO:          return "Z_ERRNO";
		case Z_STREAM_ERROR:   return "Z_STREAM_ERROR";
		case Z_DATA_ERROR:     return "Z_DATA_ERROR";
		case Z_MEM_ERROR:      return "Z_MEM_ERROR";
		case Z_BUF_ERROR:      return "Z_BUF_ERROR";
		case Z_VERSION_ERROR:  return "Z_VERSION_ERROR";
		case Z_PARAM_ERROR:    return "Z_PARAM_ERROR";
		default:               return "UNKNOWN_ERROR";
		}
	}

	std::vector<uint8_t> minizCompressFn(const uint8_t* in, size_t inSize) {
		uLongf maxCompressedSize = compressBound(static_cast<uLongf>(inSize));
		std::vector<uint8_t> compressed(maxCompressedSize);

		uLongf compressedSize = maxCompressedSize;
		int result = mz_compress2(
			reinterpret_cast<Bytef*>(compressed.data()),
			&compressedSize,
			reinterpret_cast<const Bytef*>(in),
			static_cast<uLongf>(inSize),
			Z_BEST_COMPRESSION
		);

		if (result != Z_OK) {
			throw std::runtime_error(
				std::string("Miniz compression failed: ") + minizErrorString(result)
			);
		}

		compressed.resize(compressedSize);
		return compressed;
	}

	std::vector<uint8_t> minizDecompressFn(const uint8_t* in, size_t inSize, size_t expectedSize) {
		std::vector<uint8_t> decompressed(expectedSize);

		uLongf outSize = static_cast<uLongf>(expectedSize);
		uLongf inSizeCopy = static_cast<uLongf>(inSize);

		int result = mz_uncompress2(
			reinterpret_cast<Bytef*>(decompressed.data()),
			&outSize,
			reinterpret_cast<const Bytef*>(in),
			&inSizeCopy
		);

		if (result != Z_OK) {
			throw std::runtime_error(
				std::string("Miniz decompression failed: ") + minizErrorString(result)
			);
		}

		return decompressed;
	}


	CompressFn Compress::getCompressor(CompressionType type) {
		switch (type) {
		case CompressionType::None:  return noneCompressFn;
		case CompressionType::LZ4:   return lz4CompressFn;
		case CompressionType::Miniz: return minizCompressFn;
		default:                     throw std::runtime_error("Unknown compression type");
		}
	}

	DecompressFn Compress::getDecompressor(CompressionType type) {
		switch (type) {
		case CompressionType::None:  return noneDecompressFn;
		case CompressionType::LZ4:   return lz4DecompressFn;
		case CompressionType::Miniz: return minizDecompressFn;
		default:                     throw std::runtime_error("Unknown compression type");
		}
	}

}

