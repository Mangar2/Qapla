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


#include "winboard.h"
#include "winboardprintsearchinfo.h"
#include <thread>

using namespace std;

using namespace QaplaInterface;

bool Winboard::handleMove(string move) {
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

void Winboard::printGameResult(GameResult result) {
	switch (result) {
	case GameResult::DRAW_BY_REPETITION: println("1/2-1/2 {Draw by repetition}"); break;
	case GameResult::DRAW_BY_50_MOVES_RULE: println("1/2-1/2 {Draw by 50 moves rule}"); break;
	case GameResult::DRAW_BY_STALEMATE: println("1/2-1/2 {Stalemate}"); break;
	case GameResult::DRAW_BY_NOT_ENOUGHT_MATERIAL: println("1/2-1/2 {Not enough material to win}"); break;
	case GameResult::BLACK_WINS_BY_MATE: println("0-1 {Black mates}"); break;
	case GameResult::WHITE_WINS_BY_MATE: println("1-0 {White mates}"); break;
	case GameResult::ILLEGAL_MOVE: /* nothing to print on winboard*/ break;
	case GameResult::NOT_ENDED: break; // do nothing
	}
}

Winboard::Winboard() : _sendSearchInfo(0) {
	_mode = Mode::WAIT;
	_forceMode = false;
	_computerIsWhite = false;
	_protoVer = 1;
	_xBoardMode = false;
	_editModeIsWhiteColor = true;
	_easy = true;
};

void Winboard::handleProtover() {
	if (getNextTokenNonBlocking() != "") {
		_protoVer = uint8_t(getCurrentTokenAsUnsignedInt());
		
		if (_protoVer > 1) {
			println("feature done=0");
			println("feature colors=0 ping=1 setboard=1 time=1 reuse=1 analyze=1 usermove=1");
			println("feature myname=\"" + getBoard()->getEngineInfo()["name"] + " by " + getBoard()->getEngineInfo()["author"] + "\"");
			println("feature done=1");
		}
	}
}

void Winboard::handleRemove() {
	if (_computerIsWhite != getBoard()->isWhiteToMove()) {
		getBoard()->undoMove();
		getBoard()->undoMove();
	}
}

void Winboard::runPerft(bool showMoves) {
	StdTimeControl timeControl;
	if (getNextTokenNonBlocking() != "") {
		timeControl.storeStartTime();
		uint64_t res = getBoard()->perft((uint16_t)getCurrentTokenAsUnsignedInt(), showMoves, _maxTheadCount);
		float durationInMs = (float)timeControl.getTimeSpentInMilliseconds();
		std::cout << "nodes: " << res
          << ", time: " << std::fixed << std::setprecision(4) << (durationInMs / 1000.0)
          << "s, nps: " << std::fixed << std::setprecision(0) << (res * 1000.0 / durationInMs)
          << std::endl;
	}
}

void Winboard::analyzeMove() {
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

void Winboard::ponder(string move) {
	if (_easy) return;
	if (!setMove(move)) return;
	_mode = Mode::PONDER;
	_clock.storeCalculationStartTime();
	_clock.setPonderMode();
	setInfiniteSearch(true);
	getBoard()->setClock(_clock);
	getBoard()->computeMove();
	waitIfInfiniteSearchFinishedEarly();
}

/**
 * Starts computing a move - sets analyze mode to false
 */
void Winboard::computeMove() {
	_forceMode = false;
	_computerIsWhite = getBoard()->isWhiteToMove();

	GameResult result = getBoard()->getGameResult();
	if (result != GameResult::NOT_ENDED) {
		// we may not send the game result as it will be again!
		return;
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
			println("move " + computingInfo.currentConsideredMove);
			ponderMove = computingInfo.ponderMove;
			handleMove(computingInfo.currentConsideredMove);
			_clock.storeTimeSpent();
			ponder(ponderMove);
		});
	}
}

void Winboard::WMTest() {
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
			_clock.setAnalyseMode(true);
			_clock.limitSearchDepth(depth);
			getBoard()->computeMove(_clock, false);
			totalNodesSearched += getBoard()->getComputingInfo().nodesSearched;
			totalTimeUsedInMilliseconds += getBoard()->getComputingInfo().elapsedTimeInMilliseconds;
			test.printResult(index, totalTimeUsedInMilliseconds, totalNodesSearched);
		}
	}
	*/
}

/**
 * Answers with a pong to a ping
 */
void Winboard::handlePing() {
	if (getNextTokenNonBlocking() != "") {
		string number = getCurrentToken();
		print("pong ");
		println(number);
	}
}

void Winboard::newGame() {
	getBoard()->newGame();
	setPositionByFen();
}

void Winboard::setBoard() {
	string fen = getToEOLBlocking();
	bool success = setPositionByFen(fen);
	if (!success) {
		printf("Error (illegal fen): %s \n", fen.c_str());
		setPositionByFen();
	}
}

void Winboard::handleWhatIf() {
	IWhatIf* whatIf = getBoard()->getWhatIf();
	whatIf->clear();
	getNextTokenNonBlocking();
	int32_t searchDepth = (int32_t)getCurrentTokenAsUnsignedInt();
	if (searchDepth == 0) { searchDepth = 1; }
	whatIf->setSearchDepht(searchDepth);
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
	ClockSetting whatIfClock;
	whatIfClock.setAnalyseMode();
	whatIfClock.setSearchDepthLimit(searchDepth);
	getBoard()->setClock(whatIfClock);
	getBoard()->computeMove();
	whatIf->clear();
}

void Winboard::readLevelCommand() {
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

bool Winboard::checkClockCommands() {
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
 * Checks the input for a move request, either "usermove" oder just a chess move in
 * chess move notation
 */
bool Winboard::checkMoveCommand() {
	bool moveCommandFound = false;
	if (getCurrentToken() == "usermove") {
		getNextTokenNonBlocking();
		moveCommandFound = true;
	}
	if (handleMove()) {
		moveCommandFound = true;
		if (_mode == Mode::ANALYZE) {
			stopCompute();
			analyzeMove();
		}
		else if (!_forceMode) {
			computeMove();
		}
	}
	return moveCommandFound;
}

void Winboard::undoMove() {
	_forceMode = true;
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

void Winboard::loadEgtb() {
	std::string tk = getNextTokenNonBlocking();
	std::string path = getNextTokenNonBlocking();

	if (tk == "qaplaBitbases") {
		getBoard()->setOption("qaplaBitbasePath", path);
	}
}

/**
 * Processes any input from stdio
 */
void Winboard::runLoop() {
	_mode = Mode::WAIT;
	string token = "";
	getBoard()->initialize();
	setPositionByFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	while (token != "quit" && !isFatalError()) {
		switch (_mode) {
		case Mode::ANALYZE: handleInputWhileInAnalyzeMode(); break;
		case Mode::COMPUTE: handleInputWhileComputingMove(); break;
		case Mode::EDIT: handleInputWhiteInEditMode(); break;
		case Mode::PONDER: handleInputWhileInPonderMode(); break;
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
void Winboard::handleInputWhileComputingMove() {
	const string token = getCurrentToken();
	if (token == "?") stopCompute();
	else if (token == ".") getBoard()->requestPrintSearchInfo();
	else println("Error (command not supported in computing mode): " + token);
}

void Winboard::handleInputWhileInAnalyzeMode() {
	const string token = getCurrentToken();

	if (token == "new" || token == "setboard" || token == "usermove" || token == "undo" || token == "exit") {
		stopCompute();
	}
	if (token == ".") getBoard()->requestPrintSearchInfo();
	else if (token == "ping") handlePing();
	else if (token == "usermove") checkMoveCommand();
	else if (token == "undo") getBoard()->undoMove();
	else if (token == "new") setPositionByFen();
	else if (token == "setboard") setBoard();
	else if (token == "force") { _mode = Mode::WAIT; _forceMode = true; }
	else if (token == "exit") { _mode = Mode::WAIT; }
	else {
		println("Error (command not supported in analyze mode): " + token);
	}
}

void Winboard::handleInputWhiteInEditMode() {
	const string token = getCurrentToken();
	if (token == "#") getBoard()->clearBoard();
	else if (token == "c") _editModeIsWhiteColor = !_editModeIsWhiteColor;
	else if (token == ".") _mode = Mode::WAIT;
	else {
		char piece = getCurrentToken()[0];
		uint32_t col = getCurrentToken()[1] - 'a';
		uint32_t row = getCurrentToken()[2] - '1';
		if (!_editModeIsWhiteColor) {
			piece += 'a' - 'A';
		}
		getBoard()->setPiece(col, row, piece);
	}
}

void Winboard::handleInputWhileInPonderMode() {
	const string token = getCurrentToken();
	if (token == ".") getBoard()->requestPrintSearchInfo();
	if (checkMoveCommand()) {}
	else {
		// We stop pondering for any command but a "." or a move command
		_mode = Mode::WAIT;
		stopCompute();
		// Undoes the move we pondered for
		// Here we would need to implement "real" pondering. 
		// Current challenge -> we need to implement a method to check, if the move of the user is the move we pondered for.
		// As we only deal with strings here and we cannot simply compare move-strings, this is not as simple. 
		getBoard()->undoMove();
		handleInput();
	}
}

void Winboard::handleInput() {
	const string token = getCurrentToken();
	if (token == "analyze") analyzeMove();
	else if (token == "force") _forceMode = true;
	else if (token == "go") computeMove();
	else if (token == "new") newGame();
	else if (token == "setboard") setBoard();
	else if (token == "whatif") handleWhatIf();
	else if (token == "easy") _easy = true;
	else if (token == "eval") getBoard()->printEvalInfo();
	else if (token == "hard") _easy = false;
	else if (token == "post") {}
	else if (token == "random") {}
	else if (token == "accepted")  getNextTokenNonBlocking();
	else if (token == "perft") runPerft(false);
	else if (token == "divide") runPerft(true);
	else if (token == "xboard") handleXBoard();
	else if (token == "protover") handleProtover();
	else if (token == "white") getBoard()->setWhiteToMove(true);
	else if (token == "black") getBoard()->setWhiteToMove(false);
	else if (token == "ping") handlePing();
	else if (token == "edit") { _mode = Mode::EDIT; _editModeIsWhiteColor = true; }
	else if (token == "undo") undoMove();
	else if (token == "remove") handleRemove();
	else if (token == "wmtest") WMTest();
	else if (token == "result") getToEOLBlocking();
	else if (token == "cores") readCores();
	else if (token == "memory") readMemory();
	else if (token == "egtpath") loadEgtb();
	else if (checkClockCommands()) {}
	else if (checkMoveCommand()) {}
}
