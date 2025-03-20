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


#include "statistics.h"
#include "../eval/eval.h"
#include "winboardprintsearchinfo.h"
#include <thread>
#include <vector>

using namespace std;

using namespace QaplaInterface;


bool Statistics::handleMove(string move) {
	move = move == "" ? getCurrentToken() : move;
	bool legalMove = false;
	if (!setMove(move)) {
		println("Illegal move: " + move);
	}
	else {
		printGameResult(getBoard()->getGameResult());
		legalMove = true;
	}
	return legalMove;
}

void Statistics::printGameResult(GameResult result) {
	switch (result) {
	case GameResult::DRAW_BY_REPETITION: println("1/2-1/2 {Draw by repetition}"); break;
	case GameResult::DRAW_BY_50_MOVES_RULE: println("1/2-1/2 {Draw by 50 moves rule}"); break;
	case GameResult::DRAW_BY_STALEMATE: println("1/2-1/2 {Stalemate}"); break;
	case GameResult::DRAW_BY_NOT_ENOUGHT_MATERIAL: println("1/2-1/2 {Not enough material to win}"); break;
	case GameResult::BLACK_WINS_BY_MATE: println("0-1 {Black mates}"); break;
	case GameResult::WHITE_WINS_BY_MATE: println("1-0 {White mates}"); break;
	case GameResult::NOT_ENDED: break; // do nothing
	}
}

Statistics::Statistics() : _sendSearchInfo(0) {
	_mode = Mode::WAIT;
	_computerIsWhite = false;
	_xBoardMode = false;
};

void Statistics::generateEGTB() {
	string piecesString = getNextTokenBlocking(true);
	if (piecesString == "\r" || piecesString == "\n") {
		println("usage generate pieces [cores n] [uncompressed] [trace n] [debug n] [index n]");
		return;
	}
	string token = getNextTokenBlocking(true);
	uint32_t cores = 16;
	uint32_t traceLevel = 1;
	uint32_t debugLevel = 0;
	uint64_t debugIndex = -1;
	bool uncompressed = false;
	while (token != "\n" && token != "\r") {
		if (token == "cores") {
			getNextTokenBlocking(true);
			cores = uint32_t(getCurrentTokenAsUnsignedInt());
		}
		else if (token == "uncompressed" || token == "uc") {
			uncompressed = true;
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
		else {
			break;
		}
		token = getNextTokenBlocking(true);
	}
	getBoard()->generateBitbases(piecesString, cores, uncompressed, traceLevel, debugLevel, debugIndex);
}

void Statistics::verifyEGTB() {
	string piecesString = getNextTokenBlocking(true);
	if (piecesString == "\r" || piecesString == "\n") {
		println("usage verify pieces [cores n] [trace n] [debug n]");
		return;
	}
	string token = getNextTokenBlocking(true);
	uint32_t cores = 16;
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

void Statistics::handleRemove() {
	if (_computerIsWhite != getBoard()->isWhiteToMove()) {
		getBoard()->undoMove();
		getBoard()->undoMove();
	}
}

void Statistics::analyzeMove() {
	_mode = Mode::ANALYZE;
	GameResult result = getBoard()->getGameResult();
	if (result != GameResult::NOT_ENDED) {
		printGameResult(result);
	}
	else {
		_clock.setAnalyseMode();
		setInfiniteSearch(true);
		getBoard()->setClock(_clock);
		getWorkerThread().startTask([this]() {
			getBoard()->computeMove();
			waitIfInfiniteSearchFinishedEarly();
		});
	}
}

/**
 * Starts computing a move - sets analyze mode to false
 */
void Statistics::computeMove() {
	_computerIsWhite = getBoard()->isWhiteToMove();

	GameResult result = getBoard()->getGameResult();
	if (result != GameResult::NOT_ENDED) {
		printGameResult(result);
	}
	else {
		_mode = Mode::COMPUTE;
		_clock.storeCalculationStartTime();
		_clock.setSearchMode();
		setInfiniteSearch(false);
		getBoard()->setClock(_clock);
		getWorkerThread().startTask([this]() {
			getBoard()->computeMove();
			_mode = Mode::WAIT;
			ComputingInfoExchange computingInfo = getBoard()->getComputingInfo();
			handleMove(computingInfo.currentConsideredMove);
			_clock.storeTimeSpent();
		});
	}
}

void Statistics::WMTest() {
	/*
	EPDTest test;
	test.initGames();
	FenScanner scanner;
	uint64_t totalNodesSearched = 0;
	uint64_t totalTimeUsedInMilliseconds = 0;
	if (getNextTokenNonBlocking()) {
		uint32_t depth = (uint32_t)getCurrentTokenAsUnsignedInt();
		for (uint32_t index = 0; index < 100; index++) {
			scanner.setBoard(test.Game1[index], getBoard());
			_clock.setAnalyseMode();
			_clock.setSearchDepthLimit(depth);
			getBoard()->computeMove(false);
			totalNodesSearched += getBoard()->getComputingInfo().nodesSearched;
			totalTimeUsedInMilliseconds += getBoard()->getComputingInfo().elapsedTimeInMilliseconds;
			test.printResult(index, totalTimeUsedInMilliseconds, totalNodesSearched);
		}
	}
	*/
}
void Statistics::loadGamesFromFile(const std::string& filename) {
	std::ifstream file(filename);
	if (!file) {
		std::cerr << "Error: Could not open file " << filename << "\n";
		return;
	}
	_games.clear();

	std::string line;
	uint32_t count = 0;
	while (std::getline(file, line)) {
		std::istringstream ss(line);
		std::string fen, move;
		int eval;

		if (!std::getline(ss, fen, ',')) {
			continue;
		}

		ChessGame game(fen);
		while (std::getline(ss, move, ',')) {
			if (!(ss >> eval)) {
				break;
			}
			ss.ignore(); // Ignore comma separator
			game.addMove(move, eval);
		}
		count++;
		if (count % 1000 == 0) {  // Update nur alle 1000 Zeilen
			std::cout << "\rGames loaded: " << count << std::flush;
		}
		_games.push_back(game);
		break;
	}
	std::cout << "\rGames loaded: " << count << std::endl;
}

void Statistics::train() {
	loadGamesFromFile("games.txt");
	uint32_t gameIndex = 0;
	for (auto& game : _games) {
		gameIndex++;
		setPositionByFen(game.fen);
		for (auto& movePair : game.moves) {
			
			auto indexVector = getBoard()->computeEvalIndexVector();
			auto lookupMap = getBoard()->computeEvalIndexLookupMap();
			std::map<std::string, EvalValue> aggregatedValues;

			EvalValue evalCalculated = 0;
			int32_t midgame = indexVector[0].index;
			int32_t midgameV2 = indexVector[1].index;
			assert(indexVector[0].name == "midgame" && indexVector[1].name == "midgamev2");
			for (auto indexInfo : indexVector) {
				if (indexInfo.name == "midgame" || indexInfo.name == "midgamev2") {
					continue;
				}
				auto lookupTable = lookupMap[indexInfo.name];
				auto value = lookupMap[indexInfo.name][indexInfo.index];
				auto valueColor = indexInfo.color == WHITE ? value : -value;
				evalCalculated += valueColor;
				cout << "sum: " << evalCalculated << " " << indexInfo.name << " value: " << value << " index: " << indexInfo.index << " color: " << (indexInfo.color == WHITE ? "white": "black") << endl;
				std::string modifiedName = indexInfo.name.substr(1);
				aggregatedValues[modifiedName] += valueColor;
				// aggregatedValues[indexInfo.name] += valueColor;
			}
			int32_t eval = getBoard()->eval();
			// Eval is from side to move perspective, we need the eval always from white perspective
			if (!getBoard()->isWhiteToMove()) {
				eval = -eval;
			}
			int32_t evalC = evalCalculated.getValue(midgameV2);
			if (std::abs(eval - evalC) > 1) {
				getBoard()->printEvalInfo();
				std::cout << "Error: Eval not correct: " << eval << " != " << evalCalculated << std::endl;
			}
			std::string move = movePair.first;
			int32_t positionValue = movePair.second;
			
			if (!handleMove(move)) {
				break;
			}
		}
		// print the number of game and the total games as releation to cout e.g. 5/123
		std::cout << "Game " <<  gameIndex << "/" << _games.size() << std::endl;
	}
}

void Statistics::playEpdGames(uint32_t numThreads) {
	if (getNextTokenNonBlocking() != "") {
		if (getCurrentToken() == "threads") {
			if (getNextTokenNonBlocking() != "") {
				numThreads = (uint32_t)getCurrentTokenAsUnsignedInt();
			}
		}
	}
	epdTasks.start(numThreads, _clock, _startPositions, getBoard());
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


void Statistics::undoMove() {
	if (_mode == Mode::ANALYZE) {
		stopCompute();
		analyzeMove();
	}
	else if (_mode == Mode::COMPUTE) {
		stopCompute();
		// undoes the move that is automatically set due to stopCompute resulting in a move
		getBoard()->undoMove();
		// undoes the move before -> this is the move the user asked us to undo
		getBoard()->undoMove();
	}
	else {
		waitForComputingThreadToEnd();
		getBoard()->undoMove();
	}
}

void Statistics::loadEPD() {
	std::string fileName = getNextTokenNonBlocking();
	std::ifstream file(fileName);

	if (!file) {
		std::cerr << "Error: Could not open file " << fileName << std::endl;
		return;
	}

	_startPositions.clear();
	std::string line;
	while (std::getline(file, line)) {
		if (!line.empty()) {
			_startPositions.push_back(line);
		}
	}

	file.close();
	std::cout << "Loaded " << _startPositions.size() << " positions from EPD file." << std::endl;
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
	epdTasks.stop();
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
	if (token == "analyze") analyzeMove();
	else if (token == "new") setPositionByFen();
	else if (token == "setboard") setBoard();
	else if (token == "eval") getBoard()->printEvalInfo();
	else if (token == "wmtest") WMTest();
	else if (token == "cores") readCores();
	else if (token == "memory") readMemory();
	else if (token == "generate") generateEGTB();
	else if (token == "verify") verifyEGTB();
	else if (token == "playepd") playEpdGames();
	else if (token == "train") train();
	else if (token == "epd") loadEPD();
	else if (checkClockCommands()) {}
}
