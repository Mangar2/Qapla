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
 * Provides functions to evaluate the pawn structure
 */

#ifndef __PAWN_H
#define __PAWN_H

#include <map>
#include <vector>
#include <tuple>

#include "../movegenerator/bitboardmasks.h"
#include "../movegenerator/movegenerator.h"
#include "evalresults.h"
#include "pawnrace.h"
#include "pawntt.h"
#include "eval-helper.h"
#include "../interface/uci-parameter-provider.h"

#include "../basics/types.h"
#include "../basics/pst.h"
#include "../basics/evalvalue.h"

#ifdef PARAM_OPTIMIZE
#define PARAM_OPTIMIZE_PAWN
#endif
#define PARAM_OPTIMIZE_PAWN

using namespace std;
using namespace QaplaBasics;
using namespace QaplaMoveGenerator;

namespace ChessEval {


	struct EvalPawnValues {
		using RankArray_t = array<value_t, uint32_t(Rank::COUNT)>;
		using  FileArray_t = array<value_t, uint32_t(File::COUNT)>;

		static constexpr RankArray_t ADVANCED_PAWN_VALUE = { 0,  0,   0,   0,  0,  0,  0, 0 };
	};

	class Pawn {
	public:
		friend class PawnUciAccess;

		/**
		 * Get UCI parameter access interface
		 * @return Reference to UCI parameter provider
		 */
		static UciParameterProvider& getUciAccess();

		Pawn() {}

		typedef array<value_t, uint32_t(Rank::COUNT)> RankArray_t;
		typedef array<value_t, uint32_t(File::COUNT)> FileArray_t;

		static EvalValue eval(const MoveGenerator& position, EvalResults& results, PawnTT* pawnttPtr) {
			
			EvalValue ttValue = probeTT(position, results, pawnttPtr);
			if (ttValue.midgame() != NO_VALUE) {
				return ttValue;
			}
			
			colorBB_t moveRay{
				computePawnsMoveRays<WHITE>(position.getPieceBB(PAWN + WHITE)),
				computePawnsMoveRays<BLACK>(position.getPieceBB(PAWN + BLACK))
			};

			EvalValue value =
				evalColor<WHITE, false>(position, results, moveRay, nullptr)
				- evalColor<BLACK, false>(position, results, moveRay, nullptr);
			if (pawnttPtr != nullptr) {
				pawnttPtr->setEntry(position.getPawnHash(), value, results.passedPawns);
			}
			return value;
		}

		static EvalValue evalWithDetails(const MoveGenerator& position, EvalResults& results, std::vector<PieceInfo>& details) {
			colorBB_t moveRay{ 
				computePawnsMoveRays<WHITE>(position.getPieceBB(PAWN + WHITE)), 
				computePawnsMoveRays<BLACK>(position.getPieceBB(PAWN + BLACK)) 
			};

			EvalValue value = 
				evalColor<WHITE, true>(position, results, moveRay, &details)
				- evalColor<BLACK, true>(position, results, moveRay, &details);
			return value;
		}

		static IndexLookupMap getIndexLookup() {
			IndexLookupMap indexLookup;
			indexLookup["pProperty"] = std::vector<EvalValue>{ evalValueMap.begin(), evalValueMap.end() };
			indexLookup["pPST"] = PST::getPSTLookup(PAWN);
			indexLookup["ppThreat"] = std::vector<EvalValue>{ ppThreatMap.begin(), ppThreatMap.end() };
			return indexLookup;
		}

		/**
		 * Computes the value of the pawn structure in the case there is no
		 * piece on the position
		 */
		static value_t computePawnValueNoPiece(MoveGenerator& position, EvalResults& results) {
			colorBB_t moveRay{};
			moveRay[WHITE] = computePawnsMoveRays<WHITE>(position.getPieceBB(PAWN + WHITE));
			moveRay[BLACK] = computePawnsMoveRays<BLACK>(position.getPieceBB(PAWN + BLACK));

			value_t result = position.getMaterialAndPSTValue().endgame();
			result += computePawnValueNoPieceButPawn<WHITE>(position, results, moveRay);
			result -= computePawnValueNoPieceButPawn<BLACK>(position, results, moveRay);

			PawnRace pawnRace;
			value_t runnerValue = pawnRace.runnerRace(position, 
				results.passedPawns[WHITE], results.passedPawns[BLACK]);
			if (runnerValue != 0) {
				result /= 4;
				result += runnerValue;
			}
			return result;
		}

		static EvalValue evalPassedPawnThreats(const MoveGenerator& position, const EvalResults& results) {
			return evalPassedPawnThreats<WHITE, false>(position, results, nullptr) - evalPassedPawnThreats<BLACK, false>(position, results, nullptr);
		}

		static EvalValue evalPassedPawnThreatsWithDetails(const MoveGenerator& position, EvalResults& results, std::vector<PieceInfo>& details) {
			return evalPassedPawnThreats<WHITE, true>(position, results, &details) - evalPassedPawnThreats<BLACK, true>(position, results, &details);
		}

		static void computePassedPawns(const MoveGenerator& position, EvalResults& results) {
			colorBB_t moveRay{};
			moveRay[WHITE] = computePawnsMoveRays<WHITE>(position.getPieceBB(PAWN + WHITE));
			moveRay[BLACK] = computePawnsMoveRays<BLACK>(position.getPieceBB(PAWN + BLACK));
			results.passedPawns[WHITE] = computePassedPawnBB<WHITE>(position, moveRay[BLACK]);
			results.passedPawns[BLACK] = computePassedPawnBB<BLACK>(position, moveRay[WHITE]);
		}

	private:

		/**
		 * Computes the bitboard for passed pawns
		 * How it works: if a pawn is not in front of an opponent pawn or an opponent pawn on the left/right columns, it
		 * is a passed pawn.
		 */
		template<Piece COLOR>
		inline static bitBoard_t computePassedPawnBB(const MoveGenerator& position, bitBoard_t opponentPawnsMoveRays) 
		{
			auto pawns = position.getPieceBB(PAWN + COLOR);
			bitBoard_t passerMask = opponentPawnsMoveRays;
			passerMask |= BitBoardMasks::shift<WEST>(opponentPawnsMoveRays);
			passerMask |= BitBoardMasks::shift<EAST>(opponentPawnsMoveRays);
			passerMask = ~passerMask;
			auto passedPawns = pawns & passerMask;

			// Call alternative checker (no logic here) for debugging purposes
			// verifyPassedPawnBBAlternative(position, COLOR, passedPawns);

			return passedPawns;
		}

		template<Piece COLOR>
		inline static bitBoard_t computePassedPawnCandidateBB(const MoveGenerator& position, bitBoard_t opponentPawnsMoveRays)
		{
			constexpr bitBoard_t ADVANCED_RANKS[2] = {
				BitBoardMasks::RANK_5_BITMASK | BitBoardMasks::RANK_6_BITMASK | BitBoardMasks::RANK_7_BITMASK,
				BitBoardMasks::RANK_2_BITMASK | BitBoardMasks::RANK_3_BITMASK | BitBoardMasks::RANK_4_BITMASK
			};
			constexpr Piece OPPONENT_COLOR = QaplaBasics::switchColor(COLOR);

			auto pawns = position.getPieceBB(PAWN + COLOR);
			
			auto opponentPawns = position.getPieceBB(PAWN + OPPONENT_COLOR);
			bitBoard_t passerMask = opponentPawnsMoveRays;
			passerMask |= BitBoardMasks::shift<WEST>(opponentPawnsMoveRays);
			passerMask |= BitBoardMasks::shift<EAST>(opponentPawnsMoveRays);
			passerMask = ~passerMask;
			
			auto southPasserMask = BitBoardMasks::shiftColor<COLOR, SOUTH>(passerMask);
			// Case 1: Unblocked pawns facing only levers. Advancing bypasses the last line of defense.
			auto onlyLevers = pawns & southPasserMask & ~opponentPawnsMoveRays;

			auto attacked = position.pawnAttack[OPPONENT_COLOR];

			// Case 2: Advanced blocked pawns with safe flank support to force the blocker away.
			auto baseBlockedPassers = pawns & ADVANCED_RANKS[COLOR] & southPasserMask & ~attacked;
			auto blockedWestDistractors = BitBoardMasks::shiftColor<COLOR, EAST>(opponentPawns);
			blockedWestDistractors |= BitBoardMasks::shiftColor<COLOR, SOUTH>(blockedWestDistractors) | BitBoardMasks::shiftColor<COLOR, SE>(blockedWestDistractors);
			auto westSupport = pawns & BitBoardMasks::shiftColor<COLOR, NE>(pawns) & ~blockedWestDistractors;
			auto blockedEastDistractors = BitBoardMasks::shiftColor<COLOR, WEST>(opponentPawns);
			blockedEastDistractors |= BitBoardMasks::shiftColor<COLOR, SOUTH>(blockedEastDistractors) | BitBoardMasks::shiftColor<COLOR, SW>(blockedEastDistractors);
			auto eastSupport = pawns & BitBoardMasks::shiftColor<COLOR, NW>(pawns) & ~blockedEastDistractors;
			auto blockedPassers = baseBlockedPassers & (westSupport | eastSupport);

			// Case 3: Phalanx support to bypass the last line of defense. 
			// Need to ensure that pawn advancing by two would be a passer
			auto doubleSouthPasserMask = BitBoardMasks::shiftColor<COLOR, SOUTH>(southPasserMask);
			// Need to ensure that pawn is not blocked and not attacked
			auto basePhalanxPassers = pawns & ~opponentPawnsMoveRays & doubleSouthPasserMask & ~attacked;
			auto distractors = BitBoardMasks::shiftColor<COLOR, SOUTH_2>(opponentPawns);
			auto phalanxEastDistractors = BitBoardMasks::shiftColor<COLOR, WEST>(distractors);
			auto phalanxWestDistractors = BitBoardMasks::shiftColor<COLOR, EAST>(distractors);
			auto doubleDistractors = phalanxEastDistractors & phalanxWestDistractors;
			auto eastPhalanx = pawns & BitBoardMasks::shiftColor<COLOR, WEST>(pawns);
			auto westPhalanx = pawns & BitBoardMasks::shiftColor<COLOR, EAST>(pawns);
			auto doublePhalanx = eastPhalanx & westPhalanx;
			// A single phalanx is sufficient, if there is only one distractor.
			auto singlePhalanxSufficient = (eastPhalanx | westPhalanx) & ~doubleDistractors;
			//A double phalanx is always sufficient
			auto phalanxPassers = basePhalanxPassers & (doublePhalanx | singlePhalanxSufficient);
			auto candidatePassers = onlyLevers | blockedPassers | phalanxPassers;

			return candidatePassers;
		}

		static void verifyPassedPawnBBAlternative(const MoveGenerator& position, Piece color, bitBoard_t passedPawns);


		/**
		 * Tries to get the pawn evaluation from a transposition table
		 */
		static EvalValue probeTT(const MoveGenerator& position, EvalResults& results, PawnTT* pawnttPtr) {
			constexpr hash_t NO_PAWNS_KEY = 0;
			EvalValue value = NO_VALUE;
			if (pawnttPtr == nullptr) {
				return value;
			}
			hash_t key = position.getPawnHash();
			if (key == NO_PAWNS_KEY) return NO_VALUE;
			uint32_t index = pawnttPtr->getEntryIndex(key);
			if (index != pawnttPtr->INVALID_INDEX) {
				const PawnTTEntry& entry = pawnttPtr->getEntry(index);
				if (!entry.isEmpty()) {
					results.passedPawns = entry._passedPawns;
					value = EvalValue(entry._mgValue, entry._egValue);
				}
			}
			return value;
		}

		/**
		 * Evaluates pawns per color
		 */
		template<Piece COLOR, bool STORE_DETAILS>
		static EvalValue evalColor(const MoveGenerator & position, EvalResults & results, colorBB_t moveRay, std::vector<PieceInfo>*details) {
			EvalValue value = 0;

			bitBoard_t pawns = position.getPieceBB(PAWN + COLOR);
			results.passedPawns[COLOR] = 0;
			if (pawns == 0) return 0;

			const bitBoard_t doubleBB = pawns & moveRay[COLOR];
			const auto [singleConnect, doubleConnect] = computeConnectedPawnIndex<COLOR>(position);
			const bitBoard_t passedPawnBB = computePassedPawnBB<COLOR>(position, moveRay[opponentColor<COLOR>()]);
			// const bitBoard_t passedPawnCandidateBB = computePassedPawnCandidateBB<COLOR>(position, moveRay[opponentColor<COLOR>()]);
			const bitBoard_t isolatedPawnBB = computeIsolatedPawnBB<COLOR>(moveRay[COLOR]);
			const bitBoard_t unopposedPawnBB = pawns & ~moveRay[opponentColor<COLOR>()];
			results.passedPawns[COLOR] = passedPawnBB;

			while (pawns)
			{
				const Square pawnSquare = popLSB(pawns);
				const uint32_t pawnRank = uint32_t(getRank<COLOR>(pawnSquare));
				const bitBoard_t pawnBB = squareToBB(pawnSquare);
				uint32_t propertyIndex = pawnRank
					| ((pawnBB & doubleBB) != 0) * DOUBLE_PAWN_INDEX
					| ((pawnBB & singleConnect) != 0) * SINGLE_CONNECT_INDEX
					| ((pawnBB & doubleConnect) != 0) * DOUBLE_CONNECT_INDEX
					| ((pawnBB & isolatedPawnBB) != 0) * ISOLATED_PAWN_INDEX
					| ((pawnBB & unopposedPawnBB) != 0) * UNOPPOSED_PAWN_INDEX;

				if (passedPawnBB & pawnBB) {
					propertyIndex |= computePassedPawnIndex<COLOR>(pawnSquare, position, passedPawnBB);
				}
				// else if (passedPawnCandidateBB & pawnBB) {
					//propertyIndex |= PASSED_PAWN_INDEX;
				//}

				EvalValue propertyValue = evalValueMap[propertyIndex];
								
				value += propertyValue;

				if constexpr (STORE_DETAILS) {
					const auto materialValue = EvalValue(position.getPieceValue(PAWN + COLOR));
					const auto pstValue = PST::getValue(pawnSquare, PAWN + COLOR);
					const auto property = COLOR == WHITE ? propertyValue : -propertyValue;
					IndexVector indexVector{ 
						 { "pPST", uint32_t(switchSideToWhite<COLOR>(pawnSquare)), COLOR },
						 { "material", PAWN, COLOR } };
					if (propertyIndex > RANK_MASK) {
						indexVector.push_back({ "pProperty", propertyIndex, COLOR });
					}
					details->push_back({
						PAWN + COLOR,
						pawnSquare,
						indexVector,
						propertyIndexToString(propertyIndex),
						materialValue + pstValue + property });
				}
			}
			return value;
		}

		/**
		 * Gets a pawn string to explain the index
		 */
		static string propertyIndexToString(uint16_t pawnIndex) {
			string result = "";
			const auto passedPawnIndex = pawnIndex & PASSED_PAWN_MASK;
			const bool weakPawn = ((pawnIndex & NON_WEAK_PAWN_MASK) == 0) && ((pawnIndex & UNOPPOSED_PAWN_INDEX) != 0);
			if (pawnIndex & DOUBLE_PAWN_INDEX) { result += "dub,"; }
			if (pawnIndex & SINGLE_CONNECT_INDEX) { result += "sc,"; }
			if (pawnIndex & DOUBLE_CONNECT_INDEX) { result += "dc,"; }
			if (pawnIndex & ISOLATED_PAWN_INDEX) { result += "iso,"; }
			if (weakPawn) { result += "weak"; }
			if (passedPawnIndex == PASSED_PAWN_INDEX) { result += "pp,"; }
			if (passedPawnIndex == DISTANT_PASSED_PAWN_INDEX) { result += "dpp,"; }

			if (!result.empty() && result.back() == ',') result.pop_back();
			return result;
		}

		/**
		 * Computes the pawn value for a position with no piece but pawns
		 */
		template<Piece COLOR>
		static value_t computePawnValueNoPieceButPawn(const MoveGenerator& position, EvalResults& results, colorBB_t moveRay) {
			const bool NO_PIECES_BUT_PAWNS_ON_BOARD = true;
			bitBoard_t pawns = position.getPieceBB(PAWN + COLOR);
			bitBoard_t passedPawns = computePassedPawnBB<COLOR>(position, moveRay[switchColor(COLOR)]);

			value_t pawnValue = computePawnValueForSparcelyPolulatedBitboards<COLOR>(pawns & 
				~passedPawns, EvalPawnValues::ADVANCED_PAWN_VALUE);
			bitBoard_t pp = passedPawns;
			while (pp) {
				const Square pawnSquare = popLSB(pp);
				pawnValue += computePassedPawnValue<COLOR>(pawnSquare, position, passedPawns, NO_PIECES_BUT_PAWNS_ON_BOARD).endgame();
			}
			results.passedPawns[COLOR] = passedPawns;
			return pawnValue;
		}



		/**
		 * Computes pawn values for boards with only few pawns
		 */
		template <Piece COLOR>
		inline static value_t computePawnValueForSparcelyPolulatedBitboards(bitBoard_t pawns, const RankArray_t pawnValue) {
			value_t result = 0;

			for (; pawns != 0; pawns &= pawns - 1) {
				Rank rank = getRank<COLOR>(lsb(pawns));
				result += pawnValue[uint32_t(rank)];
			}
			return result;
		}

		/* ---------------------------------------------------------------------------------------------------------------------
		 * PASSED_PAWNS
		 * --------------------------------------------------------------------------------------------------------------------- */

		 /**
		  * Checks, if a passed pawn is a distant passed pawn (having no opponents further outside)
		  */
		static bool isDistantPassedPawn(Square pawnPos, bitBoard_t ownPawns, bitBoard_t opponentPawns) {
			bool noOpponentPawnsFurtherOutside =
				(opponentPawns & distantPassedPawnCheckNoOpponentPawn[uint32_t(getFile(pawnPos))]) == 0;
			bool ownPawnsOnOtherSideOfBoard =
				(ownPawns & distantPassedPawnCheckOwnPawn[uint32_t(getFile(pawnPos))]) != 0;
			return noOpponentPawnsFurtherOutside && ownPawnsOnOtherSideOfBoard;
		}

		/**
		 * Checks, if a passed pawn is connected to another passed pawn
		 * The other passed pawn must be in an adjacent row - the file is not relevant.
		 */
		static inline bool isConnectedPassedPawn(Square pawnPos, bitBoard_t passedPawns) {
			return (passedPawns & connectedPassedPawnCheckMap[uint32_t(getFile(pawnPos))]) != 0;
		}

		/**
		 * Checks, if a passed pawn is protected by another pawn
		 */
		static constexpr bool isProtectedPassedPawn(Square pawnPos, bitBoard_t pawnAttack) {
			return (squareToBB(pawnPos) & pawnAttack) != 0;
		}

		/**
		 * Computes the value for passed pawn
		 */
		template <Piece COLOR>
		static value_t computePassedPawnIndex(Square pawnSquare, const MoveGenerator& position, bitBoard_t passedPawns, bool noPieces = false) {
			if (noPieces && isDistantPassedPawn(pawnSquare, position.getPieceBB(PAWN + COLOR),
				position.getPieceBB(PAWN + switchColor(COLOR))))
			{
				return DISTANT_PASSED_PAWN_INDEX;
			}
			return PASSED_PAWN_INDEX;
		}

		/**
		 * Computes the value for passed pawn
		 */
		template <Piece COLOR>
		static EvalValue computePassedPawnValue(Square pawnSquare, const MoveGenerator& position, bitBoard_t passedPawns, bool noPieces = false) {
			uint32_t rank = uint32_t(getRank<COLOR>(pawnSquare));
			uint32_t index = computePassedPawnIndex<COLOR>(pawnSquare, position, passedPawns, noPieces) + rank;
			return evalValueMap[index];
		}

		template <Piece COLOR>
		static inline EvalValue kingNearPassedPawn(const MoveGenerator& position, Square frontOfPP) {
			// 52,09 -> 10, 10
			// 52,65 -> rank * 3
			// 52,38 -> rank * 4
			// 51,10 -> rank * 3, 5 - distance
			EvalValue result = 0;
			const value_t rank = value_t(getRank<COLOR>(frontOfPP));
			Square ownKingSquare = position.getKingSquare<COLOR>();
			Square opponentKingSquare = position.getKingSquare<opponentColor<COLOR>()>();
			value_t ownDistance = EvalHelper::computeDistance(ownKingSquare, frontOfPP);
			value_t opponentDistance = EvalHelper::computeDistance(opponentKingSquare, frontOfPP);
			if (ownDistance < 4) {
				if (ownDistance == 0) ownDistance = 2; // Blocking
				result += EvalValue(0, (4 - ownDistance) * rank * 3);
			}
			if (opponentDistance < 4) {
				result -= EvalValue(0, (4 - opponentDistance) * rank * 3);
			}
			return result;
		}

		template <Piece COLOR, bool STORE_DETAILS>
		static EvalValue evalPassedPawnThreats(const MoveGenerator& position, const EvalResults& results, std::vector<PieceInfo>* details) {
			EvalValue value = 0;

			const Piece OPPONENT = switchColor(COLOR);
			const Square dir = COLOR == WHITE ? NORTH : SOUTH;
			bitBoard_t pp = results.passedPawns[COLOR];
			if (pp == 0) return 0;
			bitBoard_t supported = position.attackMask[COLOR] & ~position.attackMask[OPPONENT];
			bitBoard_t stopped = position.getPiecesOfOneColorBB<OPPONENT>() |
				(position.attackMask[OPPONENT] & ~position.attackMask[COLOR]);

			for (bitBoard_t pp = results.passedPawns[COLOR]; pp != 0; pp &= pp - 1) {
				Square pawnSquare = lsb(pp);
				Rank rank = getRank<COLOR>(pawnSquare);
				if (rank <= Rank::R3) continue;
				uint32_t index = uint32_t(rank);
				bool isAttacked = (position.attackMask[OPPONENT] & squareToBB(pawnSquare)) != 0;
				index += isAttacked * PP_IS_ATTACKED_INDEX;
				value_t i = 1;
				Square frontOfPawn = pawnSquare + dir;
				for (Square square = frontOfPawn; i <= 2; square += dir, i++) {
					bitBoard_t pawn = squareToBB(square);
					if (stopped & pawn) break;
					index += PP_NOT_BLOCKED_INDEX * i;
					index += ((supported & pawn) != 0) * PP_IS_SUPPORTED_INDEX * i;
					if (rank + i > Rank::R7) break;
				}
				EvalValue propertyValue = ppThreatMap[index];
				propertyValue += kingNearPassedPawn<COLOR>(position, frontOfPawn);
				value += propertyValue;

				if constexpr (STORE_DETAILS) {
					if (index > RANK_MASK) {
						const IndexVector indexVector{ { "ppThreat", index, COLOR } };
						const auto property = COLOR == WHITE ? propertyValue : -propertyValue;
						details->push_back({ PAWN + COLOR, pawnSquare, indexVector, "", property });
					}
				}
			}
			return value;
		}

		/**
		 * Computes bonus index for connected and phalanx (adjacent) pawns
		 */
		template <Piece COLOR>
		static inline std::tuple<bitBoard_t, bitBoard_t> computeConnectedPawnIndex(const MoveGenerator& position) {
			bitBoard_t pawns = position.getPieceBB(PAWN + COLOR);
			bitBoard_t pawnsNorth = pawns | BitBoardMasks::shiftColor<COLOR, NORTH>(pawns);
			bitBoard_t connectWest = BitBoardMasks::shiftColor<COLOR, WEST>(pawnsNorth) & pawns;
			bitBoard_t connectEast = BitBoardMasks::shiftColor<COLOR, EAST>(pawnsNorth) & pawns;
			bitBoard_t doubleConnect = connectWest & connectEast;
			bitBoard_t singleConnect = (connectWest | connectEast) & ~doubleConnect;
			return std::make_tuple(singleConnect, doubleConnect);
		}

		/**
		 * Computes the bitboard of isolated pawns (excluding those already on rank 7 or rank 1).
		 *
		 * How it works:
		 * - `pawnMoveRay` is a bitboard with all squares in front of pawns (toward promotion) set to 1,
		 *   stopping before the final rank (rank 8 for white, rank 1 for black).
		 *
		 * - This mask is used to index into a lookup table (`isolatedPawnBB`) that returns a bitboard
		 *   marking all isolated pawn files.
		 *
		 * Template parameter:
		 *   - COLOR: either WHITE or BLACK, determines the shift direction.
		 *
		 * @param pawnMoveRay Bitboard with all forward rays of pawns (excluding final rank)
		 * @return Bitboard marking files with isolated pawns (bits set on all ranks of isolated files)
		 */
		template <Piece COLOR>
		inline static bitBoard_t computeIsolatedPawnBB(bitBoard_t pawnMoveRay) {
			const uint64_t SHIFT = COLOR == WHITE ? 6 : 1;
			return isolatedPawnBB[(pawnMoveRay >> SHIFT * NORTH) & LOOKUP_TABLE_MASK];
		}

		template <Piece COLOR>
		inline static bitBoard_t computePawnsMoveRays(const bitBoard_t pawnBB) {
			bitBoard_t pawnMoveRay = 0;

			if (pawnBB != 0) {
				if constexpr (COLOR == WHITE) {
					pawnMoveRay = pawnBB << NORTH | pawnBB << 2 * NORTH | pawnBB << 3 * NORTH |
							pawnBB << 4 * NORTH | pawnBB << 5 * NORTH;
				}
				else {
					pawnMoveRay = pawnBB >> NORTH | pawnBB >> 2 * NORTH | pawnBB >> 3 * NORTH |
							pawnBB >> 4 * NORTH | pawnBB >> 5 * NORTH;
				}
			}

			return pawnMoveRay;

		}

		static const value_t RUNNER_BONUS = 300;
		
		static const uint32_t LOOKUP_TABLE_SIZE = 1 << NORTH;
		static const bitBoard_t LOOKUP_TABLE_MASK = LOOKUP_TABLE_SIZE - 1;

		static std::array<bitBoard_t, LOOKUP_TABLE_SIZE> computeIsolatedPawnLookupTable();
		static inline std::array<bitBoard_t, LOOKUP_TABLE_SIZE> isolatedPawnBB = computeIsolatedPawnLookupTable();

		static bitBoard_t kingInfluenceTable[COLOR_COUNT][COLOR_COUNT][BOARD_SIZE];
		static bitBoard_t kingSupportPawnTable[COLOR_COUNT][BOARD_SIZE];

		static constexpr bitBoard_t distantPassedPawnCheckNoOpponentPawn[NORTH] = {
			0x0101010101010101ULL, 0x0303030303030303ULL, 0x070707070707070FULL, 0x0F0F0F0F0F0F0F0FULL,
			0xF0F0F0F0F0F0F0F0ULL, 0xE0E0E0E0E0E0E0E0ULL, 0xC0C0C0C0C0C0C0C0ULL, 0x8080808080808080ULL
		};

		static constexpr bitBoard_t distantPassedPawnCheckOwnPawn[NORTH] = {
			0xF8F8F8F8F8F8F8F8ULL, 0xF0F0F0F0F0F0F0F0ULL, 0xE0E0E0E0E0E0E0E0ULL, 0xC0C0C0C0C0C0C0C0ULL,
			0x0303030303030303ULL, 0x0707070707070707ULL, 0x0F0F0F0F0F0F0F0FULL, 0x1F1F1F1F1F1F1F1FULL
		};

		static constexpr bitBoard_t connectedPassedPawnCheckMap[NORTH] = {
			0x0202020202020202ULL, 0x0505050505050505ULL, 0x0A0A0A0A0A0A0A0AULL, 0x1414141414141414ULL,
			0x2828282828282828ULL, 0x5050505050505050ULL, 0xA0A0A0A0A0A0A0A0ULL, 0x4040404040404040ULL
		};

		static const uint32_t RANK_MASK = 0x07;
		static const uint32_t PP_IS_ATTACKED_INDEX			= 0x08;
		static const uint32_t PP_IS_SUPPORTED_INDEX			= 0x10;
		static const uint32_t PP_NOT_BLOCKED_INDEX			= 0x40;
		static const uint32_t PP_INDEX_SIZE					= 0x100;

		static const int64_t RANK_MULTIPLIER = 20;
		static const int64_t ATTACK_MULTIPLIER = 5;
		static const int64_t SUPPORT_MULTIPLIER = 30;
		static const int64_t NOT_BLOCKED_MULTIPLIER = 20;
		static const int64_t ADVANCE_MULTIPLIER = 5;
	
		static constexpr array<value_t, PP_INDEX_SIZE> ppThreatMapDefault = [] {

			auto computeRankValue = [](uint32_t rank) -> int64_t {
				if (rank < 3) {
					return 0;
				}
				int64_t result = 10;
				for (auto rankIndex = 4; rankIndex <= rank; ++rankIndex) {
					result *= RANK_MULTIPLIER;
					result /= 10;
				}
				return result;
			};
			
			array<value_t, PP_INDEX_SIZE> map{};
			for (uint32_t bitmask = 0; bitmask < PP_INDEX_SIZE; ++bitmask) {
				int64_t value = 0;
				const value_t rank = bitmask & RANK_MASK;
				int64_t newThreatValue = computeRankValue(rank);

				bool isAttacked = static_cast<bool>(bitmask & PP_IS_ATTACKED_INDEX);
				newThreatValue *= isAttacked ? ATTACK_MULTIPLIER : 10;

				value_t maxAdvance = std::min(2, 7 - rank);
				for (value_t advance = 1; advance <= maxAdvance; advance++) {
					bool isNotBlocked = bitmask & (PP_NOT_BLOCKED_INDEX * advance);
					if (!isNotBlocked) break;
					bool isSupported = bitmask & (PP_IS_SUPPORTED_INDEX * advance);
					
					int64_t multiplier = isSupported ? SUPPORT_MULTIPLIER : NOT_BLOCKED_MULTIPLIER;
					multiplier *= advance == 1 ? 10 : ADVANCE_MULTIPLIER;
					value += newThreatValue * multiplier; 
				}
				value /= 1000;
				map[bitmask] = static_cast<value_t>(value);
			}
			return map;
			} ();

#ifndef PARAM_OPTIMIZE_PAWN
		static constexpr array<value_t, PP_INDEX_SIZE> ppThreatMap = ppThreatMapDefault;
#else
		inline static array<value_t, PP_INDEX_SIZE> ppThreatMap = ppThreatMapDefault;
		static std::array<value_t, PP_INDEX_SIZE> generatePPThreatMap(
			int64_t rankMultiplier,
			int64_t attackMultiplier,
			int64_t supportMultiplier,
			int64_t notBlockedMultiplier,
			int64_t advanceMultiplier);
#endif

		static const uint32_t DOUBLE_PAWN_INDEX				= 0x08;
		static const uint32_t SINGLE_CONNECT_INDEX			= 0x10;
		static const uint32_t DOUBLE_CONNECT_INDEX			= 0x20;
		static const uint32_t PASSED_PAWN_INDEX				= 0x40;
		static const uint32_t DISTANT_PASSED_PAWN_INDEX		= 0x80;
		static const uint32_t PASSED_PAWN_MASK				= PASSED_PAWN_INDEX | DISTANT_PASSED_PAWN_INDEX;
		static const uint32_t ISOLATED_PAWN_INDEX			= 0x0C0;
		static const uint32_t UNOPPOSED_PAWN_INDEX			= 0x100;
		static const uint32_t NON_WEAK_PAWN_MASK = SINGLE_CONNECT_INDEX | DOUBLE_CONNECT_INDEX | PASSED_PAWN_MASK;
		static const uint32_t INDEX_SIZE					= UNOPPOSED_PAWN_INDEX * 2;

		using RankEvalArray_t = array<EvalValue, uint32_t(Rank::COUNT)>;

		static constexpr EvalValue DOUBLE_PAWN_VALUE { -10, -30 };
		static constexpr EvalValue ISOLATED_PAWN_VALUE { -15, -15 };
		static constexpr EvalValue WEAK_PAWN_VALUE { -10, 0 };
		static constexpr RankEvalArray_t SINGLE_CONNECT_VALUES = { { { 0, 0 }, {5, 0}, {6, 3}, {10, 10}, {16, 20}, {25, 40}, {30, 50}, {0, 0} } };
		static constexpr RankEvalArray_t DOUBLE_CONNECT_VALUES = { { { 0, 0 }, {5, 0}, {8, 4}, {12, 12}, {20, 25}, {30, 45}, {30, 55}, {0, 0} } };
		static constexpr RankEvalArray_t PASSED_VALUES = { { { 0, 0 }, {10, 10}, {10, 10}, {20, 30}, {40, 40}, {50, 50}, {80, 80}, {0, 0} } };
		static constexpr RankEvalArray_t DISTANT_PASSED_VALUES = { { { 0, 0 }, {25, 25}, {50, 50}, {60, 60}, {80, 80}, {100, 100}, {150, 150}, {0, 0} } };

		static constexpr array<EvalValue, INDEX_SIZE> evalValueMapDefault = [] {

			array<EvalValue, INDEX_SIZE> map;
			for (uint32_t bitmask = 0; bitmask < INDEX_SIZE; ++bitmask) {
				EvalValue value;
				const value_t rank = bitmask & RANK_MASK;
				if (rank == 0 || rank == 7) {
					map[bitmask] = value;
					continue;
				}
				const auto ppIndex = bitmask & PASSED_PAWN_MASK;
				const bool weakPawn = ((bitmask & NON_WEAK_PAWN_MASK) == 0) && ((bitmask & UNOPPOSED_PAWN_INDEX) != 0);
				if (bitmask & DOUBLE_PAWN_INDEX) { value += DOUBLE_PAWN_VALUE; }
				if (bitmask & SINGLE_CONNECT_INDEX)
					value += SINGLE_CONNECT_VALUES[rank];
				if (bitmask & DOUBLE_CONNECT_INDEX)         
					value += DOUBLE_CONNECT_VALUES[rank];
				if (bitmask & ISOLATED_PAWN_INDEX) { value += ISOLATED_PAWN_VALUE; }
				if (weakPawn) { value += WEAK_PAWN_VALUE; }
				if (ppIndex == PASSED_PAWN_INDEX)           
					value += PASSED_VALUES[rank];
				if (ppIndex == DISTANT_PASSED_PAWN_INDEX)   
					value += DISTANT_PASSED_VALUES[rank];

				map[bitmask] = value;
			}
			return map;
		} ();

#ifndef PARAM_OPTIMIZE_PAWN
		static constexpr array<EvalValue, INDEX_SIZE> evalValueMap = evalValueMapDefault;
#else
		inline static array<EvalValue, INDEX_SIZE> evalValueMap = evalValueMapDefault;
		static std::array<EvalValue, INDEX_SIZE> generateEvalValueMap(
			int32_t singleConnectFactorMg,
			int32_t singleConnectFactorEg,
			int32_t singleConnectRank6Mg,
			int32_t singleConnectRank6Eg,
			int32_t doubleConnectFactorMg,
			int32_t doubleConnectFactorEg,
			int32_t doubleConnectRank6Mg,
			int32_t doubleConnectRank6Eg,
			int32_t passedFactorMg,
			int32_t passedFactorEg,
			int32_t passedRank6Mg,
			int32_t passedRank6Eg,
			int32_t distantPassedFactorMg,
			int32_t distantPassedFactorEg,
			int32_t distantPassedRank6Mg,
			int32_t distantPassedRank6Eg,
			int32_t doublePawnFactorMg,
			int32_t doublePawnFactorEg,
			int32_t isolatedPawnFactorMg,
			int32_t isolatedPawnFactorEg,
			int32_t weakPawnFactorMg,
			int32_t weakPawnFactorEg);
#endif

		// Test position: 3r1r2/p1Pqn1bk/pPn1PPpp/2p5/3p2P1/p2P1NNQ/1pPB3P/1R3R1K w - - 0 1
		// Isolated pawns: 4k3/1p1p1ppp/8/8/8/8/1PPP1P1P/4K3 w KQkq - 0 1

	};

}

#endif // __PAWN_H
