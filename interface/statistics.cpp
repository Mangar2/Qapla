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


#include "statistics.h"
#include <vector>
#include <algorithm>

using namespace std;

using namespace QaplaInterface;

Statistics::Statistics() {
	_mode = Mode::WAIT;
	_computerIsWhite = false;
	_xBoardMode = false;
};

void Statistics::handleWhatIf(std::string whatif) {
	IWhatIf* whatIf = getBoard()->getWhatIf();
	whatIf->clear();
	std::vector<std::string> tokens;
	std::string token;
	for (size_t i = 0; i < whatif.size(); ++i) {
		if (whatif[i] == ' ') {
			if (!token.empty()) {
				tokens.push_back(token);
				token.clear();
			}
		}
		else {
			token += whatif[i];
		}
	}
	if (!token.empty()) tokens.push_back(token);
	
	for (int32_t ply = 0; getNextTokenNonBlocking() != ""; ply++) {
		if (getCurrentToken() == "null") {
			whatIf->setNullmove(ply);
		}
		else {
			MoveScanner moveScanner(getCurrentToken());
			if (moveScanner.isLegal()) {
				whatIf->setMove(ply, moveScanner.piece,
					moveScanner.departureFile, moveScanner.departureRank,
					moveScanner.destinationFile, moveScanner.destinationRank, moveScanner.promote);
			}
		}
	}
	whatIf->clear();
}

void Statistics::setBoard() {
	string fen = getToEOLBlocking();
	bool success = setPositionByFen(fen);
	if (!success) {
		printf("Error (illegal fen): %s \n", fen.c_str());
		setPositionByFen();
	}
}

void Statistics::readLevelCommand() {
	uint8_t infoPos = 0;
	uint64_t curValue;
	uint64_t timeToThinkInSeconds = 0;

	while (getNextTokenNonBlocking(":") != "" && infoPos <= 4) {
		curValue = getCurrentTokenAsUnsignedInt();
		switch (infoPos) {
		case 0: _clock.setMoveAmountForClock(int32_t(curValue)); break;
		case 1: timeToThinkInSeconds = curValue * 60; break;
		case 2:
			if (getCurrentToken()[0] != ':') {
				_clock.setTimeIncrementPerMoveInMilliseconds(curValue * 1000);
				infoPos = 4;
			}
			break;
		case 3: timeToThinkInSeconds += curValue; break;
		case 4: _clock.setTimeIncrementPerMoveInMilliseconds(curValue * 1000); break;
		}
		infoPos++;
	}
	_clock.setTimeToThinkForAllMovesInMilliseconds(timeToThinkInSeconds * 1000);

}

bool Statistics::checkClockCommands() {
	bool commandProcessed = true;
	string token = getCurrentToken();
	if (token == "sd") {
		if (getNextTokenNonBlocking() != "") {
			_clock.setSearchDepthLimit((uint32_t)getCurrentTokenAsUnsignedInt());
		}
	}
	else if (token == "time") {
		if (getNextTokenNonBlocking() != "") {
			_clock.setComputerClockInMilliseconds(getCurrentTokenAsUnsignedInt() * 10);
		}
	}
	else if (token == "otim") {
		if (getNextTokenNonBlocking() != "") {
			_clock.setUserClockInMilliseconds(getCurrentTokenAsUnsignedInt() * 10);
		}
	}
	else if (token == "level")
	{
		readLevelCommand();
	}
	else if (token == "st") {
		if (getNextTokenNonBlocking() != "") {
			_clock.setExactTimePerMoveInMilliseconds(getCurrentTokenAsUnsignedInt() * 1000ULL);
		}
	}
	else {
		commandProcessed = false;
	}
	return commandProcessed;
}

/**
 * Processes any input from stdio
 */
void Statistics::runLoop() {
	_mode = Mode::WAIT;
	string token = "";
	getBoard()->initialize();
	while (token != "quit" && _mode != Mode::QUIT) {
		switch (_mode) {
		case Mode::COMPUTE: handleInputWhileComputingMove(); break;
		default: 
		{
				waitForComputingThreadToEnd();
				handleInput();
		}
		}
		token = getNextTokenBlocking();
	}
	stopCompute();
	waitForComputingThreadToEnd();
}

/**
 * Processes input while computing a move
 */
void Statistics::handleInputWhileComputingMove() {
	const string token = getCurrentToken();
	if (token == "?") stopCompute();
	else if (token == ".") getBoard()->requestPrintSearchInfo();
	else println("Error (command not supported in computing mode): " + token);
}

void Statistics::handleInput() {
	const string token = getCurrentToken();
	if (token == "new") setPositionByFen();
	else if (token == "setboard") setBoard();
	else if (token == "eval") getBoard()->printEvalInfo();
	else if (token == "wmtest") WMTest();
	else if (token == "cores") readCores();
	else if (token == "memory") readMemory();
	else if (token == "epd") loadEPD();
	else if (checkClockCommands()) {}
}
