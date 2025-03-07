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
#include "try.h"
#include "HuffmanCode.h"
#include "deflate.h"
#include "compress.h"

using namespace std;

/**
 * Compressed a vector
 */
void QaplaCompress::compress(const vector<bbt_t>& in, vector<bbt_t>& out, bool verbose) {
	vector<bbt_t> tmp;
	tmp.reserve(in.size());
	Deflate::compress(in, tmp, 0x100000);
	if (tmp.size() < 2048) {
		// This is too small for huffman
		out.push_back(bbt_t(Compression::inflated));
		out.insert(out.end(), tmp.begin(), tmp.end());
	}
	else {
		HuffmanCode huffman(tmp);
		uint64_t expectedSize = 256 + (huffman.computeSizeInBit() / (sizeof(bbt_t) * 8));
		if (expectedSize > tmp.size()) {
			out.push_back(bbt_t(Compression::inflated));
			out.insert(out.end(), tmp.begin(), tmp.end());
		} 
		else {
			out.push_back(bbt_t(Compression::huffman));
			huffman.storeCode(out);
			huffman.compress(tmp.begin(), out, uint32_t(tmp.size()));
		}
	}
	if (verbose) {
		cout << "Original: " << in.size() << " inflated: " << tmp.size() << " final: " << out.size() << endl;
	}
}


/**
 * Uncompresses a vector
 */
void QaplaCompress::uncompress(const vector<bbt_t>& in, vector<bbt_t>& out, uint64_t outSize) {
	Compression compression = Compression(in[0]);
	out.resize(outSize);
	if (compression == Compression::huffman) {
		vector<bbt_t> tmp;
		//tmp.reserve(outSize);
		HuffmanCode huffman;
		uint32_t codeSize = huffman.retrieveCode(in.begin() + 1);
		huffman.uncompress(in.begin() + codeSize + 1, tmp);
		Deflate::uncompress(tmp.begin(), tmp.end(), out.begin(), outSize);
	}
	else if (compression == Compression::inflated) {
		Deflate::uncompress(in.begin() + 1, in.end(), out.begin(), outSize);
	}
	else {
		std::copy(in.begin() + 1, in.end(), out.begin());
	}
}

