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
 * Implements the debugging functionality "whatif"
 */

#ifndef __WHATIF_H
#define __WHATIF_H

#include "../basics/move.h"
#include "../movegenerator/movegenerator.h"
#include "searchdef.h"
#include "searchstack.h"
#include "computinginfo.h"
#include "../interface/iwhatIf.h"

using namespace ChessBasics;
using namespace ChessMoveGenerator;
using namespace ChessInterface;

namespace ChessSearch {

#if defined(_DEBUG) || defined(WHATIF_RELEASE)
#define WHATIF(x) x
#define DOWHATIF true
#else
#define WHATIF(x)
#define DOWHATIF false
#endif

#if (DOWHATIF == false) 
	class WhatIf : public IWhatIf {
	public:
		WhatIf() {};
		void init(const Board& board, const ComputingInfo& computingInfo, value_t alpha, value_t beta) {};
		void printInfo(const Board& board, const ComputingInfo& computingInfo, const SearchStack& stack, Move currentMove, ply_t ply) {};
		void startSearch(const Board& board, const ComputingInfo& computingInfo, const SearchStack& stack, ply_t ply) {};
		void moveSelected(const Board& board, const ComputingInfo& computingInfo, Move currentMove, ply_t ply, bool inQsearch) {};
		void moveSelected(const Board& board, const ComputingInfo& computingInfo, const SearchStack& stack, Move currentMove, ply_t ply) {};
		void moveSearched(const Board& board, const ComputingInfo& computingInfo, const SearchStack& stack, Move currentMove, ply_t ply) {};
		void moveSearched(const Board& board, const ComputingInfo& computingInfo, Move currentMove, value_t alpha, value_t beta, value_t bestValue, ply_t ply) {};
		void cutoff(const Board& board, const ComputingInfo& computingInfo, const SearchStack& stack, ply_t ply, Cutoff cutoff) {};
		void setTT(TT* hashPtr, uint64_t hashKey, ply_t depth, ply_t ply, Move move, value_t bestValue, value_t alpha, value_t beta, bool nullMoveTrhead) {};
		virtual void clear() {};
		virtual void setSearchDepht(int32_t depth) {}
		virtual void setMove(ply_t ply, char movingPiece, uint32_t departureFile, uint32_t departureRank,
			uint32_t destinationFile, uint32_t destinationRank, char promotePiece) {};
		virtual void setNullmove(ply_t ply) {};
		void setBoard(MoveGenerator& newBoard) {};
		static WhatIf whatIf;
	};

#else 
	class WhatIf : public IWhatIf {
	public:
		WhatIf();

		void init(const Board& board, const ComputingInfo& computingInfo, value_t alpha, value_t beta);

		void printInfo(const Board& board, const ComputingInfo& computingInfo, const SearchStack& stack, Move currentMove, ply_t ply);

		void startSearch(const Board& board, const ComputingInfo& computingInfo, const SearchStack& stack, ply_t ply);

		void moveSelected(const Board& board, const ComputingInfo& computingInfo, Move currentMove, ply_t ply, bool inQsearch);
		void moveSelected(const Board& board, const ComputingInfo& computingInfo, const SearchStack& stack, Move currentMove, ply_t ply);

		void moveSearched(const Board& board, const ComputingInfo& computingInfo, const SearchStack& stack, Move currentMove, ply_t ply);
		void moveSearched(const Board& board, const ComputingInfo& computingInfo, Move currentMove, value_t alpha, value_t beta, value_t bestValue, ply_t ply);

		void cutoff(const Board& board, const ComputingInfo& computingInfo, const SearchStack& stack, ply_t ply, Cutoff cutoff);

		void setTT(TT* hashPtr, uint64_t hashKey, ply_t depth, ply_t ply, Move move, value_t bestValue, value_t alpha, value_t beta, bool nullMoveTrhead);

		virtual void clear();

		virtual void setSearchDepht(int32_t depth) { searchDepth = depth - 1; }

		virtual void setMove(ply_t ply, char movingPiece, uint32_t departureFile, uint32_t departureRank, 
			uint32_t destinationFile, uint32_t destinationRank, char promotePiece);
		virtual void setNullmove(ply_t ply);

		void setBoard(MoveGenerator& newBoard);

		static WhatIf whatIf;

	private:
		void setMove(MoveGenerator& board, uint32_t moveNo, Move move);
		void printMoves(const SearchStack& stack, Move currentMove, ply_t ply);
		void setWhatIfMoves(MoveGenerator& board, ply_t ply);

		static const uint32_t MAX_PLY = 255;
		array<Move, MAX_PLY> movesToSearch;
		int32_t amountOfMovesToSearch;
		ply_t maxPly;
		ply_t hashFoundPly;
		int32_t searchDepth;
		int32_t count;
		MoveGenerator board;
		hash_t hash;
		bool qsearch;

	};

#endif

}

#endif __WHATIF_H