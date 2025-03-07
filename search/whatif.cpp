#include "computinginfo.h"
#include "../eval/eval.h"
#include "searchstack.h"
#include "tt.h"
#include "whatIf.h"
#include "boardadapter.h"

using namespace QaplaSearch;

WhatIf WhatIf::whatIf;

#if (DOWHATIF == true)

WhatIf::WhatIf() : maxPly(0), searchDepth(0), count(0)
{
	clear();
}

void WhatIf::init(const Board &board, const ComputingInfo &computingInfo, value_t alpha, value_t beta)
{
	if (computingInfo.getSearchDepht() == searchDepth)
	{
		std::cout << "New search [w:" << std::setw(6) << beta << "," << std::setw(6) << alpha << "]\n";
	}
	hashFoundPly = -1;
}

void WhatIf::printInfo(const Board &board, const ComputingInfo &computingInfo, const SearchStack &stack,
					   Move currentMove, ply_t depth, ply_t ply, value_t result)
{
	printMoves(stack, currentMove, ply);
	WhatIfVariables variables(computingInfo, stack, currentMove, depth, ply, result, "");
	variables.printAll();
}

void WhatIf::printInfo(const WhatIfVariables &wiVariables)
{
	wiVariables.printMoves();
	wiVariables.printAll();
}

void WhatIf::moveSelected(const Board &board, const ComputingInfo &computingInfo, Move currentMove, ply_t ply, bool inQsearch)
{
	if (ply <= hashFoundPly)
	{
		hashFoundPly = -1;
	}
	if (computingInfo.getSearchDepht() == searchDepth && board.computeBoardHash() == hash && ply <= amountOfMovesToSearch + 1)
	{
		hashFoundPly = ply;
		qsearch = inQsearch;
	}
}

void WhatIf::moveSelected(const Board &board, const ComputingInfo &computingInfo, const SearchStack &stack, Move currentMove, ply_t ply)
{
	if (searchDepth == -1)
	{
		return;
	}
	moveSelected(board, computingInfo, currentMove, ply, false);
	if ((ply - 1) == hashFoundPly && -1 != hashFoundPly)
	{
		WhatIfVariables variables(computingInfo, stack, currentMove, stack[ply].getRemainingDepth(), ply - 1, NO_VALUE, "");
		variables.printSelected();
	}
}

void WhatIf::startSearch(const Board &board, const ComputingInfo &computingInfo, const SearchStack &stack, ply_t ply)
{
	if (searchDepth == -1)
	{
		return;
	}
	moveSelected(board, computingInfo, Move::EMPTY_MOVE, ply, false);
}

void WhatIf::moveSearched(const Board &board, const ComputingInfo &computingInfo, const SearchStack &stack,
						  Move currentMove, ply_t depth, ply_t ply, value_t curValue, const string searchType)
{
	if (searchDepth == -1 || ply < 0)
	{
		return;
	}
	if (ply == hashFoundPly)
	{
		WhatIfVariables variables(computingInfo, stack, currentMove, depth, ply, curValue, searchType);
		printInfo(variables);
		count++;
	}
}

void WhatIf::moveSearched(const Board &board, const ComputingInfo &computingInfo, Move currentMove, value_t alpha, value_t beta, value_t bestValue, value_t standPatValue, ply_t ply)
{
	if (hashFoundPly != -1 && qsearch)
	{
		for (ply_t i = 0; i <= ply; i++)
		{
			std::cout << ".";
		}
	}
	std::cout << currentMove.getLAN()
			  << " [w:" << std::setw(6) << beta << ","
			  << std::setw(6) << alpha
			  << "][v:" << std::setw(6) << bestValue
			  << "][eval:" << std::setw(6) << standPatValue
			  << "][hash:" << std::setw(16) << board.computeBoardHash() << "]" << std::endl;
}

/**
 * Gets a short string representation of cutoff values for whatif
 */
string getCutoffString(Cutoff cutoff)
{
	return array<string, int(Cutoff::COUNT)>{"NONE", "REPT", "HASH", "MATE", "RAZO", "NEM", "NULL", "FUTILITY"}[int(cutoff)];
}

void WhatIf::cutoff(const Board &board, const ComputingInfo &computingInfo, const SearchStack &stack, ply_t ply, Cutoff cutoff)
{
	if (cutoff == Cutoff::NONE)
	{
		return;
	}
	if (searchDepth == -1 || ply < 0)
	{
		return;
	}
	if (stack[0].remainingDepth == searchDepth && hashFoundPly != -1 && ply >= hashFoundPly - 1 && ply <= hashFoundPly)
	{
		printMoves(stack, stack[ply].previousMove, ply - 1);
		std::cout << "[w:" << std::setw(6) << stack[ply].alpha << ","
				  << std::setw(6) << stack[ply].beta << "]"
				  << "[d:" << stack[ply].remainingDepth << "]"
				  << "[v:" << std::setw(6) << stack[ply].bestValue << "]"
				  << "[hm:" << std::setw(5) << stack[ply].getTTMove().getLAN() << "]"
				  << "[c:" << getCutoffString(cutoff) << "]"
				  << "[n:" << computingInfo._nodesSearched << "]"
				  << std::endl;

		count++;
	}
}

void WhatIf::setTT(TT *ttPtr, uint64_t hashKey, ply_t depth, ply_t ply, Move move, value_t bestValue, value_t alpha, value_t beta, bool nullMoveTrhead)
{
	if (hashKey == hash)
	{
		auto ttIndex = ttPtr->getTTEntryIndex(hashKey);
		if (ttPtr->isNewEntryMoreValuable(ttIndex, depth, move, true))
		{
			std::cout << "set hash [w" << std::setw(6) << alpha << " "
					  << std::setw(6) << beta << "][d:" << std::setw(2) << depth << "]"
					  << "[v:" << std::setw(6) << bestValue << "]"
					  << "[m:" << std::setw(5) << move.getLAN() << "]"
					  << std::endl;

			ttPtr->printHash(hashKey);
		}
	}
}

void WhatIf::clear()
{
	searchDepth = -1;
	for (auto &move : movesToSearch)
	{
		move.setEmpty();
	}
	amountOfMovesToSearch = 0;
}

void WhatIf::setWhatIfMoves(MoveGenerator &board, ply_t ply)
{
	for (ply_t index = 0; index < ply && movesToSearch[index] != Move::EMPTY_MOVE; index++)
	{
		if (movesToSearch[index] == Move::NULL_MOVE)
		{
			board.doNullmove();
		}
		else
		{
			board.doMove(movesToSearch[index]);
		}
	}
}

void WhatIf::setMove(ply_t ply, char movingPiece, uint32_t departureFile, uint32_t departureRank,
					 uint32_t destinationFile, uint32_t destinationRank, char promotePiece)
{
	if (ply < MAX_PLY)
	{
		MoveGenerator curBoard = board;
		setWhatIfMoves(curBoard, ply);

		Move move = BoardAdapter::findMove(
			curBoard, movingPiece, departureFile, departureRank, destinationFile, destinationRank, promotePiece);
		if (move != Move::EMPTY_MOVE)
		{
			setMove(curBoard, ply, move);
		}
	}
}

void WhatIf::setBoard(MoveGenerator &newBoard)
{
	board = newBoard;
	hash = board.computeBoardHash();
}

void WhatIf::setMove(MoveGenerator &board, uint32_t moveNo, Move move)
{
	if (moveNo < MAX_PLY)
	{
		movesToSearch[moveNo] = move;
		amountOfMovesToSearch = moveNo;
		if (move == Move::NULL_MOVE)
		{
			board.doNullmove();
		}
		else
		{
			board.doMove(move);
		}
		hash = board.computeBoardHash();
		std::cout << "Move " << moveNo << ": " << move.getLAN() << " hash: " << hash << endl;
		/*
		ChessEval::Eval eval;
		eval.printEval(board);
		*/
	}
}

void WhatIf::setNullmove(ply_t ply)
{
	MoveGenerator curBoard = board;
	setWhatIfMoves(curBoard, ply);
	setMove(curBoard, ply, Move::NULL_MOVE);
}

void WhatIf::printMoves(const SearchStack &stack, Move currentMove, ply_t ply)
{
	stack.printMoves(currentMove, ply);
}

#endif