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
 * Implements a thread pool to support multi-threading in chess
 */

#ifndef __THREADPOOL_H
#define __THREADPOOL_H

#include <array>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <condition_variable>

using namespace std;

namespace QaplaSearch {

	class WorkPackage {
	public:
		WorkPackage() : _workerCount(0), _work(0), _lastToReserve(false) {}
		
		/**
		 * Sets the working function
		 */
		void setFunction(function<void()> func) { _work = func; }

		/**
		 * Informs the work package, that the last worker thread must move to Status::reserve and not
		 * to Status::free
		 */
		void setLastToReserve() {
			_lastToReserve = true;
		}

		/**
		 * Waits until all worker finished to work on the work package
		 * Note: woker needs to call "signalFinished"
		 */
		void waitUntilFinished() {
			unique_lock<mutex> ulWork = unique_lock<mutex>(_mtxWork);
			_cvWorkFinished.wait(ulWork, [this] { return _workerCount == 0; });
		}

		/**
		 * Informs, that a worker has been added. 
		 * This function must be called by the thread adding the worker to 
		 * ensure that the count is increased before the adding thread can call waitUntilFinished() 
		 */
		void workerAdded() {
			const lock_guard<mutex> lock(_mtxWork);
			_workerCount++;
		}

		/**
		 * Starts the work function
		 */
		void runWorkFunction() {
			if (_work != 0) {
				_work();
			}
		}

		/**
		 * Notify that the worker has been finished
		 * @returns true, if the notifying worker must move to reserve
		 */
		bool notifyWorkerFinished() {
			bool noMoreWorkers = false;
			bool toReserve = false;
			{
				const lock_guard<mutex> lock(_mtxWork);
				_workerCount--;
				noMoreWorkers = _workerCount == 0;
				toReserve = noMoreWorkers && _lastToReserve;
			}
			if (noMoreWorkers) {
				_cvWorkFinished.notify_one();
			}
			return toReserve;
		}

		/**
		 * Gets the mutex protecting changes on the workerCount
 		 */
		mutex& getWorkerCountProtectionMutex() { return _mtxWork; }

		/**
		 * @returns true, if there are workers still working
		 */
		bool hasWorker() {
			return _workerCount > 0;
		}

	private:
		function<void()> _work;
		condition_variable _cvWorkFinished;
		std::atomic<uint32_t> _workerCount;
		mutex _mtxWork;
		std::atomic<bool> _lastToReserve;
	};

	class WorkerThread {
	public:
		WorkerThread() : _status(Status::stopped), _workPackage(0) {}

		enum class Status {
			free,
			busy,
			startReserve,
			startFree,
			reserve,
			main,
			stopped
		};

		/**
		 * Start the thread and wait for work
		 * @param isPool true, if the thread is not yet available for work
		 */
		void startAndWait(bool isWorker) {
			_status = isWorker ? Status::startFree : Status::startReserve;
			_thread = thread([this]() { 
				doWork(); 
			});
		}

		/** 
		 * Stop the thread, might take time until the current work is done
		 */
		void stop() {
			if (_status == Status::stopped) return;
			{
				const lock_guard<mutex> lock(_mtxRunning);
				_status = Status::stopped;
			}
			_cvWorkAvailable.notify_one();
			if (_thread.joinable()) {
				_thread.join();
			}
		}

		/**
		 * Assign work to the worker
		 * @returns false, if the assignment did not work, because the worker is not available
		 */
		bool assignWork(WorkPackage* work) {
			{
				const lock_guard<mutex> lock(_mtxRunning);
				if (_status != Status::free) return false;
				_workPackage = work;
				_status = Status::busy;
				_workPackage->workerAdded();
			}
			_cvWorkAvailable.notify_one();
			return true;
		}

		/**
		 * Sets a thread from reserve to free
		 */
		bool activate() {
			if (_status == Status::reserve) {
				const lock_guard<mutex> lock(_mtxRunning);
				if (_status != Status::reserve) return false;
				_status = Status::free;
				return true;
			}
			return false;
		}

		/**
		 * Checks, it the thread is currently available
		 */
		bool isAvailable() { return _status == Status::free; }

		/**
		 * Gets the current thread status
		 */
		Status getStatus() { return _status; }

	private:

		/**
		 * Thread internal function, waiting for work and working on work
		 */
		void doWork() {
			unique_lock<mutex> ulWork = unique_lock<mutex>(_mtxRunning);
			_status = (_status == Status::startReserve) ? Status::reserve : 
				(_status == Status::startFree) ? Status::free : _status;
			while (_status != Status::stopped) {
				_cvWorkAvailable.wait(ulWork, [this] { 
					bool doWait = _status == Status::free || _status == Status::reserve;
					return !doWait; 
				});
				if (_status == Status::busy && _workPackage != 0) {
					_workPackage->runWorkFunction();
					bool toReserve = _workPackage->notifyWorkerFinished();
					_status = toReserve ? Status::reserve : Status::free;
				}
			}
		}

		thread _thread;
		volatile Status _status;
		mutex _mtxRunning;
		condition_variable _cvWorkAvailable;
		WorkPackage* _workPackage;
	};

	template <uint32_t POOL_SIZE>
	class ThreadPool {
	public:
		ThreadPool() : _waitingAmount(0), _workerCount(0) {}
		~ThreadPool() {
			stopWorker();
			stopExamine();
		}

		/**
		 * Starts all threads of the thread pool
		 * Waits until all threads locked the _workMutex or are in wait state
		 * @param workerCount Number of workers ready to do work
		 * @param standbyCount Number of workers in standby, doing work, once workers are waiting
		 */
		void startWorker(uint32_t workerCount, uint32_t standbyCount = 0) {
			const bool WORKER = true;

			_workerCount = workerCount > POOL_SIZE ? POOL_SIZE : workerCount;
			for (uint32_t index = 0; index < _workerCount; ++index) {
				_workerPool[index].startAndWait(WORKER);
			}

			standbyCount = _workerCount + standbyCount > POOL_SIZE ? POOL_SIZE - _workerCount : standbyCount;
			for (uint32_t index = 0; index < standbyCount; ++index) {
				_workerPool[_workerCount + index].startAndWait(!WORKER);
			}
		}

		/**
		 * Notifies a single worker, that work is available
		 */
		void stopWorker() {
			for (auto& worker : _workerPool) {
				worker.stop();
			}
		}

		/**
		 * Assigns a work package to worker some worker
		 * Assigns the work as long as free work packages are available
		 * @param work work to assign
		 * @param [amount=1] amount of worker to assign (or 0 for all)
		 * @returns amount of workers assigned to the work package
		 */
		uint32_t assignWork(WorkPackage* work, uint32_t amount = 1) {
			uint32_t workerAdded = 0;
			for (auto& worker : _workerPool) {
				if (workerAdded == amount && amount > 0) break;
				if (worker.isAvailable()) {
					if (worker.assignWork(work)) {
						workerAdded++;
					}
				}
			}
			return workerAdded;
		}

		/**
		 * Activates a thread from reserve
		 * @param amount number of threads to set to free from reserve
		 * @returns number of threads set to free
		 */
		uint32_t activateReserve(uint32_t amount = 1) {
			uint32_t done = amount;
			for (auto& worker : _workerPool) {
				if (done == 0) break;
				if (worker.activate()) {
					done--;
				}
			}
			return amount - done;
		}

		/**
		 * Waits for a workpackage to finish
		 */
		void waitForWorkpackage(WorkPackage& work) {
			{
				// Protect against race by removing one worker from reserve 
				// whithout setting another woker to reserve
				const lock_guard<mutex> lock(work.getWorkerCountProtectionMutex());
				if (work.hasWorker() && activateReserve(1) > 0) {
					work.setLastToReserve();
				}
			}
			++_waitingAmount;
			work.waitUntilFinished();
			--_waitingAmount;
		}

		/**
		 * Performance measurement, start recording
		 */
		void startExamine() {
			_examinedFree = 0;
			_examinedBusy = 0;
			_doExamine = true;
			thExamine = thread([this]() { examine(); });
		}

		/**
		 * Performance measurement, stop recording
		 */
		void stopExamine() {
			if (_doExamine) {
				_doExamine = false;
				thExamine.join();
				const float total = _examinedBusy + _examinedFree;
				cout << "Free fraction(" << total << "): " << _examinedFree / total << endl;
				cout << "Waiting fraction(" << total << "): " << _examinedWaiting / total << endl;
			}
		}

	private:
		std::atomic<uint32_t> _waitingAmount;
		float _examinedFree;
		float _examinedBusy;
		float _examinedWaiting;
		volatile bool _doExamine;

		void examine() {
			while (_doExamine) {
				this_thread::sleep_for(chrono::milliseconds(10));
				_examinedWaiting += _waitingAmount;
				uint32_t busy = 0;
				uint32_t free = 0;
				for (auto& worker : _workerPool) {
					const auto status = worker.getStatus();
					if (status == WorkerThread::Status::free) {
						free++;
					}
					else if (status == WorkerThread::Status::busy) {
						busy++;
					}
				}
				_examinedFree += free;
				_examinedBusy += (busy - _waitingAmount);
				// if (busy > _workerCount) cout << busy << " waiting: " << _waitingAmount << endl;
			}
		}

		thread thScheduler;
		thread thExamine;
		uint32_t _workerCount;
		array<WorkerThread, POOL_SIZE> _workerPool;
	};
}

#endif // __THREADPOOL_H