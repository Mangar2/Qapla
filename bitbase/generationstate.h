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

#include <mutex>
#include <atomic>
#include "bitbase.h"
#include "bitbaseindex.h"
#include "compress.h"

namespace QaplaBitbase {

	struct CandidateEntry {
		uint64_t index;
		BitbaseResult result;
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
			_computedResults.setLoaded();
			_computedPositions = Bitbase(_entryCount, 1, sig);
			_computedPositions.resize(_entryCount);
			_computedPositions.setLoaded();
			_candidates = Bitbase(_entryCount, 1, sig);
			_candidates.resize(_entryCount);
			_candidates.setLoaded();
			_candidateResults = Bitbase(_entryCount, 2, sig);
			_candidateResults.resize(_entryCount);
			_candidateResults.setLoaded();
			_pieceList = pieceList;
			_illegal = 0;
			_loss = 0;
			_draw = 0;
			_won = 0;
			_updateRunning = false;
		}

		/**
		 * Returns the piece layout of this generation state.
		 *
		 * @returns Piece list used to map index-to-position conversions.
		 */
		const PieceList& getPieceList() const { return _pieceList; }

		/**
		 * Checks whether a position should be processed in the current pass.
		 *
		 * @param index Bitbase index of the position.
		 * @param onlyCandidates If true, accepts only positions marked as candidates.
		 * @returns True when the position is not yet computed and passes candidate filtering.
		 */
		bool isPositionToCheck(uint64_t index, bool onlyCandidates) {
			return !_computedPositions.getBit(index) &&
				(!onlyCandidates || _candidates.getBit(index));
		}

		/**
		 * Checks whether a position has already been (finally) computed.
		 *
		 * @param index Bitbase index of the position.
		 * @returns True if the position is marked as computed.
		 */
		bool isPositionComputed(uint64_t index) {
			return _computedPositions.getBit(index);
		}

		/**
		 * Collects all indexes that still need processing.
		 *
		 * @param work Output vector receiving candidate work indexes.
		 */
		void getWork(vector<CandidateEntry>& work) {
			vector<uint64_t> indexes;
			_candidates.getAllIndexes(_computedPositions, indexes);
			work.reserve(indexes.size());
			for (auto idx : indexes) {
				work.push_back({idx, _candidateResults.get2Bits(idx)});
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

		/**
		 * Adds a batch of candidate indexes for future processing.
		 *
		 * @param candidates Candidate indexes to mark.
		 */
		void setCandidates(const vector<CandidateEntry>& candidates) {
			for (const auto& entry : candidates) {
				if (!_computedPositions.getBit(entry.index)) {
					setCandidate(entry.index, entry.result);
				}
			}
		}

		/**
		 * Thread-safe variant of candidate insertion.
		 *
		 * @param candidates Candidate indexes to add.
		 * @param wait If true, waits for lock; otherwise skips when another update is active.
		 * @returns True if candidates were merged, otherwise false.
		 */
		bool setCandidatesTreadSafe(const vector<CandidateEntry>& candidates, bool wait = true) {
			if (candidates.empty()) {
				return false;
			}
			if (wait || !_updateRunning) {
				const lock_guard<mutex> lock(_mtxUpdate);
				_updateRunning = true;
				setCandidates(candidates);
				_updateRunning = false;
				return true;
			} 
			else {
				return false;
			}
		}

		/**
		 * Checks whether an index is currently marked as candidate.
		 *
		 * @param index Bitbase index to test.
		 * @returns True if the index is marked in the candidate bitmap.
		 */
		bool isCandidate(uint64_t index) {
			return _candidates.getBit(index);
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

		void setValue(uint64_t index, BitbaseResult value, bool finalValue) {
			_computedResults.set2Bit(index, value);
			if (finalValue) {
				_computedPositions.setBit(index);
				_won  += (value == BitbaseResult::Win)  ? 1 : 0;
				_loss += (value == BitbaseResult::Loss) ? 1 : 0;
				_draw += (value == BitbaseResult::Draw) ? 1 : 0;
			}
		}

		/**
		 * Marks one index as won and computed.
		 *
		 * @param index Bitbase index to mark.
		 */
		void setWin(uint64_t index) {
			_won++;
			_computedResults.set2Bit(index, BitbaseResult::Win);
			_computedPositions.setBit(index);
		}

		/**
		 * Marks one index as loss and computed.
		 *
		 * @param index Bitbase index to mark.
		 */
		void setLoss(uint64_t index) {
			_loss++;
			_computedResults.set2Bit(index, BitbaseResult::Loss);
			_computedPositions.setBit(index);
		}

		/**
		 * Marks one index as draw and computed.
		 *
		 * @param index Bitbase index to mark.
		 */
		void setDraw(uint64_t index) {
			_draw++;
			_computedResults.set2Bit(index, BitbaseResult::Draw);
			_computedPositions.setBit(index);
		}


		/**
		 * Marks one index as illegal and computed.
		 *
		 * @param index Bitbase index to mark.
		 */
		void setIllegal(uint64_t index) {
			_illegal++;
			// Illegal positions are marked as draw to be able to compress bitboards without loss for white to one bit.
			_computedResults.set2Bit(index, BitbaseResult::Draw);
			_computedPositions.setBit(index);
		}

		/**
		 * Finalizes all positions that are not yet resolved as draws.
		 * After iterative propagation, any position without a final value could not be
		 * proven as win or loss — by definition these are draws (neither side can force a result).
		 * This also corrects intermediate lower-bound values (e.g. provisional Loss entries
		 * written during initialization) that were never finalized.
		 */
		void finalizeDraws() {
			for (uint64_t index = 0; index < _entryCount; ++index) {
				if (!_computedPositions.getBit(index)) {
					setDraw(index);
				}
			}
		}

		/**
		 * Prints summary statistics for wins, draws, losses, unknown, and illegal positions.
		 */
		void printStatistic() {
			uint64_t unknown = _entryCount - _won - _draw - _loss - _illegal;
			cout
				<< "Won: "     << _won     << " (" << (_won     * 100 / _entryCount) << "%) "
				<< " Draw: "   << _draw    << " (" << (_draw    * 100 / _entryCount) << "%)"
				<< " Loss: "   << _loss    << " (" << (_loss    * 100 / _entryCount) << "%)"
				<< " Unknown: "<< unknown  << " (" << (unknown  * 100 / _entryCount) << "%)"
				<< " Illegal: "<< _illegal << " (" << (_illegal * 100 / _entryCount) << "%)"
				<< " Uncompressed memory size " << _computedResults.getSize()
				<< std::endl;
			if (_won != _computedResults.computeResults(BitbaseResult::Win)) {
				std::cout << "Error, won positions do not match! Counter: " << _won
					<< " Bitbase: " << _computedResults.computeResults(BitbaseResult::Win) << std::endl;
			}
			// Illegal positions are stored as Draw=0, so the bitbase count includes both.
			if (_draw + _illegal != _computedResults.computeResults(BitbaseResult::Draw)) {
				std::cout << "Error, draw positions do not match! Counter: " << (_draw + _illegal)
					<< " Bitbase: " << _computedResults.computeResults(BitbaseResult::Draw) << std::endl;
			}
			if (_loss != _computedResults.computeResults(BitbaseResult::Loss)) {
				std::cout << "Error, loss positions do not match! Counter: " << _loss
					<< " Bitbase: " << _computedResults.computeResults(BitbaseResult::Loss) << std::endl;
			}
			uint64_t notFinal = _entryCount - _computedPositions.computeResults(BitbaseResult::Win);
			if (notFinal > 0) {
				std::cout << "Error, " << notFinal << " positions have no final value (incomplete generation)!" << std::endl;
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
			std::cout << "Computed positions: " << std::endl;
			_computedPositions.print();
			std::cout << "Candidates: " << std::endl;
			_candidates.print();
		}

	private:
		/**
		 * Marks one index as candidate for future processing.
		 *
		 * @param index Bitbase index to mark.
		 */
		void setCandidate(uint64_t index, BitbaseResult result) {
			_hasCandidates = true;
			_candidates.setBit(index);
			_candidateResults.set2Bit(index, result);
		}

		uint64_t _entryCount;
		std::atomic<uint64_t> _illegal;
		std::atomic<uint64_t> _loss;
		std::atomic<uint64_t> _draw;
		std::atomic<uint64_t>  _won;
		std::atomic<uint64_t>  _hasCandidates;
		// Bitbase holding currently computed results
		Bitbase _computedResults;

		Bitbase _computedPositions;
		Bitbase _candidates;
		Bitbase _candidateResults;
		PieceList _pieceList;
		mutex _mtxUpdate;
		bool _updateRunning;
	};

}
