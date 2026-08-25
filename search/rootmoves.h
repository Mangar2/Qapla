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
 * @copyright Copyright (c) 2025 Volker Böhm
 * @Overview
 * Class holding moves played at the root with multi-pv support
 */

#pragma once

#include <vector>
#include <string>
#include "searchdef.h"
#include "../basics/move.h"
#include "../basics/evalvalue.h"
#include "../movegenerator/movegenerator.h"
#include "../src/syzygy/tablebase.h"
#include "pv.h"
#include "searchstack.h"
#include "tt.h"

using namespace std;
using namespace QaplaBasics;

namespace QaplaSearch {

	class RootMove
	{
	public:
		RootMove() { init(); }

		/**
		 * A root move is smaller than another if:
		 * - both have tablebase info and this one sits in the worse bucket (win > cursed win >
		 *   draw > blessed loss > loss) - checked first, unconditionally, never overridden by
		 *   depth or search value
		 * - otherwise, if a position has repeated since the last zeroing move and both have
		 *   tablebase info, the one with the longer distance to zero - also unconditionally,
		 *   because only a shorter distance ends the repetition
		 * - otherwise, if it was searched to a lesser depth
		 * - otherwise, if neither move has been searched yet this run and both have tablebase
		 *   info, the one with the longer distance to zero
		 * - otherwise, if it is searched with "PV" and the other is not
		 * - if both are searched with PV: if it failed low and the other did not
		 * - if both are searched with PV and not failed low: the one with the least value
		 */
		bool operator<(const RootMove& rootMove) const;

		// Initializes all members
		void init();

		/**
		 * Chess move
		 */
		void setMove(Move move) { _move = move; }
		Move getMove() const { return _move; }

		/**
		 * Gets the pv of the move
		 */
		const PV& getPV() const {
			return _pvLine;
		}

		/**
		 * @returns true, if the search was a PV search
		 */
		bool isPVSearched() const { return _isPVSearched; }

		bool isFailLow() const { return _valueOfLastSearch <= _alphaOfLastSearch; }
		bool isFailHigh() const { return _valueOfLastSearch >= _betaOfLastSearch; }

		bool isPVSearchedInWindow(ply_t depth) const { 
			return isPVSearched(depth) && !isFailLow() && !isFailHigh();
		}

		bool isPVSearched(ply_t depth) const { return _isPVSearched && (_depthOfLastSearch >= depth); }

		/**
		 * Sets results after the search of a move is finished
		 */
		void set(value_t searchResult, const SearchStack& stack, bool isPVSearched);

		/**
 		 * Checks, if we need to research this root move.
		 * We do not need to search a move after an intial search window has been changed and 
		 * we can be sure, that the current move result was already inside the last window or 
		 * outside the window and the corresponding window border did not change.
		 * @returns true, if we need to search this root move
		 */ 
		bool doSearch(const SearchNode& variables) const;

		/**
		 * Print the move (used for debugging)
	     */
		void print() const;

		value_t getValue() const { return _valueOfLastSearch; }

		/**
		 * The value to report for this move. Where the tablebases classify it as a win, theirs is
		 * reported instead of the search value: a root win switches the tablebase probing inside
		 * the search off, so the search value says nothing about the win it is converting.
		 *
		 * A mate the search found is kept, it says more than the tables do. So does every value
		 * of a move the tables do not call won.
		 */
		value_t getReportedValue() const;
		ply_t getDepth() const { return _depthOfLastSearch;  }
		value_t getAlpha() const { return _alphaOfLastSearch;  }
		value_t getBeta() const { return _betaOfLastSearch; }

		/**
		 * Classifies this move's tablebase outcome, played from `rootPosition`, from the root
		 * mover's perspective: which of the five win/draw/loss buckets it lands in
		 * (QaplaSyzygy::Wdl), and how many plies until the next zeroing move or mate. `cnt50` is
		 * the halfmove counter already active at the root, used to demote a nominal win to a
		 * cursed win when it cannot actually be forced under the fifty-move rule from here.
		 *
		 * Leaves the move without tablebase info and returns false if any table needed along the
		 * way is missing - the caller must then not trust tablebase info on any root move, since a
		 * missing answer must never be read as "not winning".
		 */
		bool computeTablebaseInfo(QaplaMoveGenerator::MoveGenerator& rootPosition, uint32_t cnt50,
			bool distanceDecides);

		bool hasTbInfo() const { return _hasTbInfo; }
		bool hasTbDistance() const { return _tbDistanceKnown; }
		bool tbDistanceDecides() const { return _tbDistanceDecides; }
		QaplaSyzygy::Wdl getTbWdl() const { return _tbWdl; }
		int32_t getTbDtz() const { return _tbDtz; }
		void clearTbInfo() { _hasTbInfo = false; }

	private:
		Move _move;

		value_t _valueOfLastSearch;

		value_t _alphaOfLastSearch;
		value_t _betaOfLastSearch;

		bool _isPVSearched;
		ply_t _depthOfLastSearch;

		uint64_t _nodeCountOfLastSearch;
		uint64_t _totalNodeCount;
		uint64_t _totalTableBaseHits;
		uint64_t _totalBitbaseHits;
		uint64_t _timeSpendToSearchMoveInMilliseconds;

		string _pvString;
		PV _pvLine;

		bool _isExcluded;

		bool _hasTbInfo;
		QaplaSyzygy::Wdl _tbWdl;
		int32_t _tbDtz;
		// False where no distance file answered and the bucket comes from the win/draw/loss
		// tables alone - then _tbDtz means nothing and no distance comparison may use it.
		bool _tbDistanceKnown;
		// A position has repeated since the last zeroing move: the game has stopped making
		// progress, so the distance decides among equally classified moves instead of the search.
		bool _tbDistanceDecides;

	};

	class RootMoves {
	public:
		RootMoves() { clear(); }
		/**
		 * Searches for a move in the root move list
		 * @returns -1, if not found, else the position of the move
		 */
		RootMove& findMove(Move move);

		/**
		 * Sets the moves to be searched
		 * @param position the current position
		 * @param searchMoves the moves to be searched, if empty, all moves are searched
		 * @param butterflyBoard the butterfly board
		 */
		void setMoves(MoveGenerator& position, const std::vector<Move>& searchMoves, ButterflyBoard& butterflyBoard);

		/**
		 * Classifies every move already in the list against the tablebases, if `position` is
		 * probeable, and sorts the list so the tablebase bucket becomes the primary key (see
		 * RootMove::operator<). Where no distance file answers, the win/draw/loss tables alone
		 * place the move. Only a move neither of them answers fails, and then no move keeps
		 * tablebase info and the order is left exactly as it was - a missing answer must never be
		 * read as "not winning".
		 *
		 * Knows nothing about MultiPV; how many of the classified moves actually get searched is a
		 * Search-level decision, see getTablebaseBestBucketCount().
		 *
		 * @param hasRepeatedPosition a position has repeated since the last zeroing move; together
		 *        with a halfmove counter close to the fifty move draw this makes the distance the
		 *        criterion instead of a tie-break
		 * @returns true if at least one move classified as a genuine, still-forceable win
		 */
		bool computeTablebaseInfo(MoveGenerator& position, bool hasRepeatedPosition);

		/**
		 * Amount of moves at the front of the (already sorted) list that classify as a genuine
		 * win. Zero when computeTablebaseInfo has not run or found no win. Search combines this
		 * with MultiPV to decide how many moves actually need a real search call.
		 */
		uint32_t getTablebaseWinCount() const {
			uint32_t count = 0;
			while (count < _moves.size() && _moves[count].hasTbInfo()
				&& _moves[count].getTbWdl() == QaplaSyzygy::Wdl::Win
				&& _moves[count].hasTbDistance()) {
				++count;
			}
			return count;
		}

		/**
		 * Amount of moves at the front of the (already sorted) list sharing the best bucket, zero
		 * when no move carries tablebase information. Restricting the search to them loses nothing:
		 * the worst outcome of a better bucket is the best outcome of a worse one, whatever the
		 * halfmove counter is.
		 */
		uint32_t getTablebaseBestBucketCount() const {
			if (_moves.empty() || !_moves[0].hasTbInfo()) return 0;
			const QaplaSyzygy::Wdl best = _moves[0].getTbWdl();
			// Only the win bucket splits further: a proven win beats an unproven one. In every
			// other bucket a distance says nothing about the outcome, and for a loss the move
			// without one may even hold out longer.
			const bool splitByDistance = best == QaplaSyzygy::Wdl::Win && _moves[0].hasTbDistance();
			// Where the distance decides, only the moves sharing the shortest one are equivalent -
			// the others still win, but no longer within what is left of the fifty move budget.
			const bool splitByExactDistance = splitByDistance && _moves[0].tbDistanceDecides();
			const int32_t bestDtz = _moves[0].getTbDtz();
			uint32_t count = 0;
			while (count < _moves.size() && _moves[count].hasTbInfo()
				&& _moves[count].getTbWdl() == best
				&& (!splitByDistance || _moves[count].hasTbDistance())
				&& (!splitByExactDistance || _moves[count].getTbDtz() == bestDtz)) {
				++count;
			}
			return count;
		}

		/**
		 * Amount of moves carrying tablebase information - the positions the classification
		 * actually resolved from the tables, which is what the tbhits output counts.
		 */
		uint32_t getTablebaseInfoCount() const {
			uint32_t count = 0;
			for (const RootMove& move : _moves) {
				if (move.hasTbInfo()) ++count;
			}
			return count;
		}

		/**
		 * Stable sort algorithm sorting all moves from first to last
		 * @param first first move to consider, do not touch moves in front of first
		 */
		void bubbleSort(uint32_t first);

		/**
		 * Gets an iterator to iterate through the moves
		 */
		vector<RootMove>& getMoves() { return _moves; }
		const vector<RootMove>& getMoves() const { return _moves; }

		/**
		 * Gets a reference to the move
		 */
		RootMove& getMove(size_t index) { return _moves[index]; }
		const RootMove& getMove(size_t index) const { return _moves[index]; }

		/**
		 * Returns the amount of moves with full pv search of current depth and a value in the search window 
		 * at the start of the move list. 
		 */
		uint32_t countPVSearchedMovesInWindow(ply_t depth) const {
			uint32_t count = 0;
			for (const RootMove& rootMove : _moves) {
				if (rootMove.isPVSearchedInWindow(depth)) {
					count++;
				} else {
					break;
				}
			}
			return count;
		}

		/**
		 * Print the root moves (used for debugging)
		 */
		void print();

		void clear() { _moves.clear(); }

	private:
		vector<RootMove> _moves;
	};


}
