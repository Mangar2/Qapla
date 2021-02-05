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
 * Class holding moves played at the root with multi-pv support
 */

#ifndef __ROOTMOVES_H
#define __ROOTMOVES_H

#include <vector>
#include <string>
#include "searchdef.h"
#include "../basics/move.h"
#include "../basics/evalvalue.h"
#include "../movegenerator/movegenerator.h"
#include "pv.h"
#include "searchvariables.h"

using namespace std;
using namespace ChessBasics;

namespace ChessSearch {

	class RootMove
	{
	public:
		RootMove() { init(); }

		bool operator<(const RootMove& rootMove) const;
		bool operator>(const RootMove& rootMove) const;

		// Initializes all members
		void init();

		/**
		 * Chess move
		 */
		void setMove(Move move) { _move = move; }
		Move getMove() const { return _move; }

		/**
		 * @returns true, if the move is calculated as PV move
		 */
		bool IsPV() const
		{
			return _pvDepthOfLastSearch == _depthOfLastSearch;
		}

		/**
		 * Sets results after the search of a move is finished
		 */
		void set(const SearchVariables& variables, uint64_t totalNodes, uint64_t tableBaseHits,
			uint64_t timeSpentInMilliseconds, const string& pvString, const PV& pvLine);

		/**
 		 * Checks, if we need to research this root move.
		 * We do not need to search a move after an intial search window has been changed and 
		 * we can be sure, that the current move result was already inside the last window or 
		 * outside the window and the corresponding window border did not change.
		 * @returns true, if we need to search this root move
		 */ 
		bool doSearch(const SearchVariables& variables) const;
	private:
		Move _move;

		value_t _valueOfLastSearch;

		value_t _alphaOfLastSearch;
		value_t _betaOfLastSearch;

		ply_t _pvDepthOfLastSearch;
		ply_t _depthOfLastSearch;

		uint64_t _nodeCountOfLastSearch;
		uint64_t _totalNodeCount;
		uint64_t _totalTableBaseHits;
		uint64_t _totalBitbaseHits;
		uint64_t _timeSpendToSearchMoveInMilliseconds;

		string _pvString;
		PV _pvLine;

		bool _isExcluded;

	};

	class RootMoves {
	public:
		/**
		 * Searches for a move in the root move list
		 * @returns -1, if not found, else the position of the move
		 */
		int32_t findMove(Move move) const;

		/**
		 * Sets all moves
		 */
		void setMoves(MoveGenerator& position);

		/**
		 * Stable sort algorithm sorting all moves from first to last
		 * @param first first move to consider, do not touch moves in front of first
		 */
		void bubbleSort(uint32_t first);
	private:
		vector<RootMove> _moves;
	};


}

#endif // __PV