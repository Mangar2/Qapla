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

#ifndef __WINBOARD_H
#define __WINBOARD_H

//#include "EPDTest.h"
#include "chessinterface.h"

using namespace std;

namespace QaplaInterface {

	class Winboard : public ChessInterface {
	public:
		Winboard();

	private:
		/**
		 * Reads the amount of cores
		 */
		void readCores() {
			getNextTokenBlocking();
			_maxTheadCount = uint32_t(getCurrentTokenAsUnsignedInt());
		}

		/**
		 * Reads the amount of memory
		 */
		void readMemory() {
			getNextTokenBlocking();
			_maxMemory = uint32_t(getCurrentTokenAsUnsignedInt());
		}

		/**
		 * Manages a move
		 */
		bool handleMove(string move = "");

		/**
		 * Starts pondering
		 */
		void ponder(string move);

		/**
		 * Prints a game result information
		 */
		void printGameResult(GameResult result);

		/**
		 * Processes any input coming from the console
		 */
		virtual void runLoop();

		/**
		 * handles a generate EGTB command
		 */
		void generateEGTB();

		/**
		 * handles a verify EGTB command
		 */
		void verifyEGTB();

		/**
		 * Sets xBoard mode 
		 */
		void handleXBoard() {
			_xBoardMode = true;
		}

		/**
		 * Prints the protocol abilities
		 */
		void handleProtover();

		/*
		 * Removes the last two moves, if a human person is at move
		 */
		void handleRemove();
		
		/**
		 * Runs the perft command
		 */
		void runPerft(bool showMoves);

		/**
		 * Sets the computer to analyze mode
		 */
		void analyzeMove();

		/**
		 * Starts computing a move
		 */
		void computeMove();

		/**
		 * Starts a test of a list of EPD strings
		 */
		void WMTest();

		/**
		 * Answers to a ping (with a pong)
		 */
		void handlePing();

		/**
		 * Handles a new game command
		 */
		void newGame();

		/**
		 * Sets the board from fen
		 */
		void setBoard();

		/**
		 * Handles a whatif command.
		 * WhatIf is a special function of Quabla to debug the search tree
		 * The function will print the main variant for a list of moves 
		 * "whatif e4 e5" (what did you calculate as main variant after moves e4, e5)
		 */
		void handleWhatIf();

		/**
		 * Handles a play level command
		 */
		void readLevelCommand();

		/**
		 * check command to modify the clock
		 */
		bool checkClockCommands();

		/**
		 * Check for a usermove command or a move to play 
		 */
		bool checkMoveCommand();

		/**
		 * Undoes the last move
		 */
		void undoMove();

		/**
		 * Processes any input while computing a move
		 */
		void handleInputWhileComputingMove();

		/**
		 * Processes any input while in analyze mode
		 */
		void handleInputWhileInAnalyzeMode();

		/**
		 * Processes any input whil in edit mode
		 */
		void handleInputWhiteInEditMode();

		/**
		 * Processes any input while in ponder mode
		 */
		void handleInputWhileInPonderMode();

		/**
		 * Handles input while in "wait for user action" mode
		 */
		void handleInput();

		bool _editModeIsWhiteColor;
		volatile Mode _mode;
		uint8_t _protoVer;
		bool _xBoardMode;
		bool _computerIsWhite;
		bool _forceMode;
		bool _easy;
		string ponderMove;
		ISendSearchInfo* _sendSearchInfo;
	};

}

#endif // __WINBOARD_H