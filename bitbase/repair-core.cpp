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

#include "repair-core.h"

#include <unordered_map>
#include <queue>
#include <vector>
#include <utility>
#include <cstdint>

namespace QaplaRePair {

namespace {

// Sentinel written into absorbed sequence slots (value outside valid symbol range).
static constexpr uint16_t REMOVED = 0xFFFF;

inline uint32_t pairKey(uint16_t l, uint16_t r) {
    return (static_cast<uint32_t>(l) << 16) | r;
}

// ── Per-pair state ────────────────────────────────────────────────────────────

struct PairRec {
    uint32_t count    = 0;
    int32_t  occ_head = -1;   // head of the occurrence doubly-linked list
};

// ── Occurrence list ────────────────────────────────────────────────────────────
//
// Position i "owns" the pair (seq[i], seq[sl_nxt[i]]).
// It lives in exactly one pair's occurrence list at a time, linked via nxt[i]/prv[i].
//
// Invariant: a position is added to (at most) one pair's list per replacement step.
// The list is updated incrementally; stale entries may linger for detached pairs
// (pairs whose record has been cleared) but are never traversed because the
// snapshot was taken before clearing.

struct OccList {
    std::vector<int32_t> nxt, prv;

    void init(int32_t n) { nxt.assign(n, -1); prv.assign(n, -1); }

    void add(int32_t pos, PairRec& rec) {
        nxt[pos] = rec.occ_head;
        prv[pos] = -1;
        if (rec.occ_head >= 0) prv[rec.occ_head] = pos;
        rec.occ_head = pos;
        ++rec.count;
    }

    // Removes pos from rec's list.  Returns the count after decrement.
    uint32_t remove(int32_t pos, PairRec& rec) {
        const int32_t p = prv[pos], n = nxt[pos];
        if (p >= 0) nxt[p] = n; else rec.occ_head = n;
        if (n >= 0) prv[n] = p;
        nxt[pos] = prv[pos] = -1;
        return (rec.count > 0) ? --rec.count : 0u;
    }
};

} // anonymous namespace

// =============================================================================
// repairFull
// =============================================================================

void repairFull(
    std::vector<uint16_t>& seq,
    std::vector<PairRule>&  btree,
    int&                    nextSymbol,
    int                     maxVocab,
    uint32_t                maxSymlen,
    uint32_t                minPairFreq)
{
    const int32_t N = static_cast<int32_t>(seq.size());
    if (N < 2 || nextSymbol >= maxVocab) return;

    // symlen[s] = (number of terminals s expands to) - 1.
    // Terminals (0..nextSymbol-1) start at 0; non-terminals are set when created.
    std::vector<uint32_t> symlenVec(static_cast<size_t>(maxVocab), 0u);

    // ── Sequence doubly-linked list ───────────────────────────────────────────
    // sl_nxt[i] / sl_prv[i]: next / prev active position, -1 = none.
    // Position 0 is always the head: only sl_nxt[i] (= i2) is ever removed,
    // never i itself, so position 0 stays alive for the entire algorithm.
    std::vector<int32_t> sl_nxt(N), sl_prv(N);
    for (int32_t i = 0; i < N; ++i) {
        sl_nxt[i] = (i + 1 < N) ? i + 1 : -1;
        sl_prv[i] = (i > 0)     ? i - 1 : -1;
    }

    // ── Frequency table + occurrence lists ────────────────────────────────────
    // Reserve N entries upfront to avoid rehash during the initial fill.
    // Rehashes during the main loop are safe because no references to freq
    // entries persist across freq[] operator[] calls.
    std::unordered_map<uint32_t, PairRec> freq;
    freq.reserve(static_cast<size_t>(N));

    OccList ol;
    ol.init(N);

    for (int32_t i = 0; i + 1 < N; ++i)
        ol.add(i, freq[pairKey(seq[i], seq[i + 1])]);

    // ── Max-heap with lazy deletion ───────────────────────────────────────────
    // Entry: (count, pair_key).  Stale entries (count changed) are discarded on pop.
    using PQ = std::pair<uint32_t, uint32_t>;
    std::priority_queue<PQ> pq;
    for (const auto& [k, rec] : freq)
        if (rec.count > 1) pq.push({rec.count, k});

    std::vector<int32_t> snapshot;  // scratch buffer; reused each iteration

    // ── Main Re-Pair loop ─────────────────────────────────────────────────────
    while (!pq.empty() && nextSymbol < maxVocab) {
        const auto [cnt, key] = pq.top();
        pq.pop();

        // Lazy delete: discard if count has changed since this entry was pushed.
        {
            const auto it = freq.find(key);
            if (it == freq.end() || it->second.count != cnt || cnt < 2) continue;
        }

        // Max-heap: if the most frequent remaining pair is below the threshold,
        // all remaining pairs are too — no more rules would break even on overhead.
        if (cnt < minPairFreq) break;

        const uint16_t A = static_cast<uint16_t>(key >> 16);
        const uint16_t B = static_cast<uint16_t>(key & 0xFFFF);

        // Skip pairs whose merged symbol would exceed the depth limit.
        // Erase from freq so stale pq entries are discarded via lazy deletion.
        if (static_cast<uint64_t>(symlenVec[A]) + symlenVec[B] + 1 > maxSymlen) {
            freq.erase(key);
            continue;
        }

        const uint16_t C = static_cast<uint16_t>(nextSymbol++);
        symlenVec[C] = symlenVec[A] + symlenVec[B] + 1;
        btree.push_back({A, B});

        // ── Snapshot occurrence positions; detach the pair record ─────────────
        // Detaching (zeroing count and occ_head) makes any incidental ol.remove
        // call against this record a harmless no-op for the count bookkeeping.
        snapshot.clear();
        {
            PairRec& rec = freq[key];
            for (int32_t p = rec.occ_head; p >= 0; p = ol.nxt[p])
                snapshot.push_back(p);
            rec.occ_head = -1;
            rec.count    = 0;
        }

        // ── Replace each occurrence ───────────────────────────────────────────
        for (const int32_t i : snapshot) {

            // Validate: a previous replacement in this batch may have
            // absorbed this slot (overlapping pair like A A A).
            if (seq[i] != A) continue;
            const int32_t i2 = sl_nxt[i];
            if (i2 < 0 || seq[i2] != B) continue;

            const int32_t  i_left  = sl_prv[i];
            const int32_t  i_right = sl_nxt[i2];

            // Capture neighbor symbols before seq[] is modified.
            const uint16_t lsym = (i_left  >= 0) ? seq[i_left]  : REMOVED;
            const uint16_t rsym = (i_right >= 0) ? seq[i_right] : REMOVED;

            // ── Remove left-neighbor pair (lsym, A) from its occ list ─────────
            // Guard: if the pair is self-referential (A == B and lsym == A),
            // the left-neighbor key equals `key`, whose record is already detached.
            // Calling remove against a detached record would corrupt unrelated lists.
            if (i_left >= 0) {
                const uint32_t lk = pairKey(lsym, A);
                if (lk != key) {
                    auto lit = freq.find(lk);
                    if (lit != freq.end()) {
                        const uint32_t nc = ol.remove(i_left, lit->second);
                        if (nc >= 1) pq.push({nc, lk});
                    }
                }
            }

            // ── Remove right-neighbor pair (B, rsym) from its occ list ────────
            if (i_right >= 0) {
                const uint32_t rk = pairKey(B, rsym);
                if (rk != key) {
                    auto rit = freq.find(rk);
                    if (rit != freq.end()) {
                        const uint32_t nc = ol.remove(i2, rit->second);
                        if (nc >= 1) pq.push({nc, rk});
                    }
                }
            }

            // ── Update sequence: C replaces A at i; B at i2 is removed ────────
            seq[i]  = C;
            seq[i2] = REMOVED;       // sentinel so later snapshot entries skip i2
            sl_nxt[i] = i_right;
            if (i_right >= 0) sl_prv[i_right] = i;

            // ── Add new left pair (lsym, C) at position i_left ───────────────
            // freq[] operator[] may trigger rehash here; no freq references are
            // held from the removal steps above (they were scoped to their blocks).
            if (i_left >= 0) {
                const uint32_t lk = pairKey(lsym, C);
                auto& lrec = freq[lk];
                ol.add(i_left, lrec);
                pq.push({lrec.count, lk});
            }   // lrec goes out of scope before the next freq[] call

            // ── Add new right pair (C, rsym) at position i ───────────────────
            if (i_right >= 0) {
                const uint32_t rk = pairKey(C, rsym);
                auto& rrec = freq[rk];
                ol.add(i, rrec);
                pq.push({rrec.count, rk});
            }
        }

        // Erase by key (not by iterator) — safe even if rehashes occurred above.
        freq.erase(key);
    }

    // ── Reconstruct seq from the linked list ──────────────────────────────────
    // Position 0 is always the head; follow sl_nxt until -1.
    size_t w = 0;
    for (int32_t i = 0; i >= 0; i = sl_nxt[i])
        seq[w++] = seq[i];
    seq.resize(w);
}

} // namespace QaplaRePair
