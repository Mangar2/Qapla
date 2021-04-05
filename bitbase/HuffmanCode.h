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
 * Implements huffman compression for bitbases
 */

#ifndef __HUFFMANCODE_H
#define __HUFFMANCODE_H

#include <queue>
#include <array>
#include <vector>

using namespace std;

namespace QaplaBitbase {
	typedef uint8_t bbt_t;

	struct HuffmanNode {

		/**
		 * Creates a new leaf node
		 */
		HuffmanNode(uint32_t frequencyOfData, bbt_t data)
			: frequency(frequencyOfData), leafData(data), left(NULL), right(NULL)
		{}

		/**
		 * Creates a new non leaf node with childs "left" and "right"
		 */
		HuffmanNode(HuffmanNode* leftTree, HuffmanNode* rightTree)
			: left(leftTree), right(rightTree), frequency(0), leafData(0)
		{
			if (left != NULL) {
				frequency += left->frequency;
			}
			if (right != NULL) {
				frequency += right->frequency;
			}
		}

		void print() {
			if (right == NULL && left == NULL) {
				printf("%5d: %4d\n", frequency, leafData);
			}
			else {
				printf("%5d: node\n", frequency);
			}
		}

		/**
		 * True, if the node is a leaf
		 */
		bool isLeaf() const { return left == NULL; }

		uint32_t frequency;
		HuffmanNode* left;
		HuffmanNode* right;
		uint8_t leafData;
	};

	class HuffmanCode {
	public:
		/**
		 * Builds a huffman code from a data vector
		 */
		HuffmanCode() {
			clear();
		}
		HuffmanCode(const vector<bbt_t>& in) {
			builtCode(in);
		}

		/**
		 * Build the huffman code to compress
		 */
		void builtCode(const vector<bbt_t>& in) {
			clear();
			count(in);
			builtInitialForrest();
			while (forrest.size() > 1) {
				optimize();
			}
			buildCode(forrest.top(), 0, 0);
		}

		/**
		 * Stores the huffman code
		 * @param out vector the code is pushed to
		 * @returns amount of "bbt_t" elements used to store the code
		 */
		uint32_t storeCode(vector<bbt_t>& out) {
			constexpr auto sizeInBit = sizeof(bbt_t) * 8;
			uint64_t indexInBit = storeCode(out, forrest.top(), 0);
			uint32_t codeSizeInElements = uint32_t(indexInBit / sizeInBit + 1 - indexInBit % sizeInBit);
			return codeSizeInElements;
		}

		/**
		 * Use the calculated huffman code to compress the data
		 */
		void compress(vector<bbt_t>::iterator in, vector<bbt_t>& out, uint32_t size) {
			constexpr auto sizeInBit = sizeof(bbt_t) * 8;
			// stores the amount of elements compressed
			setValue(out, size * sizeInBit, size);
			uint32_t bitsLeft = sizeInBit;
			out.push_back(0);

			for (uint64_t inIndex = 0; inIndex < size; ++inIndex) {
				uint64_t code = codeMap[in[inIndex]];
				uint32_t codeLength = codeLengthMap[in[inIndex]];
				while (bitsLeft <= codeLength) {
					out.back() |= code >> (codeLength - bitsLeft);
					codeLength -= bitsLeft;
					bitsLeft = sizeInBit;
					out.push_back(0);
				}
				bitsLeft -= codeLength;
				out.back() |= code << bitsLeft;
			}
		}

		/**
		 * Uncompresses code from the 
		 */
		void uncompress(const vector<bbt_t>& in, vector<bbt_t>& out, size_t targetSizeInBit) {
			constexpr auto sizeInBit = sizeof(bbt_t) * 8;
			size_t outIndex = 0;
			size_t inIndex = 0;
			bbt_t uncompressedElement = 0;
			inIndex = retrieveCode(in, 0, 0, 0);
			inIndex += sizeInBit - (inIndex % sizeInBit);
			printCode();
			while (outIndex < targetSizeInBit) {
				const bbt_t nextElement = uncompressNext(in, inIndex);
				inIndex += codeLengthMap[nextElement];
				outIndex += sizeof(uint8_t);
				const bbt_t bitsLeft = sizeInBit - outIndex % sizeInBit;
				uncompressedElement |= nextElement << bitsLeft;
				if (bitsLeft == 0) {
					out.push_back(uncompressedElement);
					uncompressedElement = 0;
				}
			}
		}

		/**
		 * Recursively builds the code from a balanced huffman tree
		 */
		void buildCode(HuffmanNode* node, uint8_t codeLength, uint32_t code) {
			if (node->isLeaf()) {
				// node->print();
				codeMap[node->leafData] = code;
				codeLengthMap[node->leafData] = codeLength;
			}
			else {
				buildCode(node->left, codeLength + 1, code * 2 + 0);
				buildCode(node->right, codeLength + 1, code * 2 + 1);
			}
		}

		/**
		 * Prints the computed huffman coding
		 */
		void printCode() {
			cout << endl;
			for (uint32_t index = 0; index < WORD_SIZE; index++) {
				if (codeLengthMap[index] > 0) {
					cout << index << " (" << codeMap[index] << ")";
					for (int16_t bitAmount = codeLengthMap[index] - 1; bitAmount >= 0; bitAmount--) {
						cout << " " << ((codeMap[index] >> bitAmount) & 1);
					}
					cout << endl;
				}
			}
		}

		/**
		 * Computes the expected size of the data after compression
		 */
		uint64_t computeSizeInBit() {
			uint64_t sizeInBit = 0;
			for (uint32_t index = 0; index < WORD_SIZE; index++) {
				sizeInBit += codeLengthMap[index] * frequency[index] * 1ULL;
			}
			return sizeInBit;
		}

	private:

		/**
		 * Stores the huffman tree to a byte vector (recursively)
		 * @param out output storage
		 * @param node huffman tree node to store
		 * @param bitIndex index to the output vector
		 * @returns new index position
		 */
		uint64_t storeCode(vector<bbt_t>& out, const HuffmanNode* node, uint64_t bitIndex) {
			constexpr auto sizeInBit = sizeof(bbt_t) * 8;
			uint64_t bbtIndex = bitIndex / sizeInBit;
			// Add enough space to store leaf data
			while (out.size() <= bbtIndex + 2) {
				out.push_back(0);
			}
			if (node->isLeaf()) {
				setBit(out, bitIndex);
				setValue(out, bitIndex + 1, node->leafData);
				bitIndex += 1 + sizeInBit;
			}
			else {
				clearBit(out, bitIndex);
				bitIndex = storeCode(out, node->left, bitIndex + 1);
				bitIndex = storeCode(out, node->right, bitIndex + 1);
			}
			return bitIndex;
		}

		/**
		 * Retrieves the huffman tree from a byte vector (recursively)
		 * @param in input data
		 * @param node huffman tree node created
		 * @param bitIndex index to the output vector
		 * @returns new index position
		 */
		uint64_t retrieveCode(const vector<bbt_t>& in, uint32_t codeLength, uint64_t code, uint64_t bitIndex) {
			constexpr auto sizeInBit = sizeof(bbt_t) * 8;
			uint64_t bbtIndex = bitIndex / sizeInBit;
			bool isLeafNode = getBit(in, bitIndex);
			bitIndex++;
			if (isLeafNode) {
				const bbt_t value = getValue(in, bitIndex);
				codeMap[value] = code;
				codeLengthMap[value] = codeLength;
				bitIndex += sizeInBit;
			}
			else {
				code <<= 1;
				bitIndex = retrieveCode(in, codeLength + 1, code, bitIndex);
				bitIndex = retrieveCode(in, codeLength + 1, code + 1, bitIndex);
			}
			return bitIndex;
		}

		/**
		 * Sets a single bit in the _bitbase
		 */
		void setBit(vector<bbt_t>& out, uint64_t index) {
			uint64_t bbtIndex = index / (sizeof(bbt_t) * 8);
			out[bbtIndex] |= bbt_t(1) << (index % (sizeof(bbt_t) * 8));
		}

		/**
		 * Clears a single bit in the _bitbase
		 */
		void clearBit(vector<bbt_t>& out, uint64_t index) {
			uint64_t bbtIndex = index / (sizeof(bbt_t) * 8);
			out[bbtIndex] &= ~(bbt_t(1) << (index % (sizeof(bbt_t) * 8)));
		}

		/**
		 * Gets a single bit from the _bitbase
		 */
		bool getBit(const vector<bbt_t>& out, uint64_t index) const {
			uint64_t bbtIndex = index / (sizeof(bbt_t) * 8);
			return (out[bbtIndex] & (bbt_t(1) << (index % (sizeof(bbt_t) * 8)))) != 0;
		}

		/**
		 * Sets a value to the output vector
		 * @param out vector to set the value to
		 * @param indexInBit index where to set the value to
		 * @param value value to set
		 */
		void setValue(vector<bbt_t>& out, uint64_t indexInBit, bbt_t value) {
			constexpr auto sizeInBit = sizeof(bbt_t) * 8;
			auto bitsLeft = sizeInBit - (indexInBit % sizeInBit);
			auto index = indexInBit / sizeInBit;
			out[index] |= value >> (sizeInBit - bitsLeft);
			index++;
			out[index] |= value << bitsLeft;
		}

		/**
		 * Sets a value to the output vector
		 * @param out vector to set the value to
		 * @param indexInBit index where to set the value to
		 * @param value value to set
		 */
		void setValue(vector<bbt_t>& out, uint64_t indexInBit, uint32_t value) {
			constexpr auto sizeInBit = sizeof(bbt_t) * 8;
			bbt_t* valuePtr = (bbt_t*)&value;
			int count = sizeof(uint32_t) / sizeof(bbt_t);
			for (; count > 0; count--, indexInBit += sizeInBit, valuePtr++) {
				setValue(out, indexInBit, *valuePtr);
			}
		}

		/**
		 * Sets a value to the input vector
		 * @param in vector to get the value from
		 * @param indexInBit index where to get the value from
		 * @returns value read
		 */
		bbt_t getValue(const vector<bbt_t>& in, uint64_t indexInBit) const {
			constexpr auto sizeInBit = sizeof(bbt_t) * 8;
			auto bitsLeft = sizeInBit - (indexInBit % sizeInBit);
			auto index = indexInBit / sizeInBit;
			bbt_t result = 0;
			result |= in[index] << (sizeInBit - bitsLeft);
			index++;
			result |= in[index] >> bitsLeft;
			return result;
		}

		/** 
		 * Uncompresses the next element from the compressed data
		 */
		uint8_t uncompressNext(vector<bbt_t> compressed, uint64_t bitIndex) {
			size_t elementSizeInBit = sizeof(bbt_t) * 8;
			size_t vectorIndex = bitIndex / elementSizeInBit;
			size_t elementIndex = elementSizeInBit - (bitIndex % elementSizeInBit);
			uint8_t result = 0;
			HuffmanNode* node = forrest.top();

			while (!node->isLeaf()) {
				bbt_t bit = compressed[vectorIndex] & (1L << elementIndex);
				node = (bit != 0) ? node->left : node->right;
				if (elementIndex == 0) {
					elementIndex = elementSizeInBit;
					++vectorIndex;
				}
				--elementIndex;
			}

			return node->leafData;
		}

		/**
		 * Compares two huffman nodes accoring its frequency
		 */
		struct compare {
			bool operator()(const HuffmanNode* left, const HuffmanNode* right) const {
				return (left->frequency > right->frequency);
			}
		};

		/**
		 * One step optimization, take the two trees with the highest 
		 */
		void optimize() {
			HuffmanNode* first = forrest.top();
			forrest.pop();
			HuffmanNode* second = forrest.top();
			forrest.pop();
			forrest.push(new HuffmanNode(second, first));
		}

		/**
		 * prints the huffman forrest
		 */
		void print() {
			while (!forrest.empty()) {
				forrest.top()->print();
				forrest.pop();
			}
		}

		/**
		 * Clears the huffman forrest
		 */
		void clear() {
			codeLengthMap.fill(0);
			codeMap.fill(0);
			frequency.fill(0);
		}


		/**
		 * Counts the number of equal 8 bit elements in the data
		 */
		void count(const vector<bbt_t>& data) {
			count((uint8_t*)&data[0], sizeof(bbt_t) * data.size());
		}

		/**
		 * Counts the number of equal 8 bit elements in the data
		 */
		void count(const uint8_t* data, size_t size) {
			for (size_t index = 0; index < size; index++) {
				frequency[data[index]]++;
			}
		}

		/**
		 * Build a forrest of one node trees for each value in the data
		 */
		void builtInitialForrest() {
			for (uint32_t index = 0; index < WORD_SIZE; index++) {
				if (frequency[index] != 0) {
					forrest.push(new HuffmanNode(frequency[index], index));
				}
			}
		}

		static const uint32_t WORD_SIZE = 256;
		array<uint32_t, WORD_SIZE> frequency;
		array<uint32_t, WORD_SIZE> codeLengthMap;
		array<uint64_t, WORD_SIZE> codeMap;
		priority_queue<HuffmanNode*, std::vector<HuffmanNode*>, compare> forrest;
	};
}

#endif		// __HUFFMANCODE_H