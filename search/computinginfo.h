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

#include <array>
#include "../basics/move.h"
#include "../interface/stdtimecontrol.h"
#include "pv.h"
#include "../interface/isendsearchinfo.h"
#include "../interface/computinginfoexchange.h"
#include "rootmoves.h"
#include "searchparameter.h"
#include "searchstack.h"

using namespace QaplaInterface;

namespace QaplaSearch {
	class ComputingInfo {
	public:
		ComputingInfo() : _sendSearchInfo(0), _multiPV(1) {
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

		uint32_t getMultiPV() const {
			return _multiPV;
		}

		void setMultiPV(uint32_t multiPV) {
			_multiPV = multiPV;
		}


		/**
		 * Initializes the members
		 */
		void clear() {
			_searchDepth = 0;
			_nodesSearched = 0;
			_tbHits = 0;
			_debug = false;
			_totalAmountOfMovesToConcider = 0;
			_currentMoveNoSearched = 0;
			_positionValueInCentiPawn = 0;
			_printRequest = false;
			_timeControl.storeStartTime();
			_lastMultiPVInfo = 0;
		}

		/**
		 * initializes data before starting to search
		 */
		void initNewSearch(MoveGenerator& position, ButterflyBoard butterflyBoard) {
			_rootMoves.setMoves(position, butterflyBoard);
			_nodesSearched = 0;
			_tbHits = 0;
			_timeControl.storeStartTime();
		}

		/**
		 * Starts searching the next iteration
		 */
		void nextIteration(const SearchVariables& searchInfo) {
			_totalAmountOfMovesToConcider = searchInfo.moveProvider.getTotalMoveAmount();
			_currentConcideredMove.setEmpty();
			_currentMoveNoSearched = 0;
			_searchDepth = searchInfo.remainingDepth;
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
					_tbHits,
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
		void printSearchResult(const PV& pv, value_t bestValue, value_t alpha, value_t beta, ply_t depth, uint32_t pvNo = 1) const
		{
			MoveStringList primaryVariant;
			for (uint8_t ply = 0; ply < pv.MAX_PV_LENGTH; ply++) {
				Move move = pv.getMove(ply);
				if (move == Move::EMPTY_MOVE) {
					break;
				}
				primaryVariant.push_back(move.getLAN());
			}
			if (_verbose && _sendSearchInfo != 0) {
				_sendSearchInfo->informAboutFinishedSearchAtCurrentDepth(
					depth,
					bestValue,
					bestValue >= beta,
					bestValue <= alpha,
					_timeControl.getTimeSpentInMilliseconds(),
					_nodesSearched,
					_tbHits,
					primaryVariant,
					pvNo);
			}
		}
		void printSearchResult(uint32_t moveNo, uint32_t multiPVNo = 1) const {
			const auto& rootMove = _rootMoves.getMove(moveNo);
			printSearchResult(rootMove.getPV(), rootMove.getValue(), rootMove.getAlpha(), rootMove.getBeta(), rootMove.getDepth(), multiPVNo);
		}

		/**
		 * Prints all variants in multi-pv
		 */
		void printSearchResult() {
			if (_multiPV == 1) {
				printSearchResult(0);
			} else {
				const auto& timeSinceLastInfo = _timeControl.getTimeSpentInMilliseconds() - _lastMultiPVInfo;
				const auto& pvCount = _rootMoves.countPVSearchedMovesInWindow(_searchDepth);
				if (pvCount >= _multiPV) {
					_lastMultiPVInfo = _timeControl.getTimeSpentInMilliseconds();
					for (uint32_t moveNo = 0; moveNo < _multiPV; moveNo++) {
						printSearchResult(moveNo, moveNo + 1);
					}
				}
			}
		}

		/**
		 * Set verbose mode (prints more info)
		 */
		void setVerbose(bool isVerbose) {
			_verbose = isVerbose;
		}

		/**
		 * Depth of the current search
		 */
		void setSearchDepth(uint32_t searchDepth) {
			_searchDepth = searchDepth;
		}

		uint32_t getSearchDepht() const {
			return _searchDepth;
		}

		/**
		 * Sets the current concidered move
		 */
		void setCurrentMove(Move move) {
			_currentConcideredMove = move;
		}

		/**
		 * Update status information on ply 0
		 */
		void printNewPV(uint32_t moveNo, const SearchVariables& node) {
			_currentMoveNoSearched = moveNo;
			if (moveNo == 0 || node.bestValue > _positionValueInCentiPawn) {
				_positionValueInCentiPawn = node.bestValue;
				if (_multiPV == 1) {
					printSearchResult(moveNo);
				}
			}
		}

		const PV& getPV() const {
			return _rootMoves.getMove(0).getPV();
		}


		/**
		 * Creates an exchange version of the computing info structure
	     */
		ComputingInfoExchange getExchangeStructure() const {
			ComputingInfoExchange exchange;
			const PV& pv = getPV();
			exchange.currentConsideredMove = pv.getMove(0).getLAN();
			exchange.ponderMove = pv.getMove(1).getLAN();
			if (exchange.ponderMove == "empty" || exchange.ponderMove == "null") {
				exchange.ponderMove = "";
			}
			exchange.nodesSearched = _nodesSearched;
			exchange.searchDepth = _searchDepth;
			exchange.elapsedTimeInMilliseconds = _timeControl.getTimeSpentInMilliseconds();
			exchange.totalAmountOfMovesToConcider = _totalAmountOfMovesToConcider;
			exchange.movesLeftToConcider = _totalAmountOfMovesToConcider - _currentMoveNoSearched;
			return exchange;
		}

		/**
		 * Gets the current position value
		 */
		value_t getPositionValueInCentiPawn(uint32_t moveNo) const {
			const auto value = _rootMoves.getMove(moveNo).getValue();
			assert(moveNo > 0 || value == _positionValueInCentiPawn);
			return value;
		}

		RootMoves& getRootMoves() {
			return _rootMoves;
		}

		const RootMoves& getRootMoves() const {
			return _rootMoves;
		}

		uint64_t _nodesSearched;
		uint64_t _tbHits;

	private:
		RootMoves _rootMoves;
		static const uint32_t MAX_PV = 40;
		ISendSearchInfo* _sendSearchInfo;
		StdTimeControl _timeControl;
		int64_t _lastMultiPVInfo;
		value_t _positionValueInCentiPawn;
		uint32_t _totalAmountOfMovesToConcider;
		uint32_t _hashFullInPermill;
		Move _currentConcideredMove;
		uint32_t _currentMoveNoSearched;
		uint32_t _searchDepth;
		volatile bool _printRequest;
		uint32_t _multiPV;
		bool _debug;
		bool _verbose;
	};

}

#endif // __COMPUTINGINFO_H
