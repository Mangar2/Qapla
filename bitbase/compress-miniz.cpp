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
#include "miniz.h"
#include "compress.h"



std::vector<QaplaCompress::bbt_t> QaplaCompress::minizCompress(const std::vector<bbt_t>& input) {
	uLongf compressedSize = compressBound(static_cast<uLongf>(input.size()));
	std::vector<bbt_t> compressed(compressedSize);

	uLongf size = static_cast<uLongf>(input.size()) * sizeof(bbt_t);
	int result = mz_compress2(
		reinterpret_cast<Bytef*>(compressed.data()),
		&compressedSize,
		reinterpret_cast<const Bytef*>(input.data()),
		static_cast<uLongf>(size),
		Z_BEST_COMPRESSION
	);

	if (result != Z_OK) {
		throw std::runtime_error("miniz compression failed");
	}

	compressed.resize(compressedSize);
	return compressed;
}

std::vector<QaplaCompress::bbt_t> QaplaCompress::minizUncompress(const std::vector<bbt_t>& compressed, size_t decompressedSize) {
	std::vector<bbt_t> decompressed(decompressedSize);
	uLongf decompressed_size = static_cast<uLongf>(decompressedSize);
	uLongf compressed_size = static_cast<uLongf>(compressed.size());

	int result = mz_uncompress2(
		reinterpret_cast<Bytef*>(decompressed.data()),
		&decompressed_size,
		reinterpret_cast<const Bytef*>(compressed.data()),
		&compressed_size);

	if (result != Z_OK) {
		throw std::runtime_error("miniz decompression failed");
	}

	return decompressed;
}



