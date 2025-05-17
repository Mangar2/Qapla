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
 * Implements a transposition table for chess
 * Each entry consists of two elements:
 * The first element stores the "best search" usually with the largest search depth.
 * The second element stores the most actual element and overwrites always.
 */

#ifndef __TT_H
#define __TT_H

#include <vector>
#include <fstream>
#include <iterator>
#include <type_traits>
#include <cassert>
#include "ttentry.h"
#include "../eval/pawntt.h"
// #include "FileClass.h"

using namespace std;

namespace QaplaSearch {
	class TT
	{
	public:

		TT() { 
			clear(); 
			_pawnTT.setSizeInKilobytes(1024);
		}

		/**
		 * Clears the transposition table
		 */
		void clear()
		{
			for (TTEntry& entry : _tt) {
				entry.clear();
			}
			_ageIndicator = 0;
			_entries = 0;
			_pawnTT.clear();
			
		}

		ChessEval::PawnTT* getPawnTT() {
			return &_pawnTT;
		}

		/**
		 * For assertions
		 */
		bool hasDrawEntry() const {
			for (uint32_t i = 0; i < _tt.size(); i++) {
				if (_tt[i].alwaysUseValue()) {
					return true;
				}
			}
			return false;
		}

		/**
		 * checks if the new entry is more valuable to store than the current entry
		 * Tested, but not good: overwrite less, if no hash move is provided
		 */
		bool isNewEntryMoreValuable(uint32_t index, ply_t computedDepth, Move move, bool isPV) const {
			if (isEntryFromFormerSearch(_tt[index])) return true;
			// We always overwrite on PV
			if (isPV) return true;
			auto entry = getEntry(index);
			if (entry.isPV()) return false;
			int16_t newWeight = computedDepth + !move.isEmpty() * 2;
			int16_t oldWeight = entry.getComputedDepth() + !entry.getMove().isEmpty() * 2;
			return newWeight >= oldWeight;
		}

		/**
		 * Gets the size of the transposition table in bytes
		 */
		size_t getSizeInBytes() const { 
			return _tt.size() * sizeof(TTEntry); 
		}

		/**
		 * Computes the hash index of a hash key 
		 */
		int32_t	computeEntryIndex(hash_t hashKey) const {
			return int32_t(hashKey % _tt.size()) & ~1;
		}

		/**
		 * Resizes the tt so that it has a certain amount of kilobytes
		 */
		void setSizeInKilobytes(int32_t sizeInKiloBytes)
		{
			uint64_t newSize = ( 1024ULL * sizeInKiloBytes) / sizeof(TTEntry);
			setSize(newSize);
		}

		/**
		 * Sets a hash entry either to the primary entry (if better) or to 
		 * the secondary always replace entry
		 */
		uint32_t setEntry(
			hash_t hashKey, bool isPV, int32_t computedDepth, ply_t ply, Move move,
			value_t eval, value_t positionValue, value_t alpha, value_t beta, int32_t nullmoveThreat)
		{
			uint32_t index = computeEntryIndex(hashKey);
			/*
			if (hashKey == 7541197627045278876) {
				printHash(hashKey);
			}
			*/
			if (_tt[index].isEmpty())
			{
				_entries++;
				set(index, isPV, hashKey, computedDepth, ply, move, eval, positionValue, alpha, beta, nullmoveThreat);
				return index;
			}

			bool hashIsDifferent = !_tt[index].hasHash(hashKey);
			if (!(hashIsDifferent && !_tt[index].isPV()) || isNewEntryMoreValuable(index, computedDepth, move, isPV))
			{
				if (hashIsDifferent && _tt[index + 1].doOverwriteAlwaysReplaceEntry(
					positionValue, alpha, beta, computedDepth)) 
				{
					// Logically equivalent to: _tt[index + 1] = new; swap(_tt[index], _tt[index + 1]);
					// This allows checking if _tt[index + 1] was from a previous search before overwriting.
					// If so, we can safely increment _entries only once for _tt[index + 1].
					if (isEntryFromFormerSearch(_tt[index + 1])) _entries++;
					_tt[index + 1] = _tt[index];
				}
				set(index, isPV, hashKey, computedDepth, ply, move, eval, positionValue, alpha, beta, nullmoveThreat);
			}
			else if (_tt[index + 1].doOverwriteAlwaysReplaceEntry(positionValue, alpha, beta, computedDepth))
			{
				if (isEntryFromFormerSearch(_tt[index + 1])) _entries++;
				set(index + 1, isPV, hashKey, computedDepth, ply, move, eval, positionValue, alpha, beta, nullmoveThreat);
			}
			return index;
		}


		/**
		 * Gets a valid tt entry index
		 * @returns tt entry index with the correct hash signature or INVALID_INDEX
		 */
		uint32_t getTTEntryIndex(hash_t hashKey) const
		{
			uint32_t index = computeEntryIndex(hashKey);
			uint32_t result = INVALID_INDEX;
			if (_tt[index].hasHash(hashKey)) {
				result = index;
			}
			else if (_tt[index + 1].hasHash(hashKey)) {
				result = index + 1;
			}
			return result;
		}

		/**
		 * Gets a element by index
		 */
		inline const TTEntry& getEntry(uint32_t index) const {
			return _tt[index];
		}

		/**
	     * Gets a element by index
		 */
		inline TTEntry& getEntry(uint32_t index) {
			return _tt[index];
		}

		/**
		 * Checks, if the hash indicates a beta-cutoff situation
		 */
		bool isTTValueBelowBeta(hash_t hashKey, value_t beta, ply_t ply) const {
			hash_t index = getTTEntryIndex(hashKey);
			bool result = false;
			if (index != INVALID_INDEX) {
				result = _tt[index].isTTValueBelowBeta(beta, ply);
			}
			return result;
		}

		/**
		 * Prints a full hash entry of two elements
		 */
		void printHash(hash_t hashKey) const
		{
			uint32_t index = computeEntryIndex(hashKey);
			printf("1. ");
			printHashEntry(index);
			printf("2. ");
			printHashEntry(index + 1);
		}

		/**
		 * Sets needed values to indicate a next search
		 */
		void setNextSearch()
		{
			_ageIndicator++;
			_ageIndicator &= TTEntry::getAgeIndicatorRangeMask();
			_entries = 0;
		}

		/**
		 * Calculates an optimized amount of entries
		 */
		int32_t optimizeHashEntryAmount(int32_t sizeInKiloBytes) const
		{
			int32_t newEntryAmount = sizeInKiloBytes * (1024 / sizeof(TTEntry));
			if (newEntryAmount % 2 == 1) newEntryAmount--;
			if (newEntryAmount < 16) {
				newEntryAmount = 16;
			}
			return newEntryAmount;
		}
	
		/**
		 * Gets the age indicator of the current search
		 */
		int32_t getEntryAgeIndicator() const { 
			return _ageIndicator; 
		}

		/**
		 * Prints a hash entry for debugging
		 */
		void printHashEntry(uint32_t index) const {
			auto& entry = _tt[index];
		
			if (entry.isEmpty()) {
				std::cout << "<Empty>\n";
			} else {
				std::cout << "[key:" << std::hex << entry.getHash() << std::dec << "]"
						  << "[idx:" << index << "]"
						  << "[dpt:" << entry.getComputedDepth() << "]"
						  << "[val:" << entry.getPositionValue(0) << "]"
						  << "[eval:" << entry.getEval() << "]"
						  << "[pre:" << entry.getComputedPrecision() << "]"
						  << "[mov:" << entry.getMove().getLAN() << "]\n";
			}
		}

		/**
		 * Gets the fill rate in percent only counting entries of current search
		 */
		uint32_t getHashFillRateInPermill() const {
			return uint32_t(uint64_t(_entries) * 1000ULL / _tt.size());
		}

		/**
		 * Writes the current transposition table to the provided file.
		 * @param filename Name of the file to write the transposition table to.
		 */
		bool write(const std::string& filename) const {
			std::ofstream ofs(filename, std::ios::binary);
			if (!ofs) {
				return false;
			}

			int64_t size = static_cast<int64_t>(_tt.size());
			ofs.write(reinterpret_cast<const char*>(&size), sizeof(size));
			ofs.write(reinterpret_cast<const char*>(&_ageIndicator), sizeof(_ageIndicator));
			ofs.write(reinterpret_cast<const char*>(&_entries), sizeof(_entries));

			if (!_tt.empty()) {
				ofs.write(reinterpret_cast<const char*>(_tt.data()), sizeof(TTEntry) * _tt.size());
			}
			return true;
		}

		/**
 		 * Reads a transposition table from the provided file.
		 * @param filename Name of the file to read the transposition table from.
		 */
		bool read(const std::string& filename) {
			std::ifstream ifs(filename, std::ios::binary);
			if (!ifs) {
				return false;
			}

			int64_t size = 0;
			ifs.read(reinterpret_cast<char*>(&size), sizeof(size));
			ifs.read(reinterpret_cast<char*>(&_ageIndicator), sizeof(_ageIndicator));
			ifs.read(reinterpret_cast<char*>(&_entries), sizeof(_entries));

			_tt.resize(size);

			if (!_tt.empty()) {
				ifs.read(reinterpret_cast<char*>(_tt.data()), sizeof(TTEntry) * _tt.size());
			}
			return true;
		}

		static const uint32_t INVALID_INDEX = UINT32_MAX;

	private:

		/**
		 * Sets the values of a single entry
		 */
		void set(
			uint32_t index, bool isPV, hash_t hashKey, 
			uint32_t computedDepth, ply_t ply, Move move, 
			value_t eval, value_t positionValue, value_t alpha, value_t beta, uint32_t nullmoveThreat)
		{
			auto& entry = _tt[index];
			entry.setInfo(computedDepth, _ageIndicator, nullmoveThreat);
			entry.setEval(eval);
			entry.setValue(positionValue, alpha, beta, ply);
			entry.setPV(isPV);
			// Keep the hash move, if the hash keys are identical and the new entry does not provide a move
			if (!move.isEmpty() || !entry.hasHash(hashKey)) {
				entry.setMove(move);
			}
			entry.setTT(hashKey);
		}

		/**
		 * Sets the transposition table size
		 */
		void setSize(uint64_t newSize) {
			_tt = std::vector<TTEntry>(newSize);
			clear();
		}

		/** 
		 * returns true, if the entry is not from the current search
		 */ 
		bool isEntryFromFormerSearch(const TTEntry& entry) const {
			return _ageIndicator != entry.getAgeIndicator();
		}

		// Transposition table
		vector<TTEntry> _tt;
		int32_t _ageIndicator;
		int32_t _entries;


		// Pawn hash
		ChessEval::PawnTT _pawnTT;

	};

}

#endif // __TT_H
