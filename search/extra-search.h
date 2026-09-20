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
 * A search that runs beside the master's search on the same position, with threads of its
 * own, and contributes nothing but what it writes into the shared transposition table. It
 * starts with the master's search and is stopped when the master is done. It never checks
 * the clock and reports nothing.
 */

#ifndef __EXTRA_SEARCH_H
#define __EXTRA_SEARCH_H

#include <thread>
#include <vector>
#include "aspirationwindow.h"
#include "clockmanager.h"
#include "search-thread.h"

namespace QaplaSearch {

	class ExtraSearch {
	public:
		~ExtraSearch() {
			stop();
		}

		/**
		 * Starts the search on its own thread
		 * @param threads threads the search may use, its master included
		 */
		void start(const MoveGenerator& position, const std::vector<Move>& searchMoves,
			bool hasRepeatedPosition, uint32_t threads, TT* tt)
		{
			stop();
			_threads.resize(threads, tt);
			_root = position;
			_searchMoves = searchMoves;
			_hasRepeatedPosition = hasRepeatedPosition;
			_tt = tt;
			// Analyse mode: the clock never ends this search, stop does
			ClockSetting setting;
			setting.setAnalyseMode();
			_clock.startCalculatingMove(60, setting);
			_thread = std::thread([this] { run(); });
		}

		/**
		 * Stops the search and waits for it
		 */
		void stop() {
			if (!_thread.joinable()) return;
			_clock.stopSearch();
			_thread.join();
		}

		uint64_t getNodesSearched() {
			uint64_t nodes = 0;
			for (uint32_t index = 0; index < _threads.size(); index++) {
				nodes += _threads[index].search.getNodesSearched();
			}
			return nodes;
		}

	private:
		void run() {
			Search& search = _threads.master().search;
			SearchStack& stack = _threads.master().stack;
			search.initAsExtraSearch(&_clock, _tt);
			search.startNewSearch(_root, _searchMoves, _hasRepeatedPosition);
			for (uint32_t index = 1; index < _threads.size(); index++) {
				SearchThread& helper = _threads[index];
				helper.search.initAsHelper(search, &_clock, _tt);
				helper.position = _root;
			}
			if (search.getComputingInfo().getMovesAmount() == 0) return;
			_window.initSearch();

			const ply_t maxDepth = SearchConfig::MAX_SEARCH_DEPTH - 28;
			for (ply_t depth = 0; depth < maxDepth && !_clock.isSearchStopped(); depth++) {
				stack.clear();
				_window.newDepth(depth);
				uint32_t pvSearched = 0;
				do {
					stack.initSearchAtRoot(_root, _window.getAlpha(), _window.getBeta(), depth, search.getPawnTT());
					_clock.setCalculationDepth(depth);
					search.negaMaxRoot(_root, stack, 0, _clock);
					const auto& info = search.getComputingInfo();
					pvSearched = info.countPVSearchedMovesInWindow(depth);
					_window.setSearchResult(info.getPVMoveValueInCentiPawn(0));
				} while (!_clock.isSearchStopped() && pvSearched < 1);
			}
			_threads.waitUntilIdle();
		}

		SearchThreads _threads;
		AspirationWindow _window;
		ClockManager _clock;
		MoveGenerator _root;
		std::vector<Move> _searchMoves;
		bool _hasRepeatedPosition = false;
		TT* _tt = nullptr;
		std::thread _thread;
	};

}

#endif // __EXTRA_SEARCH_H
