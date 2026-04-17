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
 */

#include "recursive-pairing.h"
#include "repair-core.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <queue>
#include <stdexcept>

namespace QaplaRePair {

constexpr bool VERBOSE_COMPRESS = true;

namespace {

// =============================================================================
// Byte-stream helpers
// =============================================================================

template<typename T>
void appendValue(std::vector<uint8_t>& buf, const T& val) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&val);
    buf.insert(buf.end(), p, p + sizeof(T));
}

template<typename T>
T readValue(const uint8_t*& p, const uint8_t* end) {
    if (p + sizeof(T) > end)
        throw std::runtime_error("RePair deserialize: unexpected end of data");
    T val;
    std::memcpy(&val, p, sizeof(T));
    p += sizeof(T);
    return val;
}

// =============================================================================
// 12-bit LR packing — identical to Syzygy's LR struct layout:
//   byte0 = left[7:0]
//   byte1 = left[11:8] | (right[3:0] << 4)
//   byte2 = right[11:4]
// =============================================================================

inline void writeLR(std::vector<uint8_t>& buf, uint16_t left, uint16_t right) {
    buf.push_back(static_cast<uint8_t>(left & 0xFF));
    buf.push_back(static_cast<uint8_t>(((left >> 8) & 0xF) | ((right & 0xF) << 4)));
    buf.push_back(static_cast<uint8_t>(right >> 4));
}

inline void readLR(const uint8_t*& p, const uint8_t* end, uint16_t& left, uint16_t& right) {
    const uint8_t b0 = readValue<uint8_t>(p, end);
    const uint8_t b1 = readValue<uint8_t>(p, end);
    const uint8_t b2 = readValue<uint8_t>(p, end);
    left  = static_cast<uint16_t>(b0 | ((b1 & 0xF) << 8));
    right = static_cast<uint16_t>((b1 >> 4) | (static_cast<uint16_t>(b2) << 4));
}

// =============================================================================
// Bit I/O  (MSB first within each byte)
// =============================================================================

// Read 4 bytes as a big-endian uint32 so that buf64 >> (64-len) gives the top `len` bits.
inline uint32_t readBE32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16)
         | (uint32_t(p[2]) <<  8) |  uint32_t(p[3]);
}

// Write `len` bits of `code` MSB-first into buf at bit position bitPos.
void writeBits(uint8_t* buf, int& bitPos, uint32_t code, int len) {
    for (int i = len - 1; i >= 0; --i, ++bitPos) {
        if ((code >> i) & 1u)
            buf[bitPos >> 3] |= static_cast<uint8_t>(1 << (7 - (bitPos & 7)));
    }
}

// Returns floor(log2(x)) for x >= 1.  Used to encode power-of-2 sizes as exponents.
inline uint8_t log2floor(uint32_t x) {
    uint8_t n = 0;
    while (x > 1) { x >>= 1; ++n; }
    return n;
}

// =============================================================================
// Huffman tree: build symOrder[], symCount[], minSymLen, maxSymLen
// =============================================================================

struct HuffBuildResult {
    std::vector<uint8_t>   lengths;   // per-symbol code length (0 = unused), indexed by symbol
    std::vector<uint16_t>  symOrder;  // all coded symbols sorted by (length asc, index asc)
    uint8_t minLen = 0;
    uint8_t maxLen = 0;
};

HuffBuildResult buildHuffman(const std::vector<uint64_t>& freq, int numSymbols) {
    HuffBuildResult r;
    r.lengths.assign(numSymbols, 0);

    struct Node {
        uint64_t freq;
        int      left;   // index into nodes[], -1 = leaf
        int      right;
        int      sym;    // >= 0 for leaves
    };
    std::vector<Node> nodes;
    nodes.reserve(numSymbols * 2);

    auto cmp = [&](int a, int b){ return nodes[a].freq > nodes[b].freq; };
    std::priority_queue<int, std::vector<int>, decltype(cmp)> pq(cmp);

    for (int i = 0; i < numSymbols; ++i) {
        if (freq[i] > 0) {
            int idx = static_cast<int>(nodes.size());
            nodes.push_back({freq[i], -1, -1, i});
            pq.push(idx);
        }
    }
    if (pq.empty()) return r;

    if (pq.size() == 1) {
        r.lengths[nodes[pq.top()].sym] = 1;
    } else {
        while (pq.size() > 1) {
            int a = pq.top(); pq.pop();
            int b = pq.top(); pq.pop();
            int idx = static_cast<int>(nodes.size());
            nodes.push_back({nodes[a].freq + nodes[b].freq, a, b, -1});
            pq.push(idx);
        }
        // Assign depths via iterative DFS
        std::vector<std::pair<int,int>> stack;
        stack.push_back({pq.top(), 0});
        while (!stack.empty()) {
            auto [ni, d] = stack.back(); stack.pop_back();
            const Node& n = nodes[ni];
            if (n.sym >= 0) {
                if (d > MAX_HUFF_LEN)
                    throw std::runtime_error("RePair Huffman: code length exceeds MAX_HUFF_LEN");
                r.lengths[n.sym] = static_cast<uint8_t>(d);
            } else {
                if (n.left  >= 0) stack.push_back({n.left,  d + 1});
                if (n.right >= 0) stack.push_back({n.right, d + 1});
            }
        }
    }

    // Find minLen / maxLen
    r.minLen = 255; r.maxLen = 0;
    for (int i = 0; i < numSymbols; ++i) {
        if (r.lengths[i] > 0) {
            if (r.lengths[i] < r.minLen) r.minLen = r.lengths[i];
            if (r.lengths[i] > r.maxLen) r.maxLen = r.lengths[i];
        }
    }
    if (r.minLen == 255) { r.minLen = 0; return r; }

    // Build symOrder: all symbols with Huffman codes, sorted by (length asc, index asc).
    for (int i = 0; i < numSymbols; ++i)
        if (r.lengths[i] > 0)
            r.symOrder.push_back(static_cast<uint16_t>(i));
    std::sort(r.symOrder.begin(), r.symOrder.end(), [&](uint16_t a, uint16_t b) {
        return r.lengths[a] < r.lengths[b]
            || (r.lengths[a] == r.lengths[b] && a < b);
    });

    return r;
}

// =============================================================================
// Canonical encoder codes  (for compress() only)
// =============================================================================

struct HuffCode {
    uint32_t bits;
    int      len;   // 0 = unused symbol
};

// Assign canonical codes sorted by (length asc, symbol_index asc).
std::vector<HuffCode> buildEncodeCodes(const std::vector<uint8_t>& lengths, int numSymbols) {
    std::vector<HuffCode> codes(numSymbols, {0, 0});

    std::vector<int> syms;
    for (int i = 0; i < numSymbols; ++i)
        if (lengths[i] > 0) syms.push_back(i);
    std::sort(syms.begin(), syms.end(), [&](int a, int b) {
        return lengths[a] < lengths[b] || (lengths[a] == lengths[b] && a < b);
    });

    uint32_t code = 0;
    int prevLen = 0;
    for (int sym : syms) {
        int len = lengths[sym];
        code <<= (len - prevLen);
        codes[sym] = {code, len};
        ++code;
        prevLen = len;
    }
    return codes;
}

// =============================================================================
// base64[] — standard canonical Huffman starting codes, left-padded to 64 bits
// =============================================================================

std::vector<uint64_t> buildBase64(
    const std::vector<uint16_t>& symCount,
    uint8_t minLen)
{
    const int n = static_cast<int>(symCount.size());
    std::vector<uint64_t> base64(n, 0);

    uint64_t code = 0;
    for (int li = 0; li < n; ++li) {
        const int shift = 64 - li - static_cast<int>(minLen);
        base64[li] = code << shift;
        code = (code + symCount[li]) << 1;
    }
    return base64;
}

// =============================================================================
// groupStart[] — prefix sums of symCount for indexing into symOrder[]
// =============================================================================

std::vector<uint32_t> buildGroupStart(const std::vector<uint16_t>& symCount) {
    const int n = static_cast<int>(symCount.size());
    std::vector<uint32_t> gs(n, 0);
    for (int li = 1; li < n; ++li)
        gs[li] = gs[li - 1] + symCount[li - 1];
    return gs;
}

// =============================================================================
// symlen[] — expansion count minus 1, per symbol
// =============================================================================

std::vector<uint32_t> buildSymlen(const std::vector<PairRule>& btree, int numSymbols) {
    std::vector<uint32_t> symlen(numSymbols, 0);
    for (int i = NUM_TERMINALS; i < numSymbols; ++i) {
        const PairRule& r = btree[i - NUM_TERMINALS];
        symlen[i] = symlen[r.left] + symlen[r.right] + 1;
    }
    return symlen;
}

// =============================================================================
// sparseIndex[]
// =============================================================================

std::vector<SparseEntry> buildSparseIndex(
    const std::vector<uint16_t>& blockLength, uint32_t span)
{
    if (blockLength.empty() || span == 0) return {};

    uint64_t totalTerminals = 0;
    for (uint16_t bl : blockLength) totalTerminals += static_cast<uint64_t>(bl) + 1;

    const size_t numSparse = (totalTerminals + span - 1) / span;
    std::vector<SparseEntry> idx;
    idx.reserve(numSparse);

    uint64_t posAccum = 0;
    size_t   sparseK  = 0;

    for (uint32_t b = 0; b < static_cast<uint32_t>(blockLength.size())
                          && sparseK < numSparse; ++b)
    {
        const uint32_t blockCount = static_cast<uint32_t>(blockLength[b]) + 1;
        while (sparseK < numSparse) {
            const uint64_t Ik = sparseK * span + span / 2;
            if (Ik >= posAccum + blockCount) break;
            idx.push_back({b, static_cast<uint32_t>(Ik - posAccum)});
            ++sparseK;
        }
        posAccum += blockCount;
    }

    // Fill phantom entries for sparse indices beyond totalTerminals.
    // probe() computes k = idx/span and requires sparseIndex[k] to exist for
    // every valid idx.  For k values where I(k) >= totalTerminals the inner
    // loop above never fires; we fill with positions past the last real block
    // so that probe()'s backward navigation still lands in valid data.
    if (sparseK < numSparse) {
        const uint32_t lastBlock      = static_cast<uint32_t>(blockLength.size()) - 1;
        const uint64_t lastBlockStart = totalTerminals
                                      - (static_cast<uint64_t>(blockLength[lastBlock]) + 1);
        while (sparseK < numSparse) {
            const uint64_t Ik           = static_cast<uint64_t>(sparseK) * span + span / 2;
            const uint64_t phantomOffset = Ik - lastBlockStart;
            idx.push_back({lastBlock, static_cast<uint32_t>(phantomOffset)});
            ++sparseK;
        }
    }

    return idx;
}

// =============================================================================
// Re-Pair symbol expansion (used by decompressBlock / decompress)
// =============================================================================

void expandOneSymbol(
    uint16_t sym,
    const std::vector<PairRule>& btree,
    std::vector<QaplaBitbase::BitbaseResult>& out)
{
    std::vector<uint16_t> stack;
    stack.push_back(sym);
    while (!stack.empty()) {
        uint16_t s = stack.back(); stack.pop_back();
        if (s < NUM_TERMINALS) {
            out.push_back(static_cast<QaplaBitbase::BitbaseResult>(s));
        } else {
            const PairRule& rule = btree[s - NUM_TERMINALS];
            stack.push_back(rule.right);
            stack.push_back(rule.left);
        }
    }
}

} // anonymous namespace

// =============================================================================
// Public API
// =============================================================================

RePairData compress(const std::vector<QaplaBitbase::BitbaseResult>& input,
                    size_t blockBytes, uint32_t spanParam, uint32_t maxSymlen,
                    uint32_t minPairFreq)
{
    if ((blockBytes & (blockBytes - 1)) != 0)
        throw std::runtime_error("RePair compress: blockBytes must be a power of 2");
    if ((spanParam & (spanParam - 1)) != 0)
        throw std::runtime_error("RePair compress: span must be a power of 2");

    RePairData result;
    result.sizeofBlock = blockBytes;
    result.span        = spanParam;

    if (input.empty()) return result;

    // Step 1: convert to 16-bit symbol sequence
    std::vector<uint16_t> seq(input.size());
    for (size_t i = 0; i < input.size(); ++i)
        seq[i] = static_cast<uint16_t>(input[i]);

    // Step 2: Re-Pair — O(N log N) single-pass via repair-core
    int nextSymbol = NUM_TERMINALS;
    repairFull(seq, result.btree, nextSymbol, MAX_VOCAB_SIZE, maxSymlen, minPairFreq);

    const int numSymbols = NUM_TERMINALS + static_cast<int>(result.btree.size());

    // Step 3: symlen (for probe())
    result.symlen = buildSymlen(result.btree, numSymbols);

    // Step 4: frequency count for Huffman
    std::vector<uint64_t> symFreq(numSymbols, 0);
    for (uint16_t s : seq) ++symFreq[s];

    // Step 5: build Huffman tree → symOrder, symCount, minSymLen, maxSymLen
    const HuffBuildResult huff = buildHuffman(symFreq, numSymbols);
    result.minSymLen = huff.minLen;
    result.maxSymLen = huff.maxLen;
    result.symOrder  = huff.symOrder;

    // Count symbols per relative length
    const int numLens = result.maxSymLen - result.minSymLen + 1;
    result.symCount.assign(numLens, 0);
    for (int i = 0; i < numSymbols; ++i) {
        if (huff.lengths[i] > 0)
            result.symCount[huff.lengths[i] - result.minSymLen]++;
    }

    // Derived: base64 and groupStart
    result.base64      = buildBase64(result.symCount, huff.minLen);
    result.groupStart  = buildGroupStart(result.symCount);

    // Step 6: canonical encoder codes (needed for bit-packing; not stored)
    const std::vector<HuffCode> codes = buildEncodeCodes(huff.lengths, numSymbols);

    // Step 7: bit-pack into fixed-size blocks; no symbol split across boundary
    const int blockBits = static_cast<int>(blockBytes) * 8;
    std::vector<uint8_t> blockBuf(blockBytes, 0);
    int      bitsUsed      = 0;
    uint32_t blockTerminals = 0;

    for (uint16_t sym : seq) {
        const HuffCode& c = codes[sym];
        if (c.len == 0)
            throw std::runtime_error("RePair compress: symbol has no Huffman code");
        if (c.len > blockBits)
            throw std::runtime_error("RePair compress: Huffman code longer than block size");

        if (bitsUsed + c.len > blockBits) {
            result.encoded.insert(result.encoded.end(), blockBuf.begin(), blockBuf.end());
            if (blockTerminals == 0 || blockTerminals > 65536u)
                throw std::runtime_error("RePair compress: block terminal count out of range");
            result.blockLength.push_back(static_cast<uint16_t>(blockTerminals - 1u));
            std::fill(blockBuf.begin(), blockBuf.end(), 0);
            bitsUsed       = 0;
            blockTerminals = 0;
        }

        writeBits(blockBuf.data(), bitsUsed, c.bits, c.len);
        blockTerminals += result.symlen[sym] + 1u;
    }

    if (bitsUsed > 0) {
        result.encoded.insert(result.encoded.end(), blockBuf.begin(), blockBuf.end());
        if (blockTerminals == 0 || blockTerminals > 65536u)
            throw std::runtime_error("RePair compress: block terminal count out of range");
        result.blockLength.push_back(static_cast<uint16_t>(blockTerminals - 1u));
    }

    result.blocksNum = static_cast<uint32_t>(result.blockLength.size());

    // Step 8: build sparseIndex
    result.sparseIndex = buildSparseIndex(result.blockLength, result.span);

    // Step 9: 8 zero bytes of read-ahead padding for the 64-bit rolling buffer in probe()
    result.encoded.insert(result.encoded.end(), 8, 0);

    return result;
}


std::vector<uint8_t> serialize(const RePairData& data) {
    const uint32_t numBlocks   = data.blocksNum;
    const int      numLens     = data.maxSymLen - data.minSymLen + 1;
    const uint32_t numSymTotal = static_cast<uint32_t>(data.symOrder.size());

    // sizeofBlock and span must be powers of 2
    if ((data.sizeofBlock & (data.sizeofBlock - 1)) != 0)
        throw std::runtime_error("RePair serialize: sizeofBlock must be a power of 2");
    if ((data.span & (data.span - 1)) != 0)
        throw std::runtime_error("RePair serialize: span must be a power of 2");

    std::vector<uint8_t> buf;
    buf.reserve(
        sizeof(uint16_t)                           // numRules
        + data.btree.size() * 3                    // btree (3 bytes per rule, 12+12 bit)
        + 1 + 1                                    // maxSymLen, minSymLen
        + numLens * sizeof(uint16_t)               // symCount[]
        + numSymTotal * sizeof(uint16_t)           // symOrder[] (uint16_t for 12-bit symbols)
        + 1                                        // log2_sizeofBlock
        + 1                                        // log2_span
        + sizeof(uint32_t)                         // blocksNum
        + numBlocks * sizeof(uint16_t)             // blockLength[]
        + data.encoded.size());                    // bit-packed stream (incl. 8-byte padding)

    // 1. Grammar (btree): each rule packed as 3 bytes (two 12-bit children)
    appendValue(buf, static_cast<uint16_t>(data.btree.size()));
    for (const PairRule& r : data.btree)
        writeLR(buf, r.left, r.right);

    // 2. Huffman: maxSymLen, minSymLen, then symCount[], then symOrder[]
    buf.push_back(data.maxSymLen);
    buf.push_back(data.minSymLen);
    for (uint16_t sc : data.symCount)  appendValue(buf, sc);
    for (uint16_t so : data.symOrder)  appendValue(buf, so);

    // 3. Block layout: sizes stored as log2 exponents (1 byte each)
    buf.push_back(log2floor(static_cast<uint32_t>(data.sizeofBlock)));
    buf.push_back(log2floor(static_cast<uint32_t>(data.span)));
    appendValue(buf, numBlocks);

    // 4. blockLength[] (count-1)
    for (uint16_t len : data.blockLength) appendValue(buf, len);

    // 5. Bit-packed encoded stream (includes 8-byte read-ahead padding)
    buf.insert(buf.end(), data.encoded.begin(), data.encoded.end());

    if (VERBOSE_COMPRESS) {
        const size_t btreeHeader  = sizeof(uint16_t);
        const size_t btreeRules   = data.btree.size() * 3;
        const size_t huffFixed    = 2;                              // maxSymLen + minSymLen
        const size_t huffCount    = static_cast<size_t>(numLens) * sizeof(uint16_t);
        const size_t huffOrder    = numSymTotal * sizeof(uint16_t);
        const size_t blockHeader  = 1 + 1 + sizeof(uint32_t);      // log2_block + log2_span + blocksNum
        const size_t blockLengths = numBlocks * sizeof(uint16_t);
        const size_t encodedData  = numBlocks * data.sizeofBlock;
        const size_t encodedPad   = 8;
        const size_t total        = buf.size();

        std::cerr << "\n=== RePair serialize breakdown ===\n"
                  << std::setw(28) << std::left << "  btree numRules (uint16):"
                  << std::setw(6) << std::right << btreeHeader  << " bytes  (" << data.btree.size() << " rules)\n"
                  << std::setw(28) << std::left << "  btree rules (3B each):"
                  << std::setw(6) << std::right << btreeRules   << " bytes\n"
                  << std::setw(28) << std::left << "  Huffman maxLen+minLen:"
                  << std::setw(6) << std::right << huffFixed    << " bytes  (min=" << +data.minSymLen << " max=" << +data.maxSymLen << ")\n"
                  << std::setw(28) << std::left << "  Huffman symCount[]:"
                  << std::setw(6) << std::right << huffCount    << " bytes  (" << numLens << " lengths)\n"
                  << std::setw(28) << std::left << "  Huffman symOrder[]:"
                  << std::setw(6) << std::right << huffOrder    << " bytes  (" << numSymTotal << " symbols)\n"
                  << std::setw(28) << std::left << "  block log2+blocksNum:"
                  << std::setw(6) << std::right << blockHeader  << " bytes  (sizeofBlock=" << data.sizeofBlock << " span=" << data.span << ")\n"
                  << std::setw(28) << std::left << "  blockLength[]:"
                  << std::setw(6) << std::right << blockLengths << " bytes  (" << numBlocks << " blocks)\n"
                  << std::setw(28) << std::left << "  encoded data:"
                  << std::setw(6) << std::right << encodedData  << " bytes\n"
                  << std::setw(28) << std::left << "  encoded padding:"
                  << std::setw(6) << std::right << encodedPad   << " bytes\n"
                  << "  " << std::string(34, '-') << "\n"
                  << std::setw(28) << std::left << "  Total payload:"
                  << std::setw(6) << std::right << total        << " bytes\n\n";
    }

    return buf;
}


RePairData deserialize(const uint8_t* raw, size_t size) {
    const uint8_t* p   = raw;
    const uint8_t* end = raw + size;
    RePairData result;

    // 1. Grammar (btree): each rule is 3 bytes (two 12-bit children)
    const uint16_t numRules = readValue<uint16_t>(p, end);
    result.btree.resize(numRules);
    for (uint16_t i = 0; i < numRules; ++i)
        readLR(p, end, result.btree[i].left, result.btree[i].right);

    // 2. Huffman: maxSymLen, minSymLen, symCount[], symOrder[]
    result.maxSymLen = readValue<uint8_t>(p, end);
    result.minSymLen = readValue<uint8_t>(p, end);
    const int numLens = result.maxSymLen - result.minSymLen + 1;
    result.symCount.resize(numLens);
    for (int i = 0; i < numLens; ++i)
        result.symCount[i] = readValue<uint16_t>(p, end);

    uint32_t totalSyms = 0;
    for (uint16_t c : result.symCount) totalSyms += c;
    result.symOrder.resize(totalSyms);
    for (uint32_t i = 0; i < totalSyms; ++i)
        result.symOrder[i] = readValue<uint16_t>(p, end);

    // 3. Block layout: read log2 exponents and reconstruct actual sizes
    result.sizeofBlock = static_cast<size_t>(1u) << readValue<uint8_t>(p, end);
    result.span        = 1u << readValue<uint8_t>(p, end);
    const uint32_t numBlocks = readValue<uint32_t>(p, end);
    result.blocksNum = numBlocks;

    // 4. blockLength[]
    result.blockLength.resize(numBlocks);
    for (uint32_t i = 0; i < numBlocks; ++i)
        result.blockLength[i] = readValue<uint16_t>(p, end);

    // 5. Bit-packed encoded stream (with 8-byte read-ahead padding)
    const size_t encodedBytes = static_cast<size_t>(numBlocks) * result.sizeofBlock + 8;
    if (p + encodedBytes > end)
        throw std::runtime_error("RePair deserialize: encoded stream truncated");
    result.encoded.assign(p, p + encodedBytes);

    // ── Derived fields ────────────────────────────────────────────────────────
    const int numSymbols = NUM_TERMINALS + static_cast<int>(numRules);
    result.symlen      = buildSymlen(result.btree, numSymbols);
    result.base64      = buildBase64(result.symCount, result.minSymLen);
    result.groupStart  = buildGroupStart(result.symCount);
    result.sparseIndex = buildSparseIndex(result.blockLength, result.span);

    return result;
}


WDLValue probe(const RePairData& data, uint64_t idx) {
    if (data.blockLength.empty())
        throw std::runtime_error("RePair probe: no blocks");
    if (data.sparseIndex.empty())
        throw std::runtime_error("RePair probe: sparseIndex not built");

    // ── Step 1: jump close to the correct block via sparseIndex ──────────────
    const uint32_t k = static_cast<uint32_t>(idx / data.span);
    uint32_t block  = data.sparseIndex[k].block;
    int64_t  offset = static_cast<int64_t>(data.sparseIndex[k].offset);

    const int64_t diff = static_cast<int64_t>(idx % data.span)
                       - static_cast<int64_t>(data.span / 2);
    offset += diff;

    while (offset < 0)
        offset += static_cast<int64_t>(data.blockLength[--block]) + 1;
    while (offset > static_cast<int64_t>(data.blockLength[block]))
        offset -= static_cast<int64_t>(data.blockLength[block++]) + 1;

    // ── Step 2: decode symbols with 64-bit rolling buffer ────────────────────
    const uint8_t* blockStart = data.encoded.data()
                              + static_cast<size_t>(block) * data.sizeofBlock;
    const uint32_t* ptr = reinterpret_cast<const uint32_t*>(blockStart);

    uint64_t buf64    = (static_cast<uint64_t>(readBE32(blockStart))     << 32)
                      |  static_cast<uint64_t>(readBE32(blockStart + 4));
    int buf64Size = 64;
    ptr += 2;

    const int      numLens   = data.maxSymLen - data.minSymLen + 1;
    const uint8_t  minSymLen = data.minSymLen;

    uint16_t sym = 0;
    while (true) {
        // Refill when 32 or fewer bits remain
        if (buf64Size <= 32) {
            buf64Size += 32;
            buf64 |= static_cast<uint64_t>(readBE32(
                         reinterpret_cast<const uint8_t*>(ptr++)))
                   << (64 - buf64Size);
        }

        // Find length group: advance len while buf64 >= next group's start code
        int len = 0;
        while (len < numLens - 1 && buf64 >= data.base64[len + 1])
            ++len;

        // Offset within the length group → look up original grammar symbol
        const uint32_t offset_in_group = static_cast<uint32_t>(
            (buf64 - data.base64[len]) >> (64 - len - minSymLen));
        sym = data.symOrder[data.groupStart[len] + offset_in_group];

        const int64_t expansion = static_cast<int64_t>(data.symlen[sym]) + 1;
        if (offset < expansion)
            break;   // this symbol contains our target position

        offset    -= expansion;
        const int realLen = len + minSymLen;
        buf64     <<= realLen;
        buf64Size  -= realLen;
    }

    // ── Step 3: navigate btree to find the exact terminal ────────────────────
    while (data.symlen[sym] > 0) {
        const uint16_t left    = data.btree[sym - NUM_TERMINALS].left;
        const int64_t  leftExp = static_cast<int64_t>(data.symlen[left]) + 1;
        if (offset < leftExp) {
            sym = left;
        } else {
            offset -= leftExp;
            sym = data.btree[sym - NUM_TERMINALS].right;
        }
    }

    return static_cast<WDLValue>(sym);
}


std::vector<QaplaBitbase::BitbaseResult> decompressBlock(
    const RePairData& data, uint32_t blockIndex)
{
    if (blockIndex >= data.blockLength.size()) return {};

    const int numLens    = data.maxSymLen - data.minSymLen + 1;
    const uint8_t minLen = data.minSymLen;

    const uint8_t* blockData =
        data.encoded.data() + static_cast<size_t>(blockIndex) * data.sizeofBlock;
    const size_t target = static_cast<size_t>(data.blockLength[blockIndex]) + 1u;

    std::vector<QaplaBitbase::BitbaseResult> result;
    result.reserve(target);

    int bitPos = 0;
    auto readBit1 = [&]() -> uint64_t {
        return static_cast<uint64_t>((blockData[bitPos >> 3] >> (7 - (bitPos & 7))) & 1);
    };

    while (result.size() < target) {
        uint64_t s64 = 0;
        for (int l = 0; l < minLen; ++l) {
            s64 = (s64 << 1) | readBit1();
            ++bitPos;
        }
        s64 <<= (64 - minLen);

        int len = 0;
        while (len < numLens - 1 && s64 >= data.base64[len + 1]) {
            s64 |= readBit1() << (64 - minLen - len - 1);
            ++bitPos;
            ++len;
        }

        const uint32_t offset_in_group = static_cast<uint32_t>(
            (s64 - data.base64[len]) >> (64 - len - minLen));
        const uint16_t sym = data.symOrder[data.groupStart[len] + offset_in_group];

        expandOneSymbol(sym, data.btree, result);
    }

    if (result.size() != target)
        throw std::runtime_error("RePair decompressBlock: decoded size mismatch");
    return result;
}


std::vector<QaplaBitbase::BitbaseResult> decompress(const RePairData& data) {
    uint64_t totalOut = 0;
    for (uint16_t len : data.blockLength) totalOut += static_cast<uint64_t>(len) + 1u;

    std::vector<QaplaBitbase::BitbaseResult> result;
    result.reserve(static_cast<size_t>(totalOut));

    for (uint32_t b = 0; b < data.blocksNum; ++b) {
        auto block = decompressBlock(data, b);
        result.insert(result.end(), block.begin(), block.end());
    }
    return result;
}

} // namespace QaplaRePair
