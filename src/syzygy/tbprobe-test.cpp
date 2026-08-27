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
 * Standalone check of the tablebase format layer: builds a piece list by hand,
 * looks it up and compares against answers that are known independently.
 *
 * It links tbprobe.cpp alone - no board, no move generator, no search - which is
 * the point: if this runs, the layer really is free of the engine.
 *
 * The main is behind a define so that the engine build, which globs every .cpp,
 * does not end up with two of them. Build and run it with:
 *
 *   clang++ -std=c++20 -O2 -DQAPLA_SYZYGY_TESTMAIN \
 *       src/syzygy/tbprobe.cpp src/syzygy/tbprobe-test.cpp -o build/tbtest.exe
 *   build/tbtest.exe C:/Chess/syzygy/tables
 */

#ifdef QAPLA_SYZYGY_TESTMAIN

#include "tbprobe.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace QaplaSyzygy;

namespace {

	const char* wdlName(Wdl value) {
		switch (value) {
		case Wdl::Loss:        return "loss";
		case Wdl::BlessedLoss: return "blessed loss";
		case Wdl::Draw:        return "draw";
		case Wdl::CursedWin:   return "cursed win";
		case Wdl::Win:         return "win";
		}
		return "?";
	}

	const char* statusName(Status status) {
		switch (status) {
		case Status::Ok:              return "ok";
		case Status::NoTable:         return "no table";
		case Status::OtherSideToMove: return "other side to move";
		}
		return "?";
	}

	/** Piece letter to handover code, upper case white, lower case black. */
	int pieceCodeOf(char c) {
		static const std::string white = "PNBRQK";
		static const std::string black = "pnbrqk";
		const size_t w = white.find(c);
		if (w != std::string::npos) return int(WhitePawn) + int(w);
		const size_t b = black.find(c);
		if (b != std::string::npos) return int(BlackPawn) + int(b);
		return -1;
	}

	/**
	 * Reads placement and side to move of a FEN into the handover structure.
	 * The list has to be sorted ascending by piece, so the squares are collected
	 * per piece code first.
	 */
	bool fenToTbPosition(const std::string& fen, TbPosition& pos) {

		std::vector<uint8_t> squaresOf[TbPieceCodeAmount];

		int file = 0, rank = 7;
		size_t index = 0;

		for (; index < fen.size() && fen[index] != ' '; ++index) {
			const char c = fen[index];
			if (c == '/') { file = 0; --rank; continue; }
			if (c >= '1' && c <= '8') { file += c - '0'; continue; }

			const int code = pieceCodeOf(c);
			if (code < 0 || rank < 0 || file > 7) return false;
			squaresOf[code].push_back(uint8_t(rank * 8 + file));
			++file;
		}

		while (index < fen.size() && fen[index] == ' ') ++index;
		pos.whiteToMove = index >= fen.size() || fen[index] != 'b';

		uint8_t amount = 0;
		for (int code = 0; code < TbPieceCodeAmount; ++code)
			for (const uint8_t square : squaresOf[code]) {
				if (amount >= TB_MAX_PIECES) return false;
				pos.square[amount] = square;
				pos.piece[amount] = uint8_t(code);
				++amount;
			}

		pos.pieceCount = amount;
		return amount >= 2;
	}

	/** Same position with the colours swapped and the ranks flipped. */
	TbPosition mirrored(const TbPosition& pos) {

		TbPosition result{};
		std::vector<std::pair<uint8_t, uint8_t>> entries;   // code, square

		for (int i = 0; i < pos.pieceCount; ++i) {
			const int code = pos.piece[i];
			const int swapped = code < int(BlackPawn) ? code + 6 : code - 6;
			entries.emplace_back(uint8_t(swapped), uint8_t(pos.square[i] ^ 56));
		}

		std::stable_sort(entries.begin(), entries.end(),
			[](const auto& a, const auto& b) { return a.first < b.first; });

		for (size_t i = 0; i < entries.size(); ++i) {
			result.piece[i] = entries[i].first;
			result.square[i] = entries[i].second;
		}
		result.pieceCount = pos.pieceCount;
		result.whiteToMove = !pos.whiteToMove;
		return result;
	}

	struct Case {
		const char* fen;
		const char* comment;
		bool        checked;      // false: report only, no expectation
		Wdl         expected;
	};

	// The two king and pawn positions are the ground truth: Qapla's own compiled-in
	// KPK bitbase answers draw for the first and win for the second.
	const Case cases[] = {
		{ "8/8/8/8/8/1k6/1P6/1K6 w",  "KPvK, king in front, opposition", true,  Wdl::Draw },
		{ "8/8/3K4/3P4/8/8/8/3k4 w",  "KPvK, black king far away",       true,  Wdl::Win  },
		{ "8/8/8/8/8/1k6/8/K1Q5 w",   "KQvK",                            true,  Wdl::Win  },
		{ "8/8/8/8/8/1k6/8/K1R5 w",   "KRvK",                            true,  Wdl::Win  },
		{ "8/8/8/8/8/1k6/8/K1N5 w",   "KNvK, insufficient material",     true,  Wdl::Draw },
		{ "8/8/8/8/8/1k6/8/K1B5 w",   "KBvK, insufficient material",     true,  Wdl::Draw },
		{ "8/8/8/8/8/1k6/8/K1Q5 b",   "KQvK, weak side to move",         true,  Wdl::Loss },
		{ "8/8/8/8/8/1k6/8/K1R5 b",   "KRvK, weak side to move",         true,  Wdl::Loss },
		{ "8/8/8/8/8/1k6/8/K1R3r1 w", "KRvKR",                           false, Wdl::Draw },
		{ "8/2p5/8/8/8/8/2P5/K6k w",  "KPvKP",                           false, Wdl::Draw },
	};

	int failures = 0;

	void runCase(const Case& item) {

		TbPosition pos{};
		if (!fenToTbPosition(item.fen, pos)) {
			std::cout << "  FAIL  cannot read " << item.fen << "\n";
			++failures;
			return;
		}

		const WdlEntry entry = probeWdlEntry(pos);

		std::cout << "  " << (item.checked ? "check " : "info  ")
			<< item.comment << "\n"
			<< "        " << item.fen
			<< "  ->  " << statusName(entry.status);

		if (entry.status == Status::Ok) std::cout << ", " << wdlName(entry.value);
		std::cout << "\n";

		if (entry.status != Status::Ok) {
			std::cout << "  FAIL  no answer\n";
			++failures;
			return;
		}

		if (item.checked && entry.value != item.expected) {
			std::cout << "  FAIL  expected " << wdlName(item.expected) << "\n";
			++failures;
			return;
		}

		// Swapping the colours and flipping the ranks puts the same material side on
		// move again, and the value is relative to the side to move - so it has to come
		// out identical. This exercises the mirroring inside the index computation
		// without needing an external reference.
		const TbPosition flipped = mirrored(pos);
		const WdlEntry mirrorEntry = probeWdlEntry(flipped);

		if (mirrorEntry.status != Status::Ok) {
			std::cout << "  FAIL  mirror has no answer\n";
			++failures;
			return;
		}

		if (mirrorEntry.value != entry.value) {
			std::cout << "  FAIL  mirror gives " << wdlName(mirrorEntry.value)
				<< ", expected " << wdlName(entry.value) << "\n";
			++failures;
		}
	}

	/** Distance lookup for one position, reported only - it needs corrections we do not do here. */
	void reportDistance(const char* fen) {

		TbPosition pos{};
		if (!fenToTbPosition(fen, pos)) return;

		const WdlEntry wdl = probeWdlEntry(pos);
		if (wdl.status != Status::Ok) return;

		const DtzEntry dtz = probeDtzEntry(pos, wdl.value);

		std::cout << "  info  distance for " << fen << "  ->  " << statusName(dtz.status);
		if (dtz.status == Status::Ok) std::cout << ", " << dtz.distance << " (raw entry)";
		std::cout << "\n";
	}

}

/** Reports what the tables hold for one position, without any correction. */
static void reportPosition(const char* fen) {

	TbPosition pos{};
	if (!fenToTbPosition(fen, pos)) {
		std::cout << "  cannot read " << fen << "\n";
		return;
	}

	const WdlEntry wdl = probeWdlEntry(pos);
	std::cout << fen << "\n  wdl: " << statusName(wdl.status);
	if (wdl.status == Status::Ok) std::cout << ", " << wdlName(wdl.value);

	if (wdl.status == Status::Ok) {
		const DtzEntry dtz = probeDtzEntry(pos, wdl.value);
		std::cout << "\n  dtz: " << statusName(dtz.status);
		if (dtz.status == Status::Ok) std::cout << ", " << dtz.distance << " (raw entry)";
	}
	std::cout << "\n\n";
}

int main(int argc, char** argv) {

	const std::string path = argc > 1 ? argv[1] : "C:/Chess/syzygy/tables";

	std::cout << "Syzygy format layer test\npath: " << path << "\n";

	const LoadResult loaded = setPath(path);

	std::cout << "found " << loaded.wdlFiles << " win/draw/loss and " << loaded.dtzFiles
		<< " distance files, up to " << loaded.maxCardinality << " pieces\n\n";

	if (loaded.wdlFiles == 0) {
		std::cout << "no tables found, nothing to check\n";
		return 1;
	}

	// Any further arguments are positions to look up instead of running the checks.
	if (argc > 2) {
		for (int index = 2; index < argc; ++index) reportPosition(argv[index]);
		release();
		return 0;
	}

	for (const Case& item : cases) runCase(item);

	std::cout << "\n";
	reportDistance("8/8/3K4/3P4/8/8/8/3k4 w");
	reportDistance("8/8/8/8/8/1k6/8/K1Q5 w");

	release();

	std::cout << "\n" << (failures == 0 ? "all checks passed" : "FAILURES: " + std::to_string(failures))
		<< "\n";
	return failures == 0 ? 0 : 1;
}

#endif
