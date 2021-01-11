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
#include <thread>

using namespace std;

using namespace ChessInterface;

/**
 * Starts computing a move
 */
static void threadComputeMove(
	IChessBoard* chessBoard,
	IInputOutput* ioHandler,
	ClockSetting* clock,
	volatile bool* computingMove)
{

	chessBoard->computeMove(*clock);
	if (!clock->getAnalyseMode()) {
		ComputingInfoExchange computingInfo = chessBoard->getComputingInfo();
		ioHandler->print("move ");
		ioHandler->println(computingInfo.currentConsideredMove);
		HandleMove::handleMove(chessBoard, ioHandler, computingInfo.currentConsideredMove);
		clock->storeTimeSpent();
	}

	*computingMove = false;
}

bool HandleMove::handleMove(IChessBoard* chessBoard, IInputOutput* ioHandler, const string move) {
	bool legalMove;
	if (move == "") {
		return false;
	}
	if (!scanMove(chessBoard, move)) {
		ioHandler->print("Illegal move: ");
		ioHandler->println(ioHandler->getCurrentToken());
		legalMove = false;
	}
	else {
		printGameResult(chessBoard->getGameResult(), ioHandler);
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

bool HandleMove::scanMove(IChessBoard* chessBoard, const string move) {
	MoveScanner scanner(move);
	bool res = false;
	if (scanner.isLegal()) {
		res = chessBoard->doMove(
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
			ioHandler->println("feature myname=\"Qapla 0.0.1\"");
			ioHandler->println("feature done=1");
		}
	}
}

void Winboard::handleRemove(IChessBoard* chessBoard) {
	if (computerIsWhite != chessBoard->isWhiteToMove()) {
		chessBoard->undoMove();
		chessBoard->undoMove();
	}
}

void Winboard::runPerft(IChessBoard* chessBoard, IInputOutput* ioHandler, bool showMoves) {
	StdTimeControl timeControl;
	if (ioHandler->getNextTokenNonBlocking() != "") {
		timeControl.storeStartTime();
		uint64_t res = chessBoard->perft((uint16_t)ioHandler->getCurrentTokenAsUnsignedInt());
		float durationInMs = (float)timeControl.getTimeSpentInMilliseconds();
		printf("nodes: %lld, time: %5.4fs, nps: %10.0f \n", res, durationInMs / 1000, res * 1000.0 / durationInMs);
	}
}

void Winboard::analyzeMove(IChessBoard* chessBoard, IInputOutput* ioHandler) {
	char move[10];
	move[9] = 0;
	analyzeMode = true;
	GameResult result = chessBoard->getGameResult();
	if (result != GameResult::NOT_ENDED) {
		HandleMove::printGameResult(result, ioHandler);
	}
	else {
		clock.setAnalyseMode(true);
		computingMove = true;
		computeThread = std::thread(&threadComputeMove, chessBoard, ioHandler, &clock, &computingMove);
	}
}

/**
 * Starts computing a move - sets analyze mode to false
 */
void Winboard::computeMove(IChessBoard* chessBoard, IInputOutput* ioHandler) {
	forceMode = false;
	computerIsWhite = chessBoard->isWhiteToMove();

	GameResult result = chessBoard->getGameResult();
	if (result != GameResult::NOT_ENDED) {
		HandleMove::printGameResult(result, ioHandler);
	}
	else {
		clock.storeCalculationStartTime();
		clock.setAnalyseMode(false);
		computingMove = true;
		computeThread = std::thread(&threadComputeMove, chessBoard, ioHandler, &clock, &computingMove);
	}
}

void Winboard::WMTest(IChessBoard* chessBoard, IInputOutput* ioHandler) {
	/*
	EPDTest test;
	test.initGames();
	FenScanner scanner;
	uint64_t totalNodesSearched = 0;
	uint64_t totalTimeUsedInMilliseconds = 0;
	if (ioHandler->getNextTokenNonBlocking()) {
		uint32_t depth = (uint32_t)ioHandler->getCurrentTokenAsUnsignedInt();
		for (uint32_t index = 0; index < 100; index++) {
			scanner.setBoard(test.Game1[index], chessBoard);
			clock.setAnalyseMode(true);
			clock.limitSearchDepth(depth);
			chessBoard->computeMove(clock, false);
			totalNodesSearched += chessBoard->getComputingInfo().nodesSearched;
			totalTimeUsedInMilliseconds += chessBoard->getComputingInfo().elapsedTimeInMilliseconds;
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
		string number = ioHandler->getNextTokenNonBlocking();
		ioHandler->print("pong ");
		ioHandler->println(number);
	}
}

/**
 * Processes a board change (setboard) request
 */
bool Winboard::handleBoardChanges(IChessBoard* chessBoard, IInputOutput* ioHandler) {
	bool result = true;
	string startPosition = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
	if (ioHandler->isTokenEqualTo("new")) {
		FenScanner scanner;
		scanner.setBoard(startPosition, chessBoard);
	}
	else if (ioHandler->isTokenEqualTo("setboard")) {
		FenScanner scanner;
		string fen = ioHandler->getToEOLBlocking();
		bool success = scanner.setBoard(fen, chessBoard);
		if (!success) {
			printf("Error (illegal fen): %s \n", fen.c_str());
			scanner.setBoard(startPosition, chessBoard);
		}
	}
	else {
		result = false;
	}
	return result;
}

bool Winboard::handleWhatIf(IChessBoard* chessBoard, IInputOutput* ioHandler) {
	bool result = false;
	if (ioHandler->isTokenEqualTo("whatif")) {
		IWhatIf* whatIf = chessBoard->getWhatIf();
		whatIf->clear();
		ioHandler->getNextTokenNonBlocking();
		int32_t searchDepth = (int32_t)ioHandler->getCurrentTokenAsUnsignedInt();
		if (searchDepth == 0) { searchDepth = 1; }
		whatIf->setSearchDepht(searchDepth);
		for (int32_t ply = 0; ioHandler->getNextTokenNonBlocking() != ""; ply++) {
			if (ioHandler->isTokenEqualTo("null")) {
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
		whatIfClock.limitSearchDepth(searchDepth);
		chessBoard->computeMove(whatIfClock);
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

bool Winboard::checkClockCommands(IChessBoard* chessBoard, IInputOutput* ioHandler) {
	bool commandProcessed = true;
	if (ioHandler->isTokenEqualTo("sd")) {
		if (ioHandler->getNextTokenNonBlocking() != "") {
			clock.limitSearchDepth((uint32_t)ioHandler->getCurrentTokenAsUnsignedInt());
		}
	}
	else if (ioHandler->isTokenEqualTo("time")) {
		if (ioHandler->getNextTokenNonBlocking() != "") {
			clock.setComputerClockInMilliseconds(ioHandler->getCurrentTokenAsUnsignedInt() * 10);
		}
	}
	else if (ioHandler->isTokenEqualTo("otim")) {
		if (ioHandler->getNextTokenNonBlocking() != "") {
			clock.setUserClockInMilliseconds(ioHandler->getCurrentTokenAsUnsignedInt() * 10);
		}
	}
	else if (ioHandler->isTokenEqualTo("level"))
	{
		readLevelCommand(ioHandler);
	}
	else if (ioHandler->isTokenEqualTo("st")) {
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
bool Winboard::checkMoveCommand(IChessBoard* chessBoard, IInputOutput* ioHandler) {
	bool moveCommandFound = false;
	if (ioHandler->isTokenEqualTo("usermove")) {
		ioHandler->getNextTokenNonBlocking();
		moveCommandFound = true;
	}
	if (HandleMove::handleMove(chessBoard, ioHandler, ioHandler->getCurrentToken())) {
		moveCommandFound = true;
		if (analyzeMode) {
			chessBoard->moveNow();
			waitForComputingThreadToEnd();
			analyzeMove(chessBoard, ioHandler);
		}
		else if (!forceMode) {
			computeMove(chessBoard, ioHandler);
		}
	}
	return moveCommandFound;
}

/**
 * Processes any input from stdio
 */
void Winboard::processInput(IChessBoard* chessBoard, IInputOutput* ioHandler) {
	while (!quit) {
		ioHandler->getNextTokenBlocking();
		if (analyzeMode) {
			handleInputWhileInAnalyzeMode(chessBoard, ioHandler);
		}
		else if (computingMove) {
			handleInputWhileComputingMove(chessBoard, ioHandler);
		}
		else if (editMode) {
			handleInputWhiteInEditMode(chessBoard, ioHandler);
		}
		else {
			waitForComputingThreadToEnd();
			handleInput(chessBoard, ioHandler);
		}
	}
	waitForComputingThreadToEnd();
}

/**
 * Processes input while computing a move
 */
void Winboard::handleInputWhileComputingMove(IChessBoard* chessBoard, IInputOutput* ioHandler) {
	if (ioHandler->isTokenEqualTo("quit")) {
		chessBoard->moveNow();
		quit = true;
	}
	else if (ioHandler->isTokenEqualTo("?")) {
		chessBoard->moveNow();
	}
	else if (ioHandler->isTokenEqualTo(".")) {
		chessBoard->requestPrintSearchInfo();
	}
}

void Winboard::handleInputWhileInAnalyzeMode(IChessBoard* chessBoard, IInputOutput* ioHandler) {
	if (ioHandler->isTokenEqualTo("quit")) {
		chessBoard->moveNow();
		analyzeMode = false;
		quit = true;
	}
	else if (ioHandler->isTokenEqualTo(".")) {
		chessBoard->requestPrintSearchInfo();
	}
	else if (ioHandler->isTokenEqualTo("exit")) {
		chessBoard->moveNow();
		waitForComputingThreadToEnd();
		analyzeMode = false;
	}
	else if (ioHandler->isTokenEqualTo("new")) {
		chessBoard->moveNow();
		waitForComputingThreadToEnd();
		analyzeMove(chessBoard, ioHandler);
	}
	else if (ioHandler->isTokenEqualTo("usermove")) {
		checkMoveCommand(chessBoard, ioHandler);
	}
}

void Winboard::handleInputWhiteInEditMode(IChessBoard* chessBoard, IInputOutput* ioHandler) {
	if (ioHandler->isTokenEqualTo("#")) {
		chessBoard->clearBoard();
	}
	else if (ioHandler->isTokenEqualTo("c")) {
		editModeIsWhiteColor = !editModeIsWhiteColor;
	}
	else if (ioHandler->isTokenEqualTo(".")) {
		editMode = false;
	}
	else {
		char piece = ioHandler->getCurrentToken()[0];
		uint32_t col = ioHandler->getCurrentToken()[1] - 'a';
		uint32_t row = ioHandler->getCurrentToken()[2] - '1';
		if (!editModeIsWhiteColor) {
			piece += 'a' - 'A';
		}
		chessBoard->setPiece(col, row, piece);
	}
}

void Winboard::handleInput(IChessBoard* chessBoard, IInputOutput* ioHandler) {

	if (ioHandler->isTokenEqualTo("quit")) {
		quit = true;
	}
	else if (ioHandler->isTokenEqualTo("analyze")) {
		analyzeMove(chessBoard, ioHandler);
	}
	else if (ioHandler->isTokenEqualTo("force")) {
		forceMode = true;
	}
	else if (ioHandler->isTokenEqualTo("go")) {
		computeMove(chessBoard, ioHandler);
	}
	else if (handleBoardChanges(chessBoard, ioHandler)) {
	}
	else if (handleWhatIf(chessBoard, ioHandler)) {
	}
	else if (ioHandler->isTokenEqualTo("easy")) {
	}
	else if (ioHandler->isTokenEqualTo("eval")) {
		chessBoard->printEvalInfo();
	}
	else if (ioHandler->isTokenEqualTo("hard")) {
	}
	else if (ioHandler->isTokenEqualTo("post")) {
	}
	else if (ioHandler->isTokenEqualTo("random")) {
	}
	else if (ioHandler->isTokenEqualTo("accepted")) {
		ioHandler->getNextTokenNonBlocking();
	}
	else if (ioHandler->isTokenEqualTo("perft")) {
		runPerft(chessBoard, ioHandler, false);
	}
	else if (ioHandler->isTokenEqualTo("divide")) {
		runPerft(chessBoard, ioHandler, true);
	}
	else if (ioHandler->isTokenEqualTo("xboard")) {
		handleXBoard(ioHandler);
	}
	else if (ioHandler->isTokenEqualTo("protover")) {
		handleProtover(ioHandler);
	}
	else if (ioHandler->isTokenEqualTo("white")) {
		chessBoard->setWhiteToMove(true);
	}
	else if (ioHandler->isTokenEqualTo("black")) {
		chessBoard->setWhiteToMove(false);
	}
	else if (ioHandler->isTokenEqualTo("ping")) {
		handlePing(ioHandler);
	}
	else if (ioHandler->isTokenEqualTo("edit")) {
		editMode = true;
		editModeIsWhiteColor = true;
	}
	else if (ioHandler->isTokenEqualTo("undo")) {
		chessBoard->undoMove();
		forceMode = true;
	}
	else if (ioHandler->isTokenEqualTo("remove")) {
		handleRemove(chessBoard);
	}
	else if (ioHandler->isTokenEqualTo("wmtest")) {
		WMTest(chessBoard, ioHandler);
	}
	else if (checkClockCommands(chessBoard, ioHandler)) {
	}
	else if (checkMoveCommand(chessBoard, ioHandler)) {
	}
}
