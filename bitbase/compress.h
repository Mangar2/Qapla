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
 */

#ifndef __COMPRESS_H
#define __COMPRESS_H

#include <queue>
#include <array>
#include <vector>
#include <unordered_map>

using namespace std;

namespace QaplaBitbase {

	enum CompType {
		COPY = 0,
		REPEAT = 1
	};

	static const uint8_t CONTINUE = 0x80;

	class Sequences {
		Sequences() : _index(0), _key(0) {}

		void addValue(uint8_t value) {
			_index++;
			_key = (_key << 8) + value;
			if (_index >= 4) {
				pair<uint64_t, uint64_t> elem(_key, _index);
				_sequences.insert(elem);
			}
		}

		/**
		 * Gets the longest matching index
		 */
		uint64_t countLongestMatch(const vector<uint8_t>& in, uint64_t index) {
			if (in.size() - index < 8) {
				return 0;
			}
			uint64_t key = 0; 
			for (int i = 0; i < 8; ++i) {
				key = (key << 8) + in[index + i];
			}
			auto iterator = _sequences.begin(key);
			for (; iterator != _sequences.end(key); ++iterator) {
				uint64_t start = iterator->first;
				uint64_t match = sizeof(uint64_t);
				for (; in.size() < index + match; ++match) {
					if (in[start + match] != in[index + match]) {
						break;
					}
				}
			}
		}

	private:
		uint64_t _key;
		uint64_t _index;
		unordered_map<uint64_t, uint64_t> _sequences;
	};

	/**
	 * Counts the amount of values that are equal
	 * @param data data stream
	 * @param index index to start counting equal values
	 * @returns amount of equal values starting at index (minimum = 1)
	 */
	static uint64_t countEqualValues(const vector<uint8_t>& in, uint64_t index) {
		uint8_t cur = in[index];
		uint64_t compIndex;
		uint64_t result = 1;
		for (compIndex = index + 1; compIndex < in.size(); compIndex++, result++) {
			if (in[compIndex] != cur) break;
		}
		return result;
	}

	/**
	 * Adds an info element to identify the compression type and length
	 * @param data data stream to add the info
	 * @type type of compression
	 * @count number of elements compressed
	 */
	static void addCompressionInfo(vector<uint8_t>& out, CompType type, uint64_t count) {
		uint8_t command = type;
		command |= (count & 0x1F) << 2;
		count >>= 5;
		while (count != 0) {
			command |= CONTINUE;
			out.push_back(command);
			command = count & 0x7F;
			count >>= 7;
		}
		out.push_back(command);
	}

	/**
	 * Adds compressed data of type "equal values"
	 */
	static uint64_t addEqualValues(vector<uint8_t>& out, uint64_t count, uint8_t value) {
		addCompressionInfo(out, REPEAT, count);
		out.push_back(value);
		return count;
	}

	/**
	 * Adds data with no compression
	 */
	static void addUncompressedValues(vector<uint8_t>& out, uint64_t count, const vector<uint8_t>& in, uint64_t index) {
		if (count == 0) return;
		addCompressionInfo(out, COPY, count);
		for (index = index - count; count > 0; --count, ++index) {
			out.push_back(in[index]);
		}
	}

	/**
	 * Compressed a vector
	 */
	static void compress(const vector<uint8_t>& in, vector<uint8_t>& out) {
		uint64_t uncompressed = 0;
		uint64_t index = 0;
		uint64_t last4Bytes = 0;
		
		while (index < in.size()) {
			const uint64_t equalValues = countEqualValues(in, index);
			last4Bytes = (last4Bytes << 8) + in[index];
			if (equalValues > 3) {
				addUncompressedValues(out, uncompressed, in, index);
				uncompressed = 0;
				index += addEqualValues(out, equalValues, in[index]);
			}
			else {
				index++;
				uncompressed++;
			}
		}
		addUncompressedValues(out, uncompressed, in, index);
	}

	/**
	 * Copy values from in to out starting by index
	 */
	static uint64_t copyValues(const vector<uint8_t>& in, vector<uint8_t>& out, uint64_t index, uint64_t count) {
		for (uint64_t i = count; i > 0; --i, ++index) {
			out.push_back(in[index]);
		}
		return count;
	}

	/**
	 * Repeat values from in to out
	 */
	static uint64_t repeatValues(const vector<uint8_t>& in, vector<uint8_t>& out, uint64_t index, uint64_t count) {
		for (; count > 0; --count) {
			out.push_back(in[index]);
		}
		return 1;
	}


	/**
	 * Uncompresses a vector
	 */
	static void uncompress(const vector<uint8_t>& in, vector<uint8_t>& out) {
		for (uint64_t index = 0; index < in.size();) {
			const CompType type = CompType(in[index] & 0x3);
			uint64_t count = (in[index] & 0x7F) >> 2;
			uint64_t mul = 1 << 5;
			while (in[index] & CONTINUE) {
				index++;
				count += (in[index] & 0x7F) * mul;
				mul <<= 7;
			}
			index++;
			switch (type) {
			case COPY:
				index += copyValues(in, out, index, count);
				break;
			case REPEAT:
				index += repeatValues(in, out, index, count);
				break;
			default:
				// intentionally left blank
				break;
			}
		}
	}


}


#endif		// __COMPRESS_H