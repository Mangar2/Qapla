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

#ifndef __CHESSINTERFACE_H
#define __CHESSINTERFACE_H

#include <string>
#include "ichessboard.h"
#include "iinputoutput.h"
#include "clocksetting.h"
#include "stdtimecontrol.h"
#include "movescanner.h"
#include "fenscanner.h"
#include <thread>
#include <mutex>
#include <condition_variable>

using namespace std;

namespace QaplaInterface {

	class ChessInterface {

	public:
		ChessInterface() : _board(0), _ioHandler(0) {}

		/**
		 * Runs the UCI protocol interface steering the chess engine
		 */
		void run(IChessBoard* chessBoard, IInputOutput* ioHandler) {
			_ioHandler = ioHandler;
			_board = chessBoard;
			runLoop();
		}


	protected:

		/**
		 * Override this function to process 
		 */
		virtual void runLoop() = 0;

		/**
		 * Sets the board to a position
		 */
		bool setPositionByFen(string position = "") {
			if (position == "") {
				position = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
			}
			FenScanner scanner;
			bool success = scanner.setBoard(position, _board);
			return success;
		}

		/**
		 * Gets a move to the board
		 */
		bool setMove(string move) {
			if (move == "") return false;
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
		 * Wait for stopCompute
		 */
		void waitOnInfiniteSearch() {
			unique_lock<mutex> lock = unique_lock<mutex>(_waitProtect);
			_waitCondition.wait(lock, [this] {
					return !_isInfiniteSearch;
			});
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
			{
				const lock_guard<mutex> lock(_waitProtect);
				_isInfiniteSearch = false;
				_board->moveNow();
				_waitCondition.notify_one();
			}
			waitForComputingThreadToEnd();
			
		}

		/**
		 * Sets a wait flag
		 */
		void setInfiniteSearch(bool infinite) {
			_isInfiniteSearch = infinite;
		}

		/**
		 * Handles a ponder - hit
		 */
		void ponderHit() {
			const lock_guard<mutex> lock(_waitProtect);
			_isInfiniteSearch = false;
			_board->ponderHit();
			_clock.setSearchMode();
			_waitCondition.notify_one();
		}

	protected:
		enum class Mode { WAIT, COMPUTE, ANALYZE, EDIT };
		void println(const string& output) { _ioHandler->println(output); }
		void print(const string& output) { _ioHandler->print(output); }
		string getCurrentToken() { return _ioHandler->getCurrentToken(); }
		string getNextTokenBlocking(bool getEOL = false) { return _ioHandler->getNextTokenBlocking(getEOL); }
		string getNextTokenNonBlocking(string separators = "") { return _ioHandler->getNextTokenNonBlocking(separators); }
		string getToEOLBlocking() { return _ioHandler->getToEOLBlocking(); }
		uint64_t getCurrentTokenAsUnsignedInt() { return _ioHandler->getCurrentTokenAsUnsignedInt(); }
		IChessBoard* _board;
		IInputOutput* _ioHandler;
		ClockSetting _clock;
		condition_variable _waitCondition;
		mutex _waitProtect;
		bool _isInfiniteSearch;
		thread _computeThread;
		uint32_t _maxTheadCount; 
		uint32_t _maxMemory;
		string _egtPath;
		string _bitbasePath;
	};

}

#endif // __CHESSINTERFACE_H