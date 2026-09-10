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
#include "tbindex.h"

#include <algorithm>
#include <atomic>
#include <bit>
#include <cassert>
#include <cctype>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <unordered_set>
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

		// The format itself - square and piece encoding, material key, grouping and
		// index computation - lives in tbindex.h, because the writer has to agree
		// with it to the last bit.
		using namespace internal;

		/** The key layout is dense, so it needs mixing before it is used as a bucket index. */
		inline uint32_t hashOfKey(Key key) {
			return uint32_t((key * 0x9E3779B97F4A7C15ULL) >> 32);
		}

		// ------------------------------------------------------------------
		// Format constants and tables
		// ------------------------------------------------------------------

		int MaxCardinality = 0;


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

		/**
		 * Names of all files found in the configured directories, lower case.
		 *
		 * Table discovery asks for every material combination that the format supports,
		 * which is far more names than any set on disk holds. Trying to open each one
		 * costs a file system round trip per miss and adds up to tens of milliseconds
		 * on every engine start; reading each directory once and asking this set costs
		 * nothing. Lower case because Windows matches names that way and the set has to
		 * behave like the file system it stands for.
		 */
		std::unordered_set<std::string> AvailableFiles;

		std::string toLower(std::string name) {
			std::transform(name.begin(), name.end(), name.begin(),
				[](unsigned char c) { return char(std::tolower(c)); });
			return name;
		}

		void collectAvailableFiles(const std::string& paths) {
			AvailableFiles.clear();
			if (paths.empty()) return;

#ifndef _WIN32
			constexpr char SepChar = ':';
#else
			constexpr char SepChar = ';';
#endif
			std::stringstream ss(paths);
			std::string path;

			while (std::getline(ss, path, SepChar)) {
				if (path.empty()) continue;
				std::error_code error;
				for (const auto& entry : std::filesystem::directory_iterator(path, error)) {
					if (error) break;
					AvailableFiles.insert(toLower(entry.path().filename().string()));
				}
			}
		}

		bool isFileAvailable(const std::string& name) {
			return AvailableFiles.find(toLower(name)) != AvailableFiles.end();
		}

		// ------------------------------------------------------------------
		// PairsData / TBTable / TBTables
		// ------------------------------------------------------------------

		struct PairsData : IndexGroups {
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
			uint16_t  map_idx[4];
		};


		template<TBType Type>
		struct TBTable : IndexMaterial {
			static constexpr int Sides = Type == WDL ? 2 : 1;

			std::atomic_bool ready;
			void* baseAddress;
			uint8_t* map;
			uint64_t         mapping;
			PairsData        items[Sides][4];

			PairsData* get(int stm, int f) { return &items[stm % Sides][hasPawns ? f : 0]; }

			TBTable() : IndexMaterial(), ready(false), baseAddress(nullptr), map(nullptr),
				mapping(0) {}

			explicit TBTable(const std::string& code);
			explicit TBTable(const TBTable<WDL>& wdl);

			~TBTable() {
				if (baseAddress) TBFile::unmap(baseAddress, mapping);
			}
		};

		template<>
		TBTable<WDL>::TBTable(const std::string& code) : TBTable() {
			static_cast<IndexMaterial&>(*this) = materialFromCode(code);
		}

		template<>
		TBTable<DTZ>::TBTable(const TBTable<WDL>& wdl) : TBTable() {
			static_cast<IndexMaterial&>(*this) = wdl;
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

			if (isFileAvailable(code + ".rtbz")) foundDTZFiles++;

			if (!isFileAvailable(code + ".rtbw")) return;   // Only the WDL file decides

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

		/**
		 * Computes the offset of the position in the table and reads the entry.
		 * The index computation itself is shared with the writer - see tbindex.h.
		 */
		template<typename T>
		int doProbeTable(const ProbeBoard& pos, T* entry, Wdl wdl, Status& status) {

			IndexContext ctx = beginIndex(*entry, *entry->get(0, 0), pos);

			// Distance tables are one sided: they store either the white to move or the
			// black to move positions, so leave early when the other side is on move.
			if (!checkDtzStm(entry, ctx.stm, ctx.tbFile)) {
				status = Status::OtherSideToMove;
				return 0;
			}

			PairsData* const d = entry->get(ctx.stm, ctx.tbFile);
			const uint64_t idx = finishIndex(*entry, *d, pos, ctx);

			status = Status::Ok;
			return mapScore(entry, ctx.tbFile, decompressPairs(d, idx), wdl);
		}

		// ------------------------------------------------------------------
		// Table setup, done at first access to a file
		// ------------------------------------------------------------------


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
					setGroups(e, *e.get(i, f), order[i], f);
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


	}   // anonymous namespace

	// ----------------------------------------------------------------------
	// Public interface
	// ----------------------------------------------------------------------

	LoadResult setPath(const std::string& paths) {

		Tables.clear();
		MaxCardinality = 0;
		TBFile::Paths = paths;
		collectAvailableFiles(paths);

		if (paths.empty()) return LoadResult{};

		ensureMaps();

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
