#ifndef __BITBASE_H
#define __BITBASE_H

#include "stdafx.h"
#include "Move.h"
#include "BitBoardMasks.h"
#include "GenBoard.h"
#include "MoveList.h"
#include "FileClass.h"

class BitBase {
public:
	BitBase() : bitBasePtr(0), loaded(false) {

	}
	BitBase(uint64_t bitBaseSizeInBit) : sizeInBit(bitBaseSizeInBit), bitBasePtr(0), loaded(true) {
		bitBasePtr = new uint32_t[sizeInBit / BITS_IN_ELEMENT];
		clear();
	};
	~BitBase() { delete[] bitBasePtr; }
	
	void clear() {
		for (uint32_t index = 0; index < sizeInBit / BITS_IN_ELEMENT; index++) {
			bitBasePtr[index] = 0;
		}
	}

	void setBit(uint64_t index) {
		if (index < sizeInBit) {
			bitBasePtr[index / BITS_IN_ELEMENT] |= 1UL << (index % BITS_IN_ELEMENT);
		}
	}

	bool getBit(uint64_t index) const {
		bool result = false;
		if (loaded && index < sizeInBit) {
			result = (bitBasePtr[index / BITS_IN_ELEMENT] & 1UL << (index % BITS_IN_ELEMENT)) != 0;
		}
		return result;
	}

	void storeToFile(char* fileName) {
		FileClass file;
		file.open(fileName, "w+b");
		file.write(&sizeInBit, sizeof(uint64_t), 1);
		uint32_t size = (uint32_t)sizeInBit / BITS_IN_ELEMENT;
		file.write(bitBasePtr, sizeof(uint32_t), (uint32_t)sizeInBit/BITS_IN_ELEMENT);
	}

	void readFromFile(char* fileName) {
		FileClass file;
		file.open(fileName, "rb");
		file.read(&sizeInBit, sizeof(uint64_t), 1);
		bitBasePtr = new uint32_t[sizeInBit / BITS_IN_ELEMENT];
		file.read(bitBasePtr, sizeof(uint32_t), (uint32_t)sizeInBit/BITS_IN_ELEMENT);
		if (file.getLastError() == 0) {
			loaded = true;
			printf("%s file loaded\n", fileName);
		} else {
			printf("%s file not found\n", fileName);
			delete[] bitBasePtr;
			bitBasePtr = 0;
		}
	}

	bool isLoaded() { return loaded; }

private:
	bool loaded;
	static const uint64_t BITS_IN_ELEMENT = sizeof(uint32_t) * 8;
	uint32_t* bitBasePtr;
	uint64_t sizeInBit;
};

#endif // BITBASE_H