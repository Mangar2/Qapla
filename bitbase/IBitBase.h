#pragma once
#include "BitBaseIndex.h"
#include "FileClass.h"
#include <string>
using namespace std;

class IBitBase {
public:
	virtual void clear() = 0;

	virtual void setBit(uint64_t index) = 0;
	virtual void setBit(BitBaseIndex& index) = 0;

	virtual bool getBit(uint64_t index) const = 0;
	virtual bool getBit(BitBaseIndex& index) = 0;

	virtual void storeToFile(string fileName) = 0;

	virtual void readFromFile(FileClass& file) = 0;
	virtual void readFromFile(string fileName) = 0;

};