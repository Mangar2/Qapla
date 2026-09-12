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
 * Workpackage for a thread in bitbase generation
 */

#ifndef __WORKPACKAGE_H
#define __WORKPACKAGE_H

#include <vector>
#include <mutex>
#include "bitbase.h"
#include "generationstate.h"

namespace QaplaBitbase {

	class Workpackage
	{
	public:
		Workpackage(GenerationState& state) {
			state.getWork(_workList);
			_workIndex = 0;
			_size = state.getEntryCount();
			_lastInfo = 0;
		}

		/**
		 * Gets the candidate entry of a work element
		 */
		CandidateEntry getCandidate(uint64_t workIndex) const { 
			return _workList[workIndex]; 
		}

		pair<uint64_t, uint64_t> getNextPackageToExamine(uint64_t count) {
			return getNextPackageToExamine(count, _workList.size());
		}

		/**
		 * Prints the progress depending on the traceleve (from level 2)
		 * @traceLevel current trace level (0..2)
		 * @workList true, if a worklist is in use
		 */
		void printProgress([[maybe_unused]]int traceLevel, bool workList) {
			uint64_t size = workList ? _workList.size() : _size;
			uint64_t onePercent = size / 100;
			if (_workIndex - _lastInfo >= onePercent) {
				cout << ".";
				_lastInfo = _workIndex;
				_lastInfo -= _lastInfo % onePercent;
			}
		}

		/**
		 * Gets the next working package, the function is thread safe (protected by a mutex) 
		 * @count number of work elements in the package
		 * @size total size of the work
		 * @return pair of index of the first element and number of elements to work on
		 */
		pair<uint64_t, uint64_t> getNextPackageToExamine(uint64_t count, uint64_t size) {
			const lock_guard<mutex> lock(_mtxWork);
			auto result = make_pair(_workIndex, min(_workIndex + count, size));
			_workIndex += count;
			return result;
		}


	private:
		std::vector<CandidateEntry> _workList;
		uint64_t _workIndex;
		uint64_t _lastInfo;
		mutex _mtxWork;
		uint64_t _size;
	};

	/**
	 * Lightweight workpackage for the initial scan phase.
	 * Distributes index ranges [0, entryCount) across threads without building
	 * any candidate list — during initial scan no candidates exist yet.
	 * Ranges are byte-aligned (multiples of 8) to avoid false sharing.
	 */
	class InitialWorkpackage
	{
	public:
		InitialWorkpackage(uint64_t entryCount)
			: _entryCount(entryCount), _workIndex(0) {}

		pair<uint64_t, uint64_t> getNextPackageToExamine(uint64_t count) {
			count = (count + 7) & ~uint64_t(7);
			const lock_guard<mutex> lock(_mtxWork);
			uint64_t first = _workIndex;
			if (first >= _entryCount) return {_entryCount, _entryCount};
			uint64_t last = min(first + count, _entryCount);
			_workIndex = last;
			return {first, last};
		}

	private:
		uint64_t _entryCount;
		uint64_t _workIndex;
		uint8_t _parityMask;
		mutex _mtxWork;
	};

	/**
	 * Alternative Workpackage that copies the candidate bitmaps at construction time.
	 * This avoids building a large CandidateEntry vector from the bitbase.
	 * getNextPackageToExamine always returns byte-aligned ranges (multiples of 8)
	 * so each thread reads its own bytes without false sharing on byte boundaries.
	 * getCandidate returns -1 (no candidate), 0 (candidate, losing move),
	 * or 1 (candidate, winning move).
	 */
	class BitWorkpackage
	{
	public:
		/**
		 * @param parity the side to move this round works on: 0 white, 1 black. The
		 *        lowest bit of the index is the side to move, so a round is every
		 *        second entry.
		 */
		BitWorkpackage(GenerationState& state, int parity)
			: _candidates(state.getCandidates())
			, _candidateResults(state.getCandidateResults())
			, _entryCount(state.getEntryCount())
			, _workIndex(0)
			, _parityMask(parity == 0 ? uint8_t(0x55) : uint8_t(0xAA))
		{
		}

		/**
		 * Returns -1 if index is not a candidate, 0 if losing candidate, 1 if winning candidate.
		 */
		int getCandidate(uint64_t index) const {
			if (!_candidates.getBitAtomic(index)) return -1;
			return _candidateResults.getBitAtomic(index) ? 1 : 0;
		}

		/**
		 * The eight candidate bits of one byte, with the bits of the other colour
		 * masked away. Zero means the whole byte can be skipped, which is what a round
		 * spends most of its time doing once the candidates become sparse.
		 */
		uint8_t getCandidateByte(uint64_t index) const {
			return uint8_t(_candidates.getBitByte(index / 8)) & _parityMask;
		}

		/**
		 * Returns the next byte-aligned [first, last) range of up to `count` entries.
		 * Both first and last are multiples of 8, so each thread works on whole bytes.
		 */
		pair<uint64_t, uint64_t> getNextPackageToExamine(uint64_t count) {
			// Round count up to next multiple of 8 so ranges stay byte-aligned.
			count = (count + 7) & ~uint64_t(7);
			const lock_guard<mutex> lock(_mtxWork);
			uint64_t first = _workIndex;
			if (first >= _entryCount) return {_entryCount, _entryCount};
			uint64_t last = min(first + count, _entryCount);
			_workIndex = last;
			return {first, last};
		}

	private:
		Bitbase _candidates;
		Bitbase _candidateResults;
		uint64_t _entryCount;
		uint64_t _workIndex;
		uint8_t _parityMask;
		mutex _mtxWork;
	};

}

#endif // __WORKPACKAGE_H
