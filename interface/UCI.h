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

#include "chessinterface.h"

using namespace std;

namespace ChessInterface {

	class UCI : public ChessInterface {
	public:
		UCI() {}

	private:
		/**
		 * Runs the UCI protocol interface steering the chess engine
		 */
		virtual void runLoop() {
			if (getCurrentToken() != "uci") {
				println("error (uci command expected): " + getCurrentToken());
				return;
			}
			while (getCurrentToken() != "quit") {
				processCommand();
			}
			stopCompute();
		}

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
			_clock.setTimeBetweenInfoInMilliseconds(1000);
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
			string space = "";
			while (token != "moves" && token != "\n" && token != "\r") {
				fen += space + token;
				space = " ";
				token = getNextTokenBlocking(true);
			}
			return fen;
		}

		/**
		 * Reads a UCI set position command and sets up the board
		 */
		void setPosition() {
			const string boardToken = getNextTokenBlocking(true);
			string debug;
			if (boardToken == "fen") setPositionByFen(readFen());
			else if (boardToken == "startpos") {
				debug = "startpos";
				setPositionByFen();
				getNextTokenBlocking(true);
			}
			if (getCurrentToken() == "moves") {
				debug += " moves";
				string token = getNextTokenBlocking(true);
				while (token != "\n" && token != "\r") {
					debug += " " + token;
					setMove(token);
					token = getNextTokenBlocking(true);
				}
			}
		}

		/**
		 * handles a uci go command
		 */
		void go() {
			stopCompute();
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
					uint64_t param = getCurrentTokenAsUnsignedInt();
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
			else if (token == "isready") println("readyok");
			else if (token == "ucinewgame") _board->newGame();
			else if (token == "position") setPosition();
			else if (token == "stop") stopCompute();
			getNextTokenBlocking(true);
		}


	};

}

#endif // __UCI_H