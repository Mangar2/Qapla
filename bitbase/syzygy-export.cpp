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
 * See syzygy-export.h.
 */

#include "syzygy-export.h"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <vector>

#include "bitbase.h"
#include "boardaccess.h"
#include "bitbase-repairfile.h"
#include "bitbaseindex.h"
#include "piecelist.h"
#include "reverseindex.h"

#include "../movegenerator/movegenerator.h"
#include "../src/syzygy/tbposition-builder.h"
#include "../src/syzygy/tbprobe.h"
#include "../src/syzygy/tbwrite.h"

using namespace QaplaBasics;
using namespace QaplaMoveGenerator;
using namespace QaplaSyzygy;

namespace QaplaBitbase {

	namespace {

		/** "KRK" as the format spells it in a file name: "KRvK". */
		std::string toFormatCode(const std::string& pieceString) {
			std::string code = pieceString;
			const size_t secondKing = code.find('K', 1);
			if (secondKing == std::string::npos) return code;
			code.insert(secondKing, "v");
			return code;
		}

		/**
		 * Qapla stores from white's point of view, the format from the side to move.
		 * Mirroring inside the index does not enter here: it maps a position to its
		 * colour reversed twin, whose value from the side to move is the same one.
		 */
		uint8_t toStoredValue(BitbaseResult result, bool whiteToMove) {
			switch (result) {
			case BitbaseResult::Win:  return whiteToMove ? StoredWin : StoredLoss;
			case BitbaseResult::Loss: return whiteToMove ? StoredLoss : StoredWin;
			default:                  return StoredDraw;
			}
		}

		/** Places the pieces of the reverse index on the board. */
		void buildPosition(MoveGenerator& position, const ReverseIndex& reverseIndex,
			const PieceList& pieceList) {

			position.clear();
			position.unsafeSetPiece(reverseIndex.getSquare(0), WHITE_KING);
			position.unsafeSetPiece(reverseIndex.getSquare(1), BLACK_KING);
			for (uint32_t pieceNo = 2; pieceNo < pieceList.getNumberOfPieces(); ++pieceNo)
				position.unsafeSetPiece(reverseIndex.getSquare(pieceNo), pieceList.getPiece(pieceNo));
			position.computeAttackMasksForBothColors();
			position.setWhiteToMove(reverseIndex.isWhiteToMove());
		}

		/** A legal placement of the material, in the piece order of the piece list. */
		struct PositionCase {
			uint8_t square[TB_MAX_PIECES] = {};
			bool    whiteToMove = false;
		};

		void placeAndCollect(const PieceList& pieceList, MoveGenerator& position,
			std::vector<PositionCase>& cases, PositionCase& current, uint32_t pieceNo,
			uint64_t used) {

			const uint32_t pieceCount = pieceList.getNumberOfPieces();

			if (pieceNo == pieceCount) {
				for (int side = 0; side < 2; ++side) {
					position.clear();
					for (uint32_t i = 0; i < pieceCount; ++i)
						position.unsafeSetPiece(Square(current.square[i]), pieceList.getPiece(i));
					position.setWhiteToMove(side == 0);
					if (!position.isLegal()) continue;
					current.whiteToMove = (side == 0);
					cases.push_back(current);
				}
				return;
			}

			for (uint8_t square = 0; square < 64; ++square) {
				if (used & (1ULL << square)) continue;
				current.square[pieceNo] = square;
				placeAndCollect(pieceList, position, cases, current, pieceNo + 1,
					used | (1ULL << square));
			}
		}

		/** Every legal position of this material, both sides to move. */
		std::vector<PositionCase> allPositions(const PieceList& pieceList) {
			std::vector<PositionCase> cases;
			MoveGenerator position;
			PositionCase current;
			placeAndCollect(pieceList, position, cases, current, 0, 0);
			return cases;
		}

		void setUpPosition(MoveGenerator& position, const PieceList& pieceList,
			const PositionCase& item) {

			position.clear();
			for (uint32_t i = 0; i < pieceList.getNumberOfPieces(); ++i)
				position.unsafeSetPiece(Square(item.square[i]), pieceList.getPiece(i));
			position.setWhiteToMove(item.whiteToMove);
			position.computeAttackMasksForBothColors();
		}

		std::string toFen(const PieceList& pieceList, const PositionCase& item) {
			char board[64] = {};
			for (uint32_t i = 0; i < pieceList.getNumberOfPieces(); ++i)
				board[item.square[i]] = pieceToChar(pieceList.getPiece(i));

			std::string fen;
			for (int rank = 7; rank >= 0; --rank) {
				int empty = 0;
				for (int file = 0; file < 8; ++file) {
					const char piece = board[rank * 8 + file];
					if (piece == 0) { ++empty; continue; }
					if (empty) { fen += char('0' + empty); empty = 0; }
					fen += piece;
				}
				if (empty) fen += char('0' + empty);
				if (rank) fen += '/';
			}
			fen += item.whiteToMove ? " w - - 0 1" : " b - - 0 1";
			return fen;
		}

		/**
		 * The resolved win/draw/loss value: the stored entry combined with everything
		 * the captures reach. See plan/syzygy-probe.md.
		 *
		 * @returns nothing when a table is missing, or when an en passant capture makes
		 *          the entry inapplicable
		 */
		std::optional<Wdl> resolveWdl(MoveGenerator& position) {

			MoveList moveList;
			position.genMovesOfMovingColor(moveList);

			Wdl best = Wdl::Loss;

			for (uint32_t i = 0; i < moveList.getTotalMoveAmount(); ++i) {
				const Move move = moveList[i];
				if (!move.isCapture()) continue;
				if (move.isEPMove()) return std::nullopt;

				const PositionSnapshot snapshot = position.getSnapshot();
				position.doMove(move);
				const bool legal = position.isLegal();
				std::optional<Wdl> reply;
				if (legal) reply = resolveWdl(position);
				position.undoMove(move, snapshot);
				position.computeAttackMasksForBothColors();

				if (!legal) continue;
				if (!reply) return std::nullopt;

				const Wdl value = Wdl(-int(*reply));
				if (int(value) > int(best)) best = value;
			}

			TbPosition tbPosition{};
			if (!buildTbPosition(position, tbPosition)) return std::nullopt;
			const WdlEntry entry = probeWdlEntry(tbPosition);
			if (entry.status != Status::Ok) return std::nullopt;

			return int(entry.value) > int(best) ? entry.value : best;
		}

		/** -1, 0 or 1 - the win/draw/loss answer with the fifty move rule folded away. */
		int sign(Wdl value) {
			if (int(value) > 0) return 1;
			if (int(value) < 0) return -1;
			return 0;
		}

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

	}   // anonymous namespace

	bool writeSyzygyWdl(const std::string& pieceString, const std::string& qwdlFile,
		const std::string& outDir, std::ostream& log) {

		const std::string code = toFormatCode(pieceString);

		WdlWriter writer(code);
		std::string reason;
		if (!writer.isSupported(reason)) {
			log << "cannot write " << code << ": " << reason << std::endl;
			return false;
		}

		BitbaseRePairFile source;
		if (!source.open(qwdlFile)) {
			log << "cannot open " << qwdlFile << std::endl;
			return false;
		}

		PieceList pieceList(pieceString);
		const BitbaseIndex indexType(pieceList);
		const uint64_t entryCount = indexType.getEntryCount();

		if (source.getEntryCount() != entryCount) {
			log << "the bitbase holds " << source.getEntryCount() << " entries, the index "
				<< entryCount << std::endl;
			return false;
		}

		// The index of the position a slot was filled from, so that a slot filled
		// twice can name both positions
		std::vector<uint64_t> sourceIndex[2][4];
		std::vector<uint8_t>  values[2][4];
		for (int side = 0; side < writer.sideCount(); ++side)
			for (int file = 0; file < writer.fileCount(); ++file)
				values[side][file].assign(size_t(writer.tableSize(side, file)), TB_UNREACHED),
				sourceIndex[side][file].assign(size_t(writer.tableSize(side, file)), 0);

		MoveGenerator position;
		uint64_t written = 0;
		uint64_t illegal = 0;
		uint64_t conflicts = 0;

		for (uint64_t index = 0; index < entryCount; ++index) {

			// The file cannot say which entries are illegal: the compressor treats
			// them as jokers and hands back a neighbour's value. So legality is
			// established here, exactly as the generator establishes it - a position
			// that is not legal, or whose index is not the canonical one of its
			// symmetry class, carries no value at all.
			const ReverseIndex reverseIndex(index, pieceList);
			if (!reverseIndex.isLegal()) { ++illegal; continue; }

			buildPosition(position, reverseIndex, pieceList);
			if (index != BoardAccess::getIndex<0>(position) || !position.isLegal()) {
				++illegal;
				continue;
			}

			const BitbaseResult result = source.probe(index);

			TbPosition tbPosition{};
			if (!buildTbPosition(position, tbPosition)) {
				log << "position of index " << index << " does not fit the format" << std::endl;
				return false;
			}

			const WdlSlot slot = writer.slotOf(tbPosition);
			const uint8_t value = toStoredValue(result, position.isWhiteToMove());

			uint8_t& stored = values[slot.side][slot.file][size_t(slot.index)];
			if (stored != TB_UNREACHED && stored != value) {
				if (++conflicts <= 5) {
					PositionCase a, b;
					const ReverseIndex other(sourceIndex[slot.side][slot.file][size_t(slot.index)], pieceList);
					for (uint32_t i = 0; i < pieceList.getNumberOfPieces(); ++i) {
						a.square[i] = uint8_t(other.getSquare(i));
						b.square[i] = uint8_t(reverseIndex.getSquare(i));
					}
					a.whiteToMove = other.isWhiteToMove();
					b.whiteToMove = reverseIndex.isWhiteToMove();
					log << "conflict at side " << slot.side << " offset " << slot.index << ":\n"
						<< "   " << toFen(pieceList, a) << " value " << int(stored) << "\n"
						<< "   " << toFen(pieceList, b) << " value " << int(value) << std::endl;
				}
			}
			sourceIndex[slot.side][slot.file][size_t(slot.index)] = index;
			stored = value;
			++written;
		}

		uint64_t slots = 0;
		uint64_t unreached = 0;
		for (int side = 0; side < writer.sideCount(); ++side)
			for (int file = 0; file < writer.fileCount(); ++file) {
				slots += values[side][file].size();
				unreached += std::count(values[side][file].begin(), values[side][file].end(),
					uint8_t(TB_UNREACHED));
			}

		const std::filesystem::path filePath = std::filesystem::path(outDir) / (code + ".rtbw");
		std::filesystem::create_directories(outDir);

		try {
			writer.write(filePath.string(), values);
		}
		catch (const std::exception& e) {
			log << e.what() << std::endl;
			return false;
		}

		log << code << ": " << entryCount << " indexed positions, " << illegal
			<< " illegal, " << written << " values placed in " << slots << " slots, "
			<< unreached << " never reached" << std::endl;
		log << "written " << filePath.string() << " ("
			<< std::filesystem::file_size(filePath) << " bytes)" << std::endl;

		if (conflicts > 0)
			log << "WARNING: " << conflicts << " slots were filled twice with different values."
			<< " The file holds the value that arrived first." << std::endl;

		return conflicts == 0;
	}

	bool compareSyzygyWdl(const std::string& pieceString, const std::string& ourDir,
		const std::string& refDir, std::ostream& log) {

		const std::string code = toFormatCode(pieceString);
		const PieceList pieceList(pieceString);

		const std::vector<PositionCase> cases = allPositions(pieceList);
		log << code << ": " << cases.size() << " legal positions" << std::endl;

		std::vector<int8_t> answers[2];
		const std::string paths[2] = { ourDir, refDir };
		const char* names[2] = { "ours", "reference" };

		for (int pass = 0; pass < 2; ++pass) {

			const LoadResult loaded = setPath(paths[pass]);
			log << names[pass] << " (" << paths[pass] << "): " << loaded.wdlFiles
				<< " win/draw/loss files" << std::endl;

			if (loaded.wdlFiles == 0) {
				log << "no table found in " << paths[pass] << std::endl;
				release();
				return false;
			}

			answers[pass].assign(cases.size(), 127);
			MoveGenerator position;

			for (size_t i = 0; i < cases.size(); ++i) {
				setUpPosition(position, pieceList, cases[i]);
				const std::optional<Wdl> value = resolveWdl(position);
				if (value) answers[pass][i] = int8_t(*value);
			}
			release();
		}

		uint64_t histogram[2][5] = {};
		uint64_t compared = 0;
		uint64_t missing = 0;
		uint64_t cursed = 0;
		uint64_t differences = 0;
		MoveGenerator position;

		for (size_t i = 0; i < cases.size(); ++i) {

			if (answers[0][i] == 127 || answers[1][i] == 127) { ++missing; continue; }

			const Wdl ours = Wdl(answers[0][i]);
			const Wdl reference = Wdl(answers[1][i]);
			++compared;

			histogram[0][int(ours) + 2]++;
			histogram[1][int(reference) + 2]++;

			if (int(reference) == 1 || int(reference) == -1) ++cursed;

			if (sign(ours) != sign(reference)) {
				if (++differences <= 10)
					log << "  " << toFen(pieceList, cases[i]) << " : ours " << wdlName(ours)
					<< ", reference " << wdlName(reference) << std::endl;
			}
		}

		log << compared << " positions compared, " << missing << " skipped for a missing table"
			<< std::endl;

		for (int pass = 0; pass < 2; ++pass) {
			log << names[pass] << ":";
			for (int value = -2; value <= 2; ++value)
				log << " " << wdlName(Wdl(value)) << " " << histogram[pass][value + 2];
			log << std::endl;
		}

		log << cursed << " of them are a cursed win or a blessed loss in the reference and a "
			"plain win or loss here" << std::endl;
		log << differences << " positions differ in sign" << std::endl;

		return differences == 0;
	}

}
