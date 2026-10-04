/**
 * Reads packed game files and writes training batches - the data path of train.py, compiled.
 *
 * The python loader replayed every game and built the features of every position in a python
 * loop, one process per stream. It was fast enough for the gpu of the mac, but it took six or
 * eight cores to get there, and on a machine with a strong gpu and few cores it was the limit.
 * This does the same work in a fraction of one core.
 *
 * What it computes must be exactly what format.py computes: the same board replay, the same
 * HalfKA features in the same order, the same padding. --dump writes the positions of the games it
 * is given in replay order, unshuffled, so that gamedata.py can compare them one by one.
 *
 * What stays in python is everything that makes two runs comparable: the index, the split into
 * training and validation, and the order of the games, which is drawn with numpy exactly as before
 * and handed over on stdin. Only the shuffle of positions inside the buffer is done here, with a
 * generator of its own - the buffers hold the same positions as before, in a different order.
 *
 *   batcher --batch 16384 --buffer 131072 --seed 7 <game files...>  < game ids (uint32)
 *   batcher --dump <game files...>                                   < game ids (uint32)
 *   either of the two with --skip-tactical: no position whose move captures or that is in check
 *   batcher --index <one game file>
 *
 * --index writes, for every game of the file, its offset (uint64), its length in plies (uint8) and
 * how many of its positions carry a value (uint16) - the three arrays build_index() computed in a
 * python loop, which took about twenty-five minutes for one of the larger sets.
 *
 * A batch on stdout: own features [B][32] uint16, opponent features [B][32] uint16, value codes [B]
 * uint16, results [B] uint8. Only whole batches are written; what is left at the end is dropped, as
 * the python loader did.
 */

#include <algorithm>
#include <array>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

namespace {

	// --- the piece encoding of Qapla, see basics/types.h and format.py ----------------------
	constexpr int NO_PIECE = 0;
	constexpr int WHITE = 0, BLACK = 1;
	constexpr int PAWN = 2, KNIGHT = 4, BISHOP = 6, ROOK = 8, QUEEN = 10, KING = 12;
	constexpr int MIN_PIECE = PAWN;

	// --- the packed move, see src/book/packed-move.h and format.py --------------------------
	constexpr int DIRECTIONS[16] = {
		+2 * 8 - 1, +2 * 8 + 1, +1 * 8 + 2, -1 * 8 + 2,
		+1 * 8 - 2, -1 * 8 - 2, -2 * 8 - 1, -2 * 8 + 1,
		+0 * 8 + 1, +0 * 8 - 1, +1 * 8 + 0, -1 * 8 + 0,
		+1 * 8 + 1, -1 * 8 + 1, +1 * 8 - 1, -1 * 8 - 1,
	};
	constexpr int KNIGHT_DIRECTIONS = 8;
	constexpr int PROMOTION_PIECES[8] = {
		QUEEN + BLACK, ROOK + BLACK, BISHOP + BLACK, KNIGHT + BLACK,
		KNIGHT + WHITE, BISHOP + WHITE, ROOK + WHITE, QUEEN + WHITE,
	};
	constexpr uint32_t TO_MASK = 0x3F, TO_RANK_MASK = 0x38, DIRECTION_SHIFT = 6;
	constexpr uint32_t DIRECTION_MASK = 0x3C0, PROMOTION_FLAG = 0x400;

	// --- the features, see src/nnue/nnue-features.h and format.py ---------------------------
	constexpr int PIECE_PLANES = 11;
	constexpr int FEATURE_COUNT = PIECE_PLANES * 64 * 64;          // 45056
	constexpr int SLOTS = 32;
	constexpr uint16_t PADDING = FEATURE_COUNT;
	constexpr uint32_t NO_GAME_VALUE = 0;

	struct Board {
		std::array<int8_t, 64> squares{};
		int kings[2] = { 4, 60 };
		bool whiteToMove = true;

		Board() {
			const int back[8] = { ROOK, KNIGHT, BISHOP, QUEEN, KING, BISHOP, KNIGHT, ROOK };
			for (int file = 0; file < 8; ++file) {
				squares[file] = int8_t(back[file] + WHITE);
				squares[8 + file] = int8_t(PAWN + WHITE);
				squares[48 + file] = int8_t(PAWN + BLACK);
				squares[56 + file] = int8_t(back[file] + BLACK);
			}
		}

		void apply(int departure, int destination, int promotion) {
			const int piece = squares[departure];
			const int kind = piece & ~1;
			if (kind == KING) {
				kings[piece & 1] = destination;
				const int step = destination - departure;
				if (step == 2) {
					squares[destination - 1] = squares[destination + 1];
					squares[destination + 1] = NO_PIECE;
				}
				else if (step == -2) {
					squares[destination + 1] = squares[destination - 2];
					squares[destination - 2] = NO_PIECE;
				}
			}
			else if (kind == PAWN && (departure & 7) != (destination & 7)
				&& squares[destination] == NO_PIECE) {
				squares[(departure & ~7) | (destination & 7)] = NO_PIECE;
			}
			squares[destination] = int8_t(promotion != NO_PIECE ? promotion : piece);
			squares[departure] = NO_PIECE;
			whiteToMove = !whiteToMove;
		}
	};

	/** from, to and promotion of a packed move - unpack_move in format.py. */
	void unpackMove(uint32_t bits, const Board& board, int& from, int& to, int& promotion) {
		promotion = NO_PIECE;
		if (bits & PROMOTION_FLAG) {
			const uint32_t code = (bits & TO_RANK_MASK) >> 3;
			promotion = PROMOTION_PIECES[code];
			bits = code > 3 ? (bits | TO_RANK_MASK) : (bits & ~TO_RANK_MASK);
		}
		to = int(bits & TO_MASK);
		const int direction = int((bits & DIRECTION_MASK) >> DIRECTION_SHIFT);
		if (direction < KNIGHT_DIRECTIONS) {
			from = to + DIRECTIONS[direction];
			return;
		}
		const int step = DIRECTIONS[direction];
		int square = to - step;
		while (square >= 0 && square < 64 && board.squares[square] == NO_PIECE) {
			square -= step;
		}
		from = square;
	}

	/** The features of one perspective, in square order, padded - features() in format.py. */
	void features(const Board& board, int perspective, uint16_t* out) {
		const int flip = perspective == BLACK ? 0x38 : 0;
		const int king = board.kings[perspective] ^ flip;
		const int base = king * PIECE_PLANES;
		int count = 0;
		for (int square = 0; square < 64; ++square) {
			const int piece = board.squares[square];
			if (piece == NO_PIECE) continue;
			const int relative = (piece - MIN_PIECE) ^ perspective;
			if (relative == 10) continue;                       // the own king
			const int plane = relative == 11 ? 10 : relative;
			out[count++] = uint16_t((base + plane) * 64 + (square ^ flip));
		}
		while (count < SLOTS) out[count++] = PADDING;
	}

	struct Position {
		uint16_t own[SLOTS];
		uint16_t other[SLOTS];
		uint16_t code;
		uint8_t result;
	};

	/** A game file, mapped rather than read: every process of a training shares one copy in the page
	 * cache. Read into memory instead, six processes over six sets held 6 x 3.1 GB. */
	struct File {
		const uint8_t* bytes = nullptr;
		size_t length = 0;
		const uint8_t* data() const { return bytes; }
		size_t size() const { return length; }
		uint8_t operator[](size_t at) const { return bytes[at]; }
	};

	struct Game {
		uint32_t file;
		uint64_t offset;       // of the length byte
		uint32_t plies;
	};

	[[noreturn]] void fail(const std::string& message) {
		std::fprintf(stderr, "batcher: %s\n", message.c_str());
		std::exit(1);
	}

	/** Every game of every file, numbered across the files in the order given - load_indexes(). */
	std::vector<Game> index(const std::vector<File>& files) {
		std::vector<Game> games;
		for (uint32_t f = 0; f < files.size(); ++f) {
			const auto& data = files[f];
			if (data.size() < 12 || std::memcmp(data.data(), "QAPLAGM2", 8) != 0) {
				fail("file " + std::to_string(f) + " is not a game file");
			}
			uint64_t offset = 12;
			while (offset < data.size()) {
				const uint32_t plies = data[offset];
				if (offset + 1 + 3ull * plies > data.size()) fail("a file ends inside a game");
				games.push_back({ f, offset, plies });
				offset += 1 + 3ull * plies;
			}
		}
		return games;
	}

	/** Whether a piece of the given colour attacks the square. */
	bool attacked(const Board& board, int square, int by) {
		const int file = square & 7, rank = square >> 3;
		auto holds = [&](int f, int r, int kind) {
			return f >= 0 && f < 8 && r >= 0 && r < 8 && board.squares[r * 8 + f] == kind + by;
		};
		// A white pawn attacks one rank up, a black one one rank down.
		const int pawnRank = by == WHITE ? rank - 1 : rank + 1;
		if (holds(file - 1, pawnRank, PAWN) || holds(file + 1, pawnRank, PAWN)) return true;
		static constexpr int KNIGHT_STEPS[8][2] = {
			{ 1, 2 }, { 2, 1 }, { 2, -1 }, { 1, -2 }, { -1, -2 }, { -2, -1 }, { -2, 1 }, { -1, 2 } };
		for (const auto& step : KNIGHT_STEPS) {
			if (holds(file + step[0], rank + step[1], KNIGHT)) return true;
		}
		static constexpr int RAYS[8][2] = {
			{ 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };
		for (int ray = 0; ray < 8; ++ray) {
			const int straight = ray < 4 ? ROOK : BISHOP;
			int f = file + RAYS[ray][0], r = rank + RAYS[ray][1];
			if (holds(f, r, KING)) return true;
			while (f >= 0 && f < 8 && r >= 0 && r < 8) {
				const int piece = board.squares[r * 8 + f];
				if (piece != NO_PIECE) {
					if (piece == straight + by || piece == QUEEN + by) return true;
					break;
				}
				f += RAYS[ray][0];
				r += RAYS[ray][1];
			}
		}
		return false;
	}

	/**
	 * --skip-tactical: a position whose label the net cannot read off the board. The move played
	 * from it captures - the value may hold a recapture the search sees and the board does not show -
	 * or the side to move is in check, which an engine never evaluates statically. Stockfish's trainer
	 * skips both.
	 */
	bool tactical(const Board& board, int from, int to) {
		const int own = board.whiteToMove ? WHITE : BLACK;
		if (attacked(board, board.kings[own], own ^ 1)) return true;
		if (board.squares[to] != NO_PIECE) return true;
		const bool pawn = (board.squares[from] & ~1) == PAWN;
		return pawn && (from & 7) != (to & 7);                    // en passant
	}

	bool skipTactical = false;

	/** Appends the positions of one game that carry a value - _positions_of() in gamedata.py. */
	void replay(const File& data, const Game& game, std::vector<Position>& out) {
		Board board;
		const uint8_t* p = data.data() + game.offset + 1;
		for (uint32_t i = 0; i < game.plies; ++i, p += 3) {
			const uint32_t record = uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16);
			const uint32_t move = record & 0x7FF;
			const uint32_t result = (record >> 11) & 0x3;
			const uint32_t value = (record >> 13) & 0x7FF;
			int from, to, promotion;
			unpackMove(move, board, from, to, promotion);
			if (value != NO_GAME_VALUE && !(skipTactical && tactical(board, from, to))) {
				Position position;
				const int own = board.whiteToMove ? WHITE : BLACK;
				features(board, own, position.own);
				features(board, own ^ 1, position.other);
				position.code = uint16_t(value);
				position.result = uint8_t(result);
				out.push_back(position);
			}
			board.apply(from, to, promotion);
		}
	}

	/** Writes positions [first, first + count) as one batch, column by column. */
	void writeBatch(const std::vector<Position>& pool, size_t first, size_t count,
		std::vector<uint8_t>& buffer) {
		const size_t featureBytes = count * SLOTS * sizeof(uint16_t);
		buffer.resize(2 * featureBytes + count * sizeof(uint16_t) + count);
		uint8_t* own = buffer.data();
		uint8_t* other = own + featureBytes;
		uint8_t* codes = other + featureBytes;
		uint8_t* results = codes + count * sizeof(uint16_t);
		for (size_t i = 0; i < count; ++i) {
			const Position& position = pool[first + i];
			std::memcpy(own + i * SLOTS * 2, position.own, SLOTS * 2);
			std::memcpy(other + i * SLOTS * 2, position.other, SLOTS * 2);
			std::memcpy(codes + i * 2, &position.code, 2);
			results[i] = position.result;
		}
		if (std::fwrite(buffer.data(), 1, buffer.size(), stdout) != buffer.size()) {
			std::exit(0);              // the reader went away - nothing left to do
		}
	}

	File mapFile(const char* path) {
		const int descriptor = ::open(path, O_RDONLY);
		if (descriptor < 0) fail(std::string("cannot open ") + path);
		struct stat info;
		if (::fstat(descriptor, &info) != 0) fail(std::string("cannot stat ") + path);
		File file;
		file.length = size_t(info.st_size);
		if (file.length > 0) {
			void* mapped = ::mmap(nullptr, file.length, PROT_READ, MAP_SHARED, descriptor, 0);
			if (mapped == MAP_FAILED) fail(std::string("cannot map ") + path);
			file.bytes = static_cast<const uint8_t*>(mapped);
		}
		::close(descriptor);
		return file;
	}

	std::vector<uint32_t> readIds() {
		std::vector<uint32_t> ids;
		uint32_t chunk[4096];
		size_t got;
		while ((got = std::fread(chunk, sizeof(uint32_t), 4096, stdin)) > 0) {
			ids.insert(ids.end(), chunk, chunk + got);
		}
		return ids;
	}
}

int main(int argc, char** argv) {
	size_t batch = 16384, bufferPositions = 1u << 17;
	uint64_t seed = 1;
	bool dump = false, onlyIndex = false;
	std::vector<const char*> paths;
	for (int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		if (arg == "--batch" && i + 1 < argc) batch = std::strtoull(argv[++i], nullptr, 10);
		else if (arg == "--buffer" && i + 1 < argc) bufferPositions = std::strtoull(argv[++i], nullptr, 10);
		else if (arg == "--seed" && i + 1 < argc) seed = std::strtoull(argv[++i], nullptr, 10);
		else if (arg == "--dump") dump = true;
		else if (arg == "--index") onlyIndex = true;
		else if (arg == "--skip-tactical") skipTactical = true;
		else paths.push_back(argv[i]);
	}
	if (paths.empty()) fail("no game files named");

	std::vector<File> files;
	for (const char* path : paths) files.push_back(mapFile(path));
	const std::vector<Game> games = index(files);
	if (onlyIndex) {
		// Three columns, one after the other, so that numpy can take each as it is.
		std::vector<uint64_t> offsets;
		std::vector<uint8_t> plies;
		std::vector<uint16_t> usable;
		for (const Game& game : games) {
			const uint8_t* p = files[game.file].data() + game.offset + 1;
			uint32_t withValue = 0;
			for (uint32_t i = 0; i < game.plies; ++i, p += 3) {
				const uint32_t record = uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16);
				if (((record >> 13) & 0x7FF) != NO_GAME_VALUE) ++withValue;
			}
			offsets.push_back(game.offset);
			plies.push_back(uint8_t(game.plies));
			usable.push_back(uint16_t(withValue));
		}
		const uint64_t count = games.size();
		std::fwrite(&count, sizeof(count), 1, stdout);
		std::fwrite(offsets.data(), sizeof(uint64_t), offsets.size(), stdout);
		std::fwrite(plies.data(), 1, plies.size(), stdout);
		std::fwrite(usable.data(), sizeof(uint16_t), usable.size(), stdout);
		std::fflush(stdout);
		return 0;
	}
	const std::vector<uint32_t> ids = readIds();

	std::vector<uint8_t> out;
	std::vector<Position> pool;
	if (dump) {
		for (uint32_t id : ids) {
			if (id >= games.size()) fail("game id out of range");
			pool.clear();
			replay(files[games[id].file], games[id], pool);
			for (size_t i = 0; i < pool.size(); ++i) writeBatch(pool, i, 1, out);
		}
		std::fflush(stdout);
		return 0;
	}

	// The same flow as _Games.__iter__: games in the order given, positions into a buffer, and
	// once the buffer is full it is shuffled and every whole batch in it goes out; the rest stays.
	std::mt19937_64 random(seed);
	pool.reserve(bufferPositions + 1024);
	for (uint32_t id : ids) {
		if (id >= games.size()) fail("game id out of range");
		replay(files[games[id].file], games[id], pool);
		if (pool.size() < bufferPositions) continue;
		std::shuffle(pool.begin(), pool.end(), random);
		const size_t keep = pool.size() % batch;
		for (size_t first = 0; first + batch <= pool.size() - keep; first += batch) {
			writeBatch(pool, first, batch, out);
		}
		pool.erase(pool.begin(), pool.end() - ptrdiff_t(keep));
	}
	std::shuffle(pool.begin(), pool.end(), random);
	for (size_t first = 0; first + batch <= pool.size(); first += batch) {
		writeBatch(pool, first, batch, out);
	}
	std::fflush(stdout);
	return 0;
}
