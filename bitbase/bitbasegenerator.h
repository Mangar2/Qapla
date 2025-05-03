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
 * @author Volker B�hm
 * @copyright Copyright (c) 2021 Volker B�hm
 * @Overview
 * Tool to generate bitbases
 */


#ifndef __BITBASEGENERATOR_H
#define __BITBASEGENERATOR_H

#include <iostream>
#include "reverseindex.h"
#include "bitbase.h"
#include "boardaccess.h"
#include "workpackage.h"
#include "generationstate.h"
#include "bitbase-reader.h"

using namespace std;
using namespace QaplaMoveGenerator;
using namespace QaplaSearch;

namespace QaplaBitbase {
	class BitbaseGenerator {
	public:
		static const bool DO_DEBUG = true;
		BitbaseGenerator() {
		}

		/**
		 * Computes a bitbase for a piece string (example KPK) and
		 * all other bitbases needed
		 */
		void computeBitbaseRec(string pieceString, uint32_t cores = 1, 
			QaplaCompress::CompressionType compress = QaplaCompress::CompressionType::Miniz,
			bool generateCpp = false, int traceLevel = 0, int debugLevel = 0, uint64_t debugIndex = 64)
		{
			_cores = std::min(MAX_THREADS, cores);
			_traceLevel = traceLevel;
			_debugIndex = debugIndex;
			_debugLevel = debugLevel;
			ClockManager clock;
			clock.setStartTime();
			computeBitbase(pieceString, compress, generateCpp);
			cout << endl << "All Bitbases generated!";
			printTimeSpent(clock, 0);
			cout << endl;
		}


	private:

		void computeBitbase(string pieceString, QaplaCompress::CompressionType compression, bool generateCpp) {
			if (pieceString == "3") {
				computeBitbase("KPK", compression, generateCpp);
			} 
			else if (pieceString == "4") {
				computeBitbase("KPPK", compression, generateCpp);
				computeBitbase("KPKP", compression, generateCpp);
			}
			else if (pieceString == "5s") {
				computeBitbase("KPPKP", compression, generateCpp);
				computeBitbase("KPKPP", compression, generateCpp);
			}
			else if (pieceString == "5") {
				computeBitbase("KPPKP", compression, generateCpp);
				computeBitbase("KPKPP", compression, generateCpp);
				computeBitbase("KPPPK", compression, generateCpp);
			}
			else if (pieceString == "6") {
				computeBitbase("KPPKPP", compression, generateCpp);
				computeBitbase("KPKPPP", compression, generateCpp);
				computeBitbase("KPPPKP", compression, generateCpp);
			}
			PieceList list(pieceString);
			computeBitbaseRec(list, true, compression, generateCpp);
		}


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
		 * Switches the sides, if the moving piece is a black piece
		 */
		template <Piece COLOR>
		constexpr Square switchSide(Square square) {
			return COLOR == WHITE ? square : QaplaBasics::switchSide(square);
		}

		/**
		 * Reverse-Generate pawn moves - moving back to possible start positions of the pawn
		 */
		template <Piece COLOR>
		void reverseGeneratePawnMoves(vector<uint64_t>& candidates, 
			const MoveGenerator& position, const PieceList& list, Move move, bool verbose);

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
		 * Prints the time spent so far
		 */
		void printTimeSpent(ClockManager& clock, int minTraceLevel, bool sameLine = false);

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
		 * Prints an information about the actual position for debugging
		 */
		void printDebugInfo(const MoveGenerator& position, uint64_t index = -1) {
			if (index != -1) cout << "Cur: " << index;
			cout << " calculated: " << BoardAccess::getIndex<0>(position) << endl;
			position.print();
		}

		/**
		 * Populates a position from a bitbase index for the squares and a piece list for the piece types
		 */
		void addPiecesToPosition(MoveGenerator& position, const ReverseIndex& reverseIndex, const PieceList& pieceList);

		/**
		 * Computes a workpackage for a compute-bitbase loop
		 * @param workpackage work provider
		 * @param state current generation state
		 */
		void computeWorkpackage(Workpackage& workpackage, GenerationState& state);

		/**
		 * Compute the bitbase by checking each position for an update as long as no further update is found
		 * @param state Current computation state
		 * @param clock time measurement for the bitbase generation
		 */
		void computeBitbase(GenerationState& state, ClockManager& clock);


		/**
		 * Sets a situation to mate or stalemate 
		 */
		Result setMateOrStalemate(QaplaMoveGenerator::MoveGenerator& position, const uint64_t index,
			QaplaBitbase::GenerationState& state);

		/**
		 * Initially probe alle positions for a mate- draw or capture situation
		 */
		Result initialComputePosition(uint64_t index, MoveGenerator& position, GenerationState& state);

		/**
		 * Computes a workpackage for the initial situation calculation
		 */
		void computeInitialWorkpackage(Workpackage& workpackage, GenerationState& state);

		/**
		 * Computes a bitbase for a set of pieces described by a piece list.
		 * @param pieceList list of pieces in the bitbase
		 * @param first true, if this is a primary bitbase and not an additionally required and recursively identified bitbase
		 * @param compression compression type
		 * @param generateCpp true, if a cpp file should be generated
		 */
		void computeBitbase(PieceList& pieceList, bool first, QaplaCompress::CompressionType compression, bool generateCpp);

		/**
		 * Recursively computes bitbases based on a bitbase string
		 * For KQKP it will compute KQK, KQKQ, KQKR, KQKB, KQKN, ...
		 * so that any bitbase KQKP can get to is available
		 */
		void computeBitbaseRec(PieceList& pieceList, bool first, QaplaCompress::CompressionType compression, bool generateCpp);

		const bool debug = false;
		uint64_t _numberOfIllegalPositions;
		uint64_t _numberOfDirectLoss;
		uint64_t _numberOfDirectDraw;

		uint32_t _cores;
		int _traceLevel;
		uint64_t _debugIndex;
		int _debugLevel;

		static constexpr uint32_t MAX_THREADS = 64;
		array<thread, MAX_THREADS> _threads;
	};

}

#endif // __BITBASEGENERATOR_H