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
 * Results of move searches handed to other threads. Every node owns one queue; a worker
 * writes the result of a move it searched for that node, the node's thread reads it.
 * Fixed size ring, the entries are never moved.
 */

#ifndef __SEARCH_RESULT_QUEUE_H
#define __SEARCH_RESULT_QUEUE_H

#include <array>
#include <atomic>
#include <mutex>
#include "../basics/move.h"
#include "pv.h"
#include "search-config.h"

namespace QaplaSearch {

	/**
	 * Result of a move searched by another thread: the move, the value of its null window
	 * search, the depth it was searched with, the nodes it took and the line it found.
	 */
	struct SearchResult {
		QaplaBasics::Move move;
		value_t value = 0;
		ply_t moveDepth = 0;
		uint64_t nodes = 0;
		// The line starts at childPly, below it pv holds nothing
		ply_t childPly = 0;
		PV pv;
	};

	class SearchResultQueue {
	public:
		SearchResultQueue() = default;
		// A node is copied to another stack for a hand-over; its queue stays where it is.
		SearchResultQueue(const SearchResultQueue&) {}
		SearchResultQueue& operator=(const SearchResultQueue&) { return *this; }

		/**
		 * Adds a result, unless the job it belongs to has been invalidated. The check and the
		 * write happen under one lock, see invalidate.
		 * @param childPly ply the line starts at
		 * @returns false, if the result was not taken
		 */
		bool push(QaplaBasics::Move move, value_t value, ply_t moveDepth, uint64_t nodes,
			const PV& line, ply_t childPly, const std::atomic<bool>& invalid)
		{
			std::lock_guard<std::mutex> lock(_mutex);
			if (invalid || _count == SIZE) return false;
			SearchResult& entry = _entries[(_head + _count) % SIZE];
			entry.move = move;
			entry.value = value;
			entry.moveDepth = moveDepth;
			entry.nodes = nodes;
			entry.childPly = childPly;
			entry.pv.copyFromPV(line, childPly);
			_count++;
			return true;
		}

		/**
		 * Takes the oldest result
		 * @returns false, if the queue is empty
		 */
		bool pop(SearchResult& result) {
			std::lock_guard<std::mutex> lock(_mutex);
			if (_count == 0) return false;
			const SearchResult& entry = _entries[_head];
			result.move = entry.move;
			result.value = entry.value;
			result.moveDepth = entry.moveDepth;
			result.nodes = entry.nodes;
			result.childPly = entry.childPly;
			result.pv.copyFromPV(entry.pv, entry.childPly);
			_head = (_head + 1) % SIZE;
			_count--;
			return true;
		}

		/**
		 * Marks a job as invalid and drops what it may have written already. The node is being
		 * left, nothing a worker still computes for it may be read by the next node on this ply.
		 */
		void invalidate(std::atomic<bool>& invalid) {
			std::lock_guard<std::mutex> lock(_mutex);
			invalid = true;
			_head = 0;
			_count = 0;
		}

	private:
		static constexpr uint32_t SIZE = SearchConfig::RESULT_QUEUE_SIZE;
		std::array<SearchResult, SIZE> _entries;
		uint32_t _head = 0;
		uint32_t _count = 0;
		std::mutex _mutex;
	};

}

#endif // __SEARCH_RESULT_QUEUE_H
