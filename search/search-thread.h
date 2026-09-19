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
#include "search.h"
#include "searchstack.h"
#include "search-worker.h"
#include "search-result-queue.h"

namespace QaplaSearch {

	/**
	 * The move a helper thread is searching. The node that handed it over invalidates the
	 * job when it is left before the result is in - the helper then drops the result.
	 */
	struct WorkerJob {
		std::atomic<bool> invalid{ false };
		SearchResultQueue* queue = nullptr;
		Move move;
		ply_t moveDepth = 0;
		ply_t lmr = 0;
		ply_t ply = 0;
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
