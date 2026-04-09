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
#include "bitbase-profiling.h"

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
			BitbaseProfiling::getStaticInstance().printStatistics();
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
			}
			else if (pieceString == "5") {
				computeBitbase("KPPKP", compression, generateCpp);
				computeBitbase("KPPPK", compression, generateCpp);
			}
			else if (pieceString == "6") {
				computeBitbase("KPPKPP", compression, generateCpp);
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
		static void addToCandidates(vector<CandidateEntry>& candidates, const CandidateEntry& entry,
			Bitbase& computedResults);

		template <Piece COLOR>
		void reverseGeneratePawnMoves(vector<CandidateEntry>& candidates, 
			const MoveGenerator& position, const PieceList& list, Move move, BitbaseResult result,
			Bitbase& computedResults, bool verbose);

		/**
		 * Computes reverse-generated candidate positions for one specific moving piece.
		 *
		 * @param candidates Output vector receiving candidate indexes.
		 * @param position Current position.
		 * @param list Piece layout used for index mapping.
		 * @param move Partially constructed move (piece and departure are set).
		 * @param verbose Enables detailed debug output.
		 */
		void computeCandidates(vector<CandidateEntry>& candidates, const MoveGenerator& position,
			const PieceList& list, Move move, BitbaseResult result,
			Bitbase& computedResults, bool verbose);

		/**
		 * Computes all reverse candidates after marking one position as newly won.
		 * Candidate positions are derived from attack masks and reverse pseudo-legal moves.
		 *
		 * @param candidates Output vector receiving candidate indexes.
		 * @param position Current position.
		 * @param verbose Enables detailed debug output.
		 */
		void computeCandidates(vector<CandidateEntry>& candidates, MoveGenerator& position, BitbaseResult result,
			Bitbase& computedResults, bool verbose);

		/**
		 * Evaluates non-capture, non-promotion moves against already-computed bitbase entries
		 * to determine and store the best result achievable from this position.
		 * Capture and promotion moves are skipped — they were resolved during initialization.
		 * Stores an intermediate lower-bound value (e.g. Draw) as soon as one is found,
		 * even before all successors are resolved. Finalizes the value once all reachable
		 * successors are known.
		 *
		 * @param index Bitbase index of the current position.
		 * @param position Current position to evaluate.
		 * @param state Mutable generation state providing the bitbase and storing the result.
		 * @param verbose Enables detailed debug output.
		 * @returns Final proven result (Win/Loss/Draw), or Unknown if any successor is still unresolved.
		 */
		BitbaseResult setComputeValue(
			uint64_t index, MoveGenerator& position, QaplaBitbase::GenerationState &state, bool verbose);

		/**
		 * Re-evaluates one position during iterative propagation and stores the result if resolved.
		 *
		 * @param index Bitbase index of the current position.
		 * @param position Position reconstructed for this index.
		 * @param state Mutable generation state.
		 * @returns true if the position reached a definitive result (Win, Loss, or Draw); false if still Unknown.
		 */
		BitbaseResult computePosition(uint64_t index, MoveGenerator& position, GenerationState& state);

		/**
		 * Attempts to directly set the result for a candidate position without full move evaluation.
		 *
		 * @param index Bitbase index of the candidate position.
		 * @param winningMove True if the candidate move is a winning move.
		 * @param whiteToMove True if white is to move in the candidate position.
		 * @param state Mutable generation state.
		 * @returns true if the position was directly resolved, false if full evaluation is needed.
		 */
		bool tryDirectEntry(uint64_t index, bool winningMove,
							bool whiteToMove, GenerationState& state);

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
		 * Sets the initial proven value for a position by consulting subordinate bitbases
		 * via all capture and promotion moves.
		 * Always writes at least the best already-achieved result into state:
		 * a forced win or loss is stored as final; a reachable draw is stored as
		 * an intermediate lower bound even when non-capture moves remain unresolved.
		 * Returns Win or Loss when the result is fully decided by captures/promotions alone,
		 * Draw when all evaluated moves are at least draws and no non-captures exist,
		 * or Unknown when non-capture moves are present and no forced win was found.
		 *
		 * @param position Current position to evaluate.
		 * @param index Bitbase index of the position.
		 * @param moveList Legal moves generated for the side to move.
		 * @param state Mutable generation state that receives the result.
		 * @returns Final proven result, or Unknown if iterative propagation is still needed.
		 */
		BitbaseResult setInitialValueByCapturesAndPromotions(
			MoveGenerator& position, const uint64_t index, MoveList& moveList, QaplaBitbase::GenerationState &state);

		/**
		 * Stores and returns the terminal result for a position with no legal moves.
		 * Black checkmated → Win, White checkmated → Loss, no check → Draw (stalemate).
		 * Always produces a final, non-Unknown result.
		 *
		 * @param position Current position with no legal moves.
		 * @param index Bitbase index of this position.
		 * @param state Mutable generation state that receives the result.
		 * @returns Win, Loss, or Draw.
		 */
		BitbaseResult setMateOrStalemate(QaplaMoveGenerator::MoveGenerator& position, const uint64_t index,
			QaplaBitbase::GenerationState& state);

		/**
		 * Classifies one position during the initial pass before iterative propagation.
		 * Illegal and terminal positions (mate/stalemate) are fully decided immediately.
		 * For positions with legal moves, the best result provable via captures and promotions
		 * is stored; positions still depending on non-capture moves return Unknown.
		 *
		 * @param index Bitbase index of this position.
		 * @param position Reconstructed position.
		 * @param state Mutable generation state that receives the result.
		 * @returns Final proven result, or Unknown if iterative propagation is still needed.
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
		static constexpr uint64_t _packageSize = 50000;

		static constexpr uint32_t MAX_THREADS = 64;
		array<thread, MAX_THREADS> _threads;
	};

}

#endif // __BITBASEGENERATOR_H
