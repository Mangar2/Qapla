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

#ifndef _QAPLA_COMPRESS_INFLATE_H
#define _QAPLA_COMPRESS_INFLATE_H

#include <queue>
#include <array>
#include <vector>
#include <unordered_map>
#include "try.h"

using namespace std;

namespace QaplaCompress {

	namespace Deflate {

		typedef uint8_t bbt_t;

		enum CompType {
			COPY = 0,
			REFERENCE = 2
		};

		static const bbt_t CONTINUE = 0x80;

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

			void limitLength(uint64_t maxLen) {
				if (_length > maxLen) {
					_gain -= _length - maxLen;
					_length = maxLen;
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

			Sequences() : _sequenceTry(16), _index(0) {}

			/**
			 * Gets the longest matching index
			 */
			SequenceResult longestMatch(const vector<bbt_t>& in, uint64_t index) {
				SequenceResult result;
				for (; _index < index; _index++) {
					_sequenceTry.add(in, uint32_t(_index));
				}
				auto newLongestMatch = _sequenceTry.searchAndAdd(in, uint32_t(index));
				if (newLongestMatch.second > 5) {
					uint64_t delta = index - newLongestMatch.first;
					result.addMatch(delta, newLongestMatch.second);
				}
				return result;
			}

			/**
			 * Clears the memorized lookups
			 */
			void clear() {
				_sequenceTry.clear();
			}

			void print() {
				_sequenceTry.print();
			}

		private:
			uint64_t _index;
			Try<uint32_t> _sequenceTry;
		};

		/**
		 * Adds an info element to identify the compression type and length
		 * @param data data stream to add the info
		 * @type type of compression
		 * @count number of elements compressed
		 */
		static void addCompressionInfo(vector<bbt_t>& out, CompType type, uint64_t count) {
			constexpr auto sizeInBit = sizeof(bbt_t) * 8;
			const bbt_t typeBits = 2;
			bbt_t command = type;
			command |= (count & 0x1F) << typeBits;
			count >>= sizeInBit - typeBits - 1;
			while (count != 0) {
				command |= CONTINUE;
				out.push_back(command);
				command = count & 0x7F;
				count >>= sizeInBit - 1;
			}
			out.push_back(command);
		}

		/**
		 * Adds data with no compression
		 */
		static void addUncompressedValues(vector<bbt_t>& out, uint64_t count, const vector<bbt_t>& in, uint64_t index) {
			if (count == 0) return;
			addCompressionInfo(out, COPY, count);
			for (index = index - count; count > 0; --count, ++index) {
				out.push_back(in[index]);
			}
		}

		/**
		 * Adds the information to copy a sequence of data from a former position
		 */
		static uint64_t addSequence(vector<bbt_t>& out, SequenceResult seqResult) {
			addCompressionInfo(out, REFERENCE, seqResult._length);
			uint64_t delta = seqResult._delta;
			bbt_t value = delta & 0x7F;
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
		 * Compressed a vector by inflating
		 */
		static void compress(const vector<bbt_t>& in, vector<bbt_t>& out, uint64_t packetSize = 0x10000) {
			uint64_t uncompressed = 0;
			uint64_t index = 0;
			uint64_t last4Bytes = 0;
			uint64_t curMax = packetSize;
			uint64_t statistic = 0;

			Sequences sequences;

			while (index < in.size()) {
				if (index >= curMax) {
					addUncompressedValues(out, uncompressed, in, index);
					statistic++;
					uncompressed = 0;
					sequences.clear();
					curMax = min(in.size(), curMax + packetSize);
				}
				SequenceResult seqResult = sequences.longestMatch(in, index);
				seqResult.limitLength(curMax - index);

				if (seqResult._gain > 1) {
					addUncompressedValues(out, uncompressed, in, index);
					statistic++;
					uncompressed = 0;
					index += addSequence(out, seqResult);
					statistic++;
				}
				else {
					index++;
					uncompressed++;
				}
			}
			addUncompressedValues(out, uncompressed, in, index);
			statistic++;
			cout << "statistic: " << statistic << endl;
		}

		/**
		 * Gets a value with flexible length from a byte vector
		 */
		static uint64_t getValue(const vector<bbt_t>& in, uint64_t& index) {
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
		 * Gets a value with flexible length from a byte vector
		 */
		static uint64_t getValue(vector<bbt_t>::const_iterator& in) {
			uint64_t mul = 1;
			uint64_t value = *in & 0x7F;
			while (*in & CONTINUE) {
				in++;
				mul <<= 7;
				value += (*in & 0x7F) * mul;
			}
			in++;
			return value;
		}

		/**
		 * Copies a reference
		 */
		static void copyReference(vector<bbt_t>::const_iterator& in, vector<bbt_t>::iterator out, uint64_t& outIndex, uint64_t count) {
			uint64_t delta = getValue(in);
			uint64_t sourceIndex = outIndex - delta;
			if (sourceIndex + count <= outIndex) {
				// May not be used to copy overlapping ranges
				std::copy(out + sourceIndex, out + sourceIndex + count, out + outIndex);
				outIndex += count;
			}
			else {
				for (; count > 0; --count, ++sourceIndex, ++outIndex) {
					out[outIndex] = out[sourceIndex];
				}
			}
		}

		/**
		 * Uncompresses a vector by deflating
		 */
		static void uncompress(vector<bbt_t>::const_iterator in, vector<bbt_t>::const_iterator end,
			vector<bbt_t>::iterator out, uint64_t outSize)
		{
			uint64_t outIndex = 0;
			while (in != end) {
				const CompType type = CompType(*in & 0x3);
				uint64_t count = (*in & 0x7F) >> 2;
				uint64_t mul = 1 << 5;
				while (*in & CONTINUE) {
					in++;
					count += (*in & 0x7F) * mul;
					mul <<= 7;
				}
				in++;
				switch (type) {
				case COPY:
					std::copy(in, in + count, out + outIndex);
					in += count;
					outIndex += count;
					break;
				case REFERENCE:
					copyReference(in, out, outIndex, count);
					break;
				default:
					// intentionally left blank
					break;
				}
			}
		}

	}

}


#endif		// _QAPLA_COMPRESS_INFLATE_H