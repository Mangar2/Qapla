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

using namespace ChessSearch;

void RootMove::init() {
	_valueOfLastSearch = -MAX_VALUE;

	_alphaOfLastSearch = -MAX_VALUE;
	_betaOfLastSearch = MAX_VALUE;

	_pvDepthOfLastSearch = 0;
	_depthOfLastSearch = 0;

	_nodeCountOfLastSearch = 0;
	_totalNodeCount = 0;
	_totalTableBaseHits = 0;
	_totalBitbaseHits = 0;
	_timeSpendToSearchMoveInMilliseconds = 0;

	_pvString = "";
	_isExcluded = false;
}

void RootMove::set(const SearchVariables& variables, uint64_t totalNodes, uint64_t tableBaseHits,
	uint64_t timeSpentInMilliseconds, const string& pvString, const PV& pvLine)
{
	_valueOfLastSearch = variables.bestValue;
	_alphaOfLastSearch = variables.alpha;
	_betaOfLastSearch = variables.beta;
	// _pvDepthOfLastSearch = variables.isInPV()
	_depthOfLastSearch = variables.remainingDepth;
}

bool RootMove::doSearch(const SearchVariables& variables) const {
	if (_isExcluded) {
		return false;
	}
	if (_depthOfLastSearch < variables.remainingDepthAtPlyStart) {
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

bool RootMove::operator<(const RootMove& rootMove) const {
}

bool RootMove::operator>(const RootMove& rootMove) const {

}

int32_t RootMoves::findMove(Move move) const {
	int32_t result = -1;
	for (int32_t i = 0; i < _moves.size(); ++i) {
		if (_moves[i].getMove() == move) {
			result = i;
			break;
		}
	}
	return result;
}

void RootMoves::setMoves(MoveGenerator& position) {
	MoveProvider moveProvider;
	moveProvider.computeMoves(position, Move::EMPTY_MOVE);
	_moves.clear();
	Move move;
	while (!(move = moveProvider.selectNextMove(position)).isEmpty()) {
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
			if (_moves[j] > _moves[j-1])
			{
				std::swap(_moves[j], _moves[j - 1]);
			}
		}
	}
}

