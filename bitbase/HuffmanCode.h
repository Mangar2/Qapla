#pragma once

#include "stdafx.h"
#include <queue>

struct HuffmanNode {
	HuffmanNode(uint32_t frequencyOfData, uint32_t data) 
		: frequency(frequencyOfData), leafData(data), left(NULL), right(NULL)
	{}
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
		} else {
			printf("%5d: node\n", frequency);
		}
	}

	bool isLeaf() { return left == NULL; }

	uint32_t frequency;
	HuffmanNode* left;
	HuffmanNode* right; 
	uint8_t leafData;
};

class HuffmanCode {
public:
	HuffmanCode(uint32_t* data, uint32_t size) {
		clear();
		count(data, size);
		buildLeafNodes();
		while (forrest.size() > 1) {
			optimize();
		}
		buildCode(forrest.top(), 0, 0);
		printf("Size in bit : %lld\n", computeSizeInBit());
		printCode();
	}

	void buildLeafNodes() {
		for (uint32_t index = 0; index < WORD_SIZE; index++) {
			if (frequency[index] != 0) {
				forrest.push(new HuffmanNode(frequency[index], index));
			}
		}
	}

	void optimize() {
		HuffmanNode* first = forrest.top();
		forrest.pop();
		HuffmanNode* second = forrest.top();
		forrest.pop();
		forrest.push(new HuffmanNode(second, first));
	}

	void buildCode(HuffmanNode* node, uint8_t codeLength, uint32_t code) {
		if (node->isLeaf()) {
			node->print();
			codeMap[node->leafData] = code;
			codeLengthMap[node->leafData] = codeLength;
		} else {
			buildCode(node->left, codeLength + 1, code * 2 + 0);
			buildCode(node->right, codeLength + 1, code * 2 + 1);
		}
	}

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

	uint64_t computeSizeInBit() {
		uint64_t sizeInBit = 0;
		for (uint32_t index = 0; index < WORD_SIZE; index++) {
			sizeInBit += codeLengthMap[index] * frequency[index] * 1ULL;
		}
		return sizeInBit;
	}

private:
	struct compare  {
		 bool operator()(const HuffmanNode* left, const HuffmanNode* right) const { 
			return (left->frequency > right->frequency); 
		}
	};

	void clear() {
		for (uint32_t index = 0; index < WORD_SIZE; index++) {
			codeLengthMap[index] = 0;
			codeMap[index] = 0;
			frequency[index] = 0;
		}
	}
	
	void count(uint8_t* data, uint32_t size) {
		for (uint32_t index = 0; index < size; index++) {
			frequency[data[index]]++; 
		}
	}
	void count(uint32_t* data, uint32_t size) {
		count((uint8_t*)data, sizeof(uint32_t) * size);
	}

	void print() {
		while(!forrest.empty()) {
			forrest.top()->print();
			forrest.pop();
		}
	}

private:
	static const uint32_t WORD_SIZE = 256;
	uint32_t frequency[WORD_SIZE];
	uint32_t codeLengthMap[WORD_SIZE];
	uint32_t codeMap[WORD_SIZE];
	std::priority_queue<HuffmanNode*, std::vector<HuffmanNode*>, compare> forrest;
};