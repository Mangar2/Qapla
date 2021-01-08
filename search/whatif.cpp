#include "computinginfo.h"
#include "../eval/eval.h"
#include "SearchStack.h"
#include "tt.h"
#include "whatif.h"
#include "boardadapter.h"

using namespace ChessSearch;

WhatIf WhatIf::whatIf;

WhatIf::WhatIf() : maxPly(0), searchDepth(0), count(0) {
	clear();
}

void WhatIf::init(const Board& board, const ComputingInfo& computingInfo, value_t alpha, value_t beta) {
	if (computingInfo.searchDepth == searchDepth) {
		printf("New search [w:%6ld,%6ld]\n", beta, alpha);
	}
	hashFoundPly = -1;
}

void WhatIf::printInfo(const Board& board, const ComputingInfo& computingInfo, const SearchStack& stack, Move currentMove, ply_t ply) {
	printMoves(stack, currentMove, ply);
	printf("[w:%6ld,%6ld]", stack[ply].beta, stack[ply].alpha);
	printf("[d:%ld]", stack[ply].remainingDepth);
	if (stack[ply].remainingDepth > 0) {
		if (stack[ply + 1].searchState != SearchVariables::SearchType::START) {
			printf("[v:%6ld]", -stack[ply + 1].bestValue);
			printf("[hm:%5s]", stack[ply + 1].getTTMove().getLAN().c_str());
		}
		else if (stack[ply + 1].cutoff == Cutoff::HASH) {
			printf("[v:%6ld]", -stack[ply + 1].bestValue);
			printf("[c:  HASH]");
		}
		else {
			printf("[v:%6s]", "-");
			printf("[hm:%5s]", "-");
		}
	}
	printf("[bm:%5s]", stack[ply].bestMove.getLAN().c_str());
	printf("[st:%8s]", stack[ply].getSearchStateName().c_str());
	printf("[n:%10lld]", computingInfo.nodesSearched);
	if (stack[ply].searchState == SearchVariables::SearchType::PV) {
		printf(" [PV:");
		stack[ply].pvMovesStore.print(ply);
		printf(" ]");
	}
	printf("\n");
}

void WhatIf::moveSelected(const Board& board, const ComputingInfo& computingInfo, Move currentMove, ply_t ply, bool inQsearch) {
	if (ply <= hashFoundPly) {
		hashFoundPly = -1;
	}
	if (computingInfo.searchDepth == searchDepth && board.computeBoardHash() == hash && ply <= amountOfMovesToSearch + 1) {
		hashFoundPly = ply;
		qsearch = inQsearch;
	}
}

void WhatIf::moveSelected(const Board& board, const ComputingInfo& computingInfo, const SearchStack& stack, Move currentMove, ply_t ply) {
	if (searchDepth == -1) {
		return;
	}
	moveSelected(board, computingInfo, currentMove, ply, false);
	if (hashFoundPly != -1) {
		moveSearched(board, computingInfo, stack, currentMove, ply - 1);
	}
}

void WhatIf::startSearch(const Board& board, const ComputingInfo& computingInfo, const SearchStack& stack, ply_t ply) {
	if (searchDepth == -1) {
		return;
	}
	moveSelected(board, computingInfo, Move::EMPTY_MOVE, ply, false);
}


void WhatIf::moveSearched(const Board& board, const ComputingInfo& computingInfo, const SearchStack& stack, Move currentMove, ply_t ply) {
	if (searchDepth == -1 || ply < 0) {
		return;
	}
	if (hashFoundPly != -1 && ply >= hashFoundPly && ply <= hashFoundPly) {
		printInfo(board, computingInfo, stack, currentMove, ply);
		count++;
	}
}

void WhatIf::moveSearched(const Board& board, const ComputingInfo& computingInfo, Move currentMove, value_t alpha, value_t beta, value_t bestValue, ply_t ply) {
	if (hashFoundPly != -1 && qsearch) {
		for (ply_t i = 0; i <= ply; i++) {
			printf(".");
		}
		printf("%s [w:%6ld,%6ld][v:%6ld]\n", currentMove.getLAN().c_str(), beta, alpha, bestValue);
	}
}

void WhatIf::cutoff(const Board& board, const ComputingInfo& computingInfo, const SearchStack& stack, ply_t ply, const char* cutoffType) {
	if (searchDepth == -1 || ply < 0) {
		return;
	}
	if (stack[0].remainingDepth == searchDepth && hashFoundPly != -1 && ply >= hashFoundPly - 1 && ply <= hashFoundPly) {
		printMoves(stack, stack[ply].previousMove, ply - 1);
		printf("[w:%6ld,%6ld]", stack[ply].beta, stack[ply].alpha);
		printf("[d:%ld]", stack[ply].remainingDepth);
		printf("[v:%6ld]", stack[ply].bestValue);
		printf("[hm:%5s]", stack[ply].getTTMove().getLAN().c_str());
		printf("[c:%s]", cutoffType);
		printf("[n:%lld]", computingInfo.nodesSearched);
		printf("\n");
		count++;
	}
}

void WhatIf::setTT(TT* ttPtr, uint64_t hashKey, ply_t depth, ply_t ply, Move move, value_t bestValue, value_t alpha, value_t beta, bool nullMoveTrhead) {
	if (hashKey == hash) {
		auto ttIndex = ttPtr->getTTEntryIndex(hashKey);
		if (ttPtr->isNewEntryMoreValuable(ttIndex, depth, move)) {
			printf("set hash [w%6ld %6ld][d:%2ld][v:%6ld][m:%5s]\n", beta, alpha, depth, bestValue, move.getLAN().c_str());
			ttPtr->printHash(hashKey);
		}
	}
}

void WhatIf::clear() {
	searchDepth = -1;
	for (ply_t ply = 0; ply < MAX_PLY; ply++) {
		movesToSearch[ply] = Move::EMPTY_MOVE;
	}
	amountOfMovesToSearch = 0;
}

void WhatIf::setWhatIfMoves(MoveGenerator& board, ply_t ply) {
	for (ply_t index = 0; index < ply && movesToSearch[index] != Move::EMPTY_MOVE; index++) {
		if (movesToSearch[index] == Move::NULL_MOVE) {
			board.doNullmove();
		}
		else {
			board.doMove(movesToSearch[index]);
		}
	}
}

void WhatIf::setMove(ply_t ply, char movingPiece, 	uint32_t departureFile, uint32_t departureRank, 
	uint32_t destinationFile, uint32_t destinationRank, char promotePiece) {
	if (ply < MAX_PLY) {
		MoveGenerator curBoard = board;
		setWhatIfMoves(curBoard, ply);

		Move move = BoardAdapter::findMove(
			curBoard, movingPiece, departureFile, departureRank, destinationFile, destinationRank, promotePiece);
		if (move != Move::EMPTY_MOVE) {
			setMove(curBoard, ply, move);
		}
	}
}

void WhatIf::setBoard(MoveGenerator& newBoard) {
	board = newBoard;
	hash = board.computeBoardHash();
}

void WhatIf::setMove(MoveGenerator& board, uint32_t moveNo, Move move) {
	if (moveNo < MAX_PLY) {
		movesToSearch[moveNo] = move;
		amountOfMovesToSearch = moveNo;
		if (move == Move::NULL_MOVE) {
			board.doNullmove();
		}
		else {
			board.doMove(move);
		}
		hash = board.computeBoardHash();
		ChessEval::Eval eval;
		eval.printEval(board);
	}
}

void WhatIf::setNullmove(ply_t ply) {
	MoveGenerator curBoard = board;
	setWhatIfMoves(curBoard, ply);
	setMove(curBoard, ply, Move::NULL_MOVE);
}

void WhatIf::printMoves(const SearchStack& stack, Move currentMove, ply_t ply) {
	for (ply_t index = 1; index <= ply + 1; index++) {
		if ((index - 1) % 2 == 0) {
			printf("%ld. ", (index / 2 + 1));
		}
		if (index <= ply) {
			printf("%s ", stack[index].previousMove.getLAN().c_str());
		}
		else if (currentMove != Move::EMPTY_MOVE) {
			printf("%s ", currentMove.getLAN().c_str());
		}
	}
}

