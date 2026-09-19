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
 * Everything a search thread owns: its Search, its stack, its position, the thread that
 * waits for jobs and the job it is working on. Shared between threads are only the
 * transposition table (through the stack) and the clock.
 */

#ifndef __SEARCH_THREAD_H
#define __SEARCH_THREAD_H

#include <atomic>
#include <mutex>
#include "search.h"
#include "searchstack.h"
#include "search-worker.h"

namespace QaplaSearch {

	/**
	 * The split point a helper thread works at. The node's thread sets abort when it closes
	 * the split point; the helper then stops a child search still running.
	 */
	struct WorkerJob {
		std::atomic<bool> abort{ false };
		// The stack of the node's thread: the helper fetches from it what it needs, see
		// SearchStack::fetchForHandover, and shares the node at ply with it. Without any
		// lock on the fetch: the node's thread does not leave the node while the helper works
		// there, so what the helper reads stays put.
		SearchStack* stack = nullptr;
		ply_t ply = 0;
		// Remaining depth of the node, extensions included, and the singular extension it
		// computed for its tt move
		ply_t depth = 0;
		ply_t seExtension = 0;
		// The node is a PV node, else an inner node; near leaf nodes open no split point
		bool pvNode = false;
		// Nodes the helper searched at this split point, taken over by the node's thread
		uint64_t nodes = 0;
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
		SearchThread(TT* tt) : stack(tt) {}
		SearchThread(const SearchThread&) = delete;
		SearchThread& operator=(const SearchThread&) = delete;

		Search search;
		SearchStack stack;
		MoveGenerator position;
		SearchWorker worker;
		WorkerJob job;
	};

}

#endif // __SEARCH_THREAD_H
