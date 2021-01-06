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
 */

#include "magics.h"
#include "BitBoardMasks.h"
#include <assert.h>

using namespace ChessMoveGenerator;
using namespace ChessBasics;

bitBoard_t Magics::_attackMap[ATTACK_MAP_SIZE];
Magics::tMagicEntry Magics::_rookTable[BOARD_SIZE];
Magics::tMagicEntry Magics::_bishopTable[BOARD_SIZE];

Magics::InitStatics Magics::_staticConstructor;

const int32_t Magics::_bishopSize[BOARD_SIZE] =
{
	6,  5,  5,  5,  5,  5,  5,  6,
	5,  5,  5,  5,  5,  5,  5,  5,
	5,  5,  7,  7,  7,  7,  5,  5,
	5,  5,  7,  9,  9,  7,  5,  5,
	5,  5,  7,  9,  9,  7,  5,  5,
	5,  5,  7,  7,  7,  7,  5,  5,
	5,  5,  5,  5,  5,  5,  5,  5,
	6,  5,  5,  5,  5,  5,  5,  6,
};

const bitBoard_t Magics::_bishopMagic[BOARD_SIZE] = 
{
	0x400a4a2208020188ULL,
	0x6004100214c10640ULL,
	0x230208e02c0142eULL,
	0x1060a1201403418ULL,
	0xc60211110f1504ULL,
	0x9c2012461080006ULL,
	0x113c01845008200bULL,
	0x1250218650086006ULL,
	0x4100104413a40400ULL,
	0x5c01180849224a00ULL,
	0x210441e484090ULL,
	0x47280a00201045ULL,
	0x61010c0421000488ULL,
	0x30c1029010481220ULL,
	0x1c02021630240424ULL,
	0x22042305082064ULL,
	0x4041120810490600ULL,
	0x250442056064540ULL,
	0x10020050140041c8ULL,
	0x6028010c20202181ULL,
	0x602401a211201090ULL,
	0xa66010b00962909ULL,
	0x6ca0520c15080809ULL,
	0x2002005c410c014dULL,
	0x1086090011301001ULL,
	0x1708240418212831ULL,
	0x64180c01420a2201ULL,
	0x14410800040a00a0ULL,
	0x1801010024904012ULL,
	0x500c401588080dULL,
	0x3c28124022010433ULL,
	0x244a404021010830ULL,
	0x200804244a112091ULL,
	0x1008251448100c13ULL,
	0x40810406000b1800ULL,
	0x23c22008000b0104ULL,
	0x1930508020220200ULL,
	0xb8a0e89000a024bULL,
	0x2048010040240226ULL,
	0x817044905221102ULL,
	0x21a2101421a7648cULL,
	0x4414241423200302ULL,
	0x3ae3120110005100ULL,
	0xc84050c18041c10ULL,
	0x891201204101480ULL,
	0xe40158810424280ULL,
	0x159c278803130a02ULL,
	0x4485042c08500080ULL,
	0x1802010402c24020ULL,
	0x1008404804100388ULL,
	0x4001222051049d1ULL,
	0x2440040561880469ULL,
	0x11b404045011214ULL,
	0x4024110010244a8ULL,
	0x230300103040f95ULL,
	0x680e0202020a0540ULL,
	0xe2202094414240fULL,
	0x812aa108061196ULL,
	0x842080104090440ULL,
	0x8a0060040840440ULL,
	0x2c804d0d0404ULL,
	0x668c000810302280ULL,
	0x8108c948284522ULL,
	0x2010312240840100ULL,
};

const int32_t Magics::_rookSize[BOARD_SIZE] = 
{
	12, 11, 11, 11, 11, 11, 11, 12,
	11, 10, 10, 10, 10, 10, 10, 11,
	11, 10, 10, 10, 10, 10, 10, 11,
	11, 10, 10, 10, 10, 10, 10, 11,
	11, 10, 10, 10, 10, 10, 10, 11,
	11, 10, 10, 10, 10, 10, 10, 11,
	11, 10, 10, 10, 10, 10, 10, 11,
	12, 11, 11, 11, 11, 11, 11, 12,
};

const bitBoard_t Magics::_rookMagic[64] = 
{
	0x208000c001906480ULL,
	0x640001004c42001ULL,
	0x1d00102001410008ULL,
	0x42000cc200201048ULL,
	0x4200240200600850ULL,
	0x4100081e5c000500ULL,
	0x1000a00109c2100ULL,
	0x2b00008474c20300ULL,
	0x2002306004084ULL,
	0x2100401000200040ULL,
	0x2541004101200410ULL,
	0xbb001000a82300ULL,
	0x3a002200847008ULL,
	0x6509001300b40008ULL,
	0x440c0028190610a4ULL,
	0xe000422004187ULL,
	0x400288002400480ULL,
	0x30a0004001500261ULL,
	0xe81050020023044ULL,
	0x10d1010030002109ULL,
	0x418010010680500ULL,
	0x2a03010002280400ULL,
	0x48100c0021221018ULL,
	0x19803200118c0043ULL,
	0x4aa506c200220082ULL,
	0x222208100400301ULL,
	0x5002324300200102ULL,
	0x40956a0200204010ULL,
	0x2415000d00080031ULL,
	0x34a001200100438ULL,
	0x30420e0400503718ULL,
	0x40300310000418aULL,
	0xc40008041800924ULL,
	0xb4260102004084ULL,
	0x413902c071002000ULL,
	0x888502202000a40ULL,
	0x4001028411000800ULL,
	0x1042002802000c50ULL,
	0x4050a500c000805ULL,
	0x800264102002484ULL,
	0x280002000c44002ULL,
	0x4082022100820040ULL,
	0x201017260010040ULL,
	0x110b0a0010c20020ULL,
	0x4c9001118010034ULL,
	0xa22001c68d20010ULL,
	0x522000c01020088ULL,
	0x3001408110660004ULL,
	0x14482b0240800100ULL,
	0x401005412008c0ULL,
	0x2129024120021100ULL,
	0xc004c2030010100ULL,
	0x1209020408001100ULL,
	0x1be000904303e00ULL,
	0x1842005401081200ULL,
	0x459c6c074c830200ULL,
	0x221014160520282ULL,
	0x141a600810112ULL,
	0x170a11004120008dULL,
	0x1000500021002409ULL,
	0x3405002e1c303801ULL,
	0x230d000400080e03ULL,
	0x1625003990804ULL,
	0x1203292041140082ULL,
};


// -------------------------- rookMask ----------------------------------------
bitBoard_t Magics::_rookMask(Square square) 
{
  assert(NORTH == 8);
  assert(BOARD_SIZE == 64);
  assert(square >= A1);
  assert(square <= H8);

  bitBoard_t res = 0ULL;
  Rank rank = getRank(square);
  File file = getFile(square);

  // Set every bit on current file, except itself and first and last field in file
  for (File curFile = file + 1; curFile < File::H; ++curFile) res |= (1ULL << computeSquare(curFile, rank));
  for (File curFile = File::B; curFile < file; ++curFile) res |= (1ULL << computeSquare(curFile, rank));
  // Set every bit on current column, except itself and first and last field in column
  for (Rank curRank = rank + 1; curRank < Rank::R8; ++curRank) res |= (1ULL << computeSquare(file, curRank));
  for (Rank curRank = Rank::R2; curRank < rank; ++curRank) res |= (1ULL << computeSquare(file, curRank));

  return res;
}

// -------------------------- rookMask ----------------------------------------
bitBoard_t Magics::_bishopMask(Square square) 
{
  assert(NORTH == 8);
  assert(BOARD_SIZE == 64);
  assert(square >= 0);
  assert(square < 64);
  bitBoard_t res = 0ULL;
  Rank rank = getRank(square);
  File file = getFile(square);
  Rank curRank;
  File curFile;

  // Set every bit on bishop diagonals, except itself and first and last field in diagonal
  for (curFile = file + 1, curRank = rank + 1; curFile < File::H && curRank < Rank::R8; ++curFile, ++curRank )
	  res |= (1ULL << computeSquare(curFile, curRank));
  for (curFile = file - 1, curRank = rank + 1; curFile > File::A && curRank < Rank::R8; --curFile, ++curRank)
	  res |= (1ULL << computeSquare(curFile, curRank));
  for (curFile = file + 1, curRank = rank - 1; curFile < File::H && curRank > Rank::R1; ++curFile, --curRank)
	  res |= (1ULL << computeSquare(curFile, curRank));
  for (curFile = file - 1, curRank = rank - 1; curFile > File::A && curRank > Rank::R1; --curFile, --curRank)
	  res |= (1ULL << computeSquare(curFile, curRank));

  return res;
}

// -------------------------- IndexToBitBoard ---------------------------------
bitBoard_t indexToBitBoard(int aIndex, int bitAmount, bitBoard_t aMask) 
{
	int32_t i, j;
	bitBoard_t res = 0ULL;

	for(i = 0; i < bitAmount; i++) 
	{
		j = BitBoardMasks::popLSB(aMask);
		if (aIndex & (1 << i)) 
			res |= (1ULL << j);
	}
	return res;
}

// -------------------------- RookAttack --------------------------------------
bitBoard_t Magics::rookAttack(Square square, bitBoard_t board) 
{
	const int32_t dir[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
	bitBoard_t res = 0ULL;
	bitBoard_t add;
	Rank rank;
	File file;
	for (int32_t i = 0; i < 4; i++) {
		rank = getRank(square) + dir[i][0];
		file = getFile(square) + dir[i][1];
		for (; isRankInBoard(rank) && isFileInBoard(file); rank += dir[i][0], file += dir[i][1]) { 
			add = 1ULL << computeSquare(file, rank);
			res |= add; 
			if (board & add) break;
		}
	}
	return res;
}

// -------------------------- BishopAttacks -----------------------------------
bitBoard_t Magics::bishopAttack(Square square, bitBoard_t board) {
	const int32_t dir[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
	bitBoard_t res = 0ULL;
	bitBoard_t add;
	Rank rank;
	File file;
	for (int32_t i = 0; i < 4; i++) {
		rank = getRank(square) + dir[i][0];
		file = getFile(square) + dir[i][1];
		for (; isRankInBoard(rank) && isFileInBoard(file); rank += dir[i][0], file += dir[i][1]) { 
			add = 1ULL << computeSquare(file, rank);
			res |= add; 
			if (board & add) break;
		}
	}

	return res;
}
 
// -------------------------- fillAttackMap -----------------------------------
void Magics::fillAttackMap(Square square, const tMagicEntry& entry, bool isRook)
{
	uint32_t i;
	bitBoard_t board;
	bitBoard_t aAttackMask;
	uint32_t bitAmount = BOARD_SIZE - entry.shift;
	for (i = 0; i < (1UL << bitAmount); i++)
	{
		// Calculate the board bits for current index
		board = indexToBitBoard(i, bitAmount, entry.mask);
		if (isRook)
			aAttackMask = rookAttack(square,  board);
		else 
			aAttackMask = bishopAttack(square, board);
		// Calculate its index
		// Multiply with magic
		board *= entry.magic;
		// Shift it 
		board >>= entry.shift;
		entry._attackMap[board] = aAttackMask;
	}
}
 
// -------------------------- Init --------------------------------------------
Magics::InitStatics::InitStatics()
{
	Square square;
	bitBoard_t* magicPtr = _attackMap;
	// Only 8x8 board sizes supported by bitboards. No border.
	assert(BOARD_SIZE == 64);
	for (square = A1; square <= H8; ++square)
	{
		_rookTable[square].magic = _rookMagic[square];
		_rookTable[square].mask  = _rookMask(square);
		_rookTable[square].shift = BOARD_SIZE - _rookSize[square];
		_rookTable[square]._attackMap = magicPtr;
		fillAttackMap(square, _rookTable[square], true);
		assert(magicPtr - _rookTable[0]._attackMap < ATTACK_MAP_SIZE);
		magicPtr += 1LL << _rookSize[square]; 
	}

	for (square = A1; square <= H8; ++square)
	{
		_bishopTable[square].magic = _bishopMagic[square];
		_bishopTable[square].mask  = _bishopMask(square);
		_bishopTable[square].shift = BOARD_SIZE - _bishopSize[square];
		_bishopTable[square]._attackMap = magicPtr;
		fillAttackMap(square, _bishopTable[square], false);
		assert(magicPtr - _rookTable[0]._attackMap < ATTACK_MAP_SIZE);
		magicPtr += 1LL << _bishopSize[square]; 
	}
	assert(magicPtr - _rookTable[0]._attackMap == ATTACK_MAP_SIZE);
}
