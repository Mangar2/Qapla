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
		REPEAT = 1,
		REFERENCE = 2
	};

	static const uint8_t CONTINUE = 0x80;

	struct SequenceResult {
		SequenceResult() : _delta(0), _length(0), _gain(0) {}

		/**
		 * Adds a match found
		 * @param delta index delta where the match has been found
		 * @param length length of the match
		 */
		void addMatch(uint64_t delta, uint64_t length) {
			int64_t newGain = computeGain(delta, length);
			if (newGain > _gain) {
				_gain = newGain;
				_length = length;
				_delta = delta;
			}
		}

		/**
		 * The gain is the length of the copied section minus the amount of 
		 * bytes needed to store the copy section.
		 */
		int64_t computeGain(uint64_t delta, uint64_t length) {
			int64_t gain = length - 2;
			if (length >= 32) {
				gain--;
				length /= 32;
			}
			while (length >= 128) {
				gain--;
				length /= 128;
			}
			while (delta >= 128) {
				gain--;
				delta /= 128;
			}
			return gain;
		}

		uint64_t _delta;
		uint64_t _length;
		int64_t _gain;
	};

	class Sequences {
	public:
		typedef uint64_t key_t;

		Sequences() : _key(0), _index(0) {}

		/**
		 * Limits the amount of entries in a multimap for a key
	 	 */
		void limitEntries(uint64_t key, uint32_t max) {
			auto range = _sequences.equal_range(_key);
			uint64_t smallestIndex = range.first->second;
			auto candidate = range.first;
			uint32_t count = 0;
			for (auto elem = range.first; elem != range.second; elem++) {
				if (elem->second < smallestIndex) {
					candidate = elem;
					smallestIndex = elem->second;
				}
				count++;
			}
			if (count > max) {
				_sequences.erase(candidate);
			}
		}

		/**
		 * Informs about new values
		 */
		void addValues(const vector<uint8_t>& in, uint64_t toIndex) {
			for (; _index <= toIndex; _index++) {
				_key = (_key << 8) + in[_index];
				if (_index >= sizeof(key_t)) {
					const uint64_t realIndex = _index - sizeof(key_t) + 1;
					pair<key_t, uint64_t> elem(_key, realIndex);
					_sequences.insert(elem);
					limitEntries(_key, 20);
				}
			}
		}

		/**
		 * Gets the longest matching index
		 */
		SequenceResult longestMatch(const vector<uint8_t>& in, uint64_t index) {
			SequenceResult result;
			if (in.size() - index < sizeof(key_t)) {
				return result;
			}
			key_t key = 0;

			for (int i = 0; i < sizeof(key); ++i) {
				key = (key << 8) + in[index + i];
			}
			auto range = _sequences.equal_range(key);
			for (auto elem = range.first; elem != range.second; elem++) {
				uint64_t start = elem->second;
				uint64_t length = sizeof(key_t);
				for (; index + length < in.size(); ++length) {
					if (in[start + length] != in[index + length]) {
						break;
					}
				}
				result.addMatch(index - start, length);
			}
			return result;
		}

	private:
		key_t _key;
		uint64_t _index;
		// unordered_map<key_t, uint64_t> _sequences;
		unordered_multimap<key_t, uint64_t> _sequences;
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
	 * Adds the information to copy a sequence of data from a former position
	 */
	static uint64_t addSequence(vector<uint8_t>& out, SequenceResult seqResult) {
		addCompressionInfo(out, REFERENCE, seqResult._length);
		uint64_t delta = seqResult._delta;
		uint8_t value = delta & 0x7F;
		delta >>= 7;
		while (delta != 0) {
			value |= CONTINUE;
			out.push_back(value);
			value = delta & 0x7F;
			delta >>= 7;
		}
		out.push_back(value);
		return seqResult._length;
	}

	/**
	 * Compressed a vector
	 */
	static void compress(const vector<uint8_t>& in, vector<uint8_t>& out) {
		uint64_t uncompressed = 0;
		uint64_t index = 0;
		uint64_t last4Bytes = 0;
		Sequences sequences;
		
		while (index < in.size()) {
			sequences.addValues(in, index);
			const SequenceResult seqResult = sequences.longestMatch(in, index);
			if (seqResult._gain > 1) {
				addUncompressedValues(out, uncompressed, in, index);
				uncompressed = 0;
				index += addSequence(out, seqResult);
			}
			else {
				index++;
				uncompressed++;
			}
		}
		addUncompressedValues(out, uncompressed, in, index);
	}

	/**
	 * Gets a value with flexible length from a byte vector
	 */
	static uint64_t getValue(const vector<uint8_t>& in, uint64_t& index) {
		uint64_t mul = 1;
		uint64_t value = in[index] & 0x7F;
		while (in[index] & CONTINUE) {
			index++;
			mul <<= 7;
			value += (in[index] & 0x7F) * mul;
		}
		index++;
		return value;
	}


	/**
	 * Copy values from in to out starting by index
	 */
	static uint64_t copyValues(const vector<uint8_t>& in, vector<uint8_t>& out, uint64_t index, uint64_t& outIndex, uint64_t count) {

		std::copy(in.begin() + index, in.begin() + index + count, out.begin() + outIndex);
		outIndex += count;
		return count;
	}

	/**
	 * Repeat values from in to out
	 */
	static uint64_t repeatValues(const vector<uint8_t>& in, vector<uint8_t>& out, uint64_t index, uint64_t& outIndex, uint64_t count) {
		fill(out.begin() + outIndex, out.begin() + outIndex + count, in[index]);
		outIndex += count;
		return 1;
	}

	/**
	 * Copies a reference
	 */
	static uint64_t copyReference(const vector<uint8_t>& in, vector<uint8_t>& out, uint64_t index, uint64_t& outIndex, uint64_t count) {
		uint64_t newIndex = index;
		uint64_t delta = getValue(in, newIndex);
		uint64_t sourceIndex = outIndex - delta;
		if (sourceIndex + count <= outIndex) {
			// May not be used to copy overlapping ranges
			std::copy(out.begin() + sourceIndex, out.begin() + sourceIndex + count, out.begin() + outIndex);
			outIndex += count;
		}
		else {
			for (; count > 0; --count, ++sourceIndex, ++outIndex) {
				out[outIndex] = out[sourceIndex];
			}
		}
		
		
		return newIndex - index;
	}


	/**
	 * Uncompresses a vector
	 */
	static void uncompress(const vector<uint8_t>& in, vector<uint8_t>& out, uint64_t outSize) {
		uint64_t outIndex = 0;
		out.resize(outSize);
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
				index += copyValues(in, out, index, outIndex, count);
				break;
			case REPEAT:
				index += repeatValues(in, out, index, outIndex, count);
				break;
			case REFERENCE:
				index += copyReference(in, out, index, outIndex, count);
				break;
			default:
				// intentionally left blank
				break;
			}
		}
	}


}


#endif		// __COMPRESS_H