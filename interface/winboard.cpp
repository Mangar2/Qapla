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
 */


#include "winboard.h"
#include "winboardprintsearchinfo.h"
#include <thread>

using namespace std;

using namespace ChessInterface;

/**
 * Starts computing a move
 */
static void threadComputeMove(
	IChessBoard* board,
	IInputOutput* ioHandler,
	ClockSetting* clock,
	volatile bool* computingMove)
{

	board->computeMove(*clock);
	if (!clock->getAnalyseMode()) {
		ComputingInfoExchange computingInfo = board->getComputingInfo();
		ioHandler->print("move ");
		ioHandler->println(computingInfo.currentConsideredMove);
		HandleMove::handleMove(board, ioHandler, computingInfo.currentConsideredMove);
		clock->storeTimeSpent();
	}

	*computingMove = false;
}

bool HandleMove::handleMove(IChessBoard* board, IInputOutput* ioHandler, const string move) {
	bool legalMove;
	if (move == "") {
		return false;
	}
	if (!scanMove(board, move)) {
		ioHandler->print("Illegal move: ");
		ioHandler->println(ioHandler->getCurrentToken());
		legalMove = false;
	}
	else {
		printGameResult(board->getGameResult(), ioHandler);
		legalMove = true;
	}
	return legalMove;
}

void HandleMove::printGameResult(GameResult result, IInputOutput* ioHandler) {
	switch (result) {
	case GameResult::DRAW_BY_REPETITION: ioHandler->println("1/2-1/2 {Draw by repetition}"); break;
	case GameResult::DRAW_BY_50_MOVES_RULE: ioHandler->println("1/2-1/2 {Draw by 50 moves rule}"); break;
	case GameResult::DRAW_BY_STALEMATE: ioHandler->println("1/2-1/2 {Stalemate}"); break;
	case GameResult::DRAW_BY_NOT_ENOUGHT_MATERIAL: ioHandler->println("1/2-1/2 {Not enough material to win}"); break;
	case GameResult::BLACK_WINS_BY_MATE: ioHandler->println("0-1 {Black mates}"); break;
	case GameResult::WHITE_WINS_BY_MATE: ioHandler->println("1-0 {White mates}"); break;
	}
}

bool HandleMove::scanMove(IChessBoard* board, const string move) {
	MoveScanner scanner(move);
	bool res = false;
	if (scanner.isLegal()) {
		res = board->doMove(
			scanner.piece,
			scanner.departureFile, scanner.departureRank,
			scanner.destinationFile, scanner.destinationRank,
			scanner.promote);
	}
	return res;
}

Winboard::Winboard() 
	: quit(false) {
	analyzeMode = false;
	computingMove = false;
	editMode = false;
	forceMode = false;
	computerIsWhite = false;
	protoVer = 1;
	xBoardMode = false;
	editModeIsWhiteColor = true;
};

void Winboard::handleProtover(IInputOutput* ioHandler) {
	if (ioHandler->getNextTokenNonBlocking() != "") {
		protoVer = uint8_t(ioHandler->getCurrentTokenAsUnsignedInt());
		if (protoVer > 1) {
			ioHandler->println("feature done=0");
			ioHandler->println("feature colors=0 ping=1 setboard=1 time=1 reuse=1 analyze=1 usermove=1");
			ioHandler->println("feature myname=\"" + _board->getEngineName() + "\"");
			ioHandler->println("feature done=1");
		}
	}
}

void Winboard::handleRemove(IChessBoard* board) {
	if (computerIsWhite != board->isWhiteToMove()) {
		board->undoMove();
		board->undoMove();
	}
}

void Winboard::runPerft(IChessBoard* board, IInputOutput* ioHandler, bool showMoves) {
	StdTimeControl timeControl;
	if (ioHandler->getNextTokenNonBlocking() != "") {
		timeControl.storeStartTime();
		uint64_t res = board->perft((uint16_t)ioHandler->getCurrentTokenAsUnsignedInt());
		float durationInMs = (float)timeControl.getTimeSpentInMilliseconds();
		printf("nodes: %lld, time: %5.4fs, nps: %10.0f \n", res, durationInMs / 1000, res * 1000.0 / durationInMs);
	}
}

void Winboard::analyzeMove(IChessBoard* board, IInputOutput* ioHandler) {
	char move[10];
	move[9] = 0;
	analyzeMode = true;
	GameResult result = board->getGameResult();
	if (result != GameResult::NOT_ENDED) {
		HandleMove::printGameResult(result, ioHandler);
	}
	else {
		clock.setAnalyseMode(true);
		computingMove = true;
		computeThread = std::thread(&threadComputeMove, board, ioHandler, &clock, &computingMove);
	}
}

/**
 * Starts computing a move - sets analyze mode to false
 */
void Winboard::computeMove(IChessBoard* board, IInputOutput* ioHandler) {
	forceMode = false;
	computerIsWhite = board->isWhiteToMove();

	GameResult result = board->getGameResult();
	if (result != GameResult::NOT_ENDED) {
		HandleMove::printGameResult(result, ioHandler);
	}
	else {
		clock.storeCalculationStartTime();
		clock.setAnalyseMode(false);
		computingMove = true;
		computeThread = std::thread(&threadComputeMove, board, ioHandler, &clock, &computingMove);
	}
}

void Winboard::WMTest(IChessBoard* board, IInputOutput* ioHandler) {
	/*
	EPDTest test;
	test.initGames();
	FenScanner scanner;
	uint64_t totalNodesSearched = 0;
	uint64_t totalTimeUsedInMilliseconds = 0;
	if (ioHandler->getNextTokenNonBlocking()) {
		uint32_t depth = (uint32_t)ioHandler->getCurrentTokenAsUnsignedInt();
		for (uint32_t index = 0; index < 100; index++) {
			scanner.setBoard(test.Game1[index], board);
			clock.setAnalyseMode(true);
			clock.limitSearchDepth(depth);
			board->computeMove(clock, false);
			totalNodesSearched += board->getComputingInfo().nodesSearched;
			totalTimeUsedInMilliseconds += board->getComputingInfo().elapsedTimeInMilliseconds;
			test.printResult(index, totalTimeUsedInMilliseconds, totalNodesSearched);
		}
	}
	*/
}

/**
 * Answers with a pong to a ping
 */
void Winboard::handlePing(IInputOutput* ioHandler) {
	if (ioHandler->getNextTokenNonBlocking() != "") {
		string number = ioHandler->getCurrentToken();
		ioHandler->print("pong ");
		ioHandler->println(number);
	}
}

/**
 * Processes a board change (setboard) request
 */
bool Winboard::handleBoardChanges(IChessBoard* board, IInputOutput* ioHandler) {
	bool result = true;
	const string token = ioHandler->getCurrentToken();
	string startPosition = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
	if (token == "new") {
		FenScanner scanner;
		scanner.setBoard(startPosition, board);
	}
	else if (token == "setboard") {
		FenScanner scanner;
		string fen = ioHandler->getToEOLBlocking();
		bool success = scanner.setBoard(fen, board);
		if (!success) {
			printf("Error (illegal fen): %s \n", fen.c_str());
			scanner.setBoard(startPosition, board);
		}
	}
	else {
		result = false;
	}
	return result;
}

bool Winboard::handleWhatIf(IChessBoard* board, IInputOutput* ioHandler) {
	const string token = ioHandler->getCurrentToken();
	bool result = false;
	if (token == "whatif") {
		IWhatIf* whatIf = board->getWhatIf();
		whatIf->clear();
		ioHandler->getNextTokenNonBlocking();
		int32_t searchDepth = (int32_t)ioHandler->getCurrentTokenAsUnsignedInt();
		if (searchDepth == 0) { searchDepth = 1; }
		whatIf->setSearchDepht(searchDepth);
		for (int32_t ply = 0; ioHandler->getNextTokenNonBlocking() != ""; ply++) {
			if (token == "null") {
				whatIf->setNullmove(ply);
			}
			else {
				MoveScanner moveScanner(ioHandler->getCurrentToken());
				if (moveScanner.isLegal()) {
					whatIf->setMove(ply, moveScanner.piece, 
						moveScanner.departureFile, moveScanner.departureRank, 
						moveScanner.destinationFile, moveScanner.destinationRank, moveScanner.promote);
				}
			}
		}
		ClockSetting whatIfClock;
		whatIfClock.setAnalyseMode(true);
		whatIfClock.setSearchDepthLimit(searchDepth);
		board->computeMove(whatIfClock);
		whatIf->clear();
	}
	else {
		result = false;
	}
	return result;
}

void Winboard::readLevelCommand(IInputOutput* ioHandler) {
	uint8_t infoPos = 0;
	uint64_t curValue;
	uint64_t timeToThinkInSeconds = 0;

	while (ioHandler->getNextTokenNonBlocking(":") != "" && infoPos <= 4) {
		curValue = ioHandler->getCurrentTokenAsUnsignedInt();
		switch (infoPos) {
		case 0: clock.setMoveAmountForClock(int32_t(curValue)); infoPos++; break;
		case 1: timeToThinkInSeconds = curValue * 60; infoPos++; break;
		case 2:
			if (ioHandler->getCurrentToken()[0] != ':') {
				clock.setTimeIncrementPerMoveInMilliseconds(curValue * 1000);
				infoPos = 4;
			}
			infoPos++;
			break;
		case 3: timeToThinkInSeconds += curValue; infoPos++; break;
		case 4: clock.setTimeIncrementPerMoveInMilliseconds(curValue * 1000); infoPos++; break;
		}
	}
	clock.setTimeToThinkForAllMovesInMilliseconds(timeToThinkInSeconds * 1000);

}

bool Winboard::checkClockCommands(IChessBoard* board, IInputOutput* ioHandler) {
	bool commandProcessed = true;
	string token = ioHandler->getCurrentToken();
	if (token == "sd") {
		if (ioHandler->getNextTokenNonBlocking() != "") {
			clock.setSearchDepthLimit((uint32_t)ioHandler->getCurrentTokenAsUnsignedInt());
		}
	}
	else if (token == "time") {
		if (ioHandler->getNextTokenNonBlocking() != "") {
			clock.setComputerClockInMilliseconds(ioHandler->getCurrentTokenAsUnsignedInt() * 10);
		}
	}
	else if (token == "otim") {
		if (ioHandler->getNextTokenNonBlocking() != "") {
			clock.setUserClockInMilliseconds(ioHandler->getCurrentTokenAsUnsignedInt() * 10);
		}
	}
	else if (token == "level")
	{
		readLevelCommand(ioHandler);
	}
	else if (token == "st") {
		if (ioHandler->getNextTokenNonBlocking() != "") {
			clock.setExactTimePerMoveInMilliseconds(ioHandler->getCurrentTokenAsUnsignedInt() * 1000ULL);
		}
	}
	else {
		commandProcessed = false;
	}
	return commandProcessed;
}

/**
 * Wait for the computing thread to end and joins the thread.
 */
void Winboard::waitForComputingThreadToEnd() {
	if (computeThread.joinable()) {
		computeThread.join();
	}
}

/**
 * Checks the input for a move request, either "usermove" oder just a chess move in
 * chess move notation
 */
bool Winboard::checkMoveCommand(IChessBoard* board, IInputOutput* ioHandler) {
	bool moveCommandFound = false;
	if (ioHandler->getCurrentToken() == "usermove") {
		ioHandler->getNextTokenNonBlocking();
		moveCommandFound = true;
	}
	if (HandleMove::handleMove(board, ioHandler, ioHandler->getCurrentToken())) {
		moveCommandFound = true;
		if (analyzeMode) {
			board->moveNow();
			waitForComputingThreadToEnd();
			analyzeMove(board, ioHandler);
		}
		else if (!forceMode) {
			computeMove(board, ioHandler);
		}
	}
	return moveCommandFound;
}

/**
 * Processes any input from stdio
 */
void Winboard::processInput(IChessBoard* board, IInputOutput* ioHandler) {
	_board = board;
	_ioHandler = ioHandler;
	while (!quit) {
		if (analyzeMode) {
			handleInputWhileInAnalyzeMode(board, ioHandler);
		}
		else if (computingMove) {
			handleInputWhileComputingMove(board, ioHandler);
		}
		else if (editMode) {
			handleInputWhiteInEditMode(board, ioHandler);
		}
		else {
			waitForComputingThreadToEnd();
			handleInput(board, ioHandler);
		}
		ioHandler->getNextTokenBlocking();
	}
	waitForComputingThreadToEnd();
}

/**
 * Processes input while computing a move
 */
void Winboard::handleInputWhileComputingMove(IChessBoard* board, IInputOutput* ioHandler) {
	const string token = ioHandler->getCurrentToken();
	if (token == "quit") {
		board->moveNow();
		quit = true;
	}
	else if (token == "?") {
		board->moveNow();
	}
	else if (token == ".") {
		board->requestPrintSearchInfo();
	}
}

void Winboard::handleInputWhileInAnalyzeMode(IChessBoard* board, IInputOutput* ioHandler) {
	const string token = ioHandler->getCurrentToken();
	if (token == "quit") {
		board->moveNow();
		analyzeMode = false;
		quit = true;
	}
	else if (token == ".") {
		board->requestPrintSearchInfo();
	}
	else if (token == "ping") {
		handlePing(ioHandler);
	}
	else if (token == "exit" || token == "force") {
		board->moveNow();
		waitForComputingThreadToEnd();
		analyzeMode = false;
		forceMode = true;
	}
	else if (token == "new" || token == "setboard") {
		board->moveNow();
		waitForComputingThreadToEnd();
		handleBoardChanges(board, ioHandler);
	}
	else if (token == "usermove") {
		checkMoveCommand(board, ioHandler);
	}
	else {
		ioHandler->println("Error (command not supported in analyze mode): " + token);
	}
}

void Winboard::handleInputWhiteInEditMode(IChessBoard* board, IInputOutput* ioHandler) {
	const string token = ioHandler->getCurrentToken();
	if (token == "#") {
		board->clearBoard();
	}
	else if (token == "c") {
		editModeIsWhiteColor = !editModeIsWhiteColor;
	}
	else if (token == ".") {
		editMode = false;
	}
	else {
		char piece = ioHandler->getCurrentToken()[0];
		uint32_t col = ioHandler->getCurrentToken()[1] - 'a';
		uint32_t row = ioHandler->getCurrentToken()[2] - '1';
		if (!editModeIsWhiteColor) {
			piece += 'a' - 'A';
		}
		board->setPiece(col, row, piece);
	}
}

void Winboard::handleInput(IChessBoard* board, IInputOutput* ioHandler) {
	const string token = ioHandler->getCurrentToken();
	if (token == "quit") {
		quit = true;
	}
	else if (token == "analyze") {
		analyzeMove(board, ioHandler);
	}
	else if (token == "force") {
		forceMode = true;
	}
	else if (token == "go") {
		computeMove(board, ioHandler);
	}
	else if (handleBoardChanges(board, ioHandler)) {
	}
	else if (handleWhatIf(board, ioHandler)) {
	}
	else if (token == "easy") {
	}
	else if (token == "eval") {
		board->printEvalInfo();
	}
	else if (token == "hard") {
	}
	else if (token == "post") {
	}
	else if (token == "random") {
	}
	else if (token == "accepted") {
		ioHandler->getNextTokenNonBlocking();
	}
	else if (token == "perft") {
		runPerft(board, ioHandler, false);
	}
	else if (token == "divide") {
		runPerft(board, ioHandler, true);
	}
	else if (token == "xboard") {
		handleXBoard(ioHandler);
	}
	else if (token == "protover") {
		handleProtover(ioHandler);
	}
	else if (token == "white") {
		board->setWhiteToMove(true);
	}
	else if (token == "black") {
		board->setWhiteToMove(false);
	}
	else if (token == "ping") {
		handlePing(ioHandler);
	}
	else if (token == "edit") {
		editMode = true;
		editModeIsWhiteColor = true;
	}
	else if (token == "undo") {
		board->undoMove();
		forceMode = true;
	}
	else if (token == "remove") {
		handleRemove(board);
	}
	else if (token == "wmtest") {
		WMTest(board, ioHandler);
	}
	else if (token == "result") {
		ioHandler->getToEOLBlocking();
	}
	else if (checkClockCommands(board, ioHandler)) {
	}
	else if (checkMoveCommand(board, ioHandler)) {
	}
}
