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
#include <array>
#include <cstring>
#include <fstream>
#include <numeric>
#include <queue>
#include <unordered_map>
#include <stdexcept>

namespace QaplaSyzygy {

	namespace {

		using namespace internal;

		constexpr uint8_t WdlMagic[4] = { 0x71, 0xE8, 0x23, 0x5D };

		/**
		 * Block sizes to try. Small blocks waste less on the padding behind the last
		 * symbol of a block, large ones need fewer entries in the length array and in
		 * the index. Which one wins depends on the table, and de Man's own files use
		 * anything from 32 to 64 bytes - so the table is packed with each of them and
		 * the smallest result is kept.
		 */
		constexpr uint8_t LOG2_BLOCK_CANDIDATES[] = { 4, 5, 6, 7 };

		/**
		 * How far one sparse index entry may reach, in blocks. The index only shortens
		 * the walk over the block lengths, so the span is made as wide as this allows -
		 * a wide span costs a few steps of that walk and saves six bytes per entry.
		 * de Man spends four entries on a table of a megabyte.
		 */
		constexpr uint64_t SPARSE_INDEX_BLOCK_REACH = 32;

		// ------------------------------------------------------------------
		// Recursive pairing, then Huffman over what it leaves
		// ------------------------------------------------------------------

		/** Symbols are twelve bits wide in the tree, so this is the whole vocabulary. */
		constexpr uint32_t MAX_SYMBOLS = 4096;

		/**
		 * A symbol may not expand to more terminals than this. The reader keeps the
		 * expansion count in a byte and stores it as count - 1, so 256 is the ceiling
		 * and a rule that would cross it is not built.
		 */
		constexpr uint32_t MAX_TERMINALS_PER_SYMBOL = 256;

		/** The reader shifts by 64 - length, so a code has to stay well inside that. */
		constexpr int MAX_CODE_LENGTH = 32;

		/**
		 * Below this a rule costs more than it saves: three bytes of tree plus its
		 * place in the code table, against a few bits per occurrence. Measured on the
		 * three and four piece tables, four is the point where lowering it further
		 * stops paying.
		 */
		constexpr uint32_t MIN_PAIR_COUNT = 4;

		/**
		 * Terminals per block. blockLength stores the count minus one in sixteen bits,
		 * and a sparse index entry has to name an offset inside a block in sixteen bits
		 * as well - with half a span added on top for the entries that sit past the end
		 * of the table. Half of the range for the block and a quarter for the span
		 * leaves both inside sixteen bits with room to spare.
		 */
		constexpr uint32_t MAX_TERMINALS_PER_BLOCK = 32768;

		/**
		 * Symbols per block. A probe decodes its way from the start of a block to the
		 * entry it wants, so half a block is what an average probe costs - and a block
		 * that is large in bytes and cheap in bits per symbol would hold a thousand of
		 * them. de Man's tables sit below a hundred; this bound keeps the choice of the
		 * block size from buying a few bytes with a slower probe.
		 */
		constexpr uint32_t MAX_SYMBOLS_PER_BLOCK = 128;

		/** Half of the widest span fits in what is left of the offset above. */
		constexpr uint8_t MAX_LOG2_SPAN = 15;

		/**
		 * The sequence of symbols and the rules that produce them.
		 *
		 * A symbol with right == 0xFFF is a leaf and left is the stored value; every
		 * other symbol stands for its two children, one after the other. That is the
		 * shape the reader walks in decompressPairs(), so the grammar is written out
		 * as it stands here.
		 */
		struct Grammar {
			std::vector<uint16_t> sequence;    // the table, as symbols
			std::vector<uint16_t> left;
			std::vector<uint16_t> right;       // 0xFFF marks a leaf, left is then the value
			std::vector<uint32_t> terminals;   // values the symbol expands to

			/**
			 * The sequence as it looked at a few vocabulary sizes on the way. More rules
			 * always shorten the sequence and always lengthen the codes of everything
			 * else; which of the two wins is a property of the table, so the caller
			 * encodes each of these and keeps the smallest.
			 */
			std::vector<std::vector<uint16_t>> stage;
			std::vector<uint32_t>              stageVocabulary;
		};

		/** Vocabulary sizes a snapshot is taken at. */
		constexpr uint32_t STAGE_VOCABULARY[] = { 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 1024, 2048 };

		/**
		 * Builds the grammar by repeatedly replacing the most frequent adjacent pair
		 * with a new symbol - recursive pairing.
		 *
		 * Runs of one value are what this finds first: the pair (v, v) becomes a
		 * symbol, the pair of that symbol with itself the next one, and so on until
		 * the expansion limit stops the doubling. That is where nearly all of the
		 * compression of an endgame table comes from.
		 */
		Grammar buildGrammar(const std::vector<uint8_t>& values) {

			Grammar grammar;

			std::vector<int> symbolOfValue(256, -1);
			for (const uint8_t value : values)
				if (symbolOfValue[value] < 0) {
					symbolOfValue[value] = int(grammar.left.size());
					grammar.left.push_back(value);
					grammar.right.push_back(0xFFF);
					grammar.terminals.push_back(1);
				}

			grammar.sequence.reserve(values.size());
			for (const uint8_t value : values)
				grammar.sequence.push_back(uint16_t(symbolOfValue[value]));

			while (grammar.left.size() < MAX_SYMBOLS && grammar.sequence.size() > 1) {

				// Adjacent pairs, counted with overlap. A run of length n reports n - 1
				// occurrences of (v, v) and the replacement below reaches half of them,
				// which is what a doubling step is.
				std::unordered_map<uint32_t, uint32_t> occurrences;
				occurrences.reserve(grammar.sequence.size() / 2 + 16);

				for (size_t i = 0; i + 1 < grammar.sequence.size(); ++i)
					occurrences[(uint32_t(grammar.sequence[i]) << 16) | grammar.sequence[i + 1]]++;

				uint32_t bestKey = 0;
				uint32_t bestCount = 0;

				// Plain frequency. Weighting it by the terminals a rule would cover was
				// measured and is worse on every table tried.
				for (const auto& [key, count] : occurrences) {
					if (count <= bestCount) continue;

					const uint16_t a = uint16_t(key >> 16);
					const uint16_t b = uint16_t(key & 0xFFFF);
					if (grammar.terminals[a] + grammar.terminals[b] > MAX_TERMINALS_PER_SYMBOL) continue;

					bestKey = key;
					bestCount = count;
				}

				// A rule pays for itself only if it is common in what is left. A pair
				// that covers a small share of the sequence shortens it barely and
				// spreads the code lengths of everything else, which costs more than
				// it saves.
				if (bestCount < MIN_PAIR_COUNT) break;

				const uint16_t a = uint16_t(bestKey >> 16);
				const uint16_t b = uint16_t(bestKey & 0xFFFF);
				const uint16_t symbol = uint16_t(grammar.left.size());

				grammar.left.push_back(a);
				grammar.right.push_back(b);
				grammar.terminals.push_back(grammar.terminals[a] + grammar.terminals[b]);

				std::vector<uint16_t> replaced;
				replaced.reserve(grammar.sequence.size());

				for (size_t i = 0; i < grammar.sequence.size(); ) {
					if (i + 1 < grammar.sequence.size()
						&& grammar.sequence[i] == a && grammar.sequence[i + 1] == b) {
						replaced.push_back(symbol);
						i += 2;
					}
					else {
						replaced.push_back(grammar.sequence[i]);
						++i;
					}
				}

				grammar.sequence.swap(replaced);

				for (const uint32_t size : STAGE_VOCABULARY)
					if (grammar.left.size() == size) {
						grammar.stage.push_back(grammar.sequence);
						grammar.stageVocabulary.push_back(uint32_t(grammar.left.size()));
					}
			}

			grammar.stage.push_back(grammar.sequence);
			grammar.stageVocabulary.push_back(uint32_t(grammar.left.size()));
			return grammar;
		}

		struct HuffCode {
			uint32_t code = 0;
			uint8_t  length = 0;
		};

		/** Code lengths by frequency. Symbols of frequency zero get no code. */
		std::vector<uint8_t> huffmanLengths(const std::vector<uint64_t>& frequency) {

			struct Node {
				uint64_t weight = 0;
				int      left = -1;
				int      right = -1;
			};

			std::vector<Node> nodes;
			std::vector<int>  nodeOfSymbol(frequency.size(), -1);

			using Item = std::pair<uint64_t, int>;   // weight, node
			std::priority_queue<Item, std::vector<Item>, std::greater<Item>> queue;

			for (size_t symbol = 0; symbol < frequency.size(); ++symbol)
				if (frequency[symbol] > 0) {
					nodeOfSymbol[symbol] = int(nodes.size());
					queue.emplace(frequency[symbol], int(nodes.size()));
					nodes.push_back({ frequency[symbol] });
				}

			std::vector<uint8_t> lengths(frequency.size(), 0);
			if (nodes.empty()) return lengths;

			while (queue.size() > 1) {
				const Item a = queue.top(); queue.pop();
				const Item b = queue.top(); queue.pop();
				const int merged = int(nodes.size());
				nodes.push_back({ a.first + b.first, a.second, b.second });
				queue.emplace(a.first + b.first, merged);
			}

			// A single symbol still needs a bit to be written, so its depth is one
			if (nodes.size() == 1) {
				lengths[std::find(nodeOfSymbol.begin(), nodeOfSymbol.end(), 0)
					- nodeOfSymbol.begin()] = 1;
				return lengths;
			}

			std::vector<uint8_t> depth(nodes.size(), 0);
			std::vector<int> stack{ queue.top().second };

			while (!stack.empty()) {
				const int node = stack.back();
				stack.pop_back();
				if (nodes[node].left < 0) continue;
				depth[nodes[node].left] = depth[nodes[node].right] = uint8_t(depth[node] + 1);
				stack.push_back(nodes[node].left);
				stack.push_back(nodes[node].right);
			}

			for (size_t symbol = 0; symbol < frequency.size(); ++symbol)
				if (nodeOfSymbol[symbol] >= 0)
					lengths[symbol] = depth[nodeOfSymbol[symbol]];

			return lengths;
		}

		/**
		 * The same, with the longest code bounded. Flattening the frequencies and
		 * trying again converges towards a balanced tree, whose depth is the logarithm
		 * of the vocabulary and therefore well inside the limit.
		 */
		std::vector<uint8_t> limitedHuffmanLengths(std::vector<uint64_t> frequency, int limit) {

			for (;;) {
				const std::vector<uint8_t> lengths = huffmanLengths(frequency);

				int longest = 0;
				for (const uint8_t length : lengths) longest = std::max(longest, int(length));
				if (longest <= limit) return lengths;

				for (uint64_t& f : frequency) if (f > 1) f = (f + 1) / 2;
			}
		}

		/** One (side, file) table in the shape the file stores it. */
		struct EncodedTable {
			bool     singleValue = false;
			uint8_t  value = 0;                 // only when singleValue

			uint8_t  maxSymLen = 0;
			uint8_t  minSymLen = 0;
			std::vector<uint16_t> lowestSym;    // one entry per length, longest last
			std::vector<uint8_t>  btree;        // three bytes per symbol
			uint16_t symbolCount = 0;           // coded symbols and rule-only ones together

			uint32_t blocksNum = 0;
			uint8_t  padding = 0;
			uint8_t  log2Span = 0;
			uint8_t  log2BlockBytes = 0;
			std::vector<uint8_t>  sparseIndex;  // six bytes per entry
			std::vector<uint16_t> blockLength;  // terminals per block, as count - 1
			std::vector<uint8_t>  data;
		};

		/**
		 * Resolves everything the caller left open, always towards the value that is
		 * already running: a slot no position reached takes its predecessor, a slot
		 * that may be stored lower takes its predecessor as well whenever that is not
		 * above its ceiling. Both cases exist to make runs longer, and this is the
		 * cheapest rule that does it.
		 */
		void resolveOpenValues(std::vector<uint8_t>& values) {

			uint8_t last = StoredDraw;
			for (const uint8_t v : values)
				if (v != TB_UNREACHED) { last = uint8_t(v & ~TB_REDUCIBLE); break; }

			for (uint8_t& v : values) {
				if (v == TB_UNREACHED)
					v = last;
				else if (v & TB_REDUCIBLE) {
					const uint8_t ceiling = uint8_t(v & ~TB_REDUCIBLE);
					v = last <= ceiling ? last : ceiling;
				}
				last = v;
			}
		}

		/**
		 * Compresses one table with a given vocabulary: the sequence of a stage of the
		 * grammar, everything the sequence can reach, and nothing else.
		 *
		 * The canonical code the format expects runs the other way round than the usual
		 * one: the longest codes carry the lowest symbol numbers, and the first code of a
		 * length follows from the next longer one,
		 *     base(len) = (base(len + 1) + count(len + 1)) / 2
		 * which is exactly what the reader undoes when it rebuilds base64[] from the
		 * stored lowestSym[]. A symbol that appears only inside a rule gets no code at
		 * all; it is numbered behind the coded ones, where the reader reaches it as a
		 * child but never out of the bit stream. de Man's own files do the same.
		 *
		 * @returns the bytes the table costs in the file, so that the caller can compare
		 *          the stages against each other
		 */
		uint64_t encodeStage(const Grammar& grammar, const std::vector<uint16_t>& sequence,
			uint64_t total, EncodedTable& table) {

			// Everything the sequence reaches. A rule refers to symbols made before it, so
			// one pass downwards marks them all.
			std::vector<uint64_t> frequency(grammar.left.size(), 0);
			std::vector<bool>     used(grammar.left.size(), false);

			for (const uint16_t symbol : sequence) { frequency[symbol]++; used[symbol] = true; }

			for (size_t symbol = grammar.left.size(); symbol-- > 0; ) {
				if (!used[symbol] || grammar.right[symbol] == 0xFFF) continue;
				used[grammar.left[symbol]] = true;
				used[grammar.right[symbol]] = true;
			}

			const std::vector<uint8_t> lengths = limitedHuffmanLengths(frequency, MAX_CODE_LENGTH);

			table.minSymLen = 0xFF;
			for (const uint8_t length : lengths)
				if (length > 0) {
					table.minSymLen = std::min(table.minSymLen, length);
					table.maxSymLen = std::max(table.maxSymLen, length);
				}

			const int classCount = table.maxSymLen - table.minSymLen + 1;
			std::vector<uint32_t> countOfLength(classCount, 0);
			for (const uint8_t length : lengths)
				if (length > 0) countOfLength[length - table.minSymLen]++;

			// base and the lowest symbol number of every length, from the longest down
			std::vector<uint64_t> base(classCount, 0);
			table.lowestSym.assign(classCount, 0);
			for (int i = classCount - 2; i >= 0; --i) {
				base[i] = (base[i + 1] + countOfLength[i + 1]) / 2;
				table.lowestSym[i] = uint16_t(table.lowestSym[i + 1] + countOfLength[i + 1]);
			}

			if (base[0] + countOfLength[0] > (uint64_t(1) << table.minSymLen))
				throw std::runtime_error("tbwrite: the huffman code does not fit its length");

			// Numbers: the longest codes lowest, ascending with the code inside a length,
			// and the symbols that carry no code behind all of them
			std::vector<uint16_t> numberOf(grammar.left.size(), 0xFFFF);
			std::vector<HuffCode> codeOf(grammar.left.size());
			uint32_t next = 0;

			for (int i = classCount - 1; i >= 0; --i)
				for (size_t symbol = 0; symbol < lengths.size(); ++symbol) {
					if (lengths[symbol] != i + table.minSymLen) continue;
					codeOf[symbol] = HuffCode{ uint32_t(base[i] + (next - table.lowestSym[i])),
						lengths[symbol] };
					numberOf[symbol] = uint16_t(next++);
				}

			for (size_t symbol = 0; symbol < lengths.size(); ++symbol)
				if (lengths[symbol] == 0 && used[symbol]) numberOf[symbol] = uint16_t(next++);

			table.symbolCount = uint16_t(next);
			table.btree.assign(size_t(table.symbolCount) * 3, 0);

			for (size_t symbol = 0; symbol < grammar.left.size(); ++symbol) {
				if (!used[symbol]) continue;

				const uint16_t left = grammar.right[symbol] == 0xFFF
					? grammar.left[symbol] : numberOf[grammar.left[symbol]];
				const uint16_t right = grammar.right[symbol] == 0xFFF
					? uint16_t(0xFFF) : numberOf[grammar.right[symbol]];

				uint8_t* const lr = &table.btree[size_t(numberOf[symbol]) * 3];
				lr[0] = uint8_t(left & 0xFF);
				lr[1] = uint8_t(((left >> 8) & 0xF) | ((right & 0xF) << 4));
				lr[2] = uint8_t(right >> 4);
			}

			// ---- packing, tried with every block size ----

			// Most significant bit first, and a symbol never crosses a block: the reader
			// starts every block with a fresh bit buffer.
			const auto pack = [&](uint8_t log2BlockBytes, EncodedTable& out) {

				const size_t blockBytes = size_t(1) << log2BlockBytes;
				const size_t blockBits = blockBytes * 8;

				std::vector<uint8_t> block(blockBytes, 0);
				std::vector<uint64_t> terminalsBeforeBlock{ 0 };
				size_t   bitPos = 0;
				uint32_t inBlock = 0;
				uint32_t symbolsInBlock = 0;
				uint64_t written = 0;

				const auto closeBlock = [&]() {
					out.data.insert(out.data.end(), block.begin(), block.end());
					out.blockLength.push_back(uint16_t(inBlock - 1));
					written += inBlock;
					terminalsBeforeBlock.push_back(written);
					std::fill(block.begin(), block.end(), uint8_t(0));
					bitPos = 0;
					inBlock = 0;
				};

				for (const uint16_t symbol : sequence) {
					const HuffCode code = codeOf[symbol];
					const uint32_t terminals = grammar.terminals[symbol];

					if (bitPos + code.length > blockBits
						|| inBlock + terminals > MAX_TERMINALS_PER_BLOCK
						|| symbolsInBlock == MAX_SYMBOLS_PER_BLOCK)
						closeBlock();

					for (int bit = code.length - 1; bit >= 0; --bit) {
						if ((code.code >> bit) & 1)
							block[bitPos >> 3] |= uint8_t(0x80 >> (bitPos & 7));
						++bitPos;
					}
					inBlock += terminals;
					++symbolsInBlock;
				}

				if (inBlock > 0) closeBlock();
				out.blocksNum = uint32_t(out.blockLength.size());
				out.log2BlockBytes = log2BlockBytes;

				// One entry per span positions, made as wide as the sixteen bit offset of an
				// entry allows: a wide span only means that the reader walks a few more block
				// lengths before it decodes.
				const uint64_t dataBytes = uint64_t(out.blocksNum) * blockBytes;

				for (out.log2Span = MAX_LOG2_SPAN; ; --out.log2Span) {

					const uint64_t span = uint64_t(1) << out.log2Span;
					const uint64_t entries = (total + span - 1) / span;

					if (out.log2Span > 6 && entries * SPARSE_INDEX_BLOCK_REACH < out.blocksNum) continue;

					out.sparseIndex.clear();
					out.sparseIndex.reserve(size_t(entries) * 6);
					bool fits = true;

					for (uint64_t k = 0; k < entries; ++k) {

						// The entry describes the position half a span into its range. That
						// position may sit past the last value of the table, and is then counted
						// on past the end of the last block: the reader adds idx % span - span / 2
						// before it looks at any block length, so the correction lands back inside
						// the table for every index it is asked about.
						const uint64_t position = k * span + span / 2;

						const auto it = std::upper_bound(terminalsBeforeBlock.begin(),
							terminalsBeforeBlock.end(), std::min(position, total - 1));
						const uint32_t blockIndex = uint32_t(it - terminalsBeforeBlock.begin() - 1);
						const uint64_t offset = position - terminalsBeforeBlock[blockIndex];

						if (offset > 0xFFFF) { fits = false; break; }

						for (int i = 0; i < 4; ++i)
							out.sparseIndex.push_back(uint8_t(blockIndex >> (8 * i)));
						for (int i = 0; i < 2; ++i)
							out.sparseIndex.push_back(uint8_t(offset >> (8 * i)));
					}

					if (fits) break;
					if (out.log2Span == 6) throw std::runtime_error("tbwrite: no usable span");
				}

				return dataBytes + out.blockLength.size() * 2 + out.sparseIndex.size();
			};

			uint64_t best = UINT64_MAX;
			for (const uint8_t log2BlockBytes : LOG2_BLOCK_CANDIDATES) {
				EncodedTable candidate;
				const uint64_t bytes = pack(log2BlockBytes, candidate);
				if (bytes >= best) continue;

				best = bytes;
				table.data.swap(candidate.data);
				table.blockLength.swap(candidate.blockLength);
				table.sparseIndex.swap(candidate.sparseIndex);
				table.blocksNum = candidate.blocksNum;
				table.log2Span = candidate.log2Span;
				table.log2BlockBytes = candidate.log2BlockBytes;
			}

			// Everything this table costs: the header of the size block, the code table, the
			// tree, and what the packing reported
			return 12 + 2 * uint64_t(table.lowestSym.size()) + 3 * uint64_t(table.symbolCount) + best;
		}

		/** Compresses one table, keeping the best of the vocabulary sizes the grammar offers. */
		EncodedTable encodeTable(std::vector<uint8_t> values) {

			EncodedTable table;
			resolveOpenValues(values);

			std::vector<uint64_t> valueFrequency(StoredWin + 1, 0);
			for (const uint8_t v : values) {
				if (v > StoredWin) throw std::runtime_error("tbwrite: value out of range");
				valueFrequency[v]++;
			}

			const size_t distinct = std::count_if(valueFrequency.begin(), valueFrequency.end(),
				[](uint64_t f) { return f > 0; });

			if (distinct <= 1) {
				table.singleValue = true;
				table.value = values.empty() ? uint8_t(StoredDraw) : values.front();
				return table;
			}


			const Grammar grammar = buildGrammar(values);

			uint64_t best = UINT64_MAX;
			for (const std::vector<uint16_t>& sequence : grammar.stage) {
				EncodedTable candidate;
				const uint64_t bytes = encodeStage(grammar, sequence, values.size(), candidate);

				if (bytes >= best) continue;

				best = bytes;
				table = std::move(candidate);
			}

			return table;
		}

		// ------------------------------------------------------------------
		// Byte output
		// ------------------------------------------------------------------

		/**
		 * The sixteen check bytes at the end of the file.
		 *
		 * de Man puts a checksum there and the probing code never looks at it; the
		 * algorithm is not part of what he published for probing, so this is our own: two
		 * independent FNV-1a lanes over the body, one forwards and one backwards, which
		 * makes it sensitive to the order of the bytes as well as to their values.
		 *
		 * @param body the file without its last sixteen bytes
		 */
		std::array<uint8_t, 16> checksumOf(const uint8_t* body, size_t size) {

			constexpr uint64_t OFFSET = 0xCBF29CE484222325ULL;
			constexpr uint64_t PRIME = 0x100000001B3ULL;

			uint64_t forwards = OFFSET;
			uint64_t backwards = OFFSET ^ size;

			for (size_t i = 0; i < size; ++i) {
				forwards = (forwards ^ body[i]) * PRIME;
				backwards = (backwards ^ body[size - 1 - i]) * PRIME;
			}

			std::array<uint8_t, 16> result{};
			for (int i = 0; i < 8; ++i) {
				result[i] = uint8_t(forwards >> (8 * i));
				result[8 + i] = uint8_t(backwards >> (8 * i));
			}
			return result;
		}

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
				put8(out, t.log2BlockBytes);
				put8(out, t.log2Span);
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

		// The reader takes a file whose size is not 64n + 16 for corrupt, so the data
		// ends on a block boundary and the last sixteen bytes are the check bytes. They
		// double as the padding the reader needs: it refills its bit buffer a word at a
		// time and may reach past the last block.
		alignTo(out, 64);

		const std::array<uint8_t, 16> checksum = checksumOf(out.data(), out.size());
		out.insert(out.end(), checksum.begin(), checksum.end());

		if (out.size() % 64 != 16)
			throw std::runtime_error("tbwrite: the file size is not 64n + 16");

		std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
		if (!file) throw std::runtime_error("tbwrite: cannot open " + filePath);
		file.write(reinterpret_cast<const char*>(out.data()), std::streamsize(out.size()));
		if (!file) throw std::runtime_error("tbwrite: cannot write " + filePath);
	}


	bool verifyChecksum(const std::string& filePath, std::string& reason) {

		std::ifstream file(filePath, std::ios::binary | std::ios::ate);
		if (!file) { reason = "cannot open " + filePath; return false; }

		const std::streamsize size = file.tellg();
		if (size < 16 || size % 64 != 16) {
			reason = "the file size is not 64n + 16";
			return false;
		}

		std::vector<uint8_t> content(static_cast<size_t>(size));
		file.seekg(0);
		file.read(reinterpret_cast<char*>(content.data()), size);
		if (!file) { reason = "cannot read " + filePath; return false; }

		const std::array<uint8_t, 16> expected = checksumOf(content.data(), content.size() - 16);
		if (!std::equal(expected.begin(), expected.end(), content.end() - 16)) {
			reason = "the check bytes do not match the content";
			return false;
		}
		return true;
	}

}
