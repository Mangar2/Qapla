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

#include "perft.h"
#include <thread>
#include <mutex>

using namespace QaplaSearch;
using namespace std;

array<PerftSearch::TTEntry, PerftSearch::TT_SIZE> PerftSearch::tt;

Move SplitPoint::selectNextMove() {
	const lock_guard<mutex> lock(_splitPoint);
	Move result;
	if (_moveList.getTotalMoveAmount() > _index) {
		result = _moveList[_index];
	}
	_index++;
	return result;
}

void SplitPoint::addResult(uint64_t result) {
	const lock_guard<mutex> lock(_splitPoint);
	_movesFound += result;
}

void PerftSearch::perftRecHelper(SplitPoint& splitPoint, WorkPackage &work, bool main) {
	MoveGenerator board = splitPoint.getBoard();
	Move move;
	uint64_t result = 0;
	while(!(move = splitPoint.selectNextMove()).isEmpty()) {
		uint32_t workerCount = threadPool.assignWork(&work, 0);
		// if (splitPoint.getCurDepth() > 1 && workerCount > 0) cout << "depth " << splitPoint.getCurDepth() << endl;
		const BoardState boardState = board.getBoardState();
		board.doMove(move);
		uint64_t movesFound = perftRec(board, splitPoint.getMaxDepth(), splitPoint.getCurDepth() + 1, splitPoint.getScipLastPly());
		result += movesFound;
		board.undoMove(move, boardState);
	}
	// cout << "Thread found: " << result << " nodes" << (main ? " (main thread) " : "") << endl;
	splitPoint.addResult(result);
}

uint64_t PerftSearch::perftRec(
	MoveGenerator& board, uint32_t maxDepth, uint32_t curDepth, bool scipLastPly, bool verbose)
{
	if (curDepth == 0) { threadPool.startExamine(); }
	MoveList moveList;
	uint64_t result = 0;
	if (curDepth == maxDepth) return 1;
	board.genMovesOfMovingColor(moveList);
	if (scipLastPly && curDepth + 1 == maxDepth) {
		return moveList.getTotalMoveAmount();
	}
	bool isSplitPoint = curDepth >= 1 && curDepth < maxDepth - 4;
	// bool isSplitPoint = curDepth == 1;
	if (isSplitPoint) {
		SplitPoint splitPoint;
		splitPoint.set(moveList, board, maxDepth, curDepth, scipLastPly);
		WorkPackage work;
		work.setFunction([this, &splitPoint, &work]() { perftRecHelper(splitPoint, work); });
		perftRecHelper(splitPoint, work, true);
		threadPool.waitForWorkpackage(work);
		result = splitPoint.getMovesFound();
	}
	else {
		for (uint32_t index = 0; index < moveList.getTotalMoveAmount(); ++index) {
			const Move move = moveList[index];
			if (move.isEmpty()) break;
			const BoardState boardState = board.getBoardState();
			board.doMove(move);
			uint64_t movesFound = perftRec(board, maxDepth, curDepth + 1, scipLastPly);
			result += movesFound;
			board.undoMove(move, boardState);
			if (verbose) {
				cout << move.getLAN() << " " << movesFound << endl;
			}
		}
	}
	if (curDepth == 0) { threadPool.stopExamine(); }
	return result;
}

/*
uint64_t PerftSearch::perftRecTT(
	MoveGenerator& board, uint32_t maxDepth, uint32_t curDepth, bool scipLastPly) 
{
	MoveList moveList;
	BoardState boardState;
	if (curDepth == maxDepth) return 1;
	board.genMovesOfMovingColor(moveList);
	if (scipLastPly && curDepth + 1 == maxDepth) {
		return moveList.getTotalMoveAmount();
	}
	bool isSplitPoint = curDepth == 2;
	if (isSplitPoint) {
		_splitPoints[0].set(moveList, board);
	}
	uint64_t result = 0;
	for (uint32_t index = 0; index < moveList.getTotalMoveAmount(); ++index) {
		const Move move = isSplitPoint ? _splitPoints[0].selectNextMove() : moveList[index];
		if (move.isEmpty()) break;
		boardState = board.getBoardState();
		board.doMove(move);
		hash_t boardHash = board.basicBoard.computeBoardHash();

		uint64_t movesFound = getFromTT(boardHash, curDepth);
		if (movesFound == 0) {
			movesFound = perftRec(board, maxDepth, curDepth + 1, scipLastPly);
			setToTT(board.basicBoard.computeBoardHash(), movesFound, curDepth);
		}
		else {
			ttMoves += movesFound;
		}
		result += movesFound;
		board.undoMove(move, boardState);
	}
	if (curDepth == 0) {
		cout << "TTMoves found: " << ttMoves << endl;
	}
	return result;
}
*/