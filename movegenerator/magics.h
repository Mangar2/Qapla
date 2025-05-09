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
 * @copyright Copyright (c) 2021 Volker Böhm
 * @Overview
 * Provides magic numbers for a bitboard move generator
 */

#pragma once

#include "../basics/types.h"
#include "../basics/move.h"

using namespace QaplaBasics;

namespace QaplaMoveGenerator {

	class Magics
	{
	public:
		Magics() = delete;
		~Magics() = delete;

		/** 
		 * Generates the attack mask for rooks
		 */ 
		inline static bitBoard_t genRookAttackMask(Square pos, bitBoard_t allPieces)
		{
			bitBoard_t index = allPieces & _rookTable[pos].mask;
			index *= _rookTable[pos].magic;
			index >>= _rookTable[pos].shift;
			return _rookTable[pos]._attackMap[index];
		}

		/**
		 * Generates the attack mask for bishops
		 */
		inline static bitBoard_t genBishopAttackMask(Square pos, bitBoard_t allPieces)
		{
			bitBoard_t index = allPieces & _bishopTable[pos].mask;
			index *= _bishopTable[pos].magic;
			index >>= _bishopTable[pos].shift;
			return _bishopTable[pos]._attackMap[index];
		}

		/**
		 * Generates the attack mask for queens
		 */
		inline static bitBoard_t genQueenAttackMask(Square pos, bitBoard_t allPieces)
		{
			return genRookAttackMask(pos, allPieces) | genBishopAttackMask(pos, allPieces);
		}

	private:
		/**
		 * Magic number entry structure
		 */
		struct tMagicEntry
		{
			// Pointer to the attack table holding attack vectors
			bitBoard_t* _attackMap;
			// Occupancy mask (no outer squares)
			bitBoard_t  mask;
			// Magic number (64-bit factor)
			bitBoard_t magic;
			// Amount of bits to shift right to get the relevant bits
			int32_t shift;
		};

		/**
		 * Generates a mask with relevant bits for a rook attack mask
		 * relevant bits are every bits possibly holding a piece that prevents the
		 * rook from moving behind the piece. Thus the row and the column of the
		 * position without the position itself and the outer positions
		 */
		static bitBoard_t _rookMask(Square pos);

		/**
		 * Generates a mask with relevant bits for bishop attack mask
		 */
		static bitBoard_t _bishopMask(Square pos);

		/**
		 * Size of magic number index for rooks
		 */
		static const int32_t _rookSize[BOARD_SIZE];

		/**
		 * Size of magic number index for bishops
		 */
		static const int32_t _bishopSize[BOARD_SIZE];

		/**
		 * Magic numbers for rooks
		 */
		static const bitBoard_t _rookMagic[BOARD_SIZE];

		/**
		 * Magic numbers for bishops
		 */
		static const bitBoard_t _bishopMagic[BOARD_SIZE];



		/**
		 * Calculate the attack map of a rook, starting from pos
		 * the board contains a 1 on every empty field on rooks rank and file
		 * except last fields.
		 */
		static bitBoard_t rookAttack(Square pos, bitBoard_t board);

		/**
		 * Calculate the attack map of a bishop with board board, starting from pos
		 * the board contains a 1 on every empty field on bishop diagonals 
		 * except last fields.
		 */
		static bitBoard_t bishopAttack(Square pos, bitBoard_t board);

		/**
		 * Computes and stores all legal attack bitboards for a given square and piece type
		 * based on all possible blocker configurations defined by the magic index.
		 *
		 * This function fills the corresponding range in the global attack map array
		 * using the magic hashing technique. For each possible blocker subset (i.e., occupancy
		 * pattern constrained to the square's relevant mask), it computes the attack bitboard
		 * and stores it at the corresponding magic index.
		 *
		 * The lookup is later performed via:
		 *
		 *     attacks = table[magic_index] -> precomputed attacks
		 *
		 * @param pos       The square the piece is placed on (0..63).
		 * @param entry     The tMagicEntry that holds the target map location, shift value,
		 *                  magic multiplier, and occupancy mask for the square.
		 * @param isRook    True if generating for rook, false for bishop.
		 */
		static void fillAttackMap(Square pos, const tMagicEntry& aEntry, bool aIsRook);

		/**
		 * Total size of the precomputed attack map used for sliding pieces (rooks and bishops).
		 *
		 * The attack map stores, for each square on the board and for each possible blocker
		 * configuration (determined via the magic index), the corresponding set of attacked squares.
		 *
		 * There are 64 rook entries and 64 bishop entries, one per square. Each entry has a variable
		 * number of possible blocker configurations, which determines the number of attack bitboards
		 * needed for that square.
		 *
		 * The total size is calculated as the sum of all individual index table sizes for each square.
		 * These sizes depend on the number of relevant occupancy bits and are stored in magics.cpp
		 * in the arrays _rookSize[] and _bishopSize[]:
		 *
		 *     TotalSize = sum(1 << _rookSize[sq]) + sum(1 << _bishopSize[sq]) for sq in 0..63
		 *
		 * For reference, the total is expanded here as a sum of known contributions:
		 *
		 *     ATTACK_MAP_SIZE =
		 *         4  * (1 << 12) +   // 4 rook squares with 12 bits of relevant occupancy
		 *        24  * (1 << 11) +
		 *        36  * (1 << 10) +
		 *         4  * (1 << 9)  +
		 *        12  * (1 << 7)  +
		 *         4  * (1 << 6)  +
		 *        44  * (1 << 5)      // bishop squares with 5 bits
		 *
		 * This gives the total number of precomputed entries used for magic move generation.
		 */
		static const int32_t ATTACK_MAP_SIZE =
			4 * (1 << 12) +
			24 * (1 << 11) +
			36 * (1 << 10) +
			4 * (1 << 9) +
			12 * (1 << 7) +
			4 * (1 << 6) +
			44 * (1 << 5);

		/**
		 * Maps magic indexes to corresponding attack masks
		 */
		static bitBoard_t _attackMap[ATTACK_MAP_SIZE];

		/**
		 * Lookup table holding magic information for all rook squares.
		 *
		 * Each entry contains:
		 *   - the precomputed attack bitboard map ('_attackMap' pointer),
		 *   - the relevant occupancy mask (excluding edges and the square itself),
		 *   - the magic multiplier (64-bit constant),
		 *   - the shift amount used to map masked occupancies to compact indices.
		 *
		 * The attack generation uses:
		 *   masked_occ = occupancy & mask
		 *   index = (masked_occ * magic) >> shift
		 *   attacks = _attackMap[index]
		 *
		 * These tables are initialized once during static initialization and used
		 * throughout the engine to resolve rook (or bishop) attacks in constant time.
		 */
		static tMagicEntry _rookTable[BOARD_SIZE];

		/**
		 * Analogous to '_rookTable', but for bishop moves.
		 */
		static tMagicEntry _bishopTable[BOARD_SIZE];


		/**
		 * Initializes static tables for the move generator
		 */
		static struct InitStatics {
			InitStatics();
		} _staticConstructor;
		
	};

}
