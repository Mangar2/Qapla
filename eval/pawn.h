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
 * Provides functions to evaluate the pawn structure
 */

#ifndef __PAWN_H
#define __PAWN_H

#include <map>
#include <cstring>
#include <vector>
#include <tuple>
#include "../basics/types.h"
#include "../basics/move.h"
#include "../basics/pst.h"
#include "../movegenerator/bitboardmasks.h"
#include "../movegenerator/movegenerator.h"
#include "evalresults.h"
#include "pawnrace.h"
#include "pawntt.h"
#include "print-eval.h"
#include "eval-helper.h"

using namespace std;
using namespace QaplaBasics;
using namespace QaplaMoveGenerator;

namespace ChessEval {

	struct EvalPawnValues {
		using RankArray_t = array<value_t, uint32_t(Rank::COUNT)>;
		using  FileArray_t = array<value_t, uint32_t(File::COUNT)>;

		/**
		 * For rank 2 to 6, ignored for rank 7
		 * Double isolated pawn are counted as one isolated pawn and a double pawn
		 */
		const static value_t MOBILITY_VALUE = 2;

		static constexpr RankArray_t ADVANCED_PAWN_VALUE = { 0,  0,   0,   0,  0,  0,  0, 0 };
		static constexpr FileArray_t PASSED_PAWN_THREAT_VALUE = { 0, 0,  0,  10, 20, 40, 80, 0 };

		static constexpr value_t KING_SUPPORT_VALUE[uint32_t(Rank::COUNT)][uint32_t(File::COUNT)] =
		{
			{ 0, 0, 0, 0, 0, 0, 0, 0 },
			{ 0, 0, 0, 0, 0, 0, 0, 0 },
			{ 0, 0, 0, 0, 0, 0, 0, 0 },
			{ 0, 0, 0, 0, 0, 0, 0, 0 },
			{ 0, 0, 0, 0, 0, 0, 0, 0 },
			{ 0, 0, 0, 0, 0, 0, 0, 0 },
			{ 0, 0, 0, 0, 0, 0, 0, 0 },
			{ 0, 0, 0, 0, 0, 0, 0, 0 }
		};
	};

	class Pawn {
	public:
		Pawn() {}

		typedef array<value_t, uint32_t(Rank::COUNT)> RankArray_t;
		typedef array<value_t, uint32_t(File::COUNT)> FileArray_t;

		static value_t eval(const MoveGenerator& position, EvalResults& results) {
			
			value_t value = probeTT(position, results);
			if (value != NO_VALUE) {
				return value;
			}

			colorBB_t moveRay{
				computePawnMoveRay<WHITE>(position.getPieceBB(PAWN + WHITE)),
				computePawnMoveRay<BLACK>(position.getPieceBB(PAWN + BLACK))
			};

			value =
				evalColor<WHITE, false>(position, results, moveRay, nullptr)
				- evalColor<BLACK, false>(position, results, moveRay, nullptr);

			_tt.setEntry(position.getPawnHash(), value, results.passedPawns);
			return value;
		}

		static value_t evalWithDetails(const MoveGenerator& position, EvalResults& results, std::vector<PieceInfo>& details) {
			colorBB_t moveRay{ 
				computePawnMoveRay<WHITE>(position.getPieceBB(PAWN + WHITE)), 
				computePawnMoveRay<BLACK>(position.getPieceBB(PAWN + BLACK)) 
			};

			value_t value = 
				evalColor<WHITE, true>(position, results, moveRay, &details)
				- evalColor<BLACK, true>(position, results, moveRay, &details);
			return value;
		}

		static IndexLookupMap getIndexLookup() {
			IndexLookupMap indexLookup;
			indexLookup["pProperty"] = std::vector<EvalValue>{ evalMap.begin(), evalMap.end() };
			indexLookup["pPST"] = PST::getPSTLookup(PAWN);
			indexLookup["ppThreat"] = std::vector<EvalValue>{ ppMap.begin(), ppMap.end() };
			return indexLookup;
		}

		/**
		 * Computes the value of the pawn structure in the case there is no
		 * piece on the position
		 */
		static value_t computePawnValueNoPiece(MoveGenerator& position, EvalResults& results) {
			colorBB_t moveRay{};
			moveRay[WHITE] = computePawnMoveRay<WHITE>(position.getPieceBB(PAWN + WHITE));
			moveRay[BLACK] = computePawnMoveRay<BLACK>(position.getPieceBB(PAWN + BLACK));

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

		static void clearPawnTT() {
			_tt.clear();
		}


	private:

		template <Piece COLOR>
		static value_t getKingDistance(const Board& position, Square square, Square kingPos) {
			const Piece OPPONENT_COLOR = switchColor(COLOR);
			Square opponentKingPos = position.getKingSquare<OPPONENT_COLOR>();
			Square myKingPos = position.getKingSquare<COLOR>();
		}

		template <Piece COLOR>
		static value_t computeKingSupport(const Board& position);

		/**
		 * Tries to get the pawn evaluation from a transposition table
		 */
		static value_t probeTT(const MoveGenerator& position, EvalResults& results) {
			value_t value = NO_VALUE;
			hash_t key = position.getPawnHash();
			uint32_t index = _tt.getTTEntryIndex(key);
			if (index != _tt.INVALID_INDEX) {
				const PawnTTEntry& entry = _tt.getEntry(index);
				results.passedPawns = entry._passedPawns;
				value = entry._mgValue;
			}
			return value;
		}

		/**
		 * Evaluates pawns per color
		 */
		template<Piece COLOR, bool STORE_DETAILS>
		static value_t evalColor(const MoveGenerator & position, EvalResults & results, colorBB_t moveRay, std::vector<PieceInfo>*details) {
			value_t value = 0;
			bitBoard_t pawns = position.getPieceBB(PAWN + COLOR);
			results.passedPawns[COLOR] = 0;
			if (pawns == 0) return 0;

			const bitBoard_t doubleBB = pawns & moveRay[COLOR];
			const auto [singleConnect, doubleConnect] = computeConnectedPawnIndex<COLOR>(position);
			const bitBoard_t passedPawnBB = computePassedPawnBB(pawns, moveRay[opponentColor<COLOR>()]);
			const bitBoard_t isolatedPawnBB = computeIsolatedPawnBB<COLOR>(moveRay[COLOR]);
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
					| ((pawnBB & isolatedPawnBB) != 0) * ISOLATED_PAWN_INDEX;

				if (passedPawnBB & pawnBB) {
					propertyIndex |= computePassedPawnIndex<COLOR>(pawnSquare, position, passedPawnBB);
				}
				value_t propertyValue = evalMap[propertyIndex];
				if (position.getEvalVersion() == 1) {
					propertyValue = testMap[propertyIndex];
				}
								
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
			if (pawnIndex & DOUBLE_PAWN_INDEX) { result += "dub,"; }
			if (pawnIndex & SINGLE_CONNECT_INDEX) { result += "sc,"; }
			if (pawnIndex & DOUBLE_CONNECT_INDEX) { result += "dc,"; }
			if (pawnIndex & ISOLATED_PAWN_INDEX) { result += "iso,"; }
			if (passedPawnIndex == PASSED_PAWN_INDEX) { result += "pp,"; }
			if (passedPawnIndex == DISTANT_PASSED_PAWN_INDEX) { result += "dpp,"; }
			if (passedPawnIndex == PROTECTED_PASSED_PAWN_INDEX) { result += "ppp,"; }
			if (passedPawnIndex == CONNECTED_PASSED_PAWN_INDEX) { result += "cpp,"; }

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
			bitBoard_t passedPawns = computePassedPawnBB(pawns, moveRay[switchColor(COLOR)]);

			value_t pawnValue = computePawnValueForSparcelyPolulatedBitboards<COLOR>(pawns & 
				~passedPawns, EvalPawnValues::ADVANCED_PAWN_VALUE);
			bitBoard_t pp = passedPawns;
			while (pp) {
				const Square pawnSquare = popLSB(pp);
				pawnValue += computePassedPawnValue<COLOR>(pawnSquare, position, passedPawns, NO_PIECES_BUT_PAWNS_ON_BOARD);
			}
			results.passedPawns[COLOR] = passedPawns;
			return pawnValue;
		}

		/**
		 * Computes a value for running pawn - use only if there are only pawns on the position
		 */
		template<Piece COLOR>
		static value_t computeRunnerPace(Board& position, bitBoard_t passedPawns) {
			const Piece OPPONENT_COLOR = switchColor(COLOR);
			const Square CHANGE_SIDE = Square(COLOR == WHITE ? 0 : 0x38);

			Square opponentKingPos = position.getKingSquare<OPPONENT_COLOR>();
			Square myKingPos = position.getKingSquare<COLOR>();
			Piece colorAtMove = position.isWhiteToMove() ? WHITE : BLACK;

			bitBoard_t kingSupport = kingSupportPawnTable[COLOR][myKingPos];
			bitBoard_t opponentInfluence = kingInfluenceTable[OPPONENT_COLOR][colorAtMove][opponentKingPos];

			bitBoard_t runner = (passedPawns & ~opponentInfluence) | (passedPawns & kingSupport);
			value_t smallestDistance = value_t(Rank::COUNT);

			for (; runner != 0; runner &= runner - 1) {
				Square pawnPos = switchSide(lsb(runner));
				value_t curDistance = value_t(getOppositRank(pawnPos));
				bool ownKingBlocksPawn = myKingPos > pawnPos && getFile(myKingPos) == getFile(pawnPos);
				if (ownKingBlocksPawn) {
					++curDistance;
				}
				if (curDistance < smallestDistance) {
					smallestDistance = curDistance;
				}
			}

			return smallestDistance;
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
		 * Computes the bitboard for passed pawns
		 * How it works: if a pawn is not in front of an opponent pawn or an opponent pawn on the left/right columns, it
		 * is a passed pawn.
		 */
		inline static bitBoard_t computePassedPawnBB(bitBoard_t pawns, bitBoard_t opponentPawnMoveRay) {
			bitBoard_t nonPasserMask = opponentPawnMoveRay;
			nonPasserMask |= BitBoardMasks::shift<WEST>(opponentPawnMoveRay);
			nonPasserMask |= BitBoardMasks::shift<EAST>(opponentPawnMoveRay);
			return pawns & ~nonPasserMask;
		}

		/**
		 * Computes the value for passed pawn
		 */
		template <Piece COLOR>
		static value_t computePassedPawnIndex(Square pawnSquare, const MoveGenerator& position, bitBoard_t passedPawns, bool noPieces = false) {
			value_t result = PASSED_PAWN_INDEX;
			if (isConnectedPassedPawn(pawnSquare, passedPawns)) {
				result = CONNECTED_PASSED_PAWN_INDEX;
			}
			else if (noPieces && isDistantPassedPawn(pawnSquare, position.getPieceBB(PAWN + COLOR),
				position.getPieceBB(PAWN + switchColor(COLOR))))
			{
				result = DISTANT_PASSED_PAWN_INDEX;
			}
			else if (isProtectedPassedPawn(pawnSquare, position.pawnAttack[COLOR])) {
				result = PROTECTED_PASSED_PAWN_INDEX;
			}
			return result;
		}

		/**
		 * Computes the value for passed pawn
		 */
		template <Piece COLOR>
		static value_t computePassedPawnValue(Square pawnSquare, const MoveGenerator& position, bitBoard_t passedPawns, bool noPieces = false) {
			uint32_t rank = uint32_t(getRank<COLOR>(pawnSquare));
			uint32_t index = computePassedPawnIndex<COLOR>(pawnSquare, position, passedPawns, noPieces) + rank;
			value_t result = evalMap[index];
			if (position.getEvalVersion() == 1) {
				result = testMap[index];
			}
			return result;
		}

		template <Piece COLOR, bool STORE_DETAILS>
		static EvalValue evalPassedPawnThreats(const MoveGenerator& position, const EvalResults& results, std::vector<PieceInfo>* details) {
			value_t value = 0;
			const Piece OPPONENT = switchColor(COLOR);
			const Square dir = COLOR == WHITE ? NORTH : SOUTH;
			bitBoard_t pp = results.passedPawns[COLOR];
			if (pp == 0) return 0;
			bitBoard_t supported = position.attackMask[COLOR] & ~position.attackMask[OPPONENT];
			bitBoard_t stopped = position.getPiecesOfOneColorBB<OPPONENT>() |
				(position.attackMask[OPPONENT] & ~position.attackMask[COLOR]);

			for (bitBoard_t pp = results.passedPawns[COLOR]; pp != 0; pp &= pp - 1) {
				Square square = lsb(pp);
				Rank rank = getRank<COLOR>(square);
				if (rank <= Rank::R3) continue;
				uint32_t index = uint32_t(rank);
				bool isAttacked = (position.attackMask[OPPONENT] & squareToBB(square)) != 0;
				index += isAttacked * PP_IS_ATTACKED_INDEX;
				value_t i = 1;
				for (Square square = lsb(pp) + dir; i <= 2; square += dir, i++) {
					bitBoard_t pawn = squareToBB(square);
					if (stopped & pawn) break;
					index += PP_NOT_BLOCKED_INDEX * i;
					index += ((supported & pawn) != 0) * PP_IS_SUPPORTED_INDEX * i;
					if (rank + i > Rank::R7) break;
				}
				value += ppMap[index];
				if constexpr (STORE_DETAILS) {
					if (index > RANK_MASK) {
						const IndexVector indexVector{ { "ppThreat", index, COLOR } };
						details->push_back({ PAWN + COLOR, square, indexVector, "", COLOR == WHITE ? value : -value });
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
			value_t result = 0;
			bitBoard_t pawns = position.getPieceBB(PAWN + COLOR);
			bitBoard_t pawnsNorth = pawns | BitBoardMasks::shiftColor<COLOR, NORTH>(pawns);
			bitBoard_t connectWest = BitBoardMasks::shiftColor<COLOR, WEST>(pawnsNorth) & pawns;
			bitBoard_t connectEast = BitBoardMasks::shiftColor<COLOR, EAST>(pawnsNorth) & pawns;
			bitBoard_t doubleConnect = connectWest & connectEast;
			bitBoard_t singleConnect = (connectWest | connectEast) & ~doubleConnect;
			return std::make_tuple(singleConnect, doubleConnect);
		}

		/** 
		 * Computes the isolated pawn penalty - for pawns that are not already on row 7/row 1
		 * How it works: the pawnMoveRay is a bitboard with bit set for all positions in front of pawns until row 7
		 * As a result, the row 7/row 1 is a bitmap with bits set to 1 for all columns having pawns. 
		 * Thus we can retrieve the isolated pawns from a lookup table using it.
		 */
		template <Piece COLOR>
		inline static uint32_t computeIsolatedPawnAmount(bitBoard_t pawnMoveRay) {
			const uint64_t SHIFT = COLOR == WHITE ? 6 : 1;
			return isolatedPawnAmountLookup[(pawnMoveRay >> SHIFT * NORTH) & LOOKUP_TABLE_MASK];
		}

		template <Piece COLOR>
		inline static bitBoard_t computeIsolatedPawnBB(bitBoard_t pawnMoveRay) {
			const uint64_t SHIFT = COLOR == WHITE ? 6 : 1;
			return isolatedPawnBB[(pawnMoveRay >> SHIFT * NORTH) & LOOKUP_TABLE_MASK];
		}

		template <Piece COLOR>
		inline static bitBoard_t computePawnMoveRay(const bitBoard_t pawnBB) {
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

		static PawnTT _tt;

		static struct InitStatics {
			InitStatics();
		} _staticConstructor;

		typedef bool testFunction_t(Square kingPos, Square pawnPos, bool atMove);
		static bool kingSupportsPassedPawn(Square kingPos, Square pawnPos, bool atMove);
		static bool kingReachesPawn(Square kingPos, Square pawnPos, bool atMove);

		static bitBoard_t computeKingInfluence(Square kingPos, bool atMove, testFunction_t testFunction);

		static void computeKingInfluenceTable();
		static void computeKingSupportTable();
		static void computeIsolatedPawnLookupTable();

		static const value_t RUNNER_BONUS = 300;
		
		static const uint32_t LOOKUP_TABLE_SIZE = 1 << NORTH;
		static const bitBoard_t LOOKUP_TABLE_MASK = LOOKUP_TABLE_SIZE - 1;
		static value_t isolatedPawnAmountLookup[LOOKUP_TABLE_SIZE];
		static bitBoard_t isolatedPawnBB[LOOKUP_TABLE_SIZE];
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

		inline static array<value_t, PP_INDEX_SIZE> ppMap = [] {
			array<value_t, PP_INDEX_SIZE> map;
			for (uint32_t bitmask = 0; bitmask < PP_INDEX_SIZE; ++bitmask) {
				value_t value = 0;
				map[bitmask] = value;
				const value_t rank = bitmask & RANK_MASK;
				value_t threatValue = EvalPawnValues::PASSED_PAWN_THREAT_VALUE[rank];
				if (threatValue == 0) continue;
				bool isAttacked = bitmask & PP_IS_ATTACKED_INDEX;
				threatValue /= (1 + isAttacked);
				value_t divisor = 1;
				for (value_t divisor = 1; divisor <= 2; divisor++) {
					bool isNotBlocked = bitmask & (PP_NOT_BLOCKED_INDEX * divisor);
					if (!isNotBlocked) break;
					bool isSupported = bitmask & (PP_IS_SUPPORTED_INDEX * divisor);
					value += threatValue * (2 + isSupported) / divisor;
				}
				map[bitmask] = value;
			}
			return map;
			} ();

		static const uint32_t DOUBLE_PAWN_INDEX				= 0x08;
		static const uint32_t SINGLE_CONNECT_INDEX			= 0x10;
		static const uint32_t DOUBLE_CONNECT_INDEX			= 0x20;
		static const uint32_t PASSED_PAWN_INDEX				= 0x40;
		static const uint32_t DISTANT_PASSED_PAWN_INDEX		= 0x80;
		static const uint32_t PROTECTED_PASSED_PAWN_INDEX	= 0xC0;
		static const uint32_t CONNECTED_PASSED_PAWN_INDEX	= 0x100;
		static const uint32_t PASSED_PAWN_MASK				= 0x1C0;
		static const uint32_t ISOLATED_PAWN_INDEX			= 0x200;
		static const uint32_t BACKWARD_PAWN_INDEX			= 0x400;
		static const uint32_t INDEX_SIZE					= 0x800;

		static inline array<value_t, INDEX_SIZE> evalMap = [] {

			array<value_t, INDEX_SIZE> map;
			for (uint32_t bitmask = 0; bitmask < INDEX_SIZE; ++bitmask) {
				value_t value = 0;
				const value_t rank = bitmask & RANK_MASK;
				const auto ppIndex = bitmask & PASSED_PAWN_MASK;
				if (bitmask & DOUBLE_PAWN_INDEX) value += std::array{ 0, -20, -20, -20, -20, -20, -20, 0 }[rank];
				if (bitmask & SINGLE_CONNECT_INDEX) value += std::array{ 0, 5, 6, 10, 20, 30, 30, 0 }[rank];
				if (bitmask & DOUBLE_CONNECT_INDEX) value += std::array{ 0, 10, 12, 20, 40, 60, 60, 0 }[rank];
				if (bitmask & ISOLATED_PAWN_INDEX) value += std::array{ 0, -15, -15, -15, -15, -15, -15, 0 }[rank];
				if (ppIndex == PASSED_PAWN_INDEX) value += std::array{ 0, 10,  20,  30,  40,  50, 60, 0 }[rank];
				if (ppIndex == PROTECTED_PASSED_PAWN_INDEX) value += std::array{ 0, 10, 20, 30, 40, 50, 60, 0 }[rank];
				if (ppIndex == CONNECTED_PASSED_PAWN_INDEX) value += std::array{ 0, 10,  20,  30, 40, 50, 60, 0 }[rank];
				if (ppIndex == DISTANT_PASSED_PAWN_INDEX) value += std::array{ 0, 25,  50,  60,  80, 100, 150, 0 }[rank];
				map[bitmask] = value;
			}
			return map;
		} ();

		inline static array<value_t, INDEX_SIZE> testMap = [] {

			array<value_t, INDEX_SIZE> map;
			for (uint32_t bitmask = 0; bitmask < INDEX_SIZE; ++bitmask) {
				value_t value = 0;
				const value_t rank = bitmask & RANK_MASK;
				const auto ppIndex = bitmask & PASSED_PAWN_MASK;
				if (bitmask & DOUBLE_PAWN_INDEX)			value += std::array{ 0, -18, -18, -18, -18, -18, -18, 0 }[rank];
				if (bitmask & SINGLE_CONNECT_INDEX)			value += std::array{ 0, 5, 6, 10, 20, 30, 30, 0 }[rank];
				if (bitmask & DOUBLE_CONNECT_INDEX)			value += std::array{ 0, 5, 6, 10, 20, 30, 30, 0 }[rank];
				if (bitmask & ISOLATED_PAWN_INDEX)			value += std::array{ 0, -15, -15, -15, -15, -15, -15, 0 }[rank];
				if (ppIndex == PASSED_PAWN_INDEX)           value += std::array{ 0, 10, 20,  30,  40, 50, 80, 0 }[rank];
				if (ppIndex == PROTECTED_PASSED_PAWN_INDEX) value += std::array{ 0, 10, 20,  30,  40, 50,  100, 0 }[rank];
				if (ppIndex == CONNECTED_PASSED_PAWN_INDEX) value += std::array{ 0, 10, 20,  30,  40, 50,  120, 0 }[rank];
				if (ppIndex == DISTANT_PASSED_PAWN_INDEX)   value += std::array{ 0, 25, 50,  60, 80, 100, 150, 0 }[rank];
				
				map[bitmask] = value;
			}
			return map;
			} ();

		// Test position: 3r1r2/p1Pqn1bk/pPn1PPpp/2p5/3p2P1/p2P1NNQ/1pPB3P/1R3R1K w - - 0 1
		// Isolated pawns: 4k3/1p1p1ppp/8/8/8/8/1PPP1P1P/4K3 w KQkq - 0 1

	};

}

#endif // __PAWN_H