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
 * Provides magic numbers for a bitboard move generator
 */

#ifndef __MAGICS_H
#define __MAGICS_H

#include "../basics/types.h"
#include "../basics/move.h"

using namespace ChessBasics;

namespace ChessMoveGenerator {

	class Magics
	{
	public:
		Magics(void);
		~Magics(void);

		/**
		 * Initializes static tables for the move generator
		 */
		static void InitStatics();

		/** 
		 * Generates the attack mask for rooks
		 */ 
		inline static bitBoard_t genRookAttackMask(Square pos, bitBoard_t allPieces)
		{
			bitBoard_t index = allPieces & rookTable[pos].mask;
			index *= rookTable[pos].magic;
			index >>= rookTable[pos].shift;
			return rookTable[pos].attackMap[index];
		}

		/**
		 * Generates the attack mask for bishops
		 */
		inline static bitBoard_t genBishopAttackMask(Square pos, bitBoard_t allPieces)
		{
			bitBoard_t index = allPieces & bishopTable[pos].mask;
			index *= bishopTable[pos].magic;
			index >>= bishopTable[pos].shift;
			return bishopTable[pos].attackMap[index];
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
		 * Generates a mask with relevant bits for a rook attack mask
		 * relevant bits are every bits possibly holding a piece that prevents the
		 * rook from moving behind the piece. Thus the row and the column of the
		 * position without the position itself and the outer positions
		 */
		static bitBoard_t rookMask(Square pos);

		/**
		 * Generates a mask with relevant bits for bishop attack mask
		 */
		static bitBoard_t bishopMask(Square pos);

		/**
		 * Size of magic number index for rooks
		 */
		static const int32_t rookSize[BOARD_SIZE];

		/**
		 * Size of magic number index for bishops
		 */
		static const int32_t bishopSize[BOARD_SIZE];

		/**
		 * Magic numbers for rooks
		 */
		static const bitBoard_t rookMagic[BOARD_SIZE];

		/**
		 * Magic numbers for bishops
		 */
		static const bitBoard_t bishopMagic[BOARD_SIZE];

		/**
		 * Magic number entry structure
		 */
		struct tMagicEntry
		{
			// Pointer to the attac table holding attac vectors
			bitBoard_t* attackMap;
			// Mask to relevant squares (no outer squares)
			bitBoard_t  mask;
			// Magic number (64-bit factor)
			bitBoard_t magic;
			// Amount of bits to shift right to get the relevant bits
			int32_t shift;
		};

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
		 * Fills the attack table for a piece position
		 */
		static void fillAttackMap(Square pos, const tMagicEntry& aEntry, bool aIsRook);

		static const int32_t ATTACK_MAP_SIZE =
			4 * (1 << 12) +
			24 * (1 << 11) +
			36 * (1 << 10) +
			4 * (1 << 9) +
			12 * (1 << 7) +
			4 * (1 << 6) +
			44 * (1 << 5);

		static bitBoard_t attackMap[ATTACK_MAP_SIZE];
		static tMagicEntry rookTable[BOARD_SIZE];
		static tMagicEntry bishopTable[BOARD_SIZE];

	};

}

#endif // MAGICS_H