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

#ifndef __MAGICS_H
#define __MAGICS_H

#include "../basics/types.h"
#include "../basics/move.h"

using namespace QaplaBasics;

namespace QaplaMoveGenerator {

	class Magics
	{
	public:
		Magics(void) {};
		~Magics(void) {};

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
		 * Magic number entry structure
		 */
		struct tMagicEntry
		{
			// Pointer to the attac table holding attac vectors
			bitBoard_t* _attackMap;
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

		/**
		 * Maps magic indexes to correspondin attack masks
		 */
		static bitBoard_t _attackMap[ATTACK_MAP_SIZE];

		/**
		 * Magic number settings for rooks
		 */
		static tMagicEntry _rookTable[BOARD_SIZE];

		/**
		 * Magic number settings for bishops
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

#endif // MAGICS_H