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
 * Implements a static class to evaluate rooks 
 */

#ifndef __ROOK_H
#define __ROOK_H

#include <cstdint>
#include <vector>
#include "../basics/types.h"
#include "../movegenerator/bitboardmasks.h"
#include "../movegenerator/movegenerator.h"
#include "../basics/evalvalue.h"
#include "../basics/pst.h"
#include "print-eval.h"
#include "eval-map.h"
#include "evalresults.h"

using namespace QaplaBasics;
using namespace QaplaMoveGenerator;

namespace ChessEval {
	class Rook {
	public:
		static EvalValue eval(const MoveGenerator& position, EvalResults& results) {
			return evalColor<WHITE, false>(position, results, nullptr) - evalColor<BLACK, false>(position, results, nullptr);
		}

		static EvalValue evalWithDetails(const MoveGenerator& position, EvalResults& results, std::vector<PieceInfo>& details) {
			return evalColor<WHITE, true>(position, results, &details) - evalColor<BLACK, true>(position, results, &details);
		}

		static IndexLookupMap getIndexLookup() {
			IndexLookupMap indexLookup;
			indexLookup["rMobility"] = std::vector<EvalValue>{ ROOK_MOBILITY_MAP.begin(), ROOK_MOBILITY_MAP.end()};
			indexLookup["rProperty"] = std::vector<EvalValue>{ evalMap.begin(), evalMap.end() };
			indexLookup["rPST"] = PST::getPSTLookup(ROOK);
			return indexLookup;
		}
	private:

		/**
		 * Evaluates Rooks
		 */
		template<Piece COLOR, bool STORE_DETAILS>
		static EvalValue evalColor(const MoveGenerator& position, EvalResults& results, std::vector<PieceInfo>* details) {
			constexpr Piece OPPONENT = COLOR == WHITE ? BLACK : WHITE;
			EvalValue value;
			results.rookAttack[COLOR] = 0;
			bitBoard_t rooks = position.getPieceBB(ROOK + COLOR);
			if (rooks == 0) return EvalValue(0, 0);

			bitBoard_t passThrough = results.queensBB | rooks;
			bitBoard_t occupiedBB = position.getAllPiecesBB();
			bitBoard_t removeMask = (~position.getPiecesOfOneColorBB<COLOR>() | passThrough) &
				~position.pawnAttack[OPPONENT];
			occupiedBB &= ~passThrough;
			const uint32_t row7Index = rooksOnRow7Index<COLOR>(rooks, position.getPieceBB(QUEEN + COLOR));

			while (rooks)
			{
				const Square rookSquare = popLSB(rooks);
				const auto propertyIndex = calcPropertyIndex<COLOR>(position, results, row7Index, rookSquare);
				const auto mobilityIndex = calcMobilityIndex<COLOR>(results, rookSquare, occupiedBB, removeMask);

				const auto mobilityValue = EvalValue(ROOK_MOBILITY_MAP[mobilityIndex]);
				// const auto mobilityValue = position.getEvalVersion() == 0 ? EvalValue(ROOK_MOBILITY_MAP[mobilityIndex]) : CandidateTrainer::getCurrentCandidate().getWeightVector(0)[mobilityIndex];

				const auto propertyValue = evalMap[propertyIndex];
				//const auto propertyValue = position.getEvalVersion() == 0 ? evalMap[propertyIndex] : CandidateTrainer::getCurrentCandidate().getWeightVector(0)[propertyIndex];

				value += mobilityValue + propertyValue;

				if constexpr (STORE_DETAILS) {
					const auto materialValue = position.getPieceValue(ROOK + COLOR);
					const auto pstValue = PST::getValue(rookSquare, ROOK + COLOR);
					const auto mobility = COLOR == WHITE ? mobilityValue : -mobilityValue;
					const auto property = COLOR == WHITE ? propertyValue : -propertyValue;
					IndexVector indexVector{ { "rMobility", mobilityIndex, COLOR }, 
						{ "rPST", uint32_t(switchSideToWhite<COLOR>(rookSquare)), COLOR },
						{ "material", ROOK, COLOR } };
					if (propertyIndex) {
						indexVector.push_back({ "rProperty", propertyIndex, COLOR });
					}
					const auto value = mobility + property + materialValue + pstValue;
					details->push_back({ ROOK + COLOR, rookSquare, indexVector, propertyIndexToString(propertyIndex), value });
				}
			}
			return value;
		}

		/**
		 * Calculates several properties for a rook and return their value
		 * Is on open file
		 * Is on half open file
		 * Is protecting a passed pawn from behind
		 * Is trapped by king
		 */
		template<Piece COLOR>
		static inline uint32_t calcPropertyIndex(const MoveGenerator& position, EvalResults& results,
			uint32_t rookOnRow7Index, Square rookSquare)
		{
			uint32_t rookIndex = 0;
			constexpr Piece OPPONENT = COLOR == WHITE ? BLACK : WHITE;
			const bitBoard_t ourPawnBB = position.getPieceBB(PAWN + COLOR);
			const bitBoard_t theirPawnBB = position.getPieceBB(PAWN + OPPONENT);
			const Square rank8Destination = (COLOR == WHITE ? A8 : A1) + Square(getFile(rookSquare));
			const bitBoard_t moveRay = BitBoardMasks::fileBB[int(getFile(rookSquare))];
			rookIndex += isOnOpenFile(results.pawnsBB, moveRay) * OPEN_FILE;
			rookIndex += isOnHalfOpenFile(ourPawnBB, moveRay) * HALF_OPEN_FILE;
			rookIndex += trappedByKing<COLOR>(rookSquare, position.getKingSquare<COLOR>()) * TRAPPED;
			rookIndex +=
				protectsPassedPawnFromBehind<COLOR>(results.passedPawns[COLOR], rookSquare, moveRay) * PROTECTS_PP;
			rookIndex += isPinned(position.pinnedMask[COLOR], rookSquare) * PINNED;
			if (squareToBB(rookSquare) & ROW_7[COLOR]) {
				rookIndex += rookOnRow7Index;
			}
			return rookIndex;
		}

		/**
		 * Calculates the results of a rook
		 */
		template<Piece COLOR>
		static uint32_t calcMobilityIndex(
			EvalResults& results, Square square, bitBoard_t occupiedBB, bitBoard_t removeBB)
		{
			bitBoard_t attackBB = Magics::genRookAttackMask(square, occupiedBB);
			results.rookAttack[COLOR] |= attackBB;
			results.piecesDoubleAttack[COLOR] |= results.piecesAttack[COLOR] & attackBB;
			results.piecesAttack[COLOR] |= attackBB;

			attackBB &= removeBB;
			return popCount(attackBB);
		}

		/**
		 * Prints a rook index
		 */
		static string propertyIndexToString(uint16_t rookIndex) {
			string result = "";
			if (rookIndex & OPEN_FILE) { result += "of,"; }
			if (rookIndex & HALF_OPEN_FILE) { result += "hof,"; }
			if (rookIndex & TRAPPED) { result += "tbk,"; }
			if (rookIndex & PROTECTS_PP) { result += "ppp,"; }
			if (rookIndex & PINNED) { result += "pin,"; }
			if (((rookIndex / ROW_7_INDEX) & 7) == 1) { result += "r7,"; }
			if (((rookIndex / ROW_7_INDEX) & 7) == 2) { result += "rr7,"; }
			if (((rookIndex / ROW_7_INDEX) & 7) == 5) { result += "rq7,"; }
			if (((rookIndex / ROW_7_INDEX) & 7) == 6) { result += "rrq7,"; }
			if (!result.empty() && result.back() == ',') result.pop_back();
			return result;
		}

		/**
		 * Returns true, if the rook is pinned
		 */
		static constexpr bool isPinned(bitBoard_t pinnedBB, Square square) {
			return (pinnedBB & squareToBB(square)) != 0;
		}

		/**
		 * Returns true, if the rook is on an open file
		 */
		static constexpr bool isOnOpenFile(bitBoard_t pawnsBB, bitBoard_t moveRay) {
			return (moveRay & pawnsBB) == 0;
		}

		/**
		 * Returns true, if the rook is on an half open file
		 */
		static constexpr bool isOnHalfOpenFile(bitBoard_t ourPawnBB, bitBoard_t moveRay) {
			return (moveRay & ourPawnBB) == 0;
		}

		/**
		 * Returns true, if the rook protects a passed pawn from behind
		 */
		template <Piece COLOR>
		static constexpr bool protectsPassedPawnFromBehind(bitBoard_t passedPawns, Square rookSquare, bitBoard_t moveRay) {
			bitBoard_t protectBB = (moveRay & passedPawns);
			if constexpr (COLOR == WHITE) {
				return (squareToBB(rookSquare) < protectBB);
			}
			else {
				return (protectBB != 0 && squareToBB(rookSquare) > protectBB);
			}
		}

		/**
		 * Returns true, if the rook is trapped by a king
		 */
		template <Piece COLOR>
		static inline bool trappedByKing(Square rookSquare, Square kingSquare) {
			static constexpr bitBoard_t kingSide[2] = { 0x00000000000000E0, 0xE000000000000000 };
			static constexpr bitBoard_t queenSide[2] = { 0x000000000000000F, 0x0F00000000000000 };
			const bitBoard_t rookAndKingBB = squareToBB(rookSquare) | squareToBB(kingSquare);
			const bitBoard_t kingSideBB = kingSide[COLOR] & rookAndKingBB;
			const bitBoard_t queenSideBB = queenSide[COLOR] & rookAndKingBB;
			bool trappedKingSide = (kingSquare < rookSquare) && (kingSideBB == rookAndKingBB);
			bool trappedQueenSide = (kingSquare > rookSquare) && (queenSideBB == rookAndKingBB);
			return trappedKingSide || trappedQueenSide;
		}

		template <Piece COLOR>
		static inline uint32_t rooksOnRow7Index(bitBoard_t rookBB, bitBoard_t queenBB) {
			const auto rooksRow7BB = rookBB & ROW_7[COLOR];
			uint32_t index = ((queenBB & ROW_7[COLOR]) != 0) * 4;
			if (rooksRow7BB > 0) {
				index++;
				if (rooksRow7BB & (rooksRow7BB - 1)) index++;
			}
			return index * ROW_7_INDEX;
		}

		static constexpr bitBoard_t ROW_7[COLOR_COUNT] = { 0x00FF000000000000, 0x000000000000FF00 };

		// Row 7 map per rook. Seems to be pretty useless in playingstrength but also does not hurt
		static constexpr std::array<EvalValue, 8> ROOK_ROW_7_MAP = { {
				// no rook, one rook, two rooks or more, not defined, queen(s), one rook and queen(s), two rooks or more and queen(s), not defined
				{ 0, 0 }, { 10, 0 }, { 10, 0 }, { 0, 0 }, { 0, 0 }, { 20, 0 }, { 20, 0 }, { 0, 0 }
			} };

		// Mobility Map
		static constexpr std::array<EvalValue, 15> ROOK_MOBILITY_MAP = { {
			{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 8, 8 }, { 12, 12 }, { 16, 16 },
			{ 20, 20 }, { 25, 25 }, { 25, 25 }, { 25, 25 }, { 25, 25 }, { 25, 25 }, { 25, 25 }
		} };

		static const uint32_t INDEX_SIZE = 32 * 8;
		static const uint32_t TRAPPED = 1;
		static const uint32_t OPEN_FILE = 2;
		static const uint32_t HALF_OPEN_FILE = 4;
		static const uint32_t PROTECTS_PP = 8;
		static const uint32_t PINNED = 0x10;
		static const uint32_t ROW_7_INDEX = 0x20;
		
		inline static array<EvalValue, INDEX_SIZE> evalMap = [] {
			const value_t _trapped[2] = { -50, -16 };
			const value_t _openFile[2] = { 18, 6 };
			const value_t _halfOpenFile[2] = { 10, 0 };
			const value_t _protectsPP[2] = { 25, 0 };
			const value_t _pinned[2] = { -23, 0 };

			array<EvalValue, INDEX_SIZE> map;
			for (uint32_t bitmask = 0; bitmask < INDEX_SIZE; ++bitmask) {
				EvalValue value;
				if (bitmask & TRAPPED) { value += _trapped; }
				if (bitmask & OPEN_FILE) { value += _openFile; }
				if (bitmask & HALF_OPEN_FILE) { value += _halfOpenFile; }
				if (bitmask & PROTECTS_PP) { value += _protectsPP; }
				if (bitmask & PINNED) { value += _pinned; }
				if (bitmask / ROW_7_INDEX) { value += ROOK_ROW_7_MAP[bitmask / ROW_7_INDEX]; }
				map[bitmask] = value;
			}
			return map;
		} ();
	};
}

// Rook Test positions
// A8, of, hof; E8, hof; 
// 4r1kr/rpqQ1ppR/2pP1n2/1PNn4/3N2b1/4b1P1/rRrRPR2/2R3KB b - - 0 1


#endif

