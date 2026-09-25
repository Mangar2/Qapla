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
 * @copyright Copyright (c) 2026 Volker Böhm
 * @Overview
 * Lets something watch the search without the search knowing what it is. Used
 * to collect positions for NNUE training data, see src/nnue-data.
 *
 * Without QAPLA_GENERATE_NNUE_DATA - see search-config.h - every call here is an
 * empty inline function and the compiler removes it: the search is the one of
 * the release build, node for node. With the define the calls cost a load and a
 * branch on a null pointer, and nothing at all while no observer is set.
 *
 * An observer sees the search of one thread. It is a single pointer for the
 * whole process, so a search that is to be watched must run single threaded -
 * the generator sets Threads to one.
 */

#pragma once

#include "searchdef.h"
#include "search-config.h"
#include "../basics/evalvalue.h"

namespace QaplaSearch {

	class SearchStack;

	/**
	 * What an observer of the search is told.
	 */
	class ISearchObserver {
	public:
		virtual ~ISearchObserver() = default;

		/**
		 * A node has been searched completely: its move loop is done, so it has
		 * a best move and a value. Nodes that were cut without searching their
		 * moves are not reported, they have neither.
		 * @param stack the search stack, the line to the node is in the
		 *        previousMove of the plies 1 to ply
		 * @param remainingDepth plies left below the node, after all reductions
		 *        and extensions this node got
		 * @param ply distance of the node from the root of the search
		 */
		virtual void nodeFinished(const SearchStack& stack, ply_t remainingDepth, ply_t ply) = 0;

		/**
		 * One iteration of the iterative deepening is done.
		 * @param remainingDepth the depth the root was searched at, which is one
		 *        less than the depth the engine reports as "info depth"
		 * @param rootValue value of the best root move, from the view of the
		 *        side to move at the root
		 */
		virtual void iterationFinished(ply_t remainingDepth, value_t rootValue) = 0;
	};

#ifdef QAPLA_GENERATE_NNUE_DATA
	constexpr bool GENERATE_NNUE_DATA = true;
#else
	constexpr bool GENERATE_NNUE_DATA = false;
#endif

	/**
	 * The one observer the search reports to. Set it through
	 * Search::setObserver, and set it back to nullptr before the object goes
	 * away.
	 */
	class SearchObserver {
	public:
		/** True if the calls below do anything at all in this build. */
		static constexpr bool isCompiledIn() {
			return GENERATE_NNUE_DATA;
		}

		static void set([[maybe_unused]] ISearchObserver* observer) {
			if constexpr (GENERATE_NNUE_DATA) _observer = observer;
		}

		static void nodeFinished([[maybe_unused]] const SearchStack& stack,
			[[maybe_unused]] ply_t remainingDepth, [[maybe_unused]] ply_t ply) {
			if constexpr (GENERATE_NNUE_DATA) {
				if (_observer != nullptr) _observer->nodeFinished(stack, remainingDepth, ply);
			}
		}

		static void iterationFinished([[maybe_unused]] ply_t remainingDepth,
			[[maybe_unused]] value_t rootValue) {
			if constexpr (GENERATE_NNUE_DATA) {
				if (_observer != nullptr) _observer->iterationFinished(remainingDepth, rootValue);
			}
		}

	private:
		inline static ISearchObserver* _observer = nullptr;
	};
}
