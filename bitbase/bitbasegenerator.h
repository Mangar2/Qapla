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
 * @copyright Copyright (c) 2025 Volker Böhm
 * @Overview
 * Tool to generate bitbases
 */


#ifndef __BITBASEGENERATOR_H
#define __BITBASEGENERATOR_H

#include "../search/clockmanager.h"

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
		/**
		 * Creates a bitbase generator instance.
		 */
		BitbaseGenerator() {
		}

		/**
		 * Computes the requested bitbase and all recursively required dependent bitbases.
		 *
		 * @param pieceString Bitbase identifier like KPK, KPPK, 3, 4, 5, or 6.
		 * @param cores Requested number of worker threads (capped at MAX_THREADS).
		 * @param compress Compression algorithm used for storing generated bitbase data.
		 * @param generateCpp If true, additionally emits generated C++ source for the bitbase.
		 * @param traceLevel Verbosity level for tracing runtime details.
		 * @param debugLevel Verbosity level for debug output.
		 * @param debugIndex Optional index used to trigger focused debug output.
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
			printTimeSpent(clock);
			cout << endl;
		}


	private:

		/**
		 * Dispatches high-level aliases (3/4/5/6) and then computes the requested bitbase.
		 *
		 * @param pieceString Requested bitbase identifier.
		 * @param compression Compression algorithm used for persisted output.
		 * @param generateCpp If true, also emits generated C++ code.
		 */
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
		 * Joins all currently running worker threads.
		 */
		void joinThreads() {
			for (auto& thr : _threads) {
				if (thr.joinable()) {
					thr.join();
				}
			}
		}
		
		/**
		 * Converts one reverse-generated move candidate into a bitbase index.
		 *
		 * @param wtm Side-to-move flag of the current position.
		 * @param list Piece layout used for index mapping.
		 * @param move Partially constructed reverse move.
		 * @param destination Candidate destination square for the reverse move.
		 * @param verbose Enables detailed debug output.
		 * @returns Candidate bitbase index to revisit.
		 */
		uint64_t computeCandidateIndex(bool wtm, const PieceList& list, Move move, Square destination, bool verbose);

		/**
		 * Mirrors a square to the opposite side when COLOR is black.
		 *
		 * @tparam COLOR Color context for side-dependent square handling.
		 * @param square Square to transform.
		 * @returns Original square for white, mirrored square for black.
		 */
		template <Piece COLOR>
		constexpr Square switchSide(Square square) {
			return COLOR == WHITE ? square : QaplaBasics::switchSide(square);
		}

		/**
		 * Reverse-generates pawn non-capture moves to possible predecessor squares.
		 *
		 * @tparam COLOR Pawn color to reverse-generate.
		 * @param candidates Output vector receiving candidate indexes.
		 * @param position Current position.
		 * @param list Piece layout used for index mapping.
		 * @param move Partially constructed move with moving piece and departure set.
		 * @param verbose Enables detailed debug output.
		 */
		template <Piece COLOR>
		void reverseGeneratePawnMoves(vector<uint64_t>& candidates, 
			const MoveGenerator& position, const PieceList& list, Move move, bool verbose);

		/**
		 * Computes reverse-generated candidate positions for one specific moving piece.
		 *
		 * @param candidates Output vector receiving candidate indexes.
		 * @param position Current position.
		 * @param list Piece layout used for index mapping.
		 * @param move Partially constructed move (piece and departure are set).
		 * @param verbose Enables detailed debug output.
		 */
		void computeCandidates(vector<uint64_t>& candidates, const MoveGenerator& position,
			const PieceList& list, Move move, bool verbose);

		/**
		 * Computes all reverse candidates after marking one position as newly won.
		 * Candidate positions are derived from attack masks and reverse pseudo-legal moves.
		 *
		 * @param candidates Output vector receiving candidate indexes.
		 * @param position Current position.
		 * @param verbose Enables detailed debug output.
		 */
		void computeCandidates(vector<uint64_t>& candidates, MoveGenerator& position, bool verbose);

		/**
		 * Probes non-capture, non-promotion continuations against the current bitbase.
		 *
		 * @param position Current position to evaluate.
		 * @param bitbase Bitbase containing known won positions.
		 * @param verbose Enables detailed debug output.
		 * @returns Bitbase value (BitbaseResult::WIN, LOSS, DRAW, or UNKNOWN)
		 */
		BitbaseResult computeValue(MoveGenerator& position, Bitbase& bitbase, bool verbose);

		/**
		 * Updates one index by evaluating whether the position is now proven as won.
		 *
		 * @param index Bitbase index of the current position.
		 * @param position Position reconstructed for this index.
		 * @param state Mutable generation state.
		 * @returns 1 if the index was newly marked as win, otherwise 0.
		 */
		uint32_t computePosition(uint64_t index, MoveGenerator& position, GenerationState& state);

		/**
		 * Prints elapsed wall-clock time for the current generation step.
		 *
		 * @param clock Clock instance tracking elapsed time.
		 */
		void printTimeSpent(ClockManager& clock);

		/**
		 * Prints aggregated generation statistics.
		 *
		 * @param state Generation state holding counters and bitmaps.
		 */
		void printStatistic(GenerationState& state) {
			state.printStatistic();
		}

		/**
		 * Prints the current position and index information for debugging.
		 *
		 * @param position Position to print.
		 * @param index Optional index shown alongside computed index.
		 */
		void printDebugInfo(const MoveGenerator& position, uint64_t index = -1) {
			if (index != -1) cout << "Cur: " << index;
			cout << " calculated: " << BoardAccess::getIndex<0>(position) << endl;
			position.print();
		}

		/**
		 * Reconstructs a position from reverse index squares and piece identities.
		 *
		 * @param position Target position that will be populated.
		 * @param reverseIndex Reverse index providing square assignments.
		 * @param pieceList Piece identities in fixed order.
		 */
		void addPiecesToPosition(MoveGenerator& position, const ReverseIndex& reverseIndex, const PieceList& pieceList);

		/**
		 * Processes one dynamic work package for iterative bitbase propagation.
		 *
		 * @param workpackage Shared work provider.
		 * @param state Shared generation state.
		 */
		void computeWorkpackage(Workpackage& workpackage, GenerationState& state);

		/**
		 * Runs iterative propagation until no additional candidate positions remain.
		 *
		 * @param state Current computation state.
		 * @param clock Clock tracking total generation time.
		 */
		void computeBitbase(GenerationState& state, ClockManager& clock);

		/**
		 * Determines the best achievable result by examining only captures and promotions,
		 * consulting already-generated subordinate bitbases for each resulting position.
		 * Returns Win/Draw/Loss if the outcome can be fully decided this way,
		 * or Unknown if non-capture moves still need to be resolved by iterative propagation.
		 *
		 * @param position Current position to evaluate.
		 * @param moveList Legal moves generated for the side to move.
		 * @returns Best proven result from white's perspective, or Unknown if undecided.
		 */
		BitbaseResult setInitialValueByCapturesAndPromotions(
			MoveGenerator& position, const uint64_t index, MoveList& moveList, QaplaBitbase::GenerationState &state);

		/**
		 * Classifies a no-move situation as checkmate or stalemate.
		 *
		 * @param position Current position with no legal moves.
		 * @param index Bitbase index of this position.
		 * @param state Mutable generation state.
		 * @returns Classified terminal result.
		 */
		BitbaseResult setMateOrStalemate(QaplaMoveGenerator::MoveGenerator& position, const uint64_t index,
			QaplaBitbase::GenerationState& state);

		/**
		 * Performs initial classification for one position before iterative propagation.
		 *
		 * @param index Bitbase index of this position.
		 * @param position Reconstructed position.
		 * @param state Mutable generation state.
		 * @returns Initial classification result.
		 */

		BitbaseResult initialComputePosition(uint64_t index, MoveGenerator& position, GenerationState& state);

		/**
		 * Processes one dynamic work package for initial position classification.
		 *
		 * @param workpackage Shared work provider.
		 * @param state Shared generation state.
		 */
		void computeInitialWorkpackage(Workpackage& workpackage, GenerationState& state);

		/**
		 * Computes and persists one concrete bitbase described by a piece list.
		 *
		 * @param pieceList Piece layout of the bitbase to generate.
		 * @param first True if this is the primary requested bitbase.
		 * @param compression Compression algorithm used for persisted output.
		 * @param generateCpp If true, also emits generated C++ code.
		 */
		void computeBitbase(PieceList& pieceList, bool first, QaplaCompress::CompressionType compression, bool generateCpp);

		/**
		 * Recursively computes all dependent bitbases reachable via captures and promotions.
		 * For KQKP this includes KQK, KQKQ, KQKR, KQKB, KQKN, and related dependencies.
		 *
		 * @param pieceList Piece layout of the current bitbase.
		 * @param first True if this is the primary requested bitbase.
		 * @param compression Compression algorithm used for persisted output.
		 * @param generateCpp If true, also emits generated C++ code.
		 */
		void computeBitbaseRec(PieceList& pieceList, bool first, QaplaCompress::CompressionType compression, bool generateCpp);

		uint32_t _cores;
		int _traceLevel;
		uint64_t _debugIndex;
		int _debugLevel;

		static constexpr uint32_t MAX_THREADS = 64;
		array<thread, MAX_THREADS> _threads;
	};

}

#endif // __BITBASEGENERATOR_H
