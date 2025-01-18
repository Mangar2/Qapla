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
 * Implements several perft algorithms (iterative, recursive, using transposition tables, ...)
 */

#ifndef __PERFT_H
#define __PERFT_H

#include "../movegenerator/movegenerator.h"
#include <iostream>
#include <array>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "threadpool.h"

using namespace std;
using namespace QaplaMoveGenerator;

namespace QaplaSearch {

	struct Stack {
	public:
		Stack() {
			moveNo = 0;
			curDepth = 0;
		}

		void doMove(MoveGenerator& board) {
			boardState = board.getBoardState();
			board.doMove(moveList[moveNo]);
		}

		void undoMoveAndSetToNextMove(MoveGenerator& board) {
			board.undoMove(moveList[moveNo], boardState);
			moveNo++;
		}

		bool isMoveAvailable() {
			return moveList.isMoveAvailable(moveNo);
		}

		void genMoves(MoveGenerator& board) {
			board.genMovesOfMovingColor(moveList);
			moveNo = 0;
		}

		MoveList moveList;
		BoardState boardState;
		uint16_t moveNo;
		uint8_t curDepth;
	};

	static void printPerftInfo(Stack& stack, uint64_t amount) {
		Move move = stack.moveList[stack.moveNo];
		cout << move.getLAN() << " " << amount << endl;
	}

	class SplitPoint {
	public:
		SplitPoint() 
			: isActive(false), _index(0), _movesFound(0), _maxDepth(0), _curDepth(0), _scipLastPly(false)
		{}

		void set(const MoveList& moveList, const MoveGenerator& board, uint32_t maxDepth, uint32_t curDepth, bool scipLastPly) {
			_moveList = moveList;
			_board = board;
			_maxDepth = maxDepth;
			_curDepth = curDepth;
			_scipLastPly = scipLastPly;
			_index = 0;
			_movesFound = 0;
			isActive = true;
		}
		void addResult(uint64_t result);
		const MoveGenerator& getBoard() const { return _board; }
		uint32_t getMaxDepth() const { return _maxDepth; }
		uint32_t getCurDepth() const { return _curDepth; }
		bool getScipLastPly() const { return _scipLastPly; }
		uint64_t getMovesFound() const { return _movesFound; }

		Move selectNextMove();
	private:
		bool isActive;
		MoveGenerator _board;
		MoveList _moveList;
		uint32_t _index;
		uint64_t _movesFound;
		uint32_t _maxDepth;
		uint32_t _curDepth;
		bool _scipLastPly;
		mutex _splitPoint;
		condition_variable _finished;
	};

	class PerftSearch {
	public:
		PerftSearch(uint32_t workerCount = 0) {
			tt.fill({ 0, 0, 0 });
			ttMoves = 0;
			threadPool.startWorker(workerCount, workerCount);
			// this_thread::sleep_for(chrono::milliseconds(10));
			// , [this]() { this->perftRecHelper(0); }
		}
		uint64_t perftRec(MoveGenerator& board, uint32_t maxDepth, uint32_t curDepth, 
			bool scipLastPly, bool verbose = false);
		void perftRecHelper(SplitPoint& splitPoint, WorkPackage& work, bool main = false);
		ThreadPool<64> threadPool;
	private:
		uint64_t ttMoves;
		auto getFromTT(hash_t boardHash, uint32_t curDepth) {
			uint64_t result = 0;
			hash_t index = boardHash % TT_SIZE;
			if (tt[index].boardHash == boardHash && curDepth == tt[index].depth) {
				result = tt[index].movesFound;
			}
			return result;
		}
		void setToTT(hash_t boardHash, uint64_t movesFound, uint32_t curDepth) {
			hash_t index = boardHash % TT_SIZE;
			if (curDepth >= 2 && tt[index].movesFound < movesFound) {
				tt[index].movesFound = movesFound;
				tt[index].boardHash = boardHash;
				tt[index].depth = curDepth;
			}
		}
		static const hash_t TT_SIZE = 5;
		struct TTEntry {
			uint32_t depth; 
			uint64_t movesFound;
			hash_t boardHash;
		};
		static array<TTEntry, TT_SIZE> tt;
	};

	static uint64_t doPerftRec(MoveGenerator& board, uint32_t maxDepth, 
		uint32_t workerCount,  bool scipLastPly = true, bool verbose = false) {
		board.computeAttackMasksForBothColors();
		PerftSearch search(workerCount);
		return search.perftRec(board, maxDepth, 0, scipLastPly, verbose);
	}

#pragma warning(suppress: 6262)
	static uint64_t doPerftIter(MoveGenerator& board, uint32_t depth, uint32_t verbose = 1) {

		const uint32_t MAX_DEPTH = 32;
		uint32_t curDepth = 0;
		uint64_t res = 0;
		uint64_t last = 0;
		Stack stack[MAX_DEPTH];

		if (depth == 0) {
			return 1;
		}

		board.computeAttackMasksForBothColors();
		stack[0].genMoves(board);

		while (true) {
			if (stack[curDepth].isMoveAvailable()) {
				if (curDepth + 1 < depth) {
					stack[curDepth].doMove(board);
					curDepth++;
					stack[curDepth].genMoves(board);
				}
				else {
					if (curDepth >= verbose) {
						res += stack[curDepth].moveList.totalMoveAmount;
						stack[curDepth].moveNo = stack[curDepth].moveList.totalMoveAmount;
					}
					else {
						stack[curDepth].doMove(board);
						printPerftInfo(stack[curDepth], 1);
						res++;
						stack[curDepth].undoMoveAndSetToNextMove(board);
					}
				}
			}
			else if (curDepth == 0) {
				break;
			}
			else {
				curDepth--;
				if (curDepth < verbose) {
					printPerftInfo(stack[curDepth], res - last);
					last = res;
				}
				stack[curDepth].undoMoveAndSetToNextMove(board);
			}
		}
		return res;
	}


}

#endif // __PERFT_H