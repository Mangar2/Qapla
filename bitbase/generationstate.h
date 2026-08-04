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
 * State of the generation for a single piece combination
 */

#pragma once

#include <atomic>
#include <mutex>
#include "bitbase.h"
#include "bitbaseindex.h"
#include "compress.h"

namespace QaplaBitbase {

	struct CandidateEntry {
		uint64_t index;
		bool winningMove;
	};

	class GenerationState
	{
	public:
		/**
		 * Creates generation state storage for one concrete piece configuration.
		 *
		 * @param pieceList Piece layout represented by this generation state.
		 * @param sig Bitbase signature used for internal bitbase construction.
		 */
		GenerationState(PieceList& pieceList, uint32_t sig) 
		{
			BitbaseIndex bitbaseIndexType(pieceList);
			_entryCount = bitbaseIndexType.getEntryCount();
			_computedResults = Bitbase(_entryCount, 2, sig);
			_computedResults.resize(_entryCount);
			_computedResults.fillAll();
			_computedResults.setLoaded();
			_candidates = Bitbase(_entryCount, 1, sig);
			_candidates.resize(_entryCount);
			_candidates.setLoaded();
			_candidateResults = Bitbase(_entryCount, 1, sig);
			_candidateResults.resize(_entryCount);
			_candidateResults.setLoaded();
			_pieceList = pieceList;
			_illegal = 0;
			_loss = 0;
			_won = 0;
		}

		/**
		 * Returns the piece layout of this generation state.
		 *
		 * @returns Piece list used to map index-to-position conversions.
		 */
		const PieceList& getPieceList() const { return _pieceList; }

		/**
		 * Returns true if the result is truly final (Win or Loss).
		 * Unknown and Draw are both non-final: Unknown means not yet evaluated,
		 * Draw means a drawing capture was found but a Win via a quiet move is still possible.
		 */
		static bool isFinal(BitbaseResult r) {
			return r == BitbaseResult::Win || r == BitbaseResult::Loss;
		}

		/**
		 * Checks whether a position should be processed in the current pass.
		 *
		 * @param index Bitbase index of the position.
		 * @param onlyCandidates If true, accepts only positions marked as candidates.
		 * @returns True when the position is not yet computed and passes candidate filtering.
		 */
		bool isPositionToCheck(uint64_t index, bool onlyCandidates) {
			return !isFinal(_computedResults.get2Bits(index)) &&
				(!onlyCandidates || _candidates.getBit(index));
		}

		/**
		 * Checks whether a position has already been (finally) computed.
		 *
		 * @param index Bitbase index of the position.
		 * @returns True if the position is marked as computed.
		 */
		bool isPositionComputed(uint64_t index) {
			return isFinal(_computedResults.get2Bits(index));
		}

		/**
		 * Collects all indexes that still need processing.
		 *
		 * @param work Output vector receiving candidate work indexes.
		 */
		void getWork(vector<CandidateEntry>& work) {
			vector<uint64_t> indexes;
			indexes.reserve(_entryCount);
			_candidates.getAllSetIndexes(indexes);
			work.reserve(indexes.size());
			for (auto idx : indexes) {
				work.push_back({idx, _candidateResults.getBit(idx) == 1});
			}
		}

		/**
		 * Returns the total number of indexed positions.
		 *
		 * @returns Bitbase size in bits (one bit per indexed position).
		 */
		uint64_t getEntryCount() const { return _entryCount; }

		/**
		 * Indicates whether new candidates were found in the previous loop.
		 *
		 * @returns True if at least one candidate is currently marked.
		 */
		bool hasCandidates() const {
			return _hasCandidates;
		}

		/**
		 * Returns the bitbase of positions currently classified as won.
		 *
		 * @returns Const or mutable reference to the computed results bitbase.
		 */
		const Bitbase& getComputedResults() const { return _computedResults; }
		Bitbase& getComputedResults() { return _computedResults; }

		// Returns copies of the candidate bitmaps for BitWorkpackage construction.
		Bitbase getCandidates() const { return _candidates; }
		Bitbase getCandidateResults() const { return _candidateResults; }

		/**
		 * Adds a batch of candidate indexes for future processing.
		 *
		 * @param candidates Candidate indexes to mark.
		 */
		void setCandidates(const vector<CandidateEntry>& candidates) {
			for (const auto& entry : candidates) {
				setCandidate(entry.index, entry.winningMove);
			}
		}

		/**
		 * Thread-safe candidate insertion.
		 * Acquires mutex, then writes the batch with plain (non-atomic) bit ops.
		 *
		 * @param candidates Candidate indexes to add.
		 */
		void setCandidatesTreadSafe(const vector<CandidateEntry>& candidates) {
			const lock_guard<mutex> lock(_mtxUpdate);
			setCandidates(candidates);
		}

		/**
		 * Checks atomically (no mutex) whether a candidate is already registered
		 * in the shared state with at least the given priority.
		 * A relaxed load is used — thread-safe on all architectures, near-zero overhead.
		 *
		 * @param index Bitbase index to test.
		 * @param winningMove True if we want to add a winning candidate.
		 * @returns True if the entry is already present and no upgrade is needed.
		 */
		bool isCandidateSet(uint64_t index, bool winningMove) {
			if (!_candidates.getBitAtomic(index)) return false;
			if (winningMove && !_candidateResults.getBitAtomic(index)) return false;
			return true;
		}

		// Non-atomic variants for the plain-read alternative in addToCandidates.
		// Safe on x86/ARM in practice (no phantom 1 possible), but UB per C++ standard.
		bool isCandidate(uint64_t index) {
			return _candidates.getBit(index);
		}
		bool isCandidateResultSet(uint64_t index) {
			return _candidateResults.getBit(index);
		}

		/**
		 * Clears the complete candidate bitmap and candidate flag.
		 */
		void clearAllCandidates() {
			_candidates.clear();
			_candidateResults.clear();
			_hasCandidates = false;
		}

		/**
		 * Removes one index from the candidate bitmap.
		 *
		 * @param index Bitbase index to clear.
		 */
		void clearCandidate(uint64_t index) {
			_candidates.clearBit(index);
		}

		void setValue(uint64_t index, BitbaseResult value) {
			_computedResults.set2Bit(index, value);
			// Draw written here is an intermediate marker (drawing capture found during initialization),
			// not a final result. It is NOT counted here; finalizeDraws() counts it when confirmed final.
			_won  += (value == BitbaseResult::Win)  ? 1 : 0;
			_loss += (value == BitbaseResult::Loss) ? 1 : 0;
			_totalWon  += (value == BitbaseResult::Win)  ? 1 : 0;
			_totalLoss += (value == BitbaseResult::Loss) ? 1 : 0;
		}

		/**
		 * Marks one index as won and computed.
		 *
		 * @param index Bitbase index to mark.
		 */
		void setWin(uint64_t index) {
			_won++;
			_totalWon++;
			_computedResults.set2Bit(index, BitbaseResult::Win);
		}

		/**
		 * Marks one index as loss and computed.
		 *
		 * @param index Bitbase index to mark.
		 */
		void setLoss(uint64_t index) {
			_loss++;
			_totalLoss++;
			_computedResults.set2Bit(index, BitbaseResult::Loss);
		}

		/**
		 * Marks one index as draw and computed.
		 *
		 * @param index Bitbase index to mark.
		 */
		void setDraw(uint64_t index) {
			_computedResults.set2Bit(index, BitbaseResult::Draw);
		}


		/**
		 * Marks one index as illegal and computed.
		 *
		 * @param index Bitbase index to mark.
		 */
		void setIllegal(uint64_t index) {
			_illegal++;
			_totalIllegal++;
			// Illegal positions are marked as Win so that isFinal() returns true and
			// propagation never reclassifies them. Win=1 fits in 1 bit, preserving
			// compressibility. Illegal indices are never queried during lookup.
			_computedResults.set2Bit(index, BitbaseResult::Win);
		}

		/**
		 * Finalizes all positions that are not yet finally resolved as draws.
		 * After iterative propagation, any Unknown position could not be proven as win
		 * or loss — it is a draw. Intermediate Draw markers (drawing capture found during
		 * initialization) are already Draw in the bitbase and need no write.
		 * The draw counter is computed arithmetically at the end: no extra bitbase reads.
		 */
		void finalizeDraws() {
			for (uint64_t index = 0; index < _entryCount; ++index) {
				if (_computedResults.get2Bits(index) == BitbaseResult::Unknown) {
					_computedResults.set2Bit(index, BitbaseResult::Draw);
				}
			}
		}

		/**
		 * Prints cumulative statistics across all generated bitbases.
		 */
		static void printTotalStatistic() {
			uint64_t total = _totalEntryCount.load();
			if (total == 0) return;
			uint64_t totalDraw = total - _totalWon - _totalLoss - _totalIllegal;
			cout << "Total Statistic:"
				<< " Won: "     << _totalWon     << " (" << (_totalWon     * 100 / total) << "%) "
				<< " Draw: "    << totalDraw    << " (" << (totalDraw    * 100 / total) << "%) "
				<< " Loss: "    << _totalLoss    << " (" << (_totalLoss    * 100 / total) << "%) "
				<< " Illegal: " << _totalIllegal << " (" << (_totalIllegal * 100 / total) << "%) "
				<< std::endl;
		}

		/**
		 * Prints summary statistics for wins, draws, losses, unknown, and illegal positions.
		 */
		void printStatistic() {
			_totalEntryCount += _entryCount;
			uint64_t draw    = _computedResults.computeResults(BitbaseResult::Draw);
			uint64_t loss    = _computedResults.computeResults(BitbaseResult::Loss);
			uint64_t win     = _computedResults.computeResults(BitbaseResult::Win);
			uint64_t illegal = _computedResults.computeResults(BitbaseResult::Unknown);
			uint64_t unknown = _entryCount - win - draw - loss - illegal;

			cout
				<< "Won: "     << _won     << " (" << (_won     * 100 / _entryCount) << "%) "
				<< " Draw: "   << draw     << " (" << (draw     * 100 / _entryCount) << "%)"
				<< " Loss: "   << _loss    << " (" << (_loss    * 100 / _entryCount) << "%)"
				<< " Illegal: "<< _illegal << " (" << (_illegal * 100 / _entryCount) << "%)"
				<< " Unknown: "<< unknown  << " (" << (unknown  * 100 / _entryCount) << "%)"
				<< " Uncompressed memory size " << _computedResults.getSize()
				<< std::endl;
			if (_won != win) {
				std::cout << "Error, won positions do not match! Counter: " << _won
					<< " Bitbase: " << win << std::endl;
			}
			if (_loss != loss) {
				std::cout << "Error, loss positions do not match! Counter: " << _loss
					<< " Bitbase: " << loss << std::endl;
			}
			if (_illegal != illegal) {
				std::cout << "Error, illegal positions do not match! Counter: " << _illegal
					<< " Bitbase: " << illegal << std::endl;
			}
			if (unknown > 0) {
				std::cout << "Error, " << unknown << " positions have no final value (incomplete generation)!" << std::endl;
			}
		}

		/**
		 * Persists the won-position bitbase to disk.
		 *
		 * @param fileName Output filename.
		 * @param signature Signature used in bitbase metadata.
		 * @param compression Compression algorithm for file storage.
		 */
		void storeToFile(string fileName, string signature, QaplaCompress::CompressionType compression) {
			_computedResults.setFilename(signature, ".btb");
			_computedResults.storeToFile(fileName, compression);
		}

		/**
		 * Generates C++ source output for the won-position bitbase.
		 *
		 * @param signature Signature used for generated artifact names.
		 */
		void generateCpp(string signature) {
			_computedResults.writeAsCppFile(signature, signature + ".h");
		}

		/**
		 * Prints internal bitmaps for debugging.
		 */
		void print() {
			std::cout << "Won positions: " << std::endl;
			_computedResults.print();
			std::cout << "Candidates: " << std::endl;
			_candidates.print();
		}

	private:
		/**
		 * Returns the priority of a candidate result for a position.
		 * A "decisive" result (tryDirectEntry fires) ranks highest (2),
		 * a draw ranks medium (1), and an indecisive result ranks lowest (0).
		 *
		 * @param result The candidate result to evaluate.
		 * @param whiteToMove True if it is white's turn in the candidate position.
		 * @returns Priority value 0, 1, or 2.
		 */
		static int candidatePriority(BitbaseResult result, bool whiteToMove) {
			if ((result == BitbaseResult::Win  &&  whiteToMove) ||
				(result == BitbaseResult::Loss && !whiteToMove)) return 2;
			if (result == BitbaseResult::Draw) return 1;
			return 0;
		}

		/**
		 * Marks one index as candidate for future processing.
		 * If the index is already a candidate, the result is only updated when the new
		 * result has strictly higher priority, so a decisive result is never overwritten
		 * by a weaker one.
		 *
		 * @param index Bitbase index to mark.
		 * @param result The result that made this position a candidate.
		 */
		void setCandidate(uint64_t index, bool winningMove) {
			// Read first (cheap) — only pay for atomic write when bit is not yet set.
			// Worst case: two threads both pass the read check and both write; that is
			// harmless because setBitAtomic uses fetch_or (idempotent).
			if (!_candidates.getBit(index)) {
				_hasCandidates = true;
				_candidates.setBit(index);
			}
			if (winningMove && !_candidateResults.getBit(index)) {
				_candidateResults.setBit(index);
			}
		}

		uint64_t _entryCount;
		std::atomic<uint64_t> _illegal;
		std::atomic<uint64_t> _loss;
		std::atomic<uint64_t>  _won;
		std::atomic<uint64_t>  _hasCandidates;

		inline static std::atomic<uint64_t> _totalWon{ 0 };
		inline static std::atomic<uint64_t> _totalEntryCount{ 0 };
		inline static std::atomic<uint64_t> _totalLoss{ 0 };
		inline static std::atomic<uint64_t> _totalIllegal{ 0 };
		// Bitbase holding currently computed results
		Bitbase _computedResults;

		Bitbase _candidates;
		Bitbase _candidateResults;
		PieceList _pieceList;
		mutex _mtxUpdate;
	};

}
