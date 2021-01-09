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
 * @Overview
 * Implements a Winboard - Interface
 */

#include <string>
#include "ichessboard.h"
#include "iinputoutput.h"
#include "clocksetting.h"
#include "stdtimecontrol.h"
#include "movescanner.h"
#include "fenscanner.h"
//#include "EPDTest.h"
#include <thread>

using namespace std;

namespace ChessInterface {

	class HandleMove {
	public:

		/**
		 * Manages a move
		 */
		static bool handleMove(IChessBoard* chessBoard, IInputOutput* ioHandler, const string move);

		/**
		 * Prints a game result information
		 */
		static void printGameResult(GameResult result, IInputOutput* ioHandler);

	private:
		/** 
		 * Scans a move
		 */
		static bool scanMove(IChessBoard* chessBoard, const string move);

	};

	class Winboard {
	public:
		Winboard();

		/**
		 * Sets xBoard mode 
		 */
		void handleXBoard(IInputOutput* ioHandler) {
			xBoardMode = true;
		}

		/**
		 * Prints the protocol abilities
		 */
		void handleProtover(IInputOutput* ioHandler);

		/*
		 * Removes the last two moves, if a human person is at move
		 */
		void handleRemove(IChessBoard* chessBoard);
		
		/**
		 * Runs the perft command
		 */
		void runPerft(IChessBoard* chessBoard, IInputOutput* ioHandler, bool showMoves);

		/**
		 * Sets the computer to analyze mode
		 */
		void analyzeMove(IChessBoard* chessBoard, IInputOutput* ioHandler);

		/**
		 * Starts computing a move
		 */
		void computeMove(IChessBoard* chessBoard, IInputOutput* ioHandler);

		/**
		 * Starts a test of a list of EPD strings
		 */
		void WMTest(IChessBoard* chessBoard, IInputOutput* ioHandler);

		/**
		 * Answers to a ping (with a pong)
		 */
		void handlePing(IInputOutput* ioHandler);

		/**
		 * Handles a fen string to or a setBoard command
		 */
		bool handleBoardChanges(IChessBoard* chessBoard, IInputOutput* ioHandler);

		/**
		 * Handles a whatif command.
		 * WhatIf is a special function of Quabla to debug the search tree
		 * The function will print the main variant for a list of moves 
		 * "whatif e4 e5" (what did you calculate as main variant after moves e4, e5)
		 */
		bool handleWhatIf(IChessBoard* chessBoard, IInputOutput* ioHandler);

		/**
		 * Handles a play level command
		 */
		void readLevelCommand(IInputOutput* ioHandler);

		/**
		 * check command to modify the clock
		 */
		bool checkClockCommands(IChessBoard* chessBoard, IInputOutput* ioHandler);

		/**
		 * wait for a computing thread to end and disables the thread
		 */
		void waitForComputingThreadToEnd();

		/**
		 * Check for a usermove command or a move to play 
		 */
		bool checkMoveCommand(IChessBoard* chessBoard, IInputOutput* ioHandler);

		/**
		 * Processes any input coming from the console
		 */
		void processInput(IChessBoard* chessBoard, IInputOutput* ioHandler);

		/**
		 * Processes any input while computing a move
		 */
		void handleInputWhileComputingMove(IChessBoard* chessBoard, IInputOutput* ioHandler);

		/**
		 * Processes any input while in analyze mode
		 */
		void handleInputWhileInAnalyzeMode(IChessBoard* chessBoard, IInputOutput* ioHandler);

		/**
		 * Processes any input whil in edit mode
		 */
		void handleInputWhiteInEditMode(IChessBoard* chessBoard, IInputOutput* ioHandler);

		/**
		 * Handles input while in "wait for user action" mode
		 */
		void handleInput(IChessBoard* chessBoard, IInputOutput* ioHandler);

		volatile bool computingMove;
		volatile bool analyzeMode;
		bool editMode;
		bool editModeIsWhiteColor;
		ClockSetting clock;

	private:
		bool quit;
		uint8_t protoVer;
		bool xBoardMode;
		bool computerIsWhite;
		bool forceMode;
		std::thread computeThread;
	};

}