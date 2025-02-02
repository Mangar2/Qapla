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

#include <ostream>
#include "../basics/move.h"
#include "../movegenerator/movegenerator.h"
#include "searchdef.h"
#include "searchstack.h"
#include "computinginfo.h"
#include "../interface/iwhatIf.h"
#include "tt.h"

using namespace QaplaBasics;
using namespace QaplaMoveGenerator;
using namespace QaplaInterface;


namespace QaplaSearch {
	
#if defined(_DEBUG) || defined(WHATIF_RELEASE)
#define WHATIF(x) x
#define DOWHATIF true
#else
#define WHATIF(x)
#define DOWHATIF false
#endif

	struct WhatIfVariables {

		WhatIfVariables(const ComputingInfo& info, const SearchStack& stack, Move currentMove, ply_t depth, ply_t ply, const string searchType)
			:_stack(stack)
		{
			_ttMove = "";
			_cutoff = "";
			_bestMove = "";
			_ply = ply;
			const SearchVariables& node = stack[ply];
			_alpha = node.alpha;
			_beta = node.beta;
			_bestValue = node.bestValue;
			_remainingDepth = depth;
			_nodeType = node.getNodeTypeName();
			if (!node.bestMove.isEmpty()) {
				_bestMove = node.bestMove.getLAN();
			}
			_curMove = currentMove;
			_searchType = computeSearchType(searchType, node);
			
			_nodesSearched = info._nodesSearched;
			_pv = node.isPVSearch() ? node.pvMovesStore.toString() : "";

			if (_remainingDepth >= 0) {
				_curValue = -stack[ply + 1].bestValue;
				if (!stack[ply + 1].getTTMove().isEmpty()) {
					_ttMove = stack[ply + 1].getTTMove().getLAN();
				}
				_cutoff = _cutoffString[int(stack[ply + 1].cutoff)];
			}
			
		}

		string computeSearchType(const string searchType, const SearchVariables& node) const {
			if (searchType == "") {
				return node.isWindowZero() ? (node.isPVNode() ? "ZeroW" : "Normal") : "PV";
			}
			return searchType;
		}

		void printMoves(ostream& stream = cout) const {
			_stack.printMoves(_curMove, _ply);
		}

		void printAll(ostream& stream = cout) const {
			stream 
				<< "[w:" << std::setw(6) << _alpha << "," << std::setw(6) << _beta << "]"
				<< "[bv:" << std::setw(6) << _bestValue << "]"
				<< "[d:" << std::setw(2) << _remainingDepth << "]"
				<< "[nt:" << std::setw(3) << _nodeType << "]";

			if (_remainingDepth >= 0) {
				stream << "[v:" << std::setw(6) << _curValue << "]";
			}
			else {
				stream << "[" << std::setw(9) << "]";
			}

			stream
				<< "[c:" << std::setw(4) << _cutoff << "]"
				<< "[ttm:" << std::setw(4) << _ttMove << "]"
				<< "[bm:" << std::setw(4) << _bestMove << "]"
				<< "[st:" << std::setw(6) << _searchType << "]"
				<< "[n:" << std::setw(8) << _nodesSearched << "]";
			if (_pv != "") {
				stream << "[pv:" << _pv << "]";
			}
			stream << endl;
		}

		void printHeader(ostream& stream = cout) const {
			stream
				<< setw(_ply * 5) << (_ply == 0 ? "" : "Moves")
				<< "[w:" << setw(6) << "alpha" << "," << setw(6) << "beta" << "]"
				<< "[bv:" << setw(6) << "BestV" << "]"
				<< "[d:" << setw(2) << "D" << "]"
				<< "[nt:" << setw(3) << "NT" << "]"
				<< "[v:" << setw(6) << "Value" << "]"
				<< "[c:" << setw(4) << "Cutoff" << "]"
				<< "[ttm:" << setw(4) << "TTM" << "]"
				<< "[bm:" << setw(4) << "BestM" << "]"
				<< "[st:" << setw(6) << "Searchtyp" << "]"
				<< "[n:" << setw(8) << "Nodes" << "]";
		}

		void printSelected(ostream& stream = cout) const {
			_stack.printMoves(_curMove, _ply);
			stream
				<< "[w:" << std::setw(6) << _alpha << "," << std::setw(6) << _beta << "]"
				<< "[bv:" << std::setw(6) << _bestValue << "]"
				<< "[d:" << std::setw(2) << _remainingDepth << "]"
				<< "[nt:" << std::setw(3) << _nodeType << "]"
				<< "[" << std::setw(9) << "]"
				<< "[" << std::setw(7) << "]"
				<< "[" << std::setw(9) << "]"
				<< "[bm:" << std::setw(4) << _bestMove << "]"
				<< "[" << std::setw(10) << "]"
				<< "[n:" << std::setw(8) << _nodesSearched << "]";
			stream << endl;
		}

		value_t _ply;
		value_t _alpha;
		value_t _beta;
		value_t _bestValue;
		value_t _curValue;
		value_t _remainingDepth;
		string _nodeType;
		string _ttMove;
		string _bestMove;
		Move _curMove;
		string _cutoff;
		string _searchType;
		uint64_t _nodesSearched;
		string _pv;
		const SearchStack& _stack;
		static constexpr array<const char*, int(Cutoff::COUNT)> 
			_cutoffString = { "NONE", "REPT", "HASH", "MATE", "RAZO", "NEM", "NULL", "FUTL", "BITB" };
	};

#if (DOWHATIF == false) 
	class WhatIf : public IWhatIf {
	public:
		WhatIf() {};
		void init(const Board& board, const ComputingInfo& computingInfo, value_t alpha, value_t beta) {};
		void printInfo(const Board& board, const ComputingInfo& computingInfo, const SearchStack& stack, Move currentMove, ply_t depth, ply_t ply) {};
		void printInfo(const WhatIfVariables& wiVariables);
		void startSearch(const Board& board, const ComputingInfo& computingInfo, const SearchStack& stack, ply_t ply) {};

		void moveSelected(const Board& board, const ComputingInfo& computingInfo, Move currentMove, ply_t ply, bool inQsearch) {};
		void moveSelected(const Board& board, const ComputingInfo& computingInfo, const SearchStack& stack, Move currentMove, ply_t ply) {};

		void moveSearched(const Board& board, const ComputingInfo& computingInfo, const SearchStack& stack,
			Move currentMove, ply_t depth, ply_t ply, const string searchType = "") {};
		void moveSearched(const Board& board, const ComputingInfo& computingInfo, 
			Move currentMove, value_t alpha, value_t beta, value_t bestValue, value_t standPatValue, ply_t ply) {};

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

		void printInfo(const Board& board, const ComputingInfo& computingInfo, const SearchStack& stack, Move currentMove, ply_t depth, ply_t ply);
		void printInfo(const WhatIfVariables& wiVariables);

		void startSearch(const Board& board, const ComputingInfo& computingInfo, const SearchStack& stack, ply_t ply);

		void moveSelected(const Board& board, const ComputingInfo& computingInfo, Move currentMove, ply_t ply, bool inQsearch);
		void moveSelected(const Board& board, const ComputingInfo& computingInfo, const SearchStack& stack, Move currentMove, ply_t ply);

		void moveSearched(const Board& board, const ComputingInfo& computingInfo, const SearchStack& stack, 
			Move currentMove, ply_t depth, ply_t ply, const string searchType = "");
		void moveSearched(const Board& board, const ComputingInfo& computingInfo, 
			Move currentMove, value_t alpha, value_t beta, value_t bestValue, value_t standPatValue, ply_t ply);

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