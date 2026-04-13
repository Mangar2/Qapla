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

#include <algorithm>
#include <cstring>
#include <queue>
#include <stdexcept>

namespace QaplaRePair {

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

// =============================================================================
// Huffman tree: build lowestSym[], minSymLen, maxSymLen
// =============================================================================

// Build the Huffman tree and return per-symbol code lengths plus lowestSym[].
// lowestSym[li] = lowest symbol index (grammar order) at relative length li = len - minLen.
// This is the same minimal serialization Syzygy uses:  one uint16_t per length level.
//
// "Grammar order" means: canonical symbol ordering is by (code_length asc, symbol_index asc).
// Under canonical assignment the lowest-code symbol at each length is also the one with the
// smallest symbol index among all symbols of that length, so lowestSym[li] can be computed
// directly from the sorted symbol list.
struct HuffBuildResult {
    std::vector<uint8_t>  lengths;    // per-symbol code length (0 = unused)
    std::vector<uint16_t> lowestSym;  // per relative-length: lowest symbol index
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

    // Build lowestSym[].
    // Under canonical assignment (sort by length asc, symbol-index asc),
    // lowestSym[li] is the smallest symbol index at relative length li.
    const int numLens = r.maxLen - r.minLen + 1;
    r.lowestSym.assign(numLens, 0xFFFFu);
    for (int i = 0; i < numSymbols; ++i) {
        if (r.lengths[i] > 0) {
            const int li = r.lengths[i] - r.minLen;
            if (static_cast<uint16_t>(i) < r.lowestSym[li])
                r.lowestSym[li] = static_cast<uint16_t>(i);
        }
    }

    return r;
}

// =============================================================================
// Canonical encoder codes  (for compress() only)
// =============================================================================

struct HuffCode {
    uint32_t bits;
    int      len;   // 0 = unused symbol
};

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
// base64[] — the identical computation Syzygy does in set_sizes()
// =============================================================================

// Build base64[] from lowestSym[], minLen.
// base64[li] is the 64-bit left-padded (right-zero-padded to 64 bits) value of the
// lowest canonical code at relative length li = len - minLen.
//
// Syzygy's exact recurrence (from set_sizes()):
//   base64[i] = (base64[i+1] + lowestSym[i] - lowestSym[i+1]) / 2
//   then: base64[i] <<= 64 - i - minSymLen
//
// The canonical code ordering used in Syzygy is DESCENDING by code value for longer
// symbols (longer = numerically smaller code), i.e. lowestSym[i] >= lowestSym[i+1].
// We produce exactly the same ordering in buildHuffman() because we assign codes in
// ascending order to symbols sorted by (length asc, index asc), which means longer
// codes start at smaller absolute values — the same convention Syzygy uses.
std::vector<uint64_t> buildBase64(
    const std::vector<uint16_t>& lowestSym, uint8_t minLen)
{
    const int n = static_cast<int>(lowestSym.size());
    std::vector<uint64_t> base64(n, 0);

    // Recurrence from highest to lowest relative length (Syzygy's loop direction)
    for (int i = n - 2; i >= 0; --i) {
        base64[i] = (base64[i + 1] + lowestSym[i] - lowestSym[i + 1]) / 2;
    }
    // Left-shift so base64[li] is right-padded to 64 bits at length (li + minLen)
    for (int i = 0; i < n; ++i)
        base64[i] <<= 64 - i - minLen;

    return base64;
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
            idx.push_back({b, static_cast<uint16_t>(Ik - posAccum)});
            ++sparseK;
        }
        posAccum += blockCount;
    }
    return idx;
}

// =============================================================================
// Re-Pair
// =============================================================================

void expandOneSymbol(
    uint8_t sym,
    const std::vector<PairRule>& btree,
    std::vector<QaplaBitbase::BitbaseResult>& out)
{
    std::vector<uint8_t> stack;
    stack.push_back(sym);
    while (!stack.empty()) {
        uint8_t s = stack.back(); stack.pop_back();
        if (s < NUM_TERMINALS) {
            out.push_back(static_cast<QaplaBitbase::BitbaseResult>(s));
        } else {
            const PairRule& rule = btree[s - NUM_TERMINALS];
            stack.push_back(rule.right);
            stack.push_back(rule.left);
        }
    }
}

bool repairIteration(
    std::vector<uint8_t>& seq,
    std::vector<PairRule>& btree,
    int& nextSymbol,
    std::vector<uint32_t>& freq)
{
    if (nextSymbol >= MAX_VOCAB_SIZE) return false;
    if (seq.size() < 2) return false;

    std::fill(freq.begin(), freq.end(), 0);
    for (size_t i = 0; i + 1 < seq.size(); ++i)
        ++freq[static_cast<size_t>(seq[i]) * MAX_VOCAB_SIZE + seq[i + 1]];

    uint32_t bestCount = 1;
    int bestIdx = -1;
    for (int i = 0, n = MAX_VOCAB_SIZE * MAX_VOCAB_SIZE; i < n; ++i) {
        if (freq[i] > bestCount) { bestCount = freq[i]; bestIdx = i; }
    }
    if (bestIdx < 0) return false;

    const uint8_t left  = static_cast<uint8_t>(bestIdx / MAX_VOCAB_SIZE);
    const uint8_t right = static_cast<uint8_t>(bestIdx % MAX_VOCAB_SIZE);
    btree.push_back({left, right});
    const uint8_t newSym = static_cast<uint8_t>(nextSymbol++);

    size_t w = 0;
    for (size_t i = 0; i < seq.size(); ) {
        if (i + 1 < seq.size() && seq[i] == left && seq[i + 1] == right) {
            seq[w++] = newSym; i += 2;
        } else {
            seq[w++] = seq[i++];
        }
    }
    seq.resize(w);
    return true;
}

} // anonymous namespace

// =============================================================================
// Public API
// =============================================================================

RePairData compress(const std::vector<QaplaBitbase::BitbaseResult>& input,
                    size_t blockBytes, uint32_t spanParam)
{
    RePairData result;
    result.sizeofBlock = blockBytes;
    result.span        = spanParam;

    if (input.empty()) return result;

    // Step 1: convert to byte sequence
    std::vector<uint8_t> seq(input.size());
    for (size_t i = 0; i < input.size(); ++i)
        seq[i] = static_cast<uint8_t>(input[i]);

    // Step 2: Re-Pair — build grammar (btree)
    std::vector<uint32_t> pairFreq(MAX_VOCAB_SIZE * MAX_VOCAB_SIZE, 0);
    int nextSymbol = NUM_TERMINALS;
    while (repairIteration(seq, result.btree, nextSymbol, pairFreq)) {}

    const int numSymbols = NUM_TERMINALS + static_cast<int>(result.btree.size());

    // Step 3: symlen (for probe())
    result.symlen = buildSymlen(result.btree, numSymbols);

    // Step 4: frequency count for Huffman
    std::vector<uint64_t> symFreq(numSymbols, 0);
    for (uint8_t s : seq) ++symFreq[s];

    // Step 5: build Huffman tree → lowestSym, minSymLen, maxSymLen, base64
    const HuffBuildResult huff = buildHuffman(symFreq, numSymbols);
    result.minSymLen = huff.minLen;
    result.maxSymLen = huff.maxLen;
    result.lowestSym = huff.lowestSym;
    result.base64    = buildBase64(huff.lowestSym, huff.minLen);

    // Step 6: canonical encoder codes (needed for bit-packing; not stored)
    const std::vector<HuffCode> codes = buildEncodeCodes(huff.lengths, numSymbols);

    // Step 7: bit-pack into fixed-size blocks; no symbol split across boundary
    const int blockBits = static_cast<int>(blockBytes) * 8;
    std::vector<uint8_t> blockBuf(blockBytes, 0);
    int      bitsUsed      = 0;
    uint32_t blockTerminals = 0;

    for (uint8_t sym : seq) {
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
    const uint32_t numBlocks = data.blocksNum;
    const int      numLens   = data.maxSymLen - data.minSymLen + 1;

    std::vector<uint8_t> buf;
    buf.reserve(
        sizeof(uint16_t)                       // numRules
        + data.btree.size() * 2                // btree
        + 1 + 1                                // maxSymLen, minSymLen
        + numLens * sizeof(uint16_t)           // lowestSym[]
        + sizeof(uint32_t)                     // sizeofBlock
        + sizeof(uint32_t)                     // span
        + sizeof(uint32_t)                     // blocksNum
        + numBlocks * sizeof(uint16_t)         // blockLength[]
        + data.encoded.size());                // bit-packed stream (incl. 8-byte padding)

    // 1. Grammar (btree)
    appendValue(buf, static_cast<uint16_t>(data.btree.size()));
    for (const PairRule& r : data.btree) {
        buf.push_back(r.left);
        buf.push_back(r.right);
    }

    // 2. Huffman: maxSymLen, minSymLen, then lowestSym[]
    buf.push_back(data.maxSymLen);
    buf.push_back(data.minSymLen);
    for (uint16_t ls : data.lowestSym) appendValue(buf, ls);

    // 3. Block layout
    appendValue(buf, static_cast<uint32_t>(data.sizeofBlock));
    appendValue(buf, data.span);
    appendValue(buf, numBlocks);

    // 4. blockLength[] (count-1)
    for (uint16_t len : data.blockLength) appendValue(buf, len);

    // 5. Bit-packed encoded stream (includes 8-byte padding)
    buf.insert(buf.end(), data.encoded.begin(), data.encoded.end());

    return buf;
}


RePairData deserialize(const uint8_t* raw, size_t size) {
    const uint8_t* p   = raw;
    const uint8_t* end = raw + size;
    RePairData result;

    // 1. Grammar (btree)
    const uint16_t numRules = readValue<uint16_t>(p, end);
    result.btree.resize(numRules);
    for (uint16_t i = 0; i < numRules; ++i) {
        result.btree[i].left  = readValue<uint8_t>(p, end);
        result.btree[i].right = readValue<uint8_t>(p, end);
    }

    // 2. Huffman: maxSymLen, minSymLen, lowestSym[]
    result.maxSymLen = readValue<uint8_t>(p, end);
    result.minSymLen = readValue<uint8_t>(p, end);
    const int numLens = result.maxSymLen - result.minSymLen + 1;
    result.lowestSym.resize(numLens);
    for (int i = 0; i < numLens; ++i)
        result.lowestSym[i] = readValue<uint16_t>(p, end);

    // 3. Block layout
    result.sizeofBlock = static_cast<size_t>(readValue<uint32_t>(p, end));
    result.span        = readValue<uint32_t>(p, end);
    const uint32_t numBlocks = readValue<uint32_t>(p, end);
    result.blocksNum = numBlocks;

    // 4. blockLength[]
    result.blockLength.resize(numBlocks);
    for (uint32_t i = 0; i < numBlocks; ++i)
        result.blockLength[i] = readValue<uint16_t>(p, end);

    // 5. Bit-packed encoded stream (with 8-byte padding appended by serialize())
    const size_t encodedBytes = static_cast<size_t>(numBlocks) * result.sizeofBlock + 8;
    if (p + encodedBytes > end)
        throw std::runtime_error("RePair deserialize: encoded stream truncated");
    result.encoded.assign(p, p + encodedBytes);

    // ── Derived fields ────────────────────────────────────────────────────────
    const int numSymbols = NUM_TERMINALS + static_cast<int>(numRules);
    result.symlen      = buildSymlen(result.btree, numSymbols);
    result.sparseIndex = buildSparseIndex(result.blockLength, result.span);
    result.base64      = buildBase64(result.lowestSym, result.minSymLen);

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
    // Identical approach to decompress_pairs() in Syzygy's tbprobe.cpp.
    // buf64 holds the next bits in the most-significant positions.
    // ptr advances through the block 4 bytes at a time for 32-bit refills.
    const uint8_t* blockStart = data.encoded.data()
                              + static_cast<size_t>(block) * data.sizeofBlock;
    const uint32_t* ptr = reinterpret_cast<const uint32_t*>(blockStart);

    uint64_t buf64    = (static_cast<uint64_t>(readBE32(blockStart))     << 32)
                      |  static_cast<uint64_t>(readBE32(blockStart + 4));
    int buf64Size = 64;
    ptr += 2;

    const int      numLens   = data.maxSymLen - data.minSymLen + 1;
    const uint8_t  minSymLen = data.minSymLen;

    uint16_t sym;
    while (true) {
        // Refill when 32 or fewer bits remain — identical to Syzygy
        if (buf64Size <= 32) {
            buf64Size += 32;
            buf64 |= static_cast<uint64_t>(readBE32(
                         reinterpret_cast<const uint8_t*>(ptr++)))
                   << (64 - buf64Size);
        }

        // Find symbol length: walk base64[] until buf64 >= base64[len].
        // Syzygy: "while (buf64 < d->base64[len]) ++len;"
        int len = 0;
        while (len < numLens - 1 && buf64 < data.base64[len])
            ++len;

        // Compute symbol index: offset within the length group + lowestSym[len]
        sym = static_cast<uint16_t>(
                  (buf64 - data.base64[len]) >> (64 - len - minSymLen))
            + data.lowestSym[len];

        const int64_t expansion = static_cast<int64_t>(data.symlen[sym]) + 1;
        if (offset < expansion)
            break;   // this symbol contains our target position

        // Consume the symbol's bits and subtract its expansion from offset
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

    const int numLens = data.maxSymLen - data.minSymLen + 1;

    // Use base64[] for bit-by-bit decoding (performance is not critical for full decompression):
    const uint8_t* blockData =
        data.encoded.data() + static_cast<size_t>(blockIndex) * data.sizeofBlock;
    const size_t target = static_cast<size_t>(data.blockLength[blockIndex]) + 1u;

    std::vector<QaplaBitbase::BitbaseResult> result;
    result.reserve(target);

    // Bit-by-bit decode using base64[] (same principle as probe's inner loop but simpler)
    int bitPos = 0;
    auto readBit1 = [&]() -> int {
        return (blockData[bitPos >> 3] >> (7 - (bitPos & 7))) & 1;
    };

    while (result.size() < target) {
        int len = 0;
        uint64_t accum = 0;
        for (int l = 0; l < data.minSymLen; ++l)
            accum = (accum << 1) | readBit1(), ++bitPos;
        // accum now holds minSymLen bits
        // Scan lengths: pad accum to 64 bits and compare to base64[]
        uint64_t s64 = accum << (64 - data.minSymLen);
        while (len < numLens - 1 && s64 < data.base64[len]) {
            s64 = (s64 << 1) | (static_cast<uint64_t>(readBit1()) << (64 - data.minSymLen - len - 1));
            ++bitPos;
            ++len;
        }
        // Now decode symbol
        uint16_t symDec = static_cast<uint16_t>(
            (s64 - data.base64[len]) >> (64 - len - data.minSymLen))
            + data.lowestSym[len];
        expandOneSymbol(static_cast<uint8_t>(symDec), data.btree, result);
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
