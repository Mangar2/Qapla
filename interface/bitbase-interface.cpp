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


#include "bitbase-interface.h"
#include "../eval/eval.h"
#include "winboardprintsearchinfo.h"
#include <thread>
#include <vector>
#include <algorithm>

using namespace std;

using namespace QaplaInterface;

void BitbaseInterface::generateBitbases() {
	string piecesString = getNextTokenBlocking(true);
	if (piecesString == "\r" || piecesString == "\n") {
		println("usage bitgenerate pieces [cores n] [path p] [compression c] [cpp] [trace n] [debug n] [index n]");
		return;
	}
	string token = getNextTokenBlocking(true);
	uint32_t cores = 1;
	uint32_t traceLevel = 1;
	uint32_t debugLevel = 0;
	uint64_t debugIndex = -1;
	std::string compression = "miniz";
	bool generateCpp = false;
	while (token != "\n" && token != "\r") {
		if (token == "cores") {
			getNextTokenBlocking(true);
			cores = uint32_t(getCurrentTokenAsUnsignedInt());
		}
		else if (token == "path") {
			getBoard()->setOption("qaplaBitbasePathNL", getNextTokenBlocking(true));
		}
		else if (token == "compression" || token == "comp") {
			compression = getNextTokenBlocking(true);
		}
		else if (token == "trace") {
			getNextTokenBlocking(true);
			traceLevel = uint32_t(getCurrentTokenAsUnsignedInt());
		}
		else if (token == "debug") {
			getNextTokenBlocking(true);
			debugLevel = uint32_t(getCurrentTokenAsUnsignedInt());
		}
		else if (token == "index") {
			getNextTokenBlocking(true);
			debugIndex = getCurrentTokenAsUnsignedInt();
		}
		else if (token == "cpp") {
			generateCpp = true;
		}
		else {
			break;
		}
		token = getNextTokenBlocking(true);
	}
	getBoard()->generateBitbases(piecesString, cores, compression, generateCpp, traceLevel, debugLevel, debugIndex);
}

void BitbaseInterface::verifyBitbases() {
	string piecesString = getNextTokenBlocking(true);
	if (piecesString == "\r" || piecesString == "\n") {
		println("usage bitverify pieces [cores n] [trace n] [debug n]");
		return;
	}
	string token = getNextTokenBlocking(true);
	uint32_t cores = 1;
	uint32_t traceLevel = 1;
	uint32_t debugLevel = 0;
	while (token != "\n" && token != "\r") {
		if (token == "cores") {
			getNextTokenBlocking(true);
			cores = uint32_t(getCurrentTokenAsUnsignedInt());
		}
		else if (token == "trace") {
			getNextTokenBlocking(true);
			traceLevel = uint32_t(getCurrentTokenAsUnsignedInt());
		}
		else if (token == "debug") {
			getNextTokenBlocking(true);
			debugLevel = uint32_t(getCurrentTokenAsUnsignedInt());
		}
		else {
			break;
		}
		token = getNextTokenBlocking(true);
	}
	getBoard()->verifyBitbases(piecesString, cores, traceLevel, debugLevel);
}
/*
 * Processes any input from stdio
 */
void BitbaseInterface::runLoop() {
	_mode = Mode::WAIT;
	string token = "";
	getBoard()->initialize();
	while (token != "quit" && _mode != Mode::QUIT) {
		handleInput();
		token = getNextTokenBlocking();
	}
}

/**
 * Processes input while computing a move
 */
void BitbaseInterface::handleInputWhileGenerating() {
	const string token = getCurrentToken();
	if (token == "?") stopCompute();
	else println("Error (command not supported in computing mode): " + token);
}

void BitbaseInterface::handleInput() {
	const string token = getCurrentToken();
	if (token == "bitgenerate") generateBitbases();
	else if (token == "bitverify") verifyBitbases();
}
