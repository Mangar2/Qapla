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
 * Syzygy tablebase format layer. Ported from the Stockfish implementation of
 * Ronald de Man's probing code, reduced to the format itself: table discovery,
 * file mapping, index computation and decompression.
 *
 * Everything that makes or unmakes a move was left behind - it is engine code.
 * See README.md in this directory for origin, licence and the list of changes.
 */

#include "tbprobe.h"

#include <algorithm>
#include <atomic>
#include <bit>
#include <cassert>
#include <cstring>
#include <deque>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <vector>

#ifndef _WIN32
	#include <fcntl.h>
	#include <sys/mman.h>
	#include <sys/stat.h>
	#include <unistd.h>
#else
	#define WIN32_LEAN_AND_MEAN
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#include <windows.h>
#endif

namespace QaplaSyzygy {

	namespace {

		// ------------------------------------------------------------------
		// Square and bit helpers. Plain int squares, A1 = 0 ... H8 = 63.
		// ------------------------------------------------------------------

		constexpr int SQUARE_AMOUNT = 64;

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

		constexpr std::string_view PieceToChar = " PNBRQK  pnbrqk";

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

		/** The key layout is dense, so it needs mixing before it is used as a bucket index. */
		inline uint32_t hashOfKey(Key key) {
			return uint32_t((key * 0x9E3779B97F4A7C15ULL) >> 32);
		}

		// ------------------------------------------------------------------
		// Format constants and tables
		// ------------------------------------------------------------------

		constexpr int TBPIECES = TB_MAX_PIECES;

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

		constexpr bool IsLittleEndian = std::endian::native == std::endian::little;

		int MapPawns[SQUARE_AMOUNT];
		int MapB1H1H7[SQUARE_AMOUNT];
		int MapA1D1D4[SQUARE_AMOUNT];
		int MapKK[10][SQUARE_AMOUNT];

		int Binomial[6][SQUARE_AMOUNT];
		int LeadPawnIdx[6][SQUARE_AMOUNT];
		int LeadPawnsSize[6][4];

		int MaxCardinality = 0;

		bool pawnsComp(int i, int j) { return MapPawns[i] < MapPawns[j]; }

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
		T number(void* addr) {
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

		// ------------------------------------------------------------------
		// TBFile: memory maps a single .rtbw or .rtbz file
		// ------------------------------------------------------------------

		class TBFile : public std::ifstream {

			std::string fname;

		public:
			static std::string Paths;

			TBFile(const std::string& f) {
#ifndef _WIN32
				constexpr char SepChar = ':';
#else
				constexpr char SepChar = ';';
#endif
				std::stringstream ss(Paths);
				std::string path;

				while (std::getline(ss, path, SepChar)) {
					if (path.empty()) continue;
					fname = path + "/" + f;
					std::ifstream::open(fname);
					if (is_open()) return;
				}
			}

			/**
			 * Memory maps the file. A corrupt or unmappable file is reported and treated
			 * as absent - unlike the original, which terminates the process.
			 */
			uint8_t* map(void** baseAddress, uint64_t* mapping, TBType type) {
				if (is_open()) close();

#ifndef _WIN32
				struct stat statbuf;
				const int fd = ::open(fname.c_str(), O_RDONLY);
				if (fd == -1) return *baseAddress = nullptr, nullptr;

				fstat(fd, &statbuf);

				if (statbuf.st_size % 64 != 16) {
					std::cerr << "info string corrupt tablebase file " << fname << std::endl;
					::close(fd);
					return *baseAddress = nullptr, nullptr;
				}

				*mapping = statbuf.st_size;
				*baseAddress = mmap(nullptr, statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
	#if defined(MADV_RANDOM)
				madvise(*baseAddress, statbuf.st_size, MADV_RANDOM);
	#endif
				::close(fd);

				if (*baseAddress == MAP_FAILED) {
					std::cerr << "info string could not mmap " << fname << std::endl;
					return *baseAddress = nullptr, nullptr;
				}
#else
				HANDLE fd = CreateFileA(fname.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
					OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, nullptr);

				if (fd == INVALID_HANDLE_VALUE) return *baseAddress = nullptr, nullptr;

				DWORD size_high;
				DWORD size_low = GetFileSize(fd, &size_high);

				if (size_low % 64 != 16) {
					std::cerr << "info string corrupt tablebase file " << fname << std::endl;
					CloseHandle(fd);
					return *baseAddress = nullptr, nullptr;
				}

				HANDLE mmap = CreateFileMapping(fd, nullptr, PAGE_READONLY, size_high, size_low, nullptr);
				CloseHandle(fd);

				if (!mmap) {
					std::cerr << "info string CreateFileMapping failed for " << fname << std::endl;
					return *baseAddress = nullptr, nullptr;
				}

				*mapping = uint64_t(mmap);
				*baseAddress = MapViewOfFile(mmap, FILE_MAP_READ, 0, 0, 0);

				if (!*baseAddress) {
					std::cerr << "info string MapViewOfFile failed for " << fname << std::endl;
					CloseHandle(mmap);
					return *baseAddress = nullptr, nullptr;
				}
#endif
				uint8_t* data = (uint8_t*)*baseAddress;

				constexpr uint8_t Magics[][4] = { {0xD7, 0x66, 0x0C, 0xA5}, {0x71, 0xE8, 0x23, 0x5D} };

				if (memcmp(data, Magics[type == WDL], 4)) {
					std::cerr << "info string corrupted table in file " << fname << std::endl;
					unmap(*baseAddress, *mapping);
					return *baseAddress = nullptr, nullptr;
				}

				return data + 4;  // Skip the magic header
			}

			static void unmap(void* baseAddress, uint64_t mapping) {
#ifndef _WIN32
				munmap(baseAddress, mapping);
#else
				UnmapViewOfFile(baseAddress);
				CloseHandle((HANDLE)mapping);
#endif
			}
		};

		std::string TBFile::Paths;

		// ------------------------------------------------------------------
		// PairsData / TBTable / TBTables
		// ------------------------------------------------------------------

		struct PairsData {
			uint8_t   flags;
			uint8_t   maxSymLen;
			uint8_t   minSymLen;
			uint32_t  blocksNum;
			size_t    sizeofBlock;
			size_t    span;
			Sym* lowestSym;
			LR* btree;
			uint16_t* blockLength;
			uint32_t  blockLengthSize;
			SparseEntry* sparseIndex;
			size_t       sparseIndexSize;
			uint8_t* data;
			std::vector<uint64_t> base64;
			std::vector<uint8_t>  symlen;
			uint8_t   pieces[TBPIECES];
			uint64_t  groupIdx[TBPIECES + 1];
			int       groupLen[TBPIECES + 1];
			uint16_t  map_idx[4];
		};

		/** Counts per colour and piece type, the shape both a code string and a key need. */
		struct MaterialCounts {
			int count[2][KING + 1] = {};
			int total = 0;
		};

		MaterialCounts countsFromCode(const std::string& code) {
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

		Key keyFromCounts(const MaterialCounts& counts, bool mirrored) {
			Key key = 0;
			for (int colour = 0; colour < 2; ++colour)
				for (int type = PAWN; type <= KING; ++type) {
					const int stored = counts.count[mirrored ? 1 - colour : colour][type];
					for (int i = 0; i < stored; ++i)
						key = addToKey(key, makePiece(colour, type));
				}
			return key;
		}

		template<TBType Type>
		struct TBTable {
			static constexpr int Sides = Type == WDL ? 2 : 1;

			std::atomic_bool ready;
			void* baseAddress;
			uint8_t* map;
			uint64_t         mapping;
			Key              key;
			Key              key2;
			int              pieceCount;
			bool             hasPawns;
			bool             hasUniquePieces;
			uint8_t          pawnCount[2];
			PairsData        items[Sides][4];

			PairsData* get(int stm, int f) { return &items[stm % Sides][hasPawns ? f : 0]; }

			TBTable() : ready(false), baseAddress(nullptr), map(nullptr), mapping(0),
				key(0), key2(0), pieceCount(0), hasPawns(false), hasUniquePieces(false),
				pawnCount{ 0, 0 } {}

			explicit TBTable(const std::string& code);
			explicit TBTable(const TBTable<WDL>& wdl);

			~TBTable() {
				if (baseAddress) TBFile::unmap(baseAddress, mapping);
			}
		};

		template<>
		TBTable<WDL>::TBTable(const std::string& code) : TBTable() {

			const MaterialCounts counts = countsFromCode(code);

			key = keyFromCounts(counts, false);
			key2 = keyFromCounts(counts, true);
			pieceCount = counts.total;
			hasPawns = counts.count[0][PAWN] || counts.count[1][PAWN];

			hasUniquePieces = false;
			for (int colour = 0; colour < 2; ++colour)
				for (int type = PAWN; type < KING; ++type)
					if (counts.count[colour][type] == 1)
						hasUniquePieces = true;

			// Leading colour: with pawns on both sides it is the one with fewer pawns,
			// which compresses better.
			const bool c = !counts.count[1][PAWN]
				|| (counts.count[0][PAWN] && counts.count[1][PAWN] >= counts.count[0][PAWN]);

			pawnCount[0] = uint8_t(counts.count[c ? 0 : 1][PAWN]);
			pawnCount[1] = uint8_t(counts.count[c ? 1 : 0][PAWN]);
		}

		template<>
		TBTable<DTZ>::TBTable(const TBTable<WDL>& wdl) : TBTable() {
			key = wdl.key;
			key2 = wdl.key2;
			pieceCount = wdl.pieceCount;
			hasPawns = wdl.hasPawns;
			hasUniquePieces = wdl.hasUniquePieces;
			pawnCount[0] = wdl.pawnCount[0];
			pawnCount[1] = wdl.pawnCount[1];
		}

		class TBTables {

			struct Entry {
				Key           key;
				TBTable<WDL>* wdl;
				TBTable<DTZ>* dtz;

				template<TBType Type>
				TBTable<Type>* get() const {
					return (TBTable<Type>*)(Type == WDL ? (void*)wdl : (void*)dtz);
				}
			};

			static constexpr int Size = 1 << 12;
			static constexpr int Overflow = 1;

			Entry hashTable[Size + Overflow];

			std::deque<TBTable<WDL>> wdlTable;
			std::deque<TBTable<DTZ>> dtzTable;
			size_t foundDTZFiles = 0;
			size_t foundWDLFiles = 0;

			bool insert(Key key, TBTable<WDL>* wdl, TBTable<DTZ>* dtz) {
				uint32_t homeBucket = hashOfKey(key) & (Size - 1);
				Entry entry{ key, wdl, dtz };

				for (uint32_t bucket = homeBucket; bucket < Size + Overflow - 1; ++bucket) {
					const Key otherKey = hashTable[bucket].key;
					if (otherKey == key || !hashTable[bucket].get<WDL>()) {
						hashTable[bucket] = entry;
						return true;
					}

					// Robin Hood hashing: if we probed longer than this element, take its
					// place and look for a new spot for it instead.
					const uint32_t otherHomeBucket = hashOfKey(otherKey) & (Size - 1);
					if (otherHomeBucket > homeBucket) {
						std::swap(entry, hashTable[bucket]);
						key = otherKey;
						homeBucket = otherHomeBucket;
					}
				}
				std::cerr << "info string tablebase hash table size too low" << std::endl;
				return false;
			}

		public:
			template<TBType Type>
			TBTable<Type>* get(Key key) {
				for (const Entry* entry = &hashTable[hashOfKey(key) & (Size - 1)];; ++entry) {
					if (entry->key == key || !entry->get<Type>())
						return entry->get<Type>();
				}
			}

			void clear() {
				memset(hashTable, 0, sizeof(hashTable));
				wdlTable.clear();
				dtzTable.clear();
				foundDTZFiles = 0;
				foundWDLFiles = 0;
			}

			size_t wdlFiles() const { return foundWDLFiles; }
			size_t dtzFiles() const { return foundDTZFiles; }

			void add(const std::vector<int>& pieces);
		};

		TBTables Tables;

		void TBTables::add(const std::vector<int>& pieces) {

			std::string code;
			for (const int pt : pieces) code += PieceToChar[pt];
			code.insert(code.find('K', 1), "v");

			TBFile fileDtz(code + ".rtbz");
			if (fileDtz.is_open()) {
				fileDtz.close();
				foundDTZFiles++;
			}

			TBFile file(code + ".rtbw");
			if (!file.is_open()) return;   // Only the WDL file decides

			file.close();
			foundWDLFiles++;

			MaxCardinality = std::max(int(pieces.size()), MaxCardinality);

			wdlTable.emplace_back(code);
			dtzTable.emplace_back(wdlTable.back());

			// Both colours map to the same pair of tables
			insert(wdlTable.back().key, &wdlTable.back(), &dtzTable.back());
			insert(wdlTable.back().key2, &wdlTable.back(), &dtzTable.back());
		}

		// ------------------------------------------------------------------
		// Decompression
		// ------------------------------------------------------------------

		int decompressPairs(PairsData* d, uint64_t idx) {

			// Special case where all table positions store the same value
			if (d->flags & TBFlag::SingleValue) return d->minSymLen;

			// Locate the block holding idx through the sparse index, then walk
			// block lengths until the offset falls inside the current block.
			const uint32_t k = uint32_t(idx / d->span);

			uint32_t block = number<uint32_t, LittleEndian>(&d->sparseIndex[k].block);
			int offset = number<uint16_t, LittleEndian>(&d->sparseIndex[k].offset);

			offset += int(idx % d->span) - int(d->span / 2);

			while (offset < 0)
				offset += d->blockLength[--block] + 1;

			while (offset > d->blockLength[block])
				offset -= d->blockLength[block++] + 1;

			uint32_t* ptr = (uint32_t*)(d->data + (uint64_t(block) * d->sizeofBlock));

			uint64_t buf64 = number<uint64_t, BigEndian>(ptr);
			ptr += 2;
			int buf64Size = 64;
			Sym sym;

			while (true) {
				int len = 0;   // Symbol length minus minSymLen

				while (buf64 < d->base64[len]) ++len;

				sym = Sym((buf64 - d->base64[len]) >> (64 - len - d->minSymLen));
				sym += number<Sym, LittleEndian>(&d->lowestSym[len]);

				if (offset < d->symlen[sym] + 1) break;

				offset -= d->symlen[sym] + 1;
				len += d->minSymLen;
				buf64 <<= len;
				buf64Size -= len;

				if (buf64Size <= 32) {
					buf64Size += 32;
					buf64 |= uint64_t(number<uint32_t, BigEndian>(ptr++)) << (64 - buf64Size);
				}
			}

			// Descend the recursive pairing tree until a leaf holds the value
			while (d->symlen[sym]) {
				const Sym left = d->btree[sym].get<LR::Left>();

				if (offset < d->symlen[left] + 1)
					sym = left;
				else {
					offset -= d->symlen[left] + 1;
					sym = d->btree[sym].get<LR::Right>();
				}
			}

			return d->btree[sym].get<LR::Left>();
		}

		bool checkDtzStm(TBTable<WDL>*, int, int) { return true; }

		bool checkDtzStm(TBTable<DTZ>* entry, int stm, int f) {
			const auto flags = entry->get(stm, f)->flags;
			return (flags & TBFlag::STM) == stm || ((entry->key == entry->key2) && !entry->hasPawns);
		}

		int mapScore(TBTable<WDL>*, int, int value, Wdl) { return value - 2; }

		int mapScore(TBTable<DTZ>* entry, int f, int value, Wdl wdl) {

			constexpr int WDLMap[] = { 1, 3, 0, 2, 0 };

			const auto flags = entry->get(0, f)->flags;

			uint8_t* map = entry->map;
			uint16_t* idx = entry->get(0, f)->map_idx;
			if (flags & TBFlag::Mapped) {
				if (flags & TBFlag::Wide)
					value = ((uint16_t*)map)[idx[WDLMap[int(wdl) + 2]] + value];
				else
					value = map[idx[WDLMap[int(wdl) + 2]] + value];
			}

			// The tables store the distance in moves or in plies; we always return plies.
			if ((wdl == Wdl::Win && !(flags & TBFlag::WinPlies))
				|| (wdl == Wdl::Loss && !(flags & TBFlag::LossPlies))
				|| wdl == Wdl::CursedWin || wdl == Wdl::BlessedLoss)
				value *= 2;

			return value + 1;
		}

		// ------------------------------------------------------------------
		// Index computation
		// ------------------------------------------------------------------

		/** The position in the shape the index computation walks it. */
		struct ProbeBoard {
			uint64_t occupancy = 0;
			uint64_t pawns[2] = { 0, 0 };
			uint8_t  board[SQUARE_AMOUNT] = {};
			Key      key = 0;
			int      sideToMove = 0;   // 0 = white, 1 = black
			int      pieceCount = 0;
		};

		ProbeBoard toProbeBoard(const TbPosition& pos) {
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
		 * Computes the unique index of the position and reads the table entry.
		 * To encode k pieces of the same type and colour, sort them by square in
		 * ascending order s1 <= ... <= sk and take
		 *     idx = Binomial[1][s1] + Binomial[2][s2] + ... + Binomial[k][sk]
		 */
		template<typename T>
		int doProbeTable(const ProbeBoard& pos, T* entry, Wdl wdl, Status& status) {

			int squares[TBPIECES];
			uint8_t pieces[TBPIECES];
			uint64_t idx;
			int next = 0, size = 0, leadPawnsCnt = 0;
			PairsData* d;
			uint64_t b, leadPawns = 0;
			int tbFile = 0;

			// A table like KRK carries two material keys, KRvk and Kvkr. When both sides
			// hold the same material the keys are equal and only the white to move case
			// is stored, so a black to move position has to be mirrored.
			const bool symmetricBlackToMove = (entry->key == entry->key2 && pos.sideToMove);

			// Files are generated with white as the stronger side, so a position whose key
			// does not match has to be mirrored as well.
			const bool blackStronger = (pos.key != entry->key);

			const int flipColour = (symmetricBlackToMove || blackStronger) * 8;
			const int flipSquares = (symmetricBlackToMove || blackStronger) * 56;
			const int stm = (symmetricBlackToMove || blackStronger) ^ pos.sideToMove;

			// With pawns the format keeps four tables, by the file of the leading pawn
			// after reordering. The leading pawn is the one with the highest MapPawns[].
			if (entry->hasPawns) {

				// Pawns come first in the piece sequence and their colour is the reference one
				const int pc = int(entry->get(0, 0)->pieces[0]) ^ flipColour;

				assert(typeOf(pc) == PAWN);

				leadPawns = b = pos.pawns[colourOf(pc)];
				do
					squares[size++] = popLsb(b) ^ flipSquares;
				while (b);

				leadPawnsCnt = size;

				std::swap(squares[0], *std::max_element(squares, squares + leadPawnsCnt, pawnsComp));

				tbFile = edgeDistance(fileOf(squares[0]));
			}

			// Distance tables are one sided: they store either the white to move or the
			// black to move positions, so leave early when the other side is on move.
			if (!checkDtzStm(entry, stm, tbFile)) {
				status = Status::OtherSideToMove;
				return 0;
			}

			// All remaining pieces, mapped directly to the reference colour and square
			b = pos.occupancy ^ leadPawns;
			do {
				const int s = popLsb(b);
				squares[size] = s ^ flipSquares;
				pieces[size++] = uint8_t(int(pos.board[s]) ^ flipColour);
			} while (b);

			assert(size >= 2);

			d = entry->get(stm, tbFile);

			// Reorder into the sequence stored in pieces[], the one that compresses best
			for (int i = leadPawnsCnt; i < size - 1; ++i)
				for (int j = i + 1; j < size; ++j)
					if (d->pieces[i] == pieces[j]) {
						std::swap(pieces[i], pieces[j]);
						std::swap(squares[i], squares[j]);
						break;
					}

			// Map the squares so that the leading piece lands in the triangle A1-D1-D4
			if (fileOf(squares[0]) > 3)
				for (int i = 0; i < size; ++i)
					squares[i] = flipFile(squares[i]);

			// Encode the leading pawns, starting with the lowest MapPawns[] and ascending
			if (entry->hasPawns) {
				idx = LeadPawnIdx[leadPawnsCnt][squares[0]];

				std::stable_sort(squares + 1, squares + leadPawnsCnt, pawnsComp);

				for (int i = 1; i < leadPawnsCnt; ++i)
					idx += Binomial[i][MapPawns[squares[i]]];

				goto encode_remaining;   // Pawns need no further special treatment
			}

			// Without pawns, flip again so the leading piece is below rank 5
			if (rankOf(squares[0]) > 3)
				for (int i = 0; i < size; ++i)
					squares[i] = flipRank(squares[i]);

			// Find the first piece of the leading group off the A1-D4 diagonal and
			// make sure it is mapped below it
			for (int i = 0; i < d->groupLen[0]; ++i) {
				if (!offA1H8(squares[i])) continue;

				if (offA1H8(squares[i]) > 0)   // Flip along A1-H8: A3 -> C1
					for (int j = i; j < size; ++j)
						squares[j] = ((squares[j] >> 3) | (squares[j] << 3)) & 63;
				break;
			}

			// Encode the leading group. With at least three unique pieces (kings included)
			// they are encoded together, otherwise only the kings are.
			if (entry->hasUniquePieces) {

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
			idx *= d->groupIdx[0];
			int* groupSq = squares + d->groupLen[0];

			// Remaining pawns first, then pieces, by square in ascending order
			bool remainingPawns = entry->hasPawns && entry->pawnCount[1];

			while (d->groupLen[++next]) {
				std::stable_sort(groupSq, groupSq + d->groupLen[next]);
				uint64_t n = 0;

				// Map a square down when it comes later than one of a previous group
				for (int i = 0; i < d->groupLen[next]; ++i) {
					auto f = [&](int s) { return groupSq[i] > s; };
					const auto adjust = std::count_if(squares, groupSq, f);
					n += Binomial[i + 1][groupSq[i] - adjust - 8 * remainingPawns];
				}

				remainingPawns = false;
				idx += n * d->groupIdx[next];
				groupSq += d->groupLen[next];
			}

			status = Status::Ok;
			return mapScore(entry, tbFile, decompressPairs(d, idx), wdl);
		}

		// ------------------------------------------------------------------
		// Table setup, done at first access to a file
		// ------------------------------------------------------------------

		/**
		 * Groups pieces that are encoded together. A group holds pieces of the same
		 * type and colour; the leading group may hold three different pieces, or the
		 * king pair when there is no unique piece apart from the kings. With pawns,
		 * pawns always come first.
		 */
		template<typename T>
		void setGroups(T& e, PairsData* d, int order[], int f) {

			int n = 0, firstLen = e.hasPawns ? 0 : e.hasUniquePieces ? 3 : 2;
			d->groupLen[n] = 1;

			for (int i = 1; i < e.pieceCount; ++i)
				if (--firstLen > 0 || d->pieces[i] == d->pieces[i - 1])
					d->groupLen[n]++;
				else
					d->groupLen[++n] = 1;

			d->groupLen[++n] = 0;   // Zero terminated

			// If the pieces of a group g can be combined in N(g) ways, the encoding is
			//     g1 * N(g2) * N(g3) + g2 * N(g3) + g3
			// The group order is a per table parameter: the first group sits at order[0]
			// and the remaining pawns, when present, at order[1].
			const bool pp = e.hasPawns && e.pawnCount[1];
			int next = pp ? 2 : 1;
			int freeSquares = 64 - d->groupLen[0] - (pp ? d->groupLen[1] : 0);
			uint64_t idx = 1;

			for (int k = 0; next < n || k == order[0] || k == order[1]; ++k)
				if (k == order[0]) {          // Leading pawns or pieces
					d->groupIdx[0] = idx;
					idx *= e.hasPawns ? LeadPawnsSize[d->groupLen[0]][f]
						: e.hasUniquePieces ? 31332 : 462;
				}
				else if (k == order[1]) {     // Remaining pawns
					d->groupIdx[1] = idx;
					idx *= Binomial[d->groupLen[1]][48 - d->groupLen[0]];
				}
				else {                        // Remaining pieces
					d->groupIdx[next] = idx;
					idx *= Binomial[d->groupLen[next]][freeSquares];
					freeSquares -= d->groupLen[next++];
				}

			d->groupIdx[n] = idx;
		}

		/** Expands a recursive pairing symbol until the leaves are reached. */
		uint8_t setSymlen(PairsData* d, Sym s, std::vector<bool>& visited) {

			visited[s] = true;   // The tree is acyclic, so this is safe here
			const Sym sr = d->btree[s].get<LR::Right>();

			if (sr == 0xFFF) return 0;

			const Sym sl = d->btree[s].get<LR::Left>();

			if (!visited[sl]) d->symlen[sl] = setSymlen(d, sl, visited);
			if (!visited[sr]) d->symlen[sr] = setSymlen(d, sr, visited);

			return d->symlen[sl] + d->symlen[sr] + 1;
		}

		uint8_t* setSizes(PairsData* d, uint8_t* data) {

			d->flags = *data++;

			if (d->flags & TBFlag::SingleValue) {
				d->blocksNum = d->blockLengthSize = 0;
				d->span = d->sparseIndexSize = 0;
				d->minSymLen = *data++;   // Here the single value is stored
				return data;
			}

			// groupLen[] is zero terminated; the last groupIdx[] is the table size
			const uint64_t tbSize = d->groupIdx[std::find(d->groupLen, d->groupLen + 7, 0) - d->groupLen];

			d->sizeofBlock = 1ULL << *data++;
			d->span = 1ULL << *data++;
			d->sparseIndexSize = size_t((tbSize + d->span - 1) / d->span);
			const auto padding = number<uint8_t, LittleEndian>(data++);
			d->blocksNum = number<uint32_t, LittleEndian>(data);
			data += sizeof(uint32_t);
			d->blockLengthSize = d->blocksNum + padding;
			d->maxSymLen = *data++;
			d->minSymLen = *data++;
			d->lowestSym = (Sym*)data;
			d->base64.resize(d->maxSymLen - d->minSymLen + 1);

			// The canonical code orders longer symbols lower, so lowestSym[i] >= lowestSym[i+1].
			// From that we build base64[] indexed by symbol length.
			const int base64Size = static_cast<int>(d->base64.size());
			for (int i = base64Size - 2; i >= 0; --i) {
				d->base64[i] = (d->base64[i + 1] + number<Sym, LittleEndian>(&d->lowestSym[i])
					- number<Sym, LittleEndian>(&d->lowestSym[i + 1])) / 2;

				assert(d->base64[i] * 2 >= d->base64[i + 1]);
			}

			// Left shift so that base64[i] is shifted one bit more than base64[i+1]
			for (int i = 0; i < base64Size; ++i)
				d->base64[i] <<= 64 - i - d->minSymLen;

			data += base64Size * sizeof(Sym);
			d->symlen.resize(number<uint16_t, LittleEndian>(data));
			data += sizeof(uint16_t);
			d->btree = (LR*)data;

			std::vector<bool> visited(d->symlen.size());

			for (std::size_t sym = 0; sym < d->symlen.size(); ++sym)
				if (!visited[sym])
					d->symlen[sym] = setSymlen(d, Sym(sym), visited);

			return data + d->symlen.size() * sizeof(LR) + (d->symlen.size() & 1);
		}

		uint8_t* setDtzMap(TBTable<WDL>&, uint8_t* data, int) { return data; }

		uint8_t* setDtzMap(TBTable<DTZ>& e, uint8_t* data, int maxFile) {

			e.map = data;

			for (int f = 0; f <= maxFile; ++f) {
				const auto flags = e.get(0, f)->flags;
				if (flags & TBFlag::Mapped) {
					if (flags & TBFlag::Wide) {
						data += uintptr_t(data) & 1;   // Word alignment, the table may be mixed
						for (int i = 0; i < 4; ++i) {
							e.get(0, f)->map_idx[i] = uint16_t((uint16_t*)data - (uint16_t*)e.map + 1);
							data += 2 * number<uint16_t, LittleEndian>(data) + 2;
						}
					}
					else {
						for (int i = 0; i < 4; ++i) {
							e.get(0, f)->map_idx[i] = uint16_t(data - e.map + 1);
							data += *data + 1;
						}
					}
				}
			}

			return data += uintptr_t(data) & 1;   // Word alignment
		}

		/** Fills the PairsData records from the freshly mapped file. */
		template<typename T>
		void setTable(T& e, uint8_t* data) {

			PairsData* d;

			enum { Split = 1, HasPawns = 2 };

			assert(e.hasPawns == bool(*data & HasPawns));
			assert((e.key != e.key2) == bool(*data & Split));

			data++;   // First byte holds the flags

			const int sides = T::Sides == 2 && (e.key != e.key2) ? 2 : 1;
			const int maxFile = e.hasPawns ? 3 : 0;

			const bool pp = e.hasPawns && e.pawnCount[1];

			assert(!pp || e.pawnCount[0]);

			for (int f = 0; f <= maxFile; ++f) {

				for (int i = 0; i < sides; i++)
					*e.get(i, f) = PairsData();

				int order[][2] = { {*data & 0xF, pp ? *(data + 1) & 0xF : 0xF},
								   {*data >> 4, pp ? *(data + 1) >> 4 : 0xF} };
				data += 1 + pp;

				for (int k = 0; k < e.pieceCount; ++k, ++data)
					for (int i = 0; i < sides; i++)
						e.get(i, f)->pieces[k] = uint8_t(i ? *data >> 4 : *data & 0xF);

				for (int i = 0; i < sides; ++i)
					setGroups(e, e.get(i, f), order[i], f);
			}

			data += uintptr_t(data) & 1;   // Word alignment

			for (int f = 0; f <= maxFile; ++f)
				for (int i = 0; i < sides; i++)
					data = setSizes(e.get(i, f), data);

			data = setDtzMap(e, data, maxFile);

			for (int f = 0; f <= maxFile; ++f)
				for (int i = 0; i < sides; i++) {
					(d = e.get(i, f))->sparseIndex = (SparseEntry*)data;
					data += d->sparseIndexSize * sizeof(SparseEntry);
				}

			for (int f = 0; f <= maxFile; ++f)
				for (int i = 0; i < sides; i++) {
					(d = e.get(i, f))->blockLength = (uint16_t*)data;
					data += d->blockLengthSize * sizeof(uint16_t);
				}

			for (int f = 0; f <= maxFile; ++f)
				for (int i = 0; i < sides; i++) {
					data = (uint8_t*)((uintptr_t(data) + 0x3F) & ~0x3F);   // 64 byte alignment
					(d = e.get(i, f))->data = data;
					data += d->blocksNum * d->sizeofBlock;
				}
		}

		/**
		 * Returns the base address of the mapped file, mapping and initialising it on
		 * first access. Safe to call concurrently.
		 */
		template<TBType Type>
		void* mapped(TBTable<Type>& e, const ProbeBoard& pos) {

			static std::mutex mutex;

			// Acquire, so no thread sees 'ready' before the setup it guards
			if (e.ready.load(std::memory_order_acquire))
				return e.baseAddress;   // May be nullptr when the file does not exist

			std::scoped_lock<std::mutex> lk(mutex);

			if (e.ready.load(std::memory_order_relaxed))
				return e.baseAddress;

			// Piece strings in decreasing order per colour, like ("KPP", "KR")
			std::string fname, w, b;
			for (int pt = KING; pt >= PAWN; --pt) {
				int whiteCount = 0, blackCount = 0;
				uint64_t occupancy = pos.occupancy;
				while (occupancy) {
					const int s = popLsb(occupancy);
					const int piece = pos.board[s];
					if (typeOf(piece) != pt) continue;
					if (colourOf(piece) == 0) whiteCount++; else blackCount++;
				}
				w += std::string(whiteCount, PieceToChar[pt]);
				b += std::string(blackCount, PieceToChar[pt]);
			}

			fname = (e.key == pos.key ? w + 'v' + b : b + 'v' + w)
				+ (Type == WDL ? ".rtbw" : ".rtbz");

			uint8_t* data = TBFile(fname).map(&e.baseAddress, &e.mapping, Type);

			if (data) setTable(e, data);

			e.ready.store(true, std::memory_order_release);
			return e.baseAddress;
		}

		template<TBType Type>
		int probeTable(const ProbeBoard& pos, Status& status, Wdl wdl = Wdl::Draw) {

			status = Status::Ok;

			if (pos.pieceCount == 2)   // KvK
				return Type == WDL ? int(Wdl::Draw) : 0;

			TBTable<Type>* entry = Tables.get<Type>(pos.key);

			if (!entry || !mapped(*entry, pos)) {
				status = Status::NoTable;
				return 0;
			}

			return doProbeTable(pos, entry, wdl, status);
		}

		/** Builds the constant maps. Runs once, when a path is set. */
		void initMaps() {

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

	}   // anonymous namespace

	// ----------------------------------------------------------------------
	// Public interface
	// ----------------------------------------------------------------------

	LoadResult setPath(const std::string& paths) {

		Tables.clear();
		MaxCardinality = 0;
		TBFile::Paths = paths;

		if (paths.empty()) return LoadResult{};

		initMaps();

		// Register every material for which a .rtbw file exists
		for (int p1 = PAWN; p1 < KING; ++p1) {
			Tables.add({ KING, p1, KING });

			for (int p2 = PAWN; p2 <= p1; ++p2) {
				Tables.add({ KING, p1, p2, KING });
				Tables.add({ KING, p1, KING, p2 });

				for (int p3 = PAWN; p3 < KING; ++p3)
					Tables.add({ KING, p1, p2, KING, p3 });

				for (int p3 = PAWN; p3 <= p2; ++p3) {
					Tables.add({ KING, p1, p2, p3, KING });

					for (int p4 = PAWN; p4 <= p3; ++p4) {
						Tables.add({ KING, p1, p2, p3, p4, KING });

						for (int p5 = PAWN; p5 <= p4; ++p5)
							Tables.add({ KING, p1, p2, p3, p4, p5, KING });

						for (int p5 = PAWN; p5 < KING; ++p5)
							Tables.add({ KING, p1, p2, p3, p4, KING, p5 });
					}

					for (int p4 = PAWN; p4 < KING; ++p4) {
						Tables.add({ KING, p1, p2, p3, KING, p4 });

						for (int p5 = PAWN; p5 <= p4; ++p5)
							Tables.add({ KING, p1, p2, p3, KING, p4, p5 });
					}
				}

				for (int p3 = PAWN; p3 <= p1; ++p3)
					for (int p4 = PAWN; p4 <= (p1 == p3 ? p2 : p3); ++p4)
						Tables.add({ KING, p1, p2, KING, p3, p4 });
			}
		}

		LoadResult result;
		result.wdlFiles = uint32_t(Tables.wdlFiles());
		result.dtzFiles = uint32_t(Tables.dtzFiles());
		result.maxCardinality = uint32_t(MaxCardinality);
		return result;
	}

	void release() {
		Tables.clear();
		MaxCardinality = 0;
		TBFile::Paths.clear();
	}

	uint32_t maxCardinality() { return uint32_t(MaxCardinality); }

	WdlEntry probeWdlEntry(const TbPosition& pos) {

		if (pos.pieceCount > TB_MAX_PIECES || pos.pieceCount < 2)
			return WdlEntry{ Status::NoTable, Wdl::Draw };

		const ProbeBoard board = toProbeBoard(pos);

		Status status = Status::Ok;
		const int value = probeTable<WDL>(board, status);

		if (status != Status::Ok) return WdlEntry{ status, Wdl::Draw };
		return WdlEntry{ Status::Ok, Wdl(value) };
	}

	DtzEntry probeDtzEntry(const TbPosition& pos, Wdl wdl) {

		if (pos.pieceCount > TB_MAX_PIECES || pos.pieceCount < 2)
			return DtzEntry{ Status::NoTable, 0 };

		const ProbeBoard board = toProbeBoard(pos);

		Status status = Status::Ok;
		const int value = probeTable<DTZ>(board, status, wdl);

		if (status != Status::Ok) return DtzEntry{ status, 0 };
		return DtzEntry{ Status::Ok, value };
	}

}
