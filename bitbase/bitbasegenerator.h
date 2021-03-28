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
 * Tool to generate bitbases
 */


#ifndef __BITBASEGENERATOR_H
#define __BITBASEGENERATOR_H

#include <iostream>
#include "bitbase.h"
#include "boardaccess.h"
#include "workpackage.h"
#include "generationstate.h"
#include "bitbasereader.h"

using namespace std;
using namespace ChessMoveGenerator;
using namespace ChessSearch;

namespace QaplaBitbase {
	class BitbaseGenerator {
	public:
		static const int DEBUG_LEVEL = 1;
		BitbaseGenerator() {
		}

		/**
		 * Computes a bitbase for a piece string (example KPK)
		 */
		void computeBitbase(string pieceString) {
			PieceList list(pieceString);
			computeBitbase(list);
		}

		/**
		 * Computes a bitbase for a piece string (example KPK) and
		 * all other bitbases needed
		 */
		void computeBitbaseRec(string pieceString, uint32_t cores = 1, bool uncompressed = false,
				int traceLevel = 0, int debugLevel = 0, uint64_t debugIndex = 64)
		{
			_cores = min(MAX_THREADS, cores);
			_uncompressed = uncompressed;
			_traceLevel = traceLevel;
			_debugIndex = debugIndex;
			_debugLevel = debugLevel;
			ClockManager clock;
			clock.setStartTime();
			PieceList list(pieceString);
			computeBitbaseRec(list, true);
			cout << endl << "All Bitbases generated!";
			printTimeSpent(clock, 0);
			cout << endl;
		}

	private:

		/**
		 * Join all running threads
		 */
		void joinThreads() {
			for (auto& thr : _threads) {
				if (thr.joinable()) {
					thr.join();
				}
			}
		}

		/**
		 * Searches all captures and look up the position after capture in a bitboard.
		 */
		Result initialSearch(MoveGenerator& position, MoveList& moveList);

		/**
		 * Marks one candidate identified by a partially filled move and a destination square
		 */
		uint64_t computeCandidateIndex(bool wtm, const PieceList& list, Move move, Square destination, bool verbose);

		/**
		 * Computes a list of candidates and pushes them to the candidates vector
		 */
		void computeCandidates(vector<uint64_t>& candidates, const MoveGenerator& position,
			const PieceList& list, Move move, bool verbose);

		/**
		 * Mark all candidate positions we need to look at after a new bitbase position is set to 1
		 * Candidate positions are computed by running through the attack masks of every piece and 
		 * computing reverse moves (ignoring all special cases like check, captures, ...)
		 */
		void computeCandidates(vector<uint64_t>& candidates, MoveGenerator& position, bool verbose);

		/**
		 * Computes a position value by probing all moves and lookup the result in this bitmap
		 * Captures are excluded, they have been tested in the initial search. 
		 */
		bool computeValue(MoveGenerator& position, const Bitbase& bitbase, bool verbose);

		/**
		 * Sets the bitbase index for a position by computing the position value from the bitbase itself
		 * @returns 1, if the position is a win (now) and 0, if it is still unknown
		 */
		uint32_t computePosition(uint64_t index, MoveGenerator& position, GenerationState& state);

		/**
		 * Prints the difference of two bitbases
		 */
		void compareBitbases(string pieceString, Bitbase& bitbase1, Bitbase& bitbase2);

		/**
		 * Prints the differences of two bitbases in files
		 */
		void compareToFile(string pieceString, Bitbase& won) {
			Bitbase bitbase2;
			bitbase2.readFromFile("generated\\" + pieceString);
			compareBitbases(pieceString, won, bitbase2);
		}

		/**
		 * Prints the differences of two bitbases in files
		 */
		void compareFiles(string pieceString) {
			Bitbase bitbase1;
			Bitbase bitbase2;
			bool found1 = bitbase1.readFromFile(pieceString, ".btb", "./", false);
			bool found2 = bitbase2.readFromFile(pieceString, ".btb", "./generated/", false);
			if (found1 && found2) {
				compareBitbases(pieceString, bitbase1, bitbase2);
			}
		}

		/**
		 * Test a position
		 * @param position chess position without kings
		 * @param wk square of white king
		 * @param bk square of black king
		 * @param wtm white to move
		 * @param expected expected value for the position (win or non win)
		 * @param bitbase bit base to test
		 */
		void testBoard(MoveGenerator& position, Square wk, Square bk, bool wtm, bool expected, Bitbase& bitbase) {
			position.setPiece(wk, WHITE_KING);
			position.setPiece(bk, BLACK_KING);
			position.setWhiteToMove(wtm);
			uint64_t checkIndex = BoardAccess::getIndex<0>(position);
			if (bitbase.getBit(checkIndex) == expected) {
				printf("Test OK\n");
			}
			else {
				printf("%s, index:%lld\n", position.isWhiteToMove() ? "white" : "black", checkIndex);
				printf("Test failed\n");
				position.print();
				computeValue(position, bitbase, true);
				showDebugIndex("KRKP");
			}
		}

		void testKQKR(Square wk, Square bk, Square q, Square r, bool wtm, bool expected, Bitbase& bitbase) {
			MoveGenerator position;
			position.setPiece(q, WHITE_QUEEN);
			position.setPiece(r, BLACK_ROOK);
			testBoard(position, wk, bk, wtm, expected, bitbase);
		}

		void testKRKQ(Square wk, Square bk, Square r, Square q, bool wtm, bool expected, Bitbase& bitbase) {
			MoveGenerator position;
			position.setPiece(q, BLACK_QUEEN);
			position.setPiece(r, WHITE_ROOK);
			testBoard(position, wk, bk, wtm, expected, bitbase);
		}

		void testAllKQKR() {
			Bitbase bitbase;
			bitbase.readFromFile("KQKR.btb");
			testKQKR(B7, B1, C8, A2, false, false, bitbase);
			testKQKR(B7, B1, D8, A2, false, true, bitbase);
			testKQKR(B7, B1, C8, A3, false, true, bitbase);
			testKQKR(B7, B1, C8, A2, true, true, bitbase);
		}

		void testKPK(Square wk, Square bk, Square p, bool wtm, bool expected, Bitbase& bitbase) {
			MoveGenerator position;
			position.setPiece(p, WHITE_PAWN);
			testBoard(position, wk, bk, wtm, expected, bitbase);
		}

		void testAllKPK(Bitbase& bitbase) {
			testKPK(E5, E8, E7, false, false, bitbase);
			testKPK(D6, E8, E7, true, false, bitbase);
			testKPK(A2, A5, E2, true, true, bitbase);
			testKPK(A2, A5, E2, false, false, bitbase);
			testKPK(A3, A5, E2, true, false, bitbase);
			testKPK(A3, A5, E2, false, true, bitbase);
		}

		void testAllKPK() {
			Bitbase bitbase;
			bitbase.readFromFile("KPK");
			testAllKPK(bitbase);
		}

		void testKRKP(Square wk, Square bk, Square p, Square r, bool wtm, bool expected, Bitbase& bitbase) {
			MoveGenerator position;
			position.setPiece(wk, WHITE_KING);
			position.setPiece(bk, BLACK_KING);
			position.setPiece(p, BLACK_PAWN);
			position.setPiece(r, WHITE_ROOK);
			position.setWhiteToMove(wtm);
			uint64_t checkIndex = BoardAccess::getIndex<0>(position);
			if (bitbase.getBit(checkIndex) == expected) {
				printf("Test OK\n");
			}
			else {
				position.print();
				printf("%s, index:%lld\n", position.isWhiteToMove() ? "white" : "black", checkIndex);
				printf("Test failed\n");
				computeValue(position, bitbase, true);
				showDebugIndex("KRKP");

			}
		}

		void testAllKRKP() {
			Bitbase bitbase;
			bitbase.readFromFile("KRKP");
			testKRKP(F2, D4, F6, F3, true, true, bitbase);
			testKRKP(F2, D4, F6, F3, false, true, bitbase);
			testKRKP(A7, G4, F2, B5, false, false, bitbase);
			testKRKP(A7, G4, F2, B5, true, false, bitbase);
			testKRKP(A7, G4, F5, B5, true, true, bitbase);
			testKRKP(A7, G4, F5, B5, false, false, bitbase);
			testKRKP(C3, C1, D2, A2, false, false, bitbase);
		}

		void testAllKRKQ() {
			Bitbase bitbase;
			bitbase.readFromFile("KRKQ");
			testKRKQ(C3, C1, A2, D1, false, false, bitbase);
		}

		/**
		 * Prints the time spent so far
		 */
		void printTimeSpent(ClockManager& clock, int minTraceLevel);

		/**
		 * Prints win/loss/draw/illegal statistic
		 */
		void printStatistic(GenerationState& state, int minTraceLevel) {
			if (_traceLevel < minTraceLevel) {
				return;
			}
			state.printStatistic();
		}

		/**
		 * Debugging functionality, prints the position identified by the globally set 
		 * debug index
		 */
		void showDebugIndex(string pieceString);

		/**
		 * Populates a position from a bitbase index for the squares and a piece list for the piece types
		 */
		void addPiecesToPosition(MoveGenerator& position, const BitbaseIndex& bitbaseIndex, const PieceList& pieceList);

		/**
		 * Computes a workpackage for a compute-bitbase loop
		 * @param work list of indexes to work on
		 * @param candidates resulting candidates for further checks
		 */
		void computeWorkpackage(Workpackage& workpackage, 
			vector<uint64_t>& candidates, GenerationState& state, bool onlyCandidates);

		/**
		 * Compute the bitbase by checking each position for an update as long as no further update is found
		 * @param state Current computation state
		 * @param clock time measurement for the bitbase generation
		 */
		void computeBitbase(GenerationState& state, ClockManager& clock);


		/**
		 * Sets a situation to mate or stalemate 
		 */
		bool setMateOrStalemate(ChessMoveGenerator::MoveGenerator& position, const uint64_t index,
			QaplaBitbase::GenerationState& state);

		/**
		 * Initially probe alle positions for a mate- draw or capture situation
		 */
		bool initialComputePosition(uint64_t index, MoveGenerator& position, GenerationState& state);

		/**
		 * Computes a workpackage for the initial situation calculation
		 */
		void computeInitialWorkpackage(Workpackage& workpackage, GenerationState& state);

		/**
		 * Computes a bitbase for a set of pieces described by a piece list.
		 * @param pieceList list of pieces in the bitbase
		 */
		void computeBitbase(PieceList& pieceList);

		/**
		 * Recursively computes bitbases based on a bitbase string
		 * For KQKP it will compute KQK, KQKQ, KQKR, KQKB, KQKN, ...
		 * so that any bitbase KQKP can get to is available
		 */
		void computeBitbaseRec(PieceList& pieceList, bool first);

		uint64_t _numberOfIllegalPositions;
		uint64_t _numberOfDirectLoss;
		uint64_t _numberOfDirectDraw;

		uint32_t _cores;
		bool _uncompressed;
		int _traceLevel;
		uint64_t _debugIndex;
		int _debugLevel;

		static const uint32_t MAX_THREADS = 64;
		array<vector<uint64_t>, MAX_THREADS> _candidates;
		array<thread, MAX_THREADS> _threads;
	};

}

#endif // __BITBASEGENERATOR_H