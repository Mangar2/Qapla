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
 * @copyright Copyright (c) 2025 Volker Böhm
 */


#include "bitbase-interface.h"
#include "../bitbase/syzygy-export.h"

using namespace std;

using namespace QaplaInterface;

void BitbaseInterface::generateBitbases() {
	string piecesString = getNextTokenBlocking(true);
	if (piecesString == "\r" || piecesString == "\n") {
		println("usage bitgenerate pieces [cores n] [path p] [syzygy dir] [cpp] [trace n] [debug n] [index n]");
		return;
	}
	string token = getNextTokenBlocking(true);
	uint32_t cores = 1;
	uint32_t traceLevel = 1;
	uint32_t debugLevel = 0;
	uint64_t debugIndex = -1;
	std::string syzygyPath = ".";
	bool generateCpp = false;
	while (token != "\n" && token != "\r") {
		if (token == "cores") {
			getNextTokenBlocking(true);
			cores = uint32_t(getCurrentTokenAsUnsignedInt());
		}
		else if (token == "syzygy") {
			syzygyPath = getNextTokenBlocking(true);
		}
		else if (token == "path") {
			getBoard()->setOption("qaplaBitbasePathNL", getNextTokenBlocking(true));
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
	getBoard()->generateBitbases(piecesString, cores, generateCpp, traceLevel, debugLevel, debugIndex,
		syzygyPath);
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
void BitbaseInterface::writeSyzygy() {
	const string pieceString = getNextTokenBlocking(true);
	if (pieceString == "\r" || pieceString == "\n") {
		println("usage bitsyzygy pieces [qwdl file] [out dir]");
		return;
	}
	string qwdlFile = pieceString + ".qwdl";
	string outDir = ".";
	string token = getNextTokenBlocking(true);
	while (token != "\n" && token != "\r") {
		if (token == "qwdl") qwdlFile = getNextTokenBlocking(true);
		else if (token == "out") outDir = getNextTokenBlocking(true);
		else break;
		token = getNextTokenBlocking(true);
	}
	QaplaBitbase::writeSyzygyWdl(pieceString, qwdlFile, outDir, std::cout);
}

void BitbaseInterface::checkGeneratorIndex() {
	const string pieceString = getNextTokenBlocking(true);
	if (pieceString == "\r" || pieceString == "\n") {
		println("usage bitgenindex pieces");
		return;
	}
	const bool ok = QaplaBitbase::checkGeneratorIndex(pieceString, std::cout);
	println(ok ? "index ok" : "INDEX BROKEN");
}

void BitbaseInterface::checkSyzygy() {
	const string pieceString = getNextTokenBlocking(true);
	if (pieceString == "\r" || pieceString == "\n") {
		println("usage bitsyzygycheck pieces ourDir referenceDir [qwdl file]");
		return;
	}
	const string ourDir = getNextTokenBlocking(true);
	const string referenceDir = getNextTokenBlocking(true);
	string qwdlFile;
	const string token = getNextTokenBlocking(true);
	if (token == "qwdl") qwdlFile = getNextTokenBlocking(true);
	const bool equal = QaplaBitbase::compareSyzygyWdl(pieceString, ourDir, referenceDir,
		qwdlFile, std::cout);
	println(equal ? "identical" : "DIFFERENT");
}

void BitbaseInterface::speedSyzygy() {
	const string pieceString = getNextTokenBlocking(true);
	if (pieceString == "\r" || pieceString == "\n") {
		println("usage bitsyzygyspeed pieces ourDir referenceDir [positions n]");
		return;
	}
	const string ourDir = getNextTokenBlocking(true);
	const string referenceDir = getNextTokenBlocking(true);
	uint64_t amount = 1000000;
	const string token = getNextTokenBlocking(true);
	if (token == "positions") {
		getNextTokenBlocking(true);
		amount = getCurrentTokenAsUnsignedInt();
	}
	const bool fastEnough = QaplaBitbase::measureSyzygySpeed(pieceString, ourDir, referenceDir,
		amount, std::cout);
	println(fastEnough ? "not slower" : "SLOWER");
}

void BitbaseInterface::probeSyzygy() {
	const string board = getNextTokenBlocking(true);
	if (board == "\r" || board == "\n") {
		println("usage bitsyzygyprobe board w|b directory [qwdl file]");
		return;
	}
	const bool whiteToMove = getNextTokenBlocking(true) != "b";
	const string directory = getNextTokenBlocking(true);
	string qwdlFile;
	if (getNextTokenBlocking(true) == "qwdl") qwdlFile = getNextTokenBlocking(true);
	QaplaBitbase::probeSyzygyPosition(board, whiteToMove, directory, qwdlFile, std::cout);
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
	else if (token == "bitsyzygy") writeSyzygy();
	else if (token == "bitsyzygycheck") checkSyzygy();
	else if (token == "bitgenindex") checkGeneratorIndex();
	else if (token == "bitsyzygyspeed") speedSyzygy();
	else if (token == "bitsyzygyprobe") probeSyzygy();
}
