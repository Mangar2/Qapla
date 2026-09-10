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
 * @Overview
 * The index of the Syzygy format, shared by the reader and the writer.
 *
 * Everything here is what both directions have to agree on to the last bit: the
 * piece encoding the files themselves store, the material key, the grouping of
 * pieces that decides the multiplication chain, and the computation that turns a
 * position into the offset its value sits at. A second copy of this for the
 * writer would be a second definition of the format.
 *
 * Internal to src/syzygy. Nothing outside includes it, and nothing in it knows
 * about the engine.
 */

#pragma once

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "tbprobe.h"

namespace QaplaSyzygy {

	namespace internal {

		// ------------------------------------------------------------------
		// Square and bit helpers. Plain int squares, A1 = 0 ... H8 = 63.
		// ------------------------------------------------------------------

		inline constexpr int SQUARE_AMOUNT = 64;

		inline int fileOf(int square) { return square & 7; }
		inline int rankOf(int square) { return square >> 3; }
		inline int makeSquare(int file, int rank) { return rank * 8 + file; }
		inline int flipFile(int square) { return square ^ 7; }
		inline int flipRank(int square) { return square ^ 56; }
		inline int edgeDistance(int file) { return std::min(file, 7 - file); }
		inline int offA1H8(int square) { return rankOf(square) - fileOf(square); }

		inline int lsb(uint64_t bb) { return std::countr_zero(bb); }
		inline int popLsb(uint64_t& bb) { const int s = lsb(bb); bb &= bb - 1; return s; }

		inline uint64_t kingAttacks(int square) {
			const uint64_t bb = 1ULL << square;
			constexpr uint64_t notA = ~0x0101010101010101ULL;
			constexpr uint64_t notH = ~0x8080808080808080ULL;
			uint64_t result = ((bb & notA) >> 1) | ((bb & notH) << 1);
			const uint64_t row = result | bb;
			result |= (row << 8) | (row >> 8);
			return result;
		}

		// ------------------------------------------------------------------
		// Piece encoding used inside this file. It is the one the table files
		// themselves store, so it cannot be chosen freely: type in the low three
		// bits, colour in bit three. The interface encoding is translated on entry.
		// ------------------------------------------------------------------

		enum PieceType { NO_PIECE_TYPE, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };

		inline int typeOf(int piece) { return piece & 7; }
		inline int colourOf(int piece) { return (piece >> 3) & 1; }   // 0 = white, 1 = black
		inline int makePiece(int colour, int type) { return type + 8 * colour; }

		inline constexpr std::string_view PieceToChar = " PNBRQK  pnbrqk";

		/** Translates the interface encoding into the internal one. */
		inline int fromTbPieceCode(uint8_t code) {
			const int colour = code >= BlackPawn ? 1 : 0;
			const int type = int(code) - (colour ? int(BlackPawn) : int(WhitePawn)) + PAWN;
			return makePiece(colour, type);
		}

		// ------------------------------------------------------------------
		// Material key. Four bits per colour and piece type, so a key never
		// crosses the interface and both a piece list and a piece string produce
		// the same value. Counts are bounded by seven pieces in total.
		// ------------------------------------------------------------------

		using Key = uint64_t;

		inline int keyIndex(int piece) { return typeOf(piece) - 1 + colourOf(piece) * 6; }

		inline Key addToKey(Key key, int piece) { return key + (1ULL << (4 * keyIndex(piece))); }

		inline constexpr int TBPIECES = TB_MAX_PIECES;

		enum { BigEndian, LittleEndian };
		enum TBType { WDL, DTZ };

		// Each table has a set of flags: all of them refer to DTZ tables, the last one to WDL tables
		enum TBFlag {
			STM = 1,
			Mapped = 2,
			WinPlies = 4,
			LossPlies = 8,
			Wide = 16,
			SingleValue = 128
		};

		inline constexpr bool IsLittleEndian = std::endian::native == std::endian::little;

		inline int MapPawns[SQUARE_AMOUNT];
		inline int MapB1H1H7[SQUARE_AMOUNT];
		inline int MapA1D1D4[SQUARE_AMOUNT];
		inline int MapKK[10][SQUARE_AMOUNT];

		inline int Binomial[6][SQUARE_AMOUNT];
		inline int LeadPawnIdx[6][SQUARE_AMOUNT];
		inline int LeadPawnsSize[6][4];

		inline bool pawnsComp(int i, int j) { return MapPawns[i] < MapPawns[j]; }

		template<typename T, int Half = sizeof(T) / 2, int End = sizeof(T) - 1>
		inline void swapEndian(T& x) {
			static_assert(std::is_unsigned_v<T>, "Argument of swapEndian not unsigned");
			uint8_t tmp, * c = (uint8_t*)&x;
			for (int i = 0; i < Half; ++i)
				tmp = c[i], c[i] = c[End - i], c[End - i] = tmp;
		}

		template<>
		inline void swapEndian<uint8_t>(uint8_t&) {}

		template<typename T, int LE>
		inline T number(void* addr) {
			T v;
			if (uintptr_t(addr) & (alignof(T) - 1))
				std::memcpy(&v, addr, sizeof(T));
			else
				v = *((T*)addr);
			if (LE != IsLittleEndian)
				swapEndian(v);
			return v;
		}

		// Numbers in little-endian used by sparseIndex[] to point into blockLength[]
		struct SparseEntry {
			char block[4];
			char offset[2];
		};

		static_assert(sizeof(SparseEntry) == 6, "SparseEntry must be 6 bytes");

		using Sym = uint16_t;

		struct LR {
			enum Side { Left, Right };

			uint8_t lr[3];  // First 12 bits left-hand symbol, second 12 bits right-hand symbol

			template<Side S>
			Sym get() {
				return S == Left ? ((lr[1] & 0xF) << 8) | lr[0]
					: S == Right ? (lr[2] << 4) | (lr[1] >> 4)
					: (assert(false), Sym(-1));
			}
		};

		static_assert(sizeof(LR) == 3, "LR tree entry must be 3 bytes");


		/** Counts per colour and piece type, the shape both a code string and a key need. */
		struct MaterialCounts {
			int count[2][KING + 1] = {};
			int total = 0;
		};

		inline MaterialCounts countsFromCode(const std::string& code) {
			MaterialCounts counts;
			int colour = 0;
			bool firstKingSeen = false;
			for (const char c : code) {
				const size_t index = PieceToChar.find(c);
				if (index == std::string_view::npos) continue;
				const int type = int(index) & 7;
				if (type == KING) {
					if (firstKingSeen) colour = 1;
					firstKingSeen = true;
				}
				counts.count[colour][type]++;
				counts.total++;
			}
			return counts;
		}

		inline Key keyFromCounts(const MaterialCounts& counts, bool mirrored) {
			Key key = 0;
			for (int colour = 0; colour < 2; ++colour)
				for (int type = PAWN; type <= KING; ++type) {
					const int stored = counts.count[mirrored ? 1 - colour : colour][type];
					for (int i = 0; i < stored; ++i)
						key = addToKey(key, makePiece(colour, type));
				}
			return key;
		}

		// ------------------------------------------------------------------
		// Layout: what the index computation needs to know about a table. Both
		// halves are stored in the file - the material derives from its name, the
		// grouping from the piece order it carries.
		// ------------------------------------------------------------------

		/** What the material alone decides. Follows from the code string, never a choice. */
		struct IndexMaterial {
			Key     key = 0;
			Key     key2 = 0;
			int     pieceCount = 0;
			bool    hasPawns = false;
			bool    hasUniquePieces = false;
			uint8_t pawnCount[2] = { 0, 0 };
		};

		/** The piece order of one (side, file) table and the group sizes it implies. */
		struct IndexGroups {
			uint8_t  pieces[TBPIECES] = {};
			uint64_t groupIdx[TBPIECES + 1] = {};
			int      groupLen[TBPIECES + 1] = {};
		};

		/**
		 * Derives the material properties from a code string like "KRvK".
		 * The reader gets them from the file name, the writer from the material it
		 * is about to write - the same function, so they cannot drift apart.
		 */
		inline IndexMaterial materialFromCode(const std::string& code) {

			const MaterialCounts counts = countsFromCode(code);

			IndexMaterial material;
			material.key = keyFromCounts(counts, false);
			material.key2 = keyFromCounts(counts, true);
			material.pieceCount = counts.total;
			material.hasPawns = counts.count[0][PAWN] || counts.count[1][PAWN];

			for (int colour = 0; colour < 2; ++colour)
				for (int type = PAWN; type < KING; ++type)
					if (counts.count[colour][type] == 1)
						material.hasUniquePieces = true;

			// Leading colour: with pawns on both sides it is the one with fewer pawns,
			// which compresses better.
			const bool c = !counts.count[1][PAWN]
				|| (counts.count[0][PAWN] && counts.count[1][PAWN] >= counts.count[0][PAWN]);

			material.pawnCount[0] = uint8_t(counts.count[c ? 0 : 1][PAWN]);
			material.pawnCount[1] = uint8_t(counts.count[c ? 1 : 0][PAWN]);
			return material;
		}

		/**
		 * Groups pieces that are encoded together. A group holds pieces of the same
		 * type and colour; the leading group may hold three different pieces, or the
		 * king pair when there is no unique piece apart from the kings. With pawns,
		 * pawns always come first.
		 */
		inline void setGroups(const IndexMaterial& e, IndexGroups& g, const int order[], int f) {

			int n = 0, firstLen = e.hasPawns ? 0 : e.hasUniquePieces ? 3 : 2;
			g.groupLen[n] = 1;

			for (int i = 1; i < e.pieceCount; ++i)
				if (--firstLen > 0 || g.pieces[i] == g.pieces[i - 1])
					g.groupLen[n]++;
				else
					g.groupLen[++n] = 1;

			g.groupLen[++n] = 0;   // Zero terminated

			// If the pieces of a group g can be combined in N(g) ways, the encoding is
			//     g1 * N(g2) * N(g3) + g2 * N(g3) + g3
			// The group order is a per table parameter: the first group sits at order[0]
			// and the remaining pawns, when present, at order[1].
			const bool pp = e.hasPawns && e.pawnCount[1];
			int next = pp ? 2 : 1;
			int freeSquares = 64 - g.groupLen[0] - (pp ? g.groupLen[1] : 0);
			uint64_t idx = 1;

			for (int k = 0; next < n || k == order[0] || k == order[1]; ++k)
				if (k == order[0]) {          // Leading pawns or pieces
					g.groupIdx[0] = idx;
					idx *= e.hasPawns ? LeadPawnsSize[g.groupLen[0]][f]
						: e.hasUniquePieces ? 31332 : 462;
				}
				else if (k == order[1]) {     // Remaining pawns
					g.groupIdx[1] = idx;
					idx *= Binomial[g.groupLen[1]][48 - g.groupLen[0]];
				}
				else {                        // Remaining pieces
					g.groupIdx[next] = idx;
					idx *= Binomial[g.groupLen[next]][freeSquares];
					freeSquares -= g.groupLen[next++];
				}

			g.groupIdx[n] = idx;
		}

		/** The position in the shape the index computation walks it. */
		struct ProbeBoard {
			uint64_t occupancy = 0;
			uint64_t pawns[2] = { 0, 0 };
			uint8_t  board[SQUARE_AMOUNT] = {};
			Key      key = 0;
			int      sideToMove = 0;   // 0 = white, 1 = black
			int      pieceCount = 0;
		};

		inline ProbeBoard toProbeBoard(const TbPosition& pos) {
			ProbeBoard board;
			board.sideToMove = pos.whiteToMove ? 0 : 1;
			board.pieceCount = pos.pieceCount;

			for (int i = 0; i < pos.pieceCount; ++i) {
				const int piece = fromTbPieceCode(pos.piece[i]);
				const int square = pos.square[i];
				board.occupancy |= 1ULL << square;
				board.board[square] = uint8_t(piece);
				if (typeOf(piece) == PAWN) board.pawns[colourOf(piece)] |= 1ULL << square;
				board.key = addToKey(board.key, piece);
			}
			return board;
		}

		/**
		 * The state the two halves of the index computation share.
		 *
		 * The computation is split because a distance table stores only one side to
		 * move: which side a position belongs to, and which file table, is known
		 * after the first half, and the caller has to decide whether to go on.
		 */
		struct IndexContext {
			int      stm = 0;            // side the table is read for, after mirroring
			int      tbFile = 0;         // file table of the leading pawn, 0 without pawns
			int      flipColour = 0;     // 8 when the position had to be mirrored
			int      flipSquares = 0;    // 56 when the position had to be mirrored
			int      size = 0;
			int      leadPawnsCnt = 0;
			uint64_t leadPawns = 0;
			int      squares[TBPIECES] = {};
			uint8_t  pieces[TBPIECES] = {};
		};

		/**
		 * Mirrors the position onto the stored colour and finds the file table.
		 *
		 * @param first the grouping of (side 0, file 0), needed only for the colour of
		 *              the leading pawns - it is the same in every table of the file
		 */
		inline IndexContext beginIndex(const IndexMaterial& e, const IndexGroups& first,
			const ProbeBoard& pos) {

			IndexContext ctx;

			// A table like KRK carries two material keys, KRvk and Kvkr. When both sides
			// hold the same material the keys are equal and only the white to move case
			// is stored, so a black to move position has to be mirrored.
			const bool symmetricBlackToMove = (e.key == e.key2 && pos.sideToMove);

			// Files are generated with white as the stronger side, so a position whose key
			// does not match has to be mirrored as well.
			const bool blackStronger = (pos.key != e.key);

			ctx.flipColour = (symmetricBlackToMove || blackStronger) * 8;
			ctx.flipSquares = (symmetricBlackToMove || blackStronger) * 56;
			ctx.stm = (symmetricBlackToMove || blackStronger) ^ pos.sideToMove;

			// With pawns the format keeps four tables, by the file of the leading pawn
			// after reordering. The leading pawn is the one with the highest MapPawns[].
			if (e.hasPawns) {

				// Pawns come first in the piece sequence and their colour is the reference one
				const int pc = int(first.pieces[0]) ^ ctx.flipColour;

				assert(typeOf(pc) == PAWN);

				uint64_t b = ctx.leadPawns = pos.pawns[colourOf(pc)];
				do
					ctx.squares[ctx.size++] = popLsb(b) ^ ctx.flipSquares;
				while (b);

				ctx.leadPawnsCnt = ctx.size;

				std::swap(ctx.squares[0],
					*std::max_element(ctx.squares, ctx.squares + ctx.leadPawnsCnt, pawnsComp));

				ctx.tbFile = edgeDistance(fileOf(ctx.squares[0]));
			}

			return ctx;
		}

		/**
		 * Computes the offset the value of this position sits at.
		 * To encode k pieces of the same type and colour, sort them by square in
		 * ascending order s1 <= ... <= sk and take
		 *     idx = Binomial[1][s1] + Binomial[2][s2] + ... + Binomial[k][sk]
		 *
		 * @param d the grouping of the (side, file) table beginIndex selected
		 */
		inline uint64_t finishIndex(const IndexMaterial& e, const IndexGroups& d,
			const ProbeBoard& pos, IndexContext& ctx) {

			int* const squares = ctx.squares;
			uint8_t* const pieces = ctx.pieces;
			int size = ctx.size;
			uint64_t idx;
			int next = 0;

			// All remaining pieces, mapped directly to the reference colour and square
			uint64_t b = pos.occupancy ^ ctx.leadPawns;
			do {
				const int s = popLsb(b);
				squares[size] = s ^ ctx.flipSquares;
				pieces[size++] = uint8_t(int(pos.board[s]) ^ ctx.flipColour);
			} while (b);

			assert(size >= 2);

			// Reorder into the sequence stored in pieces[], the one that compresses best
			for (int i = ctx.leadPawnsCnt; i < size - 1; ++i)
				for (int j = i + 1; j < size; ++j)
					if (d.pieces[i] == pieces[j]) {
						std::swap(pieces[i], pieces[j]);
						std::swap(squares[i], squares[j]);
						break;
					}

			// Map the squares so that the leading piece lands in the triangle A1-D1-D4
			if (fileOf(squares[0]) > 3)
				for (int i = 0; i < size; ++i)
					squares[i] = flipFile(squares[i]);

			// Encode the leading pawns, starting with the lowest MapPawns[] and ascending
			if (e.hasPawns) {
				idx = LeadPawnIdx[ctx.leadPawnsCnt][squares[0]];

				std::stable_sort(squares + 1, squares + ctx.leadPawnsCnt, pawnsComp);

				for (int i = 1; i < ctx.leadPawnsCnt; ++i)
					idx += Binomial[i][MapPawns[squares[i]]];

				goto encode_remaining;   // Pawns need no further special treatment
			}

			// Without pawns, flip again so the leading piece is below rank 5
			if (rankOf(squares[0]) > 3)
				for (int i = 0; i < size; ++i)
					squares[i] = flipRank(squares[i]);

			// Find the first piece of the leading group off the A1-D4 diagonal and
			// make sure it is mapped below it
			for (int i = 0; i < d.groupLen[0]; ++i) {
				if (!offA1H8(squares[i])) continue;

				if (offA1H8(squares[i]) > 0)   // Flip along A1-H8: A3 -> C1
					for (int j = i; j < size; ++j)
						squares[j] = ((squares[j] >> 3) | (squares[j] << 3)) & 63;
				break;
			}

			// Encode the leading group. With at least three unique pieces (kings included)
			// they are encoded together, otherwise only the kings are.
			if (e.hasUniquePieces) {

				const int adjust1 = squares[1] > squares[0];
				const int adjust2 = (squares[2] > squares[0]) + (squares[2] > squares[1]);

				if (offA1H8(squares[0]))
					idx = (MapA1D1D4[squares[0]] * 63 + (squares[1] - adjust1)) * 62 + squares[2] - adjust2;

				else if (offA1H8(squares[1]))
					idx = (6 * 63 + rankOf(squares[0]) * 28 + MapB1H1H7[squares[1]]) * 62 + squares[2]
					- adjust2;

				else if (offA1H8(squares[2]))
					idx = 6 * 63 * 62 + 4 * 28 * 62 + rankOf(squares[0]) * 7 * 28
					+ (rankOf(squares[1]) - adjust1) * 28 + MapB1H1H7[squares[2]];

				else
					idx = 6 * 63 * 62 + 4 * 28 * 62 + 4 * 7 * 28 + rankOf(squares[0]) * 7 * 6
					+ (rankOf(squares[1]) - adjust1) * 6 + (rankOf(squares[2]) - adjust2);
			}
			else
				idx = MapKK[MapA1D1D4[squares[0]]][squares[1]];

		encode_remaining:
			idx *= d.groupIdx[0];
			int* groupSq = squares + d.groupLen[0];

			// Remaining pawns first, then pieces, by square in ascending order
			bool remainingPawns = e.hasPawns && e.pawnCount[1];

			while (d.groupLen[++next]) {
				std::stable_sort(groupSq, groupSq + d.groupLen[next]);
				uint64_t n = 0;

				// Map a square down when it comes later than one of a previous group
				for (int i = 0; i < d.groupLen[next]; ++i) {
					auto f = [&](int s) { return groupSq[i] > s; };
					const auto adjust = std::count_if(squares, groupSq, f);
					n += Binomial[i + 1][groupSq[i] - adjust - 8 * remainingPawns];
				}

				remainingPawns = false;
				idx += n * d.groupIdx[next];
				groupSq += d.groupLen[next];
			}

			return idx;
		}


		/** Builds the constant maps. Runs once, when a path is set. */
		inline void initMaps() {

			// MapB1H1H7[] encodes a square below the a1-h8 diagonal to 0..27
			int code = 0;
			for (int s = 0; s < SQUARE_AMOUNT; ++s)
				if (offA1H8(s) < 0) MapB1H1H7[s] = code++;

			// MapA1D1D4[] encodes a square of the a1-d1-d4 triangle to 0..9
			std::vector<int> diagonal;
			code = 0;
			for (int s = 0; s <= makeSquare(3, 3); ++s)
				if (offA1H8(s) < 0 && fileOf(s) <= 3)
					MapA1D1D4[s] = code++;
				else if (!offA1H8(s) && fileOf(s) <= 3)
					diagonal.push_back(s);

			// Diagonal squares come last
			for (const int s : diagonal) MapA1D1D4[s] = code++;

			// MapKK[] encodes the 462 legal king placements with the first king in the
			// a1-d1-d4 triangle. If it sits on the a1-d4 diagonal, the other one must not
			// be above the a1-h8 diagonal.
			std::vector<std::pair<int, int>> bothOnDiagonal;
			code = 0;
			for (int idx = 0; idx < 10; idx++)
				for (int s1 = 0; s1 <= makeSquare(3, 3); ++s1)
					if (MapA1D1D4[s1] == idx && (idx || s1 == makeSquare(1, 0))) {   // B1 maps to 0
						for (int s2 = 0; s2 < SQUARE_AMOUNT; ++s2)
							if ((kingAttacks(s1) | (1ULL << s1)) & (1ULL << s2))
								continue;                             // Illegal
							else if (!offA1H8(s1) && offA1H8(s2) > 0)
								continue;                             // First on diagonal, second above
							else if (!offA1H8(s1) && !offA1H8(s2))
								bothOnDiagonal.emplace_back(idx, s2);
							else
								MapKK[idx][s2] = code++;
					}

			// Both kings on a diagonal comes last
			for (const auto& p : bothOnDiagonal) MapKK[p.first][p.second] = code++;

			// Binomial[k][n]: ways to choose k elements out of n, by Pascal's rule
			Binomial[0][0] = 1;

			for (int n = 1; n < 64; n++)
				for (int k = 0; k < 6 && k <= n; ++k)
					Binomial[k][n] = (k > 0 ? Binomial[k - 1][n - 1] : 0) + (k < n ? Binomial[k][n - 1] : 0);

			// MapPawns[s] encodes a2-h7 to 0..47: the number of squares still available
			// when the leading pawn stands on s. The pawn with the highest MapPawns[] is
			// the leading one - nearest the edge, and on the lowest rank within a file.
			int availableSquares = 47;

			// Up to five leading pawns are possible with seven men, as in KPPPPPK
			for (int leadPawnsCnt = 1; leadPawnsCnt <= 5; ++leadPawnsCnt)
				for (int f = 0; f <= 3; ++f) {
					// The index restarts per file, because the table is split by file
					int idx = 0;

					for (int r = 1; r <= 6; ++r) {
						const int sq = makeSquare(f, r);

						// MapPawns[] is filled on the first pass. No other pawn can stand
						// below or more toward the edge than the leading one: 47 squares
						// remain for a2, two fewer per rank because of mirroring.
						if (leadPawnsCnt == 1) {
							MapPawns[sq] = availableSquares--;
							MapPawns[flipFile(sq)] = availableSquares--;
						}
						LeadPawnIdx[leadPawnsCnt][sq] = idx;
						idx += Binomial[leadPawnsCnt - 1][MapPawns[sq]];
					}
					LeadPawnsSize[leadPawnsCnt][f] = idx;
				}
		}
		/**
		 * Builds the maps once, whatever asks for them first. The reader builds them
		 * when a path is set, the writer when it is constructed, and both may happen
		 * in any order.
		 */
		inline void ensureMaps() {
			static const bool done = (initMaps(), true);
			(void)done;
		}

	}   // namespace internal

}
