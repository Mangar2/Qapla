#pragma once

#include "BitBase.h"

class BitBaseReader
{
public:
	static void loadBitBase() {
		kpk.readFromFile("KPK.btb");
	}

	static BitBase kpk;

private:
	BitBaseReader() {};
};

