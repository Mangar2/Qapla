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
 */

#include "rootmoves.h"
#include "moveprovider.h"

using namespace QaplaSearch;

void RootMove::init() {
	_valueOfLastSearch = -MAX_VALUE;

	_alphaOfLastSearch = -MAX_VALUE;
	_betaOfLastSearch = MAX_VALUE;
	_depthOfLastSearch = 0;
	_isPVSearched = false;

	_nodeCountOfLastSearch = 0;
	_totalNodeCount = 0;
	_totalTableBaseHits = 0;
	_totalBitbaseHits = 0;
	_timeSpendToSearchMoveInMilliseconds = 0;
	
	_pvString = "";
	_isExcluded = false;
}

void RootMove::set(value_t searchResult, const SearchStack& stack, bool isPVSearched)
{
	_valueOfLastSearch = searchResult;
	_alphaOfLastSearch = stack[0].alpha;
	_betaOfLastSearch = stack[0].beta;
	_isPVSearched = isPVSearched;
	_depthOfLastSearch = stack[0].remainingDepth;
	_pvLine.setMove(0, Move::EMPTY_MOVE);
	if (_isPVSearched) {
		// We cannot directly set stack[0].pvMovesStore, because stack[0] is not yet updated
		_pvLine.setMove(0, _move);
		_pvLine.copyFromPV(stack[1].pvMovesStore, 1);
	}
}

bool RootMove::doSearch(const SearchVariables& variables) const {
	if (_isExcluded) {
		return false;
	}
	if (_depthOfLastSearch < variables.getRemainingDepth()) {
		return true;
	}
	// If evaluated value was outside the window and is now inside the window, we need to search again
	if (_valueOfLastSearch >= _betaOfLastSearch && variables.beta > _betaOfLastSearch) {
		return true;
	}
	if (_valueOfLastSearch <= _alphaOfLastSearch && variables.alpha < _alphaOfLastSearch) {
		return true;
	}
	return false;
}

bool RootMove::operator<(const RootMove& other) const {
	if (_depthOfLastSearch != other._depthOfLastSearch) {
		return _depthOfLastSearch < other._depthOfLastSearch;
	}
	if (!other.isPVSearched()) return false;
	if (!isPVSearched()) return true;

	if (other.isFailLow()) return false;
	if (isFailLow()) return true;

	return _valueOfLastSearch < other._valueOfLastSearch;
}

void RootMove::print() const {
	cout << _move.getLAN()
		<< " [v:" << std::right << std::setw(5) << _valueOfLastSearch << "]"
		<< " [d:" << std::right << std::setw(2) << _depthOfLastSearch << "]"
		<< " [" << std::right << std::setw(5) << _alphaOfLastSearch << ", " 
		<< std::right << std::setw(5) << _betaOfLastSearch << "]"
		<< (isPVSearched() ? (" " + _pvLine.toString()) : "")
		<< endl;
}

RootMove& RootMoves::findMove(Move move) {
	int32_t result = -1;
	for (int32_t i = 0; i < _moves.size(); ++i) {
		if (_moves[i].getMove() == move) {
			result = i;
			break;
		}
	}
	return _moves[result];
}

void RootMoves::setMoves(MoveGenerator& position, const std::vector<Move>& searchMoves, ButterflyBoard& butterflyBoard) {
	MoveProvider moveProvider;
	position.computeAttackMasksForBothColors();
	moveProvider.computeMoves(position, butterflyBoard, Move::EMPTY_MOVE, Move::EMPTY_MOVE);
	_moves.clear();
	Move move;
	while (!(move = moveProvider.selectNextMove(position)).isEmpty()) {
		if (!searchMoves.empty() && std::find(searchMoves.begin(), searchMoves.end(), move) == searchMoves.end()) {
			continue;
		}
		RootMove rootMove;
		rootMove.setMove(move);
		_moves.push_back(rootMove);
	}
}

void RootMoves::bubbleSort(uint32_t first) {
	uint32_t last = uint32_t(_moves.size()) - 1;

	for (uint32_t i = first; i < last; ++i)
	{
		for (uint32_t j = last; j > i; --j)
		{
			if (_moves[j - 1] < _moves[j])
			{
				std::swap(_moves[j], _moves[j - 1]);
			}
		}
	}
}

void RootMoves::print() {
	for (auto& move : _moves) {
		move.print();
	}
}

