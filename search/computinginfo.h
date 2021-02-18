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
 * Class containing a set of information taken from the Chess-Search algorithm
 * - The elapsed time in milliseconds for the current search
 * - The amount of nodes (calls to the move generator) searched
 * - The search depth - the horizont for the search
 * - The amount of moves left in the current search depth to consider
 * - The total amount of moves to consider in the current search depth
 * - The currently concidered move
 */

#ifndef __COMPUTINGINFO_H
#define __COMPUTINGINFO_H

#include "../basics/move.h"
#include "../interface/stdtimecontrol.h"
#include "pv.h"
#include "../interface/isendsearchinfo.h"
//#include "StatisticForMoveOrdering.h"
#include "searchparameter.h"

using namespace ChessInterface;

namespace ChessSearch {
	class ComputingInfo {
	public:
		ComputingInfo() : _sendSearchInfo(0) {
			clear();
		}

		/**
		 * Sets the interface object to get the search - information
		 */
		void setSendSearchInfo(ISendSearchInfo* sendSearchInfo) {
			_sendSearchInfo = sendSearchInfo;
		}

		/**
		 * Sets the hash fill rate in percent
		 */
		void setHashFullInPermill(uint32_t hashFull) {
			_hashFullInPermill = hashFull;
		}

		/**
		 * Initializes the members
		 */
		void clear() {
			_searchDepth = 0;
			_nodesSearched = 0;
			_debug = false;
			_totalAmountOfMovesToConcider = 0;
			_currentMoveNoSearched = 0;
			_positionValueInCentiPawn = 0;
			_printRequest = false;
			_timeControl.storeStartTime();
		}

		/**
		 * initializes data before starting to search
		 */
		void initSearch() {
			_nodesSearched = 0;
			if (SearchParameter::CLEAR_ORDERING_STATISTIC_BEFORE_EACH_MOVE) {
				// statisticForMoveOrdering.clear();
			}
		}

		/**
		 * Starts searching the next iteration
		 */
		void nextIteration(uint32_t movesToConcider) {
			_totalAmountOfMovesToConcider = movesToConcider;
			_currentConcideredMove.setEmpty();
			_currentMoveNoSearched = 0;
		}

		void requestPrintSearchInfo() { _printRequest = true; }

		/**
		 * Prints the current serch information based on a setting, either, if
		 * the print was requested or if the parameter print is true
		 * @print true, if search info sould be printed 
		 */
		void printSearchInfo(bool print) {
			bool doPrint = _printRequest || print;
			if (doPrint && _verbose && _sendSearchInfo != 0) {
				_sendSearchInfo->informAboutAdvancementsInSearch(
					_searchDepth,
					_positionValueInCentiPawn,
					_timeControl.getTimeSpentInMilliseconds(),
					_nodesSearched,
					_totalAmountOfMovesToConcider - _currentMoveNoSearched,
					_totalAmountOfMovesToConcider,
					_currentConcideredMove.getLAN(),
					_hashFullInPermill
				);
				_printRequest = false;
			}
		}

		/**
		 * Prints the information about the result of a search
		 */
		void printSearchResult()
		{
			MoveStringList primaryVariant;
			for (uint8_t ply = 0; ply < _pvMovesStore.MAX_PV_LENGTH; ply++) {
				Move move = _pvMovesStore.getMove(ply);
				if (move == Move::EMPTY_MOVE) {
					break;
				}
				primaryVariant.push_back(move.getLAN());
			}
			if (_verbose && _sendSearchInfo != 0) {
				_sendSearchInfo->informAboutFinishedSearchAtCurrentDepth(
					_searchDepth,
					_positionValueInCentiPawn,
					_positionValueInCentiPawn >= _beta,
					_positionValueInCentiPawn <= _alpha,
					_timeControl.getTimeSpentInMilliseconds(),
					_nodesSearched,
					primaryVariant);
			}
		}

		/** 
		 * Updates the primary variant
		 */
		void updatePV(PV& pv) {
			if (pv != _pvMovesStore && !pv.getMove(0).isEmpty()) {
				_pvMovesStore = pv;
				printSearchResult();
			}
		}

		/**
		 * Set verbose mode (prints more info)
		 */
		void setVerbose(bool isVerbose) {
			_verbose = isVerbose;
		}

		ISendSearchInfo* _sendSearchInfo;
		StdTimeControl _timeControl;
		value_t _positionValueInCentiPawn;
		value_t _alpha;
		value_t _beta;
		uint32_t _searchDepth;
		uint32_t _totalAmountOfMovesToConcider;
		uint32_t _hashFullInPermill;
		Move _currentConcideredMove;
		uint32_t _currentMoveNoSearched;
		uint64_t _nodesSearched;
		// StatisticForMoveOrdering statisticForMoveOrdering;

		volatile bool _printRequest;
		PV _pvMovesStore;
		bool _debug;
		bool _verbose;
	};

}

#endif // __COMPUTINGINFO_H