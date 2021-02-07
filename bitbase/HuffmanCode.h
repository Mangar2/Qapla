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

namespace ChessBitbase {


	struct HuffmanNode {
		/**
		 * Creates a new leaf node
		 */
		HuffmanNode(uint32_t frequencyOfData, uint32_t data)
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
		bool isLeaf() { return left == NULL; }

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
		HuffmanCode(const vector<uint32_t>& data) {
			clear();
			count(data);
			builtInitialForrest();
			while (forrest.size() > 1) {
				optimize();
			}
			buildCode(forrest.top(), 0, 0);
			cout << "original size: " << data.size() * sizeof(uint32_t)
				<< " compressed size: " << (computeSizeInBit() / 8)
				<< " comressed to: " << (computeSizeInBit() * 100) / (8 * data.size() * sizeof(uint32_t)) << "%"
				<< endl;
			// printCode();
		}

		/**
		 * Use the calculated huffman code to compress the data
		 */
		vector<uint32_t> compress(const vector<uint32_t>& input) {
			vector<uint32_t> compressed;
			uint32_t current = 0;
			uint32_t bitsLeft = sizeof(current) * 8;
			uint64_t byteSize = input.size() * sizeof(uint32_t);
			const uint8_t* data = (uint8_t*) &input[0];
			for (uint64_t i = 0; i < byteSize; ++i) {
				uint8_t code = codeMap[data[i]];
				uint8_t codeLength = codeLengthMap[data[i]];
				if (bitsLeft >= codeLength) {
					current |= code << (bitsLeft - codeLength);
					bitsLeft -= codeLength;
				} 
				else {
					current |= code >> (codeLength - bitsLeft);
					compressed.push_back(current);
					bitsLeft = sizeof(current) * 8 + bitsLeft - codeLength;
					current = code << bitsLeft;
				}
			}
			compressed.push_back(current);
			return compressed;
		}

		/**
		 * Uncompresses code from the 
		 */
		vector<uint32_t> uncompress(const vector<uint32_t>& compressed, size_t targetSizeInBit) {
			vector<uint32_t> uncompressed;
			constexpr size_t elementSizeInBit = sizeof(uint32_t) * 8;
			size_t uncompressedIndex = 0;
			size_t compressedIndex = 0;
			uint32_t uncompressedElement = 0;
			while (uncompressedIndex < targetSizeInBit) {
				const uint32_t nextElement = uncompressNext(compressed, compressedIndex);
				compressedIndex += codeLengthMap[nextElement];
				uncompressedIndex += sizeof(uint8_t);
				const uint32_t bitsLeft = elementSizeInBit - uncompressedIndex % elementSizeInBit;
				uncompressedElement |= nextElement << bitsLeft;
				if (bitsLeft == 0) {
					uncompressed.push_back(uncompressedElement);
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
			for (uint32_t index = 0; index < WORD_SIZE; index++) {
				if (codeLengthMap[index] > 0) {
					printf("%3ld ", index);
					for (int16_t bitAmount = codeLengthMap[index] - 1; bitAmount >= 0; bitAmount--) {
						printf("%d", (codeMap[index] >> bitAmount) & 1);
					}
					printf("\n");
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
		 * Uncompresses the next element from the compressed data
		 */
		uint8_t uncompressNext(vector<uint32_t> compressed, uint64_t bitIndex) {
			size_t elementSizeInBit = sizeof(uint32_t) * 8;
			size_t vectorIndex = bitIndex / elementSizeInBit;
			size_t elementIndex = elementSizeInBit - (bitIndex % elementSizeInBit);
			uint8_t result = 0;
			HuffmanNode* node = forrest.top();

			while (!node->isLeaf()) {
				uint32_t bit = compressed[vectorIndex] & (1L << elementIndex);
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

	private:
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
		void count(const vector<uint32_t>& data) {
			count((uint8_t*)&data[0], sizeof(uint32_t) * data.size());
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
		array<uint32_t, WORD_SIZE> codeMap;
		priority_queue<HuffmanNode*, std::vector<HuffmanNode*>, compare> forrest;
	};
}

#endif		// __HUFFMANCODE_H