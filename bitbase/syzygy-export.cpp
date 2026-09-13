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
#include <chrono>
#include <iomanip>
#include <filesystem>
#include <optional>
#include <vector>

#include "bitbase.h"
#include "boardaccess.h"
#include "bitbase-reader.h"
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

			const size_t secondKing = pieceString.find('K', 1);
			if (secondKing == std::string::npos) return pieceString;

			std::string first = pieceString.substr(1, secondKing - 1);
			std::string second = pieceString.substr(secondKing + 1);

			// The stronger side names the file: the format stores one table for both
			// colours and mirrors a position whose key does not match, so KBvKQ and
			// KQvKB are the same table and only one of the two names is the file.
			const auto rank = [](char piece) {
				const size_t order = std::string("PNBRQ").find(piece);
				return order == std::string::npos ? 0 : int(order) + 1;
			};

			bool swap = first.size() < second.size();
			if (first.size() == second.size())
				for (size_t i = 0; i < first.size(); ++i)
					if (rank(first[i]) != rank(second[i])) {
						swap = rank(first[i]) < rank(second[i]);
						break;
					}

			if (swap) first.swap(second);
			return "K" + first + "vK" + second;
		}

		/**
		 * Qapla stores from white's point of view, the format from the side to move.
		 * Mirroring inside the index does not enter here: it maps a position to its
		 * colour reversed twin, whose value from the side to move is the same one.
		 */
		uint8_t toStoredValue(BitbaseResult result, bool whiteToMove) {
			switch (result) {
			case BitbaseResult::Win:         return whiteToMove ? StoredWin : StoredLoss;
			case BitbaseResult::Loss:        return whiteToMove ? StoredLoss : StoredWin;
			case BitbaseResult::CursedWin:   return whiteToMove ? StoredCursedWin
																: StoredBlessedLoss;
			case BitbaseResult::BlessedLoss: return whiteToMove ? StoredBlessedLoss
																: StoredCursedWin;
			default:                         return StoredDraw;
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

		/**
		 * The same position mirrored on the file axis. Qapla's index maps a pawn position
		 * and its mirror to one class, the format does not always: it mirrors by the file
		 * of the leading pawn, so a pawn set that is symmetric itself - a2 and h2 - leaves
		 * both images on the same file and they end up in different slots. Both have to be
		 * written, and the value is the same for either.
		 */
		/** The square mirrored at the d-e file boundary: a1 becomes h1. */
		int mirroredFile(int square) { return square ^ 7; }

		/** The square mirrored at the a1-h8 diagonal: file and rank change places. */
		int mirroredDiagonal(int square) { return ((square >> 3) | (square << 3)) & 63; }

		/**
		 * The image of the position under one of the symmetries of the board.
		 *
		 * Qapla's index folds a symmetry class into a single entry, the format does not
		 * always: it decides its own mirroring from the leading group, and where that
		 * group cannot decide, both images of the class get a slot of their own. So the
		 * value is written to the slot of the image as well - see the call site.
		 */
		void buildMirror(MoveGenerator& position, const PieceList& pieceList,
			const ReverseIndex& reverseIndex, int (*mirror)(int)) {

			position.clear();
			position.unsafeSetPiece(Square(mirror(int(reverseIndex.getSquare(0)))), WHITE_KING);
			position.unsafeSetPiece(Square(mirror(int(reverseIndex.getSquare(1)))), BLACK_KING);
			for (uint32_t pieceNo = 2; pieceNo < pieceList.getNumberOfPieces(); ++pieceNo)
				position.unsafeSetPiece(Square(mirror(int(reverseIndex.getSquare(pieceNo)))),
					pieceList.getPiece(pieceNo));
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

			// A pawn on the first or the last rank is not a position, and the format
			// has no index for one - the move generator does not object, so it is
			// ruled out here.
			const bool pawn = isPawn(pieceList.getPiece(pieceNo));

			for (uint8_t square = 0; square < 64; ++square) {
				if (used & (1ULL << square)) continue;
				if (pawn && (square < 8 || square >= 56)) continue;
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

		PositionCase positionCaseOf(const ReverseIndex& reverseIndex, const PieceList& pieceList) {
			PositionCase item;
			for (uint32_t i = 0; i < pieceList.getNumberOfPieces(); ++i)
				item.square[i] = uint8_t(reverseIndex.getSquare(i));
			item.whiteToMove = reverseIndex.isWhiteToMove();
			return item;
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

		std::optional<Wdl> resolveWdl(MoveGenerator& position);

		/**
		 * The best value the captures of this position reach, from the side to move.
		 * A loss when there is no capture, which is the neutral element of the maximum
		 * the reader takes between the entry and the captures.
		 *
		 * @returns nothing when a table below is missing or an en passant capture is
		 *          available, because then the answer cannot be established
		 */
		std::optional<Wdl> bestCaptureValue(MoveGenerator& position) {

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

			return best;
		}

		/**
		 * The resolved win/draw/loss value: the stored entry combined with everything
		 * the captures reach. See plan/syzygy-probe.md.
		 *
		 * @returns nothing when a table is missing, or when an en passant capture makes
		 *          the entry inapplicable
		 */
		std::optional<Wdl> resolveWdl(MoveGenerator& position) {

			const std::optional<Wdl> best = bestCaptureValue(position);
			if (!best) return std::nullopt;

			TbPosition tbPosition{};
			if (!buildTbPosition(position, tbPosition)) return std::nullopt;
			const WdlEntry entry = probeWdlEntry(tbPosition);
			if (entry.status != Status::Ok) return std::nullopt;

			return int(entry.value) > int(*best) ? entry.value : *best;
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


		/** How many layouts of the sample ranking are tried on the whole table. */
		constexpr int LAYOUT_SHORT_LIST = 3;

		/**
		 * Picks the group layout every table is written in.
		 *
		 * The format leaves two things open per table: which pieces form the leading
		 * group, and where each group sits in the multiplication chain. Both decide the
		 * order the values lie in, and the compression lives on that order - the same
		 * table came out three times as large in the worst layout as in the best one.
		 *
		 * The layouts are compared on a sample rather than on the whole table: windows
		 * spread evenly over the index, each a contiguous stretch, because it is exactly
		 * the neighbourhood of the values that is being judged. The estimate is scaled
		 * up to the size of the table, so that a layout which needs fewer entries - two
		 * equal pieces in one group instead of two groups - is credited for it.
		 *
		 * The sample ranks, it does not decide: the caller writes the first few layouts
		 * out in full and keeps the smallest.
		 *
		 * Layouts are done in batches so that the memory stays bounded when a material
		 * offers hundreds of them; every batch costs one walk over the positions.
		 */
		void chooseLayouts(WdlWriter& writer, const PieceList& pieceList,
			const std::vector<uint8_t>& valueOf, int (*mirror)(int),
			std::vector<int> shortList[2][4]) {

			constexpr uint64_t SAMPLE_WINDOWS = 32;
			constexpr uint64_t SAMPLE_WINDOW_ENTRIES = 4096;
			constexpr int      BATCH = 64;

			const int layouts = writer.layoutCount();

			for (int side = 0; side < 2; ++side)
				for (int file = 0; file < 4; ++file) shortList[side][file].assign(1, 0);

			if (layouts <= 1) return;

			// The best few of the sample ranking, in ascending estimate
			uint64_t bestBytes[2][4][LAYOUT_SHORT_LIST];
			int      bestLayout[2][4][LAYOUT_SHORT_LIST] = {};
			for (auto& sideRow : bestBytes)
				for (auto& fileRow : sideRow)
					for (auto& entry : fileRow) entry = UINT64_MAX;

			MoveGenerator position;
			MoveGenerator mirrorPosition;

			for (int first = 0; first < layouts; first += BATCH) {

				const int count = std::min(BATCH, layouts - first);

				// sample[layout - first][side][file], and the window geometry per layout
				std::vector<std::vector<uint8_t>> sample(size_t(count) * 8);
				std::vector<uint64_t> stride(size_t(count) * 4, 1);
				std::vector<uint64_t> length(size_t(count) * 4, 0);

				for (int layout = 0; layout < count; ++layout)
					for (int file = 0; file < writer.fileCount(); ++file) {

						const uint64_t size = writer.tableSizeOf(first + layout, file);
						const uint64_t windowStride = std::max<uint64_t>(1, size / SAMPLE_WINDOWS);
						const uint64_t windowLength = std::min(SAMPLE_WINDOW_ENTRIES, windowStride);

						stride[size_t(layout) * 4 + file] = windowStride;
						length[size_t(layout) * 4 + file] = windowLength;

						for (int side = 0; side < writer.sideCount(); ++side)
							sample[size_t(layout) * 8 + size_t(side) * 4 + file]
								.assign(size_t(windowLength * SAMPLE_WINDOWS), TB_UNREACHED);
					}

				std::vector<int> batch(size_t(count), 0);
				for (int layout = 0; layout < count; ++layout) batch[size_t(layout)] = first + layout;
				std::vector<WdlSlot> slots(size_t(count) * 2);

				for (uint64_t index = 0; index < valueOf.size(); ++index) {

					const uint8_t value = valueOf[size_t(index)];
					if (value == TB_UNREACHED) continue;

					const ReverseIndex reverseIndex(index, pieceList);
					buildPosition(position, reverseIndex, pieceList);
					buildMirror(mirrorPosition, pieceList, reverseIndex, mirror);

					TbPosition tbPosition{};
					TbPosition tbMirror{};
					if (!buildTbPosition(position, tbPosition)) continue;
					if (!buildTbPosition(mirrorPosition, tbMirror)) continue;

					writer.slotsOf(tbPosition, batch.data(), count, slots.data());
					writer.slotsOf(tbMirror, batch.data(), count, slots.data() + count);

					for (int layout = 0; layout < count; ++layout)
						for (int which = 0; which < 2; ++which) {

							const WdlSlot& slot = slots[size_t(which) * count + layout];
							const uint64_t windowStride = stride[size_t(layout) * 4 + slot.file];
							const uint64_t windowLength = length[size_t(layout) * 4 + slot.file];
							const uint64_t window = slot.index / windowStride;
							const uint64_t offset = slot.index - window * windowStride;

							if (window >= SAMPLE_WINDOWS || offset >= windowLength) continue;

							sample[size_t(layout) * 8 + size_t(slot.side) * 4 + slot.file]
								[size_t(window * windowLength + offset)] = value;
						}
				}

				for (int layout = 0; layout < count; ++layout)
					for (int side = 0; side < writer.sideCount(); ++side)
						for (int file = 0; file < writer.fileCount(); ++file) {

							const std::vector<uint8_t>& entries =
								sample[size_t(layout) * 8 + size_t(side) * 4 + file];
							if (entries.empty()) continue;

							// Scaled to the whole table, so that layouts of different
							// index sizes can be held against each other
							const uint64_t bytes = writer.compressedSize(entries, true)
								* writer.tableSizeOf(first + layout, file) / entries.size();

							// Sorted insert into the short list
							for (int rank = 0; rank < LAYOUT_SHORT_LIST; ++rank) {
								if (bytes >= bestBytes[side][file][rank]) continue;
								for (int k = LAYOUT_SHORT_LIST - 1; k > rank; --k) {
									bestBytes[side][file][k] = bestBytes[side][file][k - 1];
									bestLayout[side][file][k] = bestLayout[side][file][k - 1];
								}
								bestBytes[side][file][rank] = bytes;
								bestLayout[side][file][rank] = first + layout;
								break;
							}
						}
			}

			for (int side = 0; side < writer.sideCount(); ++side)
				for (int file = 0; file < writer.fileCount(); ++file) {

					std::vector<int>& list = shortList[side][file];
					list.clear();
					for (int rank = 0; rank < LAYOUT_SHORT_LIST; ++rank)
						if (bestBytes[side][file][rank] != UINT64_MAX)
							list.push_back(bestLayout[side][file][rank]);
					if (list.empty()) list.push_back(0);
				}
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

		// Tables of the materials a capture leads into answer from the same directory.
		// They decide whether an entry may be stored below its true value; where one is
		// missing, the true value is stored and nothing is lost but size.
		setPath(outDir);

		MoveGenerator position;
		MoveGenerator mirrorPosition;
		uint64_t written = 0;
		uint64_t illegal = 0;
		uint64_t conflicts = 0;
		uint64_t reducible = 0;

		// Qapla's index folds a whole symmetry class into one entry, the format does
		// not always. Without pawns it settles file and rank from its leading group but
		// cannot settle the diagonal when that group stands on it; with pawns it mirrors
		// by the file of the leading pawn, which settles nothing when the pawns are
		// their own mirror. In both cases the class has two slots, and the second one is
		// reached through this image of the position.
		int (*const mirror)(int) = pieceList.getNumberOfPawns() > 0
			? mirroredFile : mirroredDiagonal;

		// ---- the value of every position, computed once ----
		//
		// The second pass below places these values, and the search between the two
		// passes needs them as well. Keeping them costs one byte per indexed position
		// and saves the move generation of bestCaptureValue() a second time.
		std::vector<uint8_t> valueOf(size_t(entryCount), TB_UNREACHED);

		for (uint64_t index = 0; index < entryCount; ++index) {

			// The file cannot say which entries are illegal: the compressor treats them
			// as jokers and hands back a neighbour's value. So legality is established
			// here, on the position itself.
			//
			// The value is read at the index the position computes, not at the one the
			// walk is on. The two differ where the reverse index hands back another
			// member of the same symmetry class, and the generator marks exactly those
			// as illegal - skipping them would leave the whole class without a value.
			const ReverseIndex reverseIndex(index, pieceList);
			if (!reverseIndex.isLegal()) { ++illegal; continue; }

			buildPosition(position, reverseIndex, pieceList);
			if (!position.isLegal()) { ++illegal; continue; }

			const BitbaseResult result = source.probe(BoardAccess::getIndex<0>(position));

			buildMirror(mirrorPosition, pieceList, reverseIndex, mirror);

			TbPosition tbPosition{};
			TbPosition tbMirror{};
			if (!buildTbPosition(mirrorPosition, tbMirror)) { ++illegal; continue; }
			if (!buildTbPosition(position, tbPosition)) {
				log << "position of index " << index << " does not fit the format" << std::endl;
				return false;
			}

			uint8_t value = toStoredValue(result, position.isWhiteToMove());

			// Where a capture already reaches the value, the entry is free to sit below
			// it. That is what keeps a table short, so it is worth the move generation.
			const std::optional<Wdl> best = bestCaptureValue(position);
			if (best && int(*best) + 2 == int(value)) {
				value |= TB_REDUCIBLE;
				++reducible;
			}

			valueOf[size_t(index)] = value;
		}

		release();

		// ---- which layout each table is written in ----
		std::vector<int> shortList[2][4];
		chooseLayouts(writer, pieceList, valueOf, mirror, shortList);

		// The index of the position a slot was filled from, so that a slot filled
		// twice can name both positions
		std::vector<uint64_t> sourceIndex[2][4];
		std::vector<uint8_t>  values[2][4];

		// One table per short-listed layout. The sample of the search only ranks them,
		// the whole table decides - the ranking is close but not exact, and the
		// difference between the first and the second layout is a few percent of the
		// file.
		std::vector<std::vector<uint8_t>> attempt[2][4];

		for (int side = 0; side < writer.sideCount(); ++side)
			for (int file = 0; file < writer.fileCount(); ++file) {
				sourceIndex[side][file].assign(size_t(writer.tableSizeOf(
					shortList[side][file][0], file)), 0);
				for (const int layout : shortList[side][file])
					attempt[side][file].emplace_back(
						size_t(writer.tableSizeOf(layout, file)), TB_UNREACHED);
			}

		// ---- placing the values ----
		for (uint64_t index = 0; index < entryCount; ++index) {

			const uint8_t value = valueOf[size_t(index)];
			if (value == TB_UNREACHED) continue;

			const ReverseIndex reverseIndex(index, pieceList);
			buildPosition(position, reverseIndex, pieceList);
			buildMirror(mirrorPosition, pieceList, reverseIndex, mirror);

			TbPosition tbPosition{};
			TbPosition tbMirror{};
			buildTbPosition(mirrorPosition, tbMirror);
			buildTbPosition(position, tbPosition);

			// Side and file do not depend on the layout, so one lookup names the table
			// and with it the layouts this position has to be placed in.
			const WdlSlot where = writer.slotOf(tbPosition, shortList[0][0][0]);
			const std::vector<int>& list = shortList[where.side][where.file];

			WdlSlot placed[2 * LAYOUT_SHORT_LIST];
			writer.slotsOf(tbPosition, list.data(), int(list.size()), placed);
			writer.slotsOf(tbMirror, list.data(), int(list.size()), placed + list.size());

			for (size_t rank = 0; rank < list.size(); ++rank) {

				// The mirror may or may not be a slot of its own - where it is not, the
				// second write lands on the first and carries the same value.
				const WdlSlot slots[2] = { placed[rank], placed[list.size() + rank] };

				for (const WdlSlot& slot : slots) {

					uint8_t& stored =
						attempt[slot.side][slot.file][rank][size_t(slot.index)];

					if (rank == 0 && stored != TB_UNREACHED && stored != value) {
						if (++conflicts <= 5) {
							PositionCase a, b;
							const ReverseIndex other(
								sourceIndex[slot.side][slot.file][size_t(slot.index)], pieceList);
							for (uint32_t i = 0; i < pieceList.getNumberOfPieces(); ++i) {
								a.square[i] = uint8_t(other.getSquare(i));
								b.square[i] = uint8_t(reverseIndex.getSquare(i));
							}
							a.whiteToMove = other.isWhiteToMove();
							b.whiteToMove = reverseIndex.isWhiteToMove();
							log << "conflict at side " << slot.side << " offset " << slot.index
								<< ":\n   " << toFen(pieceList, a) << " value " << int(stored)
								<< "\n   " << toFen(pieceList, b) << " value " << int(value)
								<< std::endl;
						}
					}
					if (rank == 0)
						sourceIndex[slot.side][slot.file][size_t(slot.index)] = index;
					stored = value;
				}
			}
			++written;
		}

		// ---- the layout that came out smallest ----
		for (int side = 0; side < writer.sideCount(); ++side)
			for (int file = 0; file < writer.fileCount(); ++file) {

				uint64_t best = UINT64_MAX;
				size_t   bestRank = 0;

				for (size_t rank = 0; rank < attempt[side][file].size(); ++rank) {
					const uint64_t bytes =
						writer.compressedSize(attempt[side][file][rank], false);
					if (bytes >= best) continue;
					best = bytes;
					bestRank = rank;
				}

				writer.chooseLayout(side, file, shortList[side][file][bestRank]);
				values[side][file] = std::move(attempt[side][file][bestRank]);

				log << "  side " << side << " file " << file << ": layout "
					<< shortList[side][file][bestRank] << " of " << writer.layoutCount()
					<< ", " << best << " bytes" << std::endl;
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
			<< unreached << " never reached, " << reducible
			<< " reached by a capture and therefore free to store lower" << std::endl;
		std::string checksumReason;
		const bool checksumOk = verifyChecksum(filePath.string(), checksumReason);


		// Read every position back out of the file that was just written and hold it
		// against the value that went in. The writer may store below the true value where
		// a capture reaches it, so the entry may be lower - never higher, and never
		// different where nothing was allowed to lower it.
		setPath(outDir);
		uint64_t checked = 0;
		uint64_t wrong = 0;

		for (uint64_t index = 0; index < entryCount; ++index) {

			const ReverseIndex reverseIndex(index, pieceList);
			if (!reverseIndex.isLegal()) continue;
			buildPosition(position, reverseIndex, pieceList);
			if (!position.isLegal()) continue;

			TbPosition tbPosition{};
			if (!buildTbPosition(position, tbPosition)) continue;

			const WdlEntry entry = probeWdlEntry(tbPosition);
			if (entry.status != Status::Ok) continue;

			const BitbaseResult result = source.probe(BoardAccess::getIndex<0>(position));
			const uint8_t written = toStoredValue(result, position.isWhiteToMove());
			const WdlSlot slot = writer.slotOf(tbPosition);
			const bool mayBeLower =
				(values[slot.side][slot.file][size_t(slot.index)] & TB_REDUCIBLE) != 0;
			const int expected = int(written) - 2;
			++checked;

			// Lower is what the format allows where a capture reaches the value, and only
			// there. Anything else means the entry the reader finds is not the one that was
			// written - a different slot, or one that was written over.
			const bool ok = mayBeLower ? int(entry.value) <= expected
				: int(entry.value) == expected;

			if (!ok && ++wrong <= 5)
				log << "  the file answers " << int(entry.value) << " where " << expected
					<< " was written" << (mayBeLower ? " (may be lower)" : "") << " for "
					<< toFen(pieceList, positionCaseOf(reverseIndex, pieceList))
					<< " [side " << slot.side << " file " << slot.file
					<< " offset " << slot.index << "]" << std::endl;
		}

		release();
		if (wrong > 0)
			log << "WARNING: " << wrong << " of " << checked
			<< " entries did not read back as they were written" << std::endl;

		log << "written " << filePath.string() << " ("
			<< std::filesystem::file_size(filePath) << " bytes, check bytes "
			<< (checksumOk ? "ok" : checksumReason) << ")" << std::endl;

		if (conflicts > 0)
			log << "WARNING: " << conflicts << " slots were filled twice with different values."
			<< " The file holds the value that arrived first." << std::endl;

		return conflicts == 0;
	}

	bool compareSyzygyWdl(const std::string& pieceString, const std::string& ourDir,
		const std::string& refDir, const std::string& qwdlFile, std::ostream& log) {

		const std::string code = toFormatCode(pieceString);
		const PieceList pieceList(pieceString);

		// Optional third opinion: what the generator itself holds for the position.
		// It says which side of the bridge a difference sits on.
		BitbaseRePairFile source;
		const bool haveSource = !qwdlFile.empty() && source.open(qwdlFile);

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
		uint64_t generatorTooLow = 0;
		uint64_t generatorNoValue = 0;
		uint64_t generatorTooHigh = 0;
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

			// The generator's own value against the reference, for every position and not
			// only where our file differs: Qapla stores the true value, so it has to equal
			// the resolved answer of the reference. This is the check of the data itself,
			// with the writer out of the way.
			if (haveSource) {
				setUpPosition(position, pieceList, cases[i]);
				const uint64_t index = BoardAccess::getIndex<0>(position);

				// An index whose reverse hands back another member of the same class is
				// marked illegal by the generator, and the compressor fills such an entry
				// with a neighbour's value. There is no value to compare there.
				const ReverseIndex reverseIndex(index, pieceList);
				MoveGenerator canonical;
				bool hasValue = reverseIndex.isLegal();
				if (hasValue) {
					canonical.clear();
					canonical.unsafeSetPiece(reverseIndex.getSquare(0), WHITE_KING);
					canonical.unsafeSetPiece(reverseIndex.getSquare(1), BLACK_KING);
					for (uint32_t p = 2; p < pieceList.getNumberOfPieces(); ++p)
						canonical.unsafeSetPiece(reverseIndex.getSquare(p), pieceList.getPiece(p));
					canonical.computeAttackMasksForBothColors();
					canonical.setWhiteToMove(reverseIndex.isWhiteToMove());
					hasValue = canonical.isLegal() && BoardAccess::getIndex<0>(canonical) == index;
				}

				if (!hasValue) { ++generatorNoValue; }
				else {
				const BitbaseResult stored = source.probe(index);
				const int fromWhite = stored == BitbaseResult::Win ? 1
					: stored == BitbaseResult::Loss ? -1 : 0;
				const int fromMover = cases[i].whiteToMove ? fromWhite : -fromWhite;
				// Below the true value is what the format allows: the reader takes the
				// better of the entry and the captures, so an entry may sit low wherever a
				// capture reaches the value. Above it is an error under any reading.
				if (fromMover < sign(reference)) ++generatorTooLow;
				if (fromMover > sign(reference)) ++generatorTooHigh;
				}
			}

			if (sign(ours) != sign(reference)) {
				if (++differences <= 10) {
					log << "  " << toFen(pieceList, cases[i]) << " : ours " << wdlName(ours)
						<< ", reference " << wdlName(reference);

					if (haveSource) {
						setUpPosition(position, pieceList, cases[i]);
						const uint64_t index = BoardAccess::getIndex<0>(position);
						log << ", generator " << to_string(source.probe(index))
							<< " (white view, index " << index << ")";
					}
					log << std::endl;
				}
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

		if (haveSource)
			log << "the generator itself: " << generatorTooLow << " below the true value, "
				<< generatorTooHigh << " above it, " << generatorNoValue
				<< " with no value of their own" << std::endl;

		return differences == 0;
	}


	/**
	 * Measures how long a probe takes against two sets of files.
	 *
	 * The same positions in the same order, drawn from a fixed seed so that a rerun
	 * measures the same work, and the entry alone - no capture resolution - because
	 * that is what the file decides. The reference is measured in the same run rather
	 * than written down: only the ratio is a property of the files.
	 */
	bool measureSyzygySpeed(const std::string& pieceString, const std::string& ourDir,
		const std::string& refDir, uint64_t amount, std::ostream& log) {

		const std::string code = toFormatCode(pieceString);
		const PieceList pieceList(pieceString);

		// splitmix64, so that the positions do not depend on the standard library
		uint64_t state = 0x9E3779B97F4A7C15ULL;
		const auto random = [&state]() {
			uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
			z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
			z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
			return z ^ (z >> 31);
		};

		std::vector<TbPosition> probes;
		probes.reserve(size_t(amount));

		MoveGenerator position;
		PositionCase item;
		uint64_t tries = 0;

		while (probes.size() < amount) {
			++tries;
			uint64_t used = 0;
			bool ok = true;

			for (uint32_t piece = 0; piece < pieceList.getNumberOfPieces(); ++piece) {
				const uint8_t square = uint8_t(random() & 63);
				if (used & (1ULL << square)) { ok = false; break; }
				if (isPawn(pieceList.getPiece(piece)) && (square < 8 || square >= 56)) {
					ok = false;
					break;
				}
				used |= 1ULL << square;
				item.square[piece] = square;
			}
			if (!ok) continue;

			item.whiteToMove = (random() & 1) != 0;
			setUpPosition(position, pieceList, item);
			if (!position.isLegal()) continue;

			TbPosition tbPosition{};
			if (!buildTbPosition(position, tbPosition)) continue;
			probes.push_back(tbPosition);
		}

		log << code << ": " << probes.size() << " random legal positions from " << tries
			<< " draws" << std::endl;

		const std::string paths[2] = { ourDir, refDir };
		const char* names[2] = { "ours     ", "reference" };
		double nanoseconds[2] = { 0, 0 };

		for (int pass = 0; pass < 2; ++pass) {

			if (setPath(paths[pass]).wdlFiles == 0) {
				log << "no table found in " << paths[pass] << std::endl;
				release();
				return false;
			}

			int64_t answers = 0;
			uint64_t missing = 0;
			double best = 0;

			// Three runs, the fastest counts: the first one pays for the page faults of
			// the freshly mapped file, and a stray interrupt must not decide the outcome.
			for (int run = 0; run < 3; ++run) {

				const auto start = std::chrono::steady_clock::now();

				for (const TbPosition& probe : probes) {
					const WdlEntry entry = probeWdlEntry(probe);
					answers += int(entry.value);
					missing += entry.status != Status::Ok;
				}

				const double elapsed = std::chrono::duration<double, std::nano>(
					std::chrono::steady_clock::now() - start).count() / double(probes.size());
				if (run == 0 || elapsed < best) best = elapsed;
			}

			nanoseconds[pass] = best;
			// The sum is not a cross check: a stored entry is a lower bound, so ours and
			// de Man's may differ wherever a capture already reaches the value. What has
			// to match is the resolved value, which is what bitsyzygycheck compares.
			log << "  " << names[pass] << "  " << std::fixed << std::setprecision(1) << best
				<< " ns per probe, " << 1000.0 / best << " million per second"
				<< (missing ? "  (WITH MISSING TABLES)" : "")
				<< "  [entry sum " << answers << "]" << std::endl;

			release();
		}

		const double ratio = nanoseconds[0] / nanoseconds[1];
		log << "  ours takes " << std::setprecision(3) << ratio
			<< " times the reference" << std::endl;

		// A probe walks block lengths and decodes symbols; the two files differ in both,
		// so a few per cent either way says nothing. Anything beyond that does.
		constexpr double TOLERANCE = 1.05;
		if (ratio > TOLERANCE) {
			log << "  TOO SLOW: more than " << TOLERANCE << " times the reference" << std::endl;
			return false;
		}
		return true;
	}


	/**
	 * Prints what one position gets out of a set of files: the stored entry as it
	 * stands, and the value after the captures are resolved.
	 *
	 * The two differ wherever a capture already reaches the value - that is what the
	 * format allows and what the writer uses. Anything that reads the entry without
	 * resolving gets the first number, and it is a lower bound, not an answer.
	 */
	bool probeSyzygyPosition(const std::string& board, bool whiteToMove,
		const std::string& directory, const std::string& qwdlFile, std::ostream& log) {

		MoveGenerator position;
		position.clear();

		int square = 56;
		for (const char c : board) {
			if (c == '/') { square -= 16; continue; }
			if (c >= '1' && c <= '8') { square += c - '0'; continue; }
			const Piece piece = charToPiece(c);
			if (piece == NO_PIECE) { log << "cannot read " << board << std::endl; return false; }
			position.unsafeSetPiece(Square(square), piece);
			++square;
		}
		position.setWhiteToMove(whiteToMove);
		position.computeAttackMasksForBothColors();

		if (setPath(directory).wdlFiles == 0) {
			log << "no table found in " << directory << std::endl;
			release();
			return false;
		}

		log << board << (whiteToMove ? " w" : " b") << "  in " << directory << ":" << std::endl;
		log << "  legal: " << (position.isLegal() ? "yes" : "no") << std::endl;

		// The index of the position, and what the reverse index makes of that index again.
		// The writer walks the index space, so a class whose representative comes back
		// illegal is a class it never sees.
		{
			const PieceList pieceList(position);
			const uint64_t index = BoardAccess::getIndex<0>(position);
			const ReverseIndex reverseIndex(index, pieceList);
			MoveGenerator back;
			back.clear();
			back.unsafeSetPiece(reverseIndex.getSquare(0), WHITE_KING);
			back.unsafeSetPiece(reverseIndex.getSquare(1), BLACK_KING);
			for (uint32_t i = 2; i < pieceList.getNumberOfPieces(); ++i)
				back.unsafeSetPiece(reverseIndex.getSquare(i), pieceList.getPiece(i));
			back.computeAttackMasksForBothColors();
			back.setWhiteToMove(reverseIndex.isWhiteToMove());
			log << "  qapla index:    " << index
				<< ", reverse gives " << back.getFen(0)
				<< (reverseIndex.isLegal() ? "" : " [reverse index says illegal]")
				<< (back.isLegal() ? "" : " [ILLEGAL]")
				<< (BoardAccess::getIndex<0>(back) == index ? "" : " [does not round-trip]")
				<< std::endl;
		}

		TbPosition tbPosition{};
		if (buildTbPosition(position, tbPosition)) {
			const WdlEntry entry = probeWdlEntry(tbPosition);
			log << "  stored entry:   "
				<< (entry.status == Status::Ok ? wdlName(entry.value) : "no table") << std::endl;
		}

		const std::optional<Wdl> resolved = resolveWdl(position);
		log << "  after captures: " << (resolved ? wdlName(*resolved) : "unknown") << std::endl;

		// What the generator itself holds, unlowered - the file cannot say, because the
		// writer is free to store below the true value wherever a capture reaches it.
		// Asked the way the generator asks it: registered by material, and read through
		// getValueFromSingleBitbase, which mirrors when the position has the material the
		// other way round. Reading the file directly would answer the wrong table.
		BitbaseRePairFile source;
		if (!qwdlFile.empty() && source.open(qwdlFile)) {
			const std::filesystem::path path(qwdlFile);
			const std::string pieces = path.stem().string();

			// Every table of the directory, the way the generator has them registered
			// while it computes: a capture answered by only one of them would look
			// unanswered here and send the reader on a false trail.
			for (const auto& entry : std::filesystem::directory_iterator(path.parent_path()))
				if (entry.path().extension() == ".qwdl")
					BitbaseReader::registerQwdlFile(entry.path().stem().string(),
						entry.path().string());
			log << "  generator:      " << to_string(BitbaseReader::getValueFromSingleBitbase(position))
				<< " (white view, from " << pieces << ", index "
				<< BoardAccess::getIndex<0>(position) << ")" << std::endl;
		}


		// What the propagation sees for this position: every quiet move with the value of
		// its child, and for a double step the en passant captures that answer it.
		if (!qwdlFile.empty()) {
			MoveList moveList;
			position.genMovesOfMovingColor(moveList);
			const PieceList pieceList(position);
			const bool whiteToMove = position.isWhiteToMove();

			for (uint32_t moveNo = 0; moveNo < moveList.getTotalMoveAmount(); ++moveNo) {
				const Move move = moveList[moveNo];
				if (move.isCaptureOrPromote()) {
					log << "    " << move.getLAN() << "  capture" << std::endl;
					continue;
				}

				const uint64_t childIndex = BoardAccess::getIndex(!whiteToMove, pieceList, move);
				log << "    " << move.getLAN() << "  child " << to_string(source.probe(childIndex))
					<< " (index " << childIndex << ")";

				if (isPawn(move.getMovingPiece())
					&& abs(int(move.getDestination()) - int(move.getDeparture())) == 16) {

					const PositionSnapshot snapshot = position.getSnapshot();
					position.doMove(move);
					MoveList childMoves;
					position.genMovesOfMovingColor(childMoves);
					uint32_t epCount = 0;
					for (uint32_t i = 0; i < childMoves.getTotalMoveAmount(); ++i) {
						if (!childMoves[i].isEPMove()) continue;
						++epCount;
						const PositionSnapshot childSnapshot = position.getSnapshot();
						position.doMove(childMoves[i]);
						log << ", en passant " << childMoves[i].getLAN() << " -> "
							<< to_string(BitbaseReader::getValueFromSingleBitbase(position));
						position.undoMove(childMoves[i], childSnapshot);
					}
					if (epCount == 0) log << ", no en passant answer";
					position.undoMove(move, snapshot);
					position.computeAttackMasksForBothColors();
				}
				log << std::endl;
			}
		}
		release();
		return true;
	}

}
