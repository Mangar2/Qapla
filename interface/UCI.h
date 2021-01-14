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
 * Implements a UCI - Interface
 */

#ifndef __UCI_H
#define __UCI_H

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

	class ChessInterface {

	protected:

		/**
		 * Sets the board to a position
		 */
		void setPositionByFen(string position = "") {
			if (position == "") {
				position = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
			}
			FenScanner scanner;
			scanner.setBoard(position, _board);
		}

		/**
		 * Gets a move to the board
		 */
		bool setMove(string move) {
			MoveScanner scanner(move);
			bool res = false;
			if (scanner.isLegal()) {
				res = _board->doMove(
					scanner.piece,
					scanner.departureFile, scanner.departureRank,
					scanner.destinationFile, scanner.destinationRank,
					scanner.promote);
			}
			return res;
		}

		/**
 		 * Sets the playing time
		 */
		void setTime(uint64_t timeInMilliseconds, bool white) {
			if (_board->isWhiteToMove() == white) {
				_clock.setComputerClockInMilliseconds(timeInMilliseconds);
			}
			else {
				_clock.setUserClockInMilliseconds(timeInMilliseconds);
			}
		}

		/**
		 * Sets the playing time
		 */
		void setTimeInc(uint64_t timeInMilliseconds, bool white) {
			if (_board->isWhiteToMove() == white) {
				_clock.setTimeIncrementPerMoveInMilliseconds(timeInMilliseconds);
			}
		}

		/**
		 * Wait for the computing thread to end and joins the thread.
		 */
		void waitForComputingThreadToEnd() {
			if (_computeThread.joinable()) {
				_computeThread.join();
			}
		}

		/**
		 * Stop current computing
		 */
		void stopCompute() {
			_board->moveNow();
			waitForComputingThreadToEnd();
		}

	protected:
		void println(const string& output) { _ioHandler->println(output); }
		void print(const string& output) { _ioHandler->print(output); }
		string getCurrentToken() { return _ioHandler->getCurrentToken(); }
		string getNextTokenBlocking(bool getEOL = false) { return _ioHandler->getNextTokenBlocking(getEOL); }
		uint64_t getTokenAsUnsignedInt() { return _ioHandler->getCurrentTokenAsUnsignedInt(); }
		IChessBoard* _board;
		IInputOutput* _ioHandler;
		ClockSetting _clock;
		thread _computeThread;
	};

	class UCI : public ChessInterface {
	public:
		UCI() {}

		/**
		 * Runs the UCI protocol interface steering the chess engine
		 */
		void run(IChessBoard* chessBoard, IInputOutput* ioHandler) {
			_ioHandler = ioHandler;
			_board = chessBoard;
			if (getCurrentToken() != "uci") {
				println("error (uci command expected): " + getCurrentToken());
				return;
			}
			while (getCurrentToken() != "quit") {
				processCommand();
			}
		}

	protected:

		/**
		 * Starts computing a move - sets analyze mode to false
		 */
		void computeMove() {
			_clock.storeCalculationStartTime();
			_computeThread = thread([this]() {
				_board->computeMove(_clock);
				ComputingInfoExchange computingInfo = _board->getComputingInfo();
				println("bestmove " + computingInfo.currentConsideredMove);
			});
		}

		/**
		 * Starts search a move in ponder mode
		 */
		void ponder() {
			_clock.setPonderMode();
		}

		/**
		 * Processes a set option command; 
		 */
		void setoption(string name, string value) {
			_board->setOption(name, value);
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
			println("id name " + _board->getEngineName());
			println("option name Hash type spin default 32 min 1 max 1024");
			println("option name Ponder type check");
			println("uciok");
		}

		/**
		 * Reads a fen -input
		 */
		string readFen() {
			string token = getNextTokenBlocking(true);
			string fen = "";
			while (token != "moves" && token != "\n" && token != "\r") {
				fen += token;
				token = getNextTokenBlocking(true);
			}
			return fen;
		}

		/**
		 * Reads a UCI set position command and sets up the board
		 */
		void setPosition() {
			const string boardToken = getNextTokenBlocking();
			if (boardToken == "fen") setPositionByFen(readFen());
			else if (boardToken == "startpos") {
				setPositionByFen();
				getNextTokenBlocking(true);
			}
			if (getNextTokenBlocking() == "moves") {
				string token = getNextTokenBlocking(true);
				while (token != "\n" && token != "\r") {
					token = getNextTokenBlocking(true);
					setMove(token);
				}
			}
		}

		/**
		 * handles a uci go command
		 */
		void go() {
			_clock.reset();
			string token = "";
			bool ponder = false;
			while (token != "\n" && token != "\r") {
				token = getNextTokenBlocking(true);
				if (token == "\n" || token == "\r") break;
				if (token == "infinite") _clock.setAnalyseMode(true);
				else if (token == "ponder") {
					ponder = true;
					break;
				}
				else {
					getNextTokenBlocking(true);
					uint64_t param = getTokenAsUnsignedInt();
					if (token == "wtime") setTime(param, true);
					else if (token == "btime") setTime(param, false);
					else if (token == "winc") setTimeInc(param, true);
					else if (token == "binc") setTimeInc(param, true);
					else if (token == "movestogo") _clock.setMoveAmountForClock(uint32_t(param));
					else if (token == "depth") _clock.setSearchDepthLimit(uint32_t(param));
					else if (token == "nodes") _clock.setNodeCount(param);
					else if (token == "mate") _clock.setMate(param);
					else if (token == "movetime") _clock.setExactTimePerMoveInMilliseconds(param);
				}
			};
			if (ponder) {
			}
			else {
				computeMove();
			}
		}

		/**
		 * processes any uci command
		 */
		void processCommand() {
			const string token = getCurrentToken();
			if (token == "uci") uciCommand();
			else if (token == "go") go();
			else if (token == "ready") printf("readyok");
			else if (token == "ucinewgame") _board->newGame();
			else if (token == "position") setPosition();
			else if (token == "stop") stopCompute();
			getNextTokenBlocking();
		}


	};

}

#endif // __UCI_H