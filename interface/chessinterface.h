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
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

using namespace std;

namespace QaplaInterface {


	class WorkerThread {
	public:
		WorkerThread() : stop_thread(false), task_running(false) {
			worker = std::thread(&WorkerThread::run, this);
		}

		// Move-Konstruktor
		WorkerThread(WorkerThread&& other) noexcept
			: stop_thread(other.stop_thread.load()), task_running(other.task_running.load()) {
			if (other.worker.joinable()) {
				worker = std::move(other.worker);
			}
		}

		// Move-Zuweisungsoperator
		WorkerThread& operator=(WorkerThread&& other) noexcept {
			if (this != &other) {
				shutdown();
				stop_thread = other.stop_thread.load();
				task_running = other.task_running.load();
				if (other.worker.joinable()) {
					worker = std::move(other.worker);
				}
			}
			return *this;
		}

		~WorkerThread() {
			shutdown();
		}

		// Start a new task in the thread
		void startTask(std::function<void()> task) {
			{
				std::unique_lock<std::mutex> lock(mutex);
				if (task_running) {
					return;
				}
				this->task = task;
				task_running = true;
			}
			cv.notify_one();
		}

		// Wait for the current task to finish
		void waitForTaskCompletion() {
			std::unique_lock<std::mutex> lock(mutex);
			cv_task_done.wait(lock, [this] { return !task_running; });
		}

		// Shut down the thread safely
		void shutdown() {
			{
				std::unique_lock<std::mutex> lock(mutex);
				stop_thread = true;
			}
			cv.notify_one();
			waitForTaskCompletion();
			if (worker.joinable()) {
				worker.join();
			}
		}

	private:
		std::thread worker;
		std::function<void()> task;
		std::mutex mutex;
		std::condition_variable cv;
		std::condition_variable cv_task_done;
		std::atomic<bool> stop_thread;
		std::atomic<bool> task_running;

		// Main loop for the worker thread
		void run() {
			while (true) {
				std::function<void()> local_task;
				{
					std::unique_lock<std::mutex> lock(mutex);
					cv.wait(lock, [this] { return task_running || stop_thread; });

					if (stop_thread && !task_running) {
						break;
					}

					local_task = std::move(task);
				}

				if (local_task) {
					local_task();
					{
						std::unique_lock<std::mutex> lock(mutex);
						task_running = false;
					}
					cv_task_done.notify_all();
				}
			}
		}
	};



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

		static bool setPositionByFen(std::string position, IChessBoard* board) {
			FenScanner scanner;
			bool success = scanner.setBoard(position, board);
			return success;
		}

		/**
		 * Gets a move to the board
		 */
		static bool setMove(string move, IChessBoard* board) {
			if (move == "") return false;
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

		static bool isCapture(string move, IChessBoard* board) {
			if (move == "") return false;
			MoveScanner scanner(move);
			bool res = false;
			if (scanner.isLegal()) {
				res = board->isCapture(
					scanner.piece,
					scanner.departureFile, scanner.departureRank,
					scanner.destinationFile, scanner.destinationRank,
					scanner.promote);
			}
			return res;
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
			return setPositionByFen(position, _board);
		}

		bool isLegalMove(string move) {}

		/**
		 * Gets a move to the board
		 */
		bool setMove(string move) {
			return setMove(move, _board);
		}

		bool isCapture(string move) {
			return isCapture(move, _board);
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
		void waitIfInfiniteSearchFinishedEarly() {
			unique_lock<mutex> lock = unique_lock<mutex>(_protectWorkerAccess);
			_protectSearchTermination.wait(lock, [this] {
					return !_isInfiniteSearch;
			});
		}

		/**
		 * Wait for the computing thread to end and joins the thread.
		 */
		void waitForComputingThreadToEnd() {
			_computeThread.waitForTaskCompletion();
		}

		/**
		 * Stop current computing
		 */
		void stopCompute() {
			{
				const lock_guard<mutex> lock(_protectWorkerAccess);
				_isInfiniteSearch = false;
				_board->moveNow();
				_protectSearchTermination.notify_one();
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
			const lock_guard<mutex> lock(_protectWorkerAccess);
			_isInfiniteSearch = false;
			_board->ponderHit();
			_clock.setSearchMode();
			_protectSearchTermination.notify_one();
		}

	protected:
		enum class Mode { WAIT, COMPUTE, ANALYZE, EDIT, PONDER, QUIT };
		void println(const string& output) { _ioHandler->println(output); }
		void print(const string& output) { _ioHandler->print(output); }
		string getCurrentToken() { return _ioHandler->getCurrentToken(); }
		string getNextTokenBlocking(bool getEOL = false) { return _ioHandler->getNextTokenBlocking(getEOL); }
		string getNextTokenNonBlocking(string separators = "") { return _ioHandler->getNextTokenNonBlocking(separators); }
		string getToEOLBlocking() { return _ioHandler->getToEOLBlocking(); }
		uint64_t getCurrentTokenAsUnsignedInt() { return _ioHandler->getCurrentTokenAsUnsignedInt(); }
		bool isFatalError() { return _ioHandler->isFatalReadError(); }
		IChessBoard* getBoard() { return _board; }
		WorkerThread& getWorkerThread() { return _computeThread; }

	private:
		IChessBoard* _board;
		IInputOutput* _ioHandler;
		condition_variable _protectSearchTermination;
		mutex _protectWorkerAccess;
		bool _isInfiniteSearch;
		bool _stopRequested;
		WorkerThread _computeThread;
	
	protected:
		ClockSetting _clock;
		uint32_t _maxTheadCount;
		uint32_t _maxMemory;
		string _egtPath;
		string _bitbasePath;
	};

}

#endif // __CHESSINTERFACE_H