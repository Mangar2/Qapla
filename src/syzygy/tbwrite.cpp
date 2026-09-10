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
 * See tbwrite.h.
 */

#include "tbwrite.h"
#include "tbindex.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <numeric>
#include <stdexcept>

namespace QaplaSyzygy {

	namespace {

		using namespace internal;

		constexpr uint8_t WdlMagic[4] = { 0x71, 0xE8, 0x23, 0x5D };

		/** 64 byte blocks and one sparse index entry per 128 positions, as de Man uses. */
		constexpr uint8_t LOG2_BLOCK_BYTES = 6;
		constexpr uint8_t LOG2_SPAN = 7;

		/** A block never holds more terminals than blockLength, stored as count - 1, can say. */
		constexpr uint32_t MAX_TERMINALS_PER_BLOCK = 65536;

		// ------------------------------------------------------------------
		// Huffman over the stored values
		// ------------------------------------------------------------------

		struct HuffCode {
			uint32_t code = 0;
			uint8_t  length = 0;
		};

		/**
		 * Code lengths by frequency. The alphabet is the stored value set, so at most
		 * five symbols - repeatedly merging the two lightest nodes is all it takes.
		 */
		std::vector<uint8_t> huffmanLengths(const std::vector<uint64_t>& frequency) {

			struct Node {
				uint64_t weight;
				int      left = -1;
				int      right = -1;
			};

			std::vector<Node> nodes;
			std::vector<int>  live;
			std::vector<int>  nodeOfValue(frequency.size(), -1);

			for (size_t value = 0; value < frequency.size(); ++value)
				if (frequency[value] > 0) {
					nodeOfValue[value] = int(nodes.size());
					live.push_back(int(nodes.size()));
					nodes.push_back({ frequency[value] });
				}

			while (live.size() > 1) {
				// The two lightest nodes become the children of a new one
				std::partial_sort(live.begin(), live.begin() + 2, live.end(),
					[&](int a, int b) { return nodes[a].weight < nodes[b].weight; });

				const int a = live[0];
				const int b = live[1];
				const int merged = int(nodes.size());
				nodes.push_back({ nodes[a].weight + nodes[b].weight, a, b });

				live.erase(live.begin(), live.begin() + 2);
				live.push_back(merged);
			}

			// Depth of every leaf, walked iteratively so a degenerate tree cannot
			// exhaust the stack
			std::vector<uint8_t> depth(nodes.size(), 0);
			std::vector<int> stack{ live.front() };

			while (!stack.empty()) {
				const int n = stack.back();
				stack.pop_back();
				if (nodes[n].left < 0) continue;
				depth[nodes[n].left] = depth[nodes[n].right] = uint8_t(depth[n] + 1);
				stack.push_back(nodes[n].left);
				stack.push_back(nodes[n].right);
			}

			std::vector<uint8_t> lengths(frequency.size(), 0);
			for (size_t value = 0; value < frequency.size(); ++value)
				if (nodeOfValue[value] >= 0)
					lengths[value] = depth[nodeOfValue[value]];

			return lengths;
		}

		/** One (side, file) table in the shape the file stores it. */
		struct EncodedTable {
			bool     singleValue = false;
			uint8_t  value = 0;                 // only when singleValue

			uint8_t  maxSymLen = 0;
			uint8_t  minSymLen = 0;
			std::vector<uint16_t> lowestSym;    // one entry per length, longest last
			std::vector<uint8_t>  btree;        // three bytes per symbol
			uint16_t symbolCount = 0;

			uint32_t blocksNum = 0;
			uint8_t  padding = 0;
			std::vector<uint8_t>  sparseIndex;  // six bytes per entry
			std::vector<uint16_t> blockLength;  // terminals per block, as count - 1
			std::vector<uint8_t>  data;
		};

		/** Fills the slots no position reached with their nearest neighbour. */
		void fillUnreached(std::vector<uint8_t>& values) {
			uint8_t last = StoredDraw;
			for (const uint8_t v : values)
				if (v != TB_UNREACHED) { last = v; break; }

			for (uint8_t& v : values) {
				if (v == TB_UNREACHED) v = last;
				else last = v;
			}
		}

		/**
		 * Compresses one table.
		 *
		 * The canonical code the format expects runs the other way round than the
		 * usual one: the longest codes carry the lowest symbol numbers, and the first
		 * code of a length follows from the next longer one,
		 *     base(len) = (base(len + 1) + count(len + 1)) / 2
		 * which is exactly what the reader undoes when it rebuilds base64[].
		 */
		EncodedTable encodeTable(std::vector<uint8_t> values) {

			EncodedTable table;
			fillUnreached(values);

			std::vector<uint64_t> frequency(StoredWin + 1, 0);
			for (const uint8_t v : values) {
				if (v > StoredWin) throw std::runtime_error("tbwrite: value out of range");
				frequency[v]++;
			}

			const size_t distinct = std::count_if(frequency.begin(), frequency.end(),
				[](uint64_t f) { return f > 0; });

			if (distinct <= 1) {
				table.singleValue = true;
				table.value = values.empty() ? uint8_t(StoredDraw) : values.front();
				return table;
			}

			const std::vector<uint8_t> lengths = huffmanLengths(frequency);

			table.minSymLen = 0xFF;
			for (const uint8_t len : lengths)
				if (len > 0) {
					table.minSymLen = std::min(table.minSymLen, len);
					table.maxSymLen = std::max(table.maxSymLen, len);
				}

			const int classCount = table.maxSymLen - table.minSymLen + 1;
			std::vector<uint32_t> countOfLength(classCount, 0);
			for (const uint8_t len : lengths)
				if (len > 0) countOfLength[len - table.minSymLen]++;

			// base and the lowest symbol number of every length, from the longest down
			std::vector<uint32_t> base(classCount, 0);
			table.lowestSym.assign(classCount, 0);
			for (int i = classCount - 2; i >= 0; --i) {
				base[i] = (base[i + 1] + countOfLength[i + 1]) / 2;
				table.lowestSym[i] = uint16_t(table.lowestSym[i + 1] + countOfLength[i + 1]);
			}

			if (base[0] + countOfLength[0] != (1u << table.minSymLen))
				throw std::runtime_error("tbwrite: the huffman code is not complete");

			// Within a length the symbols follow the value order, codes and numbers
			// ascending together
			std::vector<HuffCode> codeOfValue(lengths.size());
			std::vector<uint32_t> nextInClass(classCount, 0);
			table.symbolCount = uint16_t(distinct);
			table.btree.assign(size_t(table.symbolCount) * 3, 0);

			for (size_t value = 0; value < lengths.size(); ++value) {
				if (lengths[value] == 0) continue;

				const int      li = lengths[value] - table.minSymLen;
				const uint32_t n = nextInClass[li]++;
				const uint32_t symbol = table.lowestSym[li] + n;

				codeOfValue[value] = HuffCode{ base[li] + n, lengths[value] };

				// A leaf: left holds the value, right is the marker 0xFFF
				uint8_t* const lr = &table.btree[size_t(symbol) * 3];
				lr[0] = uint8_t(value & 0xFF);
				lr[1] = uint8_t(((value >> 8) & 0xF) | 0xF0);
				lr[2] = 0xFF;
			}

			// ---- bit packing, most significant bit first, blocks never crossed ----

			const size_t blockBytes = size_t(1) << LOG2_BLOCK_BYTES;
			const size_t blockBits = blockBytes * 8;

			std::vector<uint8_t> block(blockBytes, 0);
			std::vector<uint64_t> terminalsBeforeBlock{ 0 };
			size_t   bitPos = 0;
			uint32_t inBlock = 0;
			uint64_t written = 0;

			const auto closeBlock = [&]() {
				table.data.insert(table.data.end(), block.begin(), block.end());
				table.blockLength.push_back(uint16_t(inBlock - 1));
				written += inBlock;
				terminalsBeforeBlock.push_back(written);
				std::fill(block.begin(), block.end(), uint8_t(0));
				bitPos = 0;
				inBlock = 0;
			};

			for (const uint8_t v : values) {
				const HuffCode code = codeOfValue[v];

				if (bitPos + code.length > blockBits || inBlock == MAX_TERMINALS_PER_BLOCK)
					closeBlock();

				for (int bit = code.length - 1; bit >= 0; --bit) {
					if ((code.code >> bit) & 1)
						block[bitPos >> 3] |= uint8_t(0x80 >> (bitPos & 7));
					++bitPos;
				}
				++inBlock;
			}

			if (inBlock > 0) closeBlock();
			table.blocksNum = uint32_t(table.blockLength.size());

			// ---- sparse index ----

			const uint64_t span = uint64_t(1) << LOG2_SPAN;
			const uint64_t total = values.size();
			const uint64_t entries = (total + span - 1) / span;
			uint64_t phantomBlocks = 0;

			table.sparseIndex.reserve(size_t(entries) * 6);

			for (uint64_t k = 0; k < entries; ++k) {
				const uint64_t position = k * span + span / 2;
				uint32_t blockIndex;
				uint32_t offset;

				if (position < total) {
					const auto it = std::upper_bound(terminalsBeforeBlock.begin(),
						terminalsBeforeBlock.end(), position);
					blockIndex = uint32_t(it - terminalsBeforeBlock.begin() - 1);
					offset = uint32_t(position - terminalsBeforeBlock[blockIndex]);
				}
				else {
					// Past the last value. The entry still has to point somewhere, so it
					// points into phantom blocks of one terminal each, which the reader
					// walks back through to reach a real one.
					blockIndex = uint32_t(table.blocksNum + (position - total));
					offset = 0;
					phantomBlocks = std::max(phantomBlocks, position - total + 1);
				}

				if (offset > 0xFFFF) throw std::runtime_error("tbwrite: sparse offset too large");

				for (int i = 0; i < 4; ++i) table.sparseIndex.push_back(uint8_t(blockIndex >> (8 * i)));
				for (int i = 0; i < 2; ++i) table.sparseIndex.push_back(uint8_t(offset >> (8 * i)));
			}

			table.padding = uint8_t(std::min<uint64_t>(phantomBlocks, 255));
			if (phantomBlocks > 255) throw std::runtime_error("tbwrite: too many phantom blocks");
			table.blockLength.resize(table.blocksNum + table.padding, 0);

			return table;
		}

		// ------------------------------------------------------------------
		// Byte output
		// ------------------------------------------------------------------

		void put8(std::vector<uint8_t>& out, uint8_t v) { out.push_back(v); }

		void put16(std::vector<uint8_t>& out, uint16_t v) {
			out.push_back(uint8_t(v));
			out.push_back(uint8_t(v >> 8));
		}

		void put32(std::vector<uint8_t>& out, uint32_t v) {
			for (int i = 0; i < 4; ++i) out.push_back(uint8_t(v >> (8 * i)));
		}

		void alignTo(std::vector<uint8_t>& out, size_t alignment) {
			while (out.size() % alignment) out.push_back(0);
		}

	}   // anonymous namespace

	// ----------------------------------------------------------------------
	// WdlWriter
	// ----------------------------------------------------------------------

	/** Material and the piece order of every table, built once. */
	struct WdlWriter::Layout {
		internal::IndexMaterial material;
		internal::IndexGroups   groups[2][4];
		uint64_t                size[2][4] = {};
	};

	WdlWriter::WdlWriter(const std::string& code)
		: _code(code), _layout(std::make_unique<Layout>()) {

		internal::ensureMaps();

		_layout->material = internal::materialFromCode(code);
		const internal::IndexMaterial& m = _layout->material;

		_sideCount = (m.key != m.key2) ? 2 : 1;
		_fileCount = m.hasPawns ? 4 : 1;

		if (m.pieceCount < 3)
			_unsupported = "a table needs at least three pieces";
		else if (m.hasPawns)
			_unsupported = "pawn tables are not written yet";

		if (!_unsupported.empty()) return;

		// The piece order. Both kings lead: without a unique piece they are the whole
		// leading group and the king map expects them there, with one they are the
		// first two of three. The third slot must hold a piece that occurs exactly
		// once, otherwise two identical pieces would be encoded as distinguishable.
		const internal::MaterialCounts counts = internal::countsFromCode(code);

		std::vector<uint8_t> order;
		order.push_back(uint8_t(internal::makePiece(0, internal::KING)));
		order.push_back(uint8_t(internal::makePiece(1, internal::KING)));

		int uniqueColour = -1;
		int uniqueType = -1;
		if (m.hasUniquePieces) {
			for (int colour = 0; colour < 2 && uniqueType < 0; ++colour)
				for (int type = internal::PAWN; type < internal::KING; ++type)
					if (counts.count[colour][type] == 1) {
						uniqueColour = colour;
						uniqueType = type;
						break;
					}
			order.push_back(uint8_t(internal::makePiece(uniqueColour, uniqueType)));
		}

		for (int colour = 0; colour < 2; ++colour)
			for (int type = internal::KING - 1; type >= internal::PAWN; --type) {
				if (colour == uniqueColour && type == uniqueType) continue;
				for (int i = 0; i < counts.count[colour][type]; ++i)
					order.push_back(uint8_t(internal::makePiece(colour, type)));
			}

		if (int(order.size()) != m.pieceCount)
			throw std::runtime_error("tbwrite: piece order does not match the material");

		for (int side = 0; side < _sideCount; ++side)
			for (int file = 0; file < _fileCount; ++file) {
				internal::IndexGroups& groups = _layout->groups[side][file];
				std::copy(order.begin(), order.end(), groups.pieces);

				const int chain[2] = { 0, 0xF };
				internal::setGroups(m, groups, chain, file);

				const int last = int(std::find(groups.groupLen, groups.groupLen + 7, 0)
					- groups.groupLen);
				_layout->size[side][file] = groups.groupIdx[last];
			}
	}

	WdlWriter::~WdlWriter() = default;

	bool WdlWriter::isSupported(std::string& reason) const {
		reason = _unsupported;
		return _unsupported.empty();
	}

	uint64_t WdlWriter::tableSize(int side, int file) const {
		return _layout->size[side][file];
	}

	WdlSlot WdlWriter::slotOf(const TbPosition& pos) const {

		const internal::ProbeBoard board = internal::toProbeBoard(pos);
		internal::IndexContext ctx =
			internal::beginIndex(_layout->material, _layout->groups[0][0], board);

		WdlSlot slot;
		slot.side = ctx.stm % _sideCount;
		slot.file = ctx.tbFile;
		slot.index = internal::finishIndex(_layout->material,
			_layout->groups[slot.side][slot.file], board, ctx);
		return slot;
	}

	void WdlWriter::write(const std::string& filePath,
		const std::vector<uint8_t> values[2][4]) const {

		std::string reason;
		if (!isSupported(reason)) throw std::runtime_error("tbwrite: " + reason);

		const internal::IndexMaterial& m = _layout->material;

		EncodedTable encoded[2][4];
		for (int file = 0; file < _fileCount; ++file)
			for (int side = 0; side < _sideCount; ++side) {
				if (values[side][file].size() != tableSize(side, file))
					throw std::runtime_error("tbwrite: table size does not match the layout");
				encoded[side][file] = encodeTable(values[side][file]);
			}

		std::vector<uint8_t> out;

		out.insert(out.end(), std::begin(WdlMagic), std::end(WdlMagic));

		// The upper nibble holds the piece count. The prober here ignores it, others
		// read it, and writing it costs nothing.
		put8(out, uint8_t((m.pieceCount << 4) | (m.hasPawns ? 2 : 0)
			| ((m.key != m.key2) ? 1 : 0)));

		for (int file = 0; file < _fileCount; ++file) {
			// Both sides put their leading group first in the multiplication chain
			put8(out, 0x00);

			for (int k = 0; k < m.pieceCount; ++k) {
				const uint8_t side0 = _layout->groups[0][file].pieces[k];
				const uint8_t side1 = _layout->groups[_sideCount - 1][file].pieces[k];
				put8(out, uint8_t((side0 & 0xF) | (side1 << 4)));
			}
		}

		alignTo(out, 2);

		for (int file = 0; file < _fileCount; ++file)
			for (int side = 0; side < _sideCount; ++side) {
				const EncodedTable& t = encoded[side][file];

				if (t.singleValue) {
					put8(out, uint8_t(internal::SingleValue));
					put8(out, t.value);
					continue;
				}

				put8(out, 0);
				put8(out, LOG2_BLOCK_BYTES);
				put8(out, LOG2_SPAN);
				put8(out, t.padding);
				put32(out, t.blocksNum);
				put8(out, t.maxSymLen);
				put8(out, t.minSymLen);
				for (const uint16_t sym : t.lowestSym) put16(out, sym);
				put16(out, t.symbolCount);
				out.insert(out.end(), t.btree.begin(), t.btree.end());
				alignTo(out, 2);
			}

		for (int file = 0; file < _fileCount; ++file)
			for (int side = 0; side < _sideCount; ++side)
				out.insert(out.end(), encoded[side][file].sparseIndex.begin(),
					encoded[side][file].sparseIndex.end());

		for (int file = 0; file < _fileCount; ++file)
			for (int side = 0; side < _sideCount; ++side)
				for (const uint16_t length : encoded[side][file].blockLength)
					put16(out, length);

		for (int file = 0; file < _fileCount; ++file)
			for (int side = 0; side < _sideCount; ++side) {
				alignTo(out, 64);
				out.insert(out.end(), encoded[side][file].data.begin(),
					encoded[side][file].data.end());
			}

		// The reader refills its bit buffer a word at a time and may reach a few bytes
		// past the last block. de Man's files end in a 16 byte checksum, which covers
		// the same ground; here it is plain padding.
		out.insert(out.end(), 16, 0);

		std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
		if (!file) throw std::runtime_error("tbwrite: cannot open " + filePath);
		file.write(reinterpret_cast<const char*>(out.data()), std::streamsize(out.size()));
		if (!file) throw std::runtime_error("tbwrite: cannot write " + filePath);
	}

}
