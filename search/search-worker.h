/**
 * @license
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Volker Böhm
 * @copyright Copyright (c) 2026 Volker Böhm
 * @Overview
 * A thread that waits for search jobs. It is started once and kept, a job is handed to it
 * with run() and the caller waits for its completion with wait().
 */

#ifndef __SEARCH_WORKER_H
#define __SEARCH_WORKER_H

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace QaplaSearch {

	class SearchWorker {
	public:
		SearchWorker() = default;
		SearchWorker(const SearchWorker&) = delete;
		SearchWorker& operator=(const SearchWorker&) = delete;
		~SearchWorker() { stop(); }

		/**
		 * Starts the thread, if it is not running yet
		 */
		void start() {
			if (!_thread.joinable()) {
				_thread = std::thread(&SearchWorker::loop, this);
			}
		}

		/**
		 * Hands a job to the thread. The thread must have been started and must be idle.
		 */
		void run(std::function<void()> job) {
			{
				std::lock_guard<std::mutex> lock(_mutex);
				_job = std::move(job);
				_hasJob = true;
			}
			_cv.notify_all();
		}

		/**
		 * Waits until the thread has finished its job
		 */
		void wait() {
			std::unique_lock<std::mutex> lock(_mutex);
			_cv.wait(lock, [this] { return !_hasJob; });
		}

		/**
		 * Stops the thread. Waits for a running job.
		 */
		void stop() {
			if (!_thread.joinable()) return;
			{
				std::lock_guard<std::mutex> lock(_mutex);
				_stop = true;
			}
			_cv.notify_all();
			_thread.join();
		}

	private:
		void loop() {
			for (;;) {
				std::unique_lock<std::mutex> lock(_mutex);
				_cv.wait(lock, [this] { return _hasJob || _stop; });
				if (_stop) return;
				lock.unlock();
				_job();
				lock.lock();
				_hasJob = false;
				lock.unlock();
				_cv.notify_all();
			}
		}

		std::thread _thread;
		std::mutex _mutex;
		std::condition_variable _cv;
		std::function<void()> _job;
		bool _hasJob = false;
		bool _stop = false;
	};

}

#endif // __SEARCH_WORKER_H
