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

#include <string>
#include "ichessboard.h"
#include "iinputoutput.h"
#include "clocksetting.h"
#include "stdtimecontrol.h"
#include "movescanner.h"
#include "fenscanner.h"
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

	class UCI {
	public:
		UCI(string engineName) :_engineName(engineName) {}

		/**
		 * Runs the UCI protocol interface steering the chess engine
		 */
		void run(IChessBoard* chessBoard, IInputOutput* ioHandler) {
			_ioHandler = ioHandler;
			_chessBoard = chessBoard;
			if (getCurrentToken() != "uci") {
				println("error (uci command expected): " + getCurrentToken());
				return;
			}
		}

	protected:

		/**
		 * Processes a set option command; 
		 */
		void setoption(string name, string value) {
			_chessBoard->setOption(name, value);
		}

		/**
		 * Processes a set option command
		 */
		void setoption() {
			if (getCurrentToken() == "setOption" && getNextTokenBlocking() == "name") {
				const string name = getNextTokenBlocking();
				if (getNextTokenBlocking() == "value") {
					const string value = getNextTokenBlocking();
					setoption(name, value);
				}
			}
		}

		/**
		 * Reply on an "UCI" command
		 */
		void uciCommand() {
			println("id name " + _engineName);
			println("option name Hash type spin default 32 min 1 max 1024");
			println("option name Ponder type check");
			println("uciok");
		}

	private:
		void println(const string& output) { _ioHandler->println(output); }
		void print(const string& output) { _ioHandler->print(output); }
		string getCurrentToken() { return _ioHandler->getCurrentToken(); }
		string getNextTokenBlocking() { return _ioHandler->getNextTokenBlocking(); }
		uint64_t getTokenAsUnsignedInt() { return _ioHandler->getCurrentTokenAsUnsignedInt(); }
		IChessBoard* _chessBoard;
		IInputOutput* _ioHandler;
		string _engineName;
	};

}

#endif // __WINBOARD_H