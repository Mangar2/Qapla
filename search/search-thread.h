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
 * Everything a search thread owns: its Search, its stack, its position, its wait state and
 * the list of split points it works under. Shared between threads are only the
 * transposition table (through the stack) and the clock.
 *
 * A thread that has nothing to do waits - a helper for its first job, an owner of a split
 * point until the helpers have left it. A waiting thread can be booked by a thread that
 * opens a split point; a waiting owner only for a split point below its own, so that it
 * reaches the split point by replaying moves from where it stands.
 */

#ifndef __SEARCH_THREAD_H
#define __SEARCH_THREAD_H

#include <array>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include "search.h"
#include "searchstack.h"

namespace QaplaSearch {

	/**
	 * A split point to join: the node, the stack it lives in and what the move loop needs
	 * to know about it.
	 */
	struct SplitJob {
		SearchNode* node = nullptr;
		SearchStack* stack = nullptr;
		ply_t ply = 0;
		// Remaining depth of the node, extensions included, and the singular extension it
		// computed for its tt move
		ply_t depth = 0;
		ply_t seExtension = 0;
		// The node is a PV node, else an inner node; near leaf nodes open no split point
		bool pvNode = false;
	};

	/**
	 * Locks the split point's node in the split instantiation of the move loop, is nothing
	 * in the other one.
	 */
	template <bool SPLIT>
	struct SplitLock {
		SplitLock(std::mutex& mutex) : _lock(mutex) {}
		std::lock_guard<std::mutex> _lock;
	};

	template <>
	struct SplitLock<false> {
		SplitLock(std::mutex&) {}
	};

	struct SearchThread {
		SearchThread(TT* tt) : stack(tt) {
			splitAt.fill(nullptr);
		}
		SearchThread(const SearchThread&) = delete;
		SearchThread& operator=(const SearchThread&) = delete;

		Search search;
		SearchStack stack;
		MoveGenerator position;

		// ---- The split points this thread works under, by ply. A thread that joins a split
		// point takes over the owner's list up to there: so a waiting owner can tell whether
		// another thread works below its own split point, by one pointer comparison.
		std::array<SearchNode*, SearchConfig::MAX_SEARCH_DEPTH + 1> splitAt;
		std::array<ply_t, SearchConfig::MAX_SEARCH_DEPTH + 1> splitPlies;
		int32_t splitAmount = 0;

		void addSplit(SearchNode* node, ply_t ply) {
			splitAt[ply] = node;
			splitPlies[splitAmount] = ply;
			splitAmount++;
		}

		/**
		 * Removes the split points above ply
		 */
		void removeSplitsAbove(ply_t ply) {
			while (splitAmount > 0 && splitPlies[splitAmount - 1] > ply) {
				splitAmount--;
				splitAt[splitPlies[splitAmount]] = nullptr;
			}
		}

		/**
		 * Takes over the split points of the owner of a split point at ply, up to there
		 */
		void takeSplitsFrom(const SearchThread& owner, ply_t ply) {
			for (int32_t index = 0; index < owner.splitAmount; index++) {
				const ply_t ownerPly = owner.splitPlies[index];
				if (ownerPly >= ply) break;
				if (splitAmount == 0 || ownerPly > splitPlies[splitAmount - 1]) {
					addSplit(owner.splitAt[ownerPly], ownerPly);
				}
			}
		}

		/**
		 * @returns true, if this thread works below the split point node at ply
		 */
		bool worksUnder(const SearchNode* node, ply_t ply) const {
			return splitAt[ply] == node;
		}

		/**
		 * @returns true, if a split point this thread works under has failed high - then
		 * nothing below it is worth searching any more
		 */
		bool hasFailedHighSplitPoint() const {
			for (int32_t index = 0; index < splitAmount; index++) {
				if (splitAt[splitPlies[index]]->isFailHigh()) return true;
			}
			return false;
		}

		// ---- Wait state, guarded by mutex. waiting says the thread can be booked: it is set
		// while the thread waits and taken back for the time it runs a job, at every nesting
		// level - a helper that owns a split point inside its job waits and helps like the
		// master does. hasJob says a booking is pending. waitingAt is the split point an
		// owner waits for, null for a helper without one.
		std::mutex mutex;
		std::condition_variable cv;
		bool waiting = false;
		bool hasJob = false;
		bool stop = false;
		SearchNode* waitingAt = nullptr;
		SplitJob job;
		std::thread thread;

		/**
		 * Books this thread for a split point, if it waits and may join: a waiting owner only
		 * below its own split point.
		 * @returns false, if the thread cannot be booked
		 */
		bool book(const SearchThread& booker, const SplitJob& splitJob) {
			{
				std::lock_guard<std::mutex> lock(mutex);
				if (!waiting || hasJob || stop) return false;
				if (waitingAt != nullptr && !booker.worksUnder(waitingAt, waitingAt->ply)) return false;
				job = splitJob;
				hasJob = true;
			}
			cv.notify_all();
			return true;
		}

		/**
		 * Wakes the thread, called by the last helper leaving the split point it waits for
		 */
		void wake() {
			std::lock_guard<std::mutex> lock(mutex);
			cv.notify_all();
		}
	};

	/**
	 * The search threads. The first one is the master, it runs on the thread that calls the
	 * search; the others are helpers with a thread of their own.
	 */
	class SearchThreads {
	public:
		~SearchThreads() {
			stopHelpers();
		}

		/**
		 * Sets the number of threads. Helper threads are started here and kept.
		 */
		void resize(uint32_t amount, TT* tt) {
			if (_threads.size() == amount) return;
			stopHelpers();
			_threads.clear();
			for (uint32_t index = 0; index < amount; index++) {
				_threads.push_back(std::make_unique<SearchThread>(tt));
				_threads[index]->search.setThreads(this, _threads[index].get());
			}
			// Started only now, they read the pool from the first moment
			for (uint32_t index = 1; index < amount; index++) {
				SearchThread* thread = _threads[index].get();
				thread->thread = std::thread([thread] { thread->search.helperLoop(*thread); });
			}
		}

		SearchThread& master() { return *_threads[0]; }
		uint32_t size() const { return uint32_t(_threads.size()); }
		SearchThread& operator[](uint32_t index) { return *_threads[index]; }

		/**
		 * Cheap test whether a booking may succeed at all
		 */
		bool hasWaitingThread() const {
			return _waitingCount.load(std::memory_order_relaxed) > 0;
		}

		void enterWaiting() { _waitingCount++; }
		void leaveWaiting() { _waitingCount--; }

		/**
		 * Books a waiting thread for a split point
		 * @returns the thread, or null if none can be booked
		 */
		SearchThread* book(const SearchThread& booker, const SplitJob& job) {
			for (auto& thread : _threads) {
				if (thread.get() == &booker) continue;
				if (thread->book(booker, job)) return thread.get();
			}
			return nullptr;
		}

		/**
		 * Waits until every helper is waiting for a job
		 */
		void waitUntilIdle() {
			for (uint32_t index = 1; index < _threads.size(); index++) {
				SearchThread& thread = *_threads[index];
				std::unique_lock<std::mutex> lock(thread.mutex);
				thread.cv.wait(lock, [&thread] { return thread.waiting && !thread.hasJob; });
			}
		}

	private:
		void stopHelpers() {
			for (uint32_t index = 1; index < _threads.size(); index++) {
				SearchThread& thread = *_threads[index];
				{
					std::lock_guard<std::mutex> lock(thread.mutex);
					thread.stop = true;
				}
				thread.cv.notify_all();
				if (thread.thread.joinable()) thread.thread.join();
			}
		}

		std::vector<std::unique_ptr<SearchThread>> _threads;
		std::atomic<int32_t> _waitingCount{ 0 };
	};

}

#endif // __SEARCH_THREAD_H
