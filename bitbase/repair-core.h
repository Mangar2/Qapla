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
 *
 * @Overview
 * O(N log N) Re-Pair core: single-pass iterative pair replacement using
 * a doubly-linked sequence list and an incrementally maintained frequency
 * table, avoiding the O(N) full rescan of the naive per-iteration approach.
 */

#pragma once

#include <cstdint>
#include <vector>

#include "recursive-pairing.h"   // PairRule, MAX_VOCAB_SIZE

namespace QaplaRePair {

/**
 * @brief Run the complete Re-Pair grammar induction in O(N log N).
 *
 * Replaces the old  while (repairIteration(...)) {}  loop.
 *
 * Data structures:
 *   - Doubly-linked list over the sequence so that removing an absorbed
 *     position is O(1) instead of shifting the array.
 *   - Hash map from pair key → {count, occurrence-list head} maintained
 *     incrementally: only the O(freq(P)) neighbor pairs of the replaced
 *     pair P are updated per iteration, not the whole sequence.
 *   - Max-heap with lazy deletion: stale (count, key) entries are discarded
 *     on pop when the stored count no longer matches the live count.
 *
 * Complexity:
 *   Build phase  O(N)       — one pass to fill freq table and occ lists.
 *   Replace loop O(N log N) — each of the N absorbed positions triggers
 *                             O(1) freq updates and O(log N) heap ops.
 *
 * @param seq        In/out: symbol sequence, replaced in-place with the
 *                   compressed stream on return.
 * @param btree      Out: grammar rules appended in replacement order.
 * @param nextSymbol In/out: next available symbol index; incremented per rule.
 * @param maxVocab   Upper bound (exclusive) for nextSymbol; stops when reached.
 * @param maxSymlen   Skip any pair whose combined symlen+1 would exceed this value.
 *                    Limits grammar depth and drastically reduces btree size for
 *                    mostly-uniform data (e.g. KQK).  Use UINT32_MAX for no cap.
 * @param minPairFreq Stop replacing pairs once the most frequent remaining pair
 *                    appears fewer than this many times.  Each rule costs ~5 bytes
 *                    of serialized overhead (3 bytes btree + 2 bytes symOrder); at
 *                    1 bit/symbol the break-even frequency is ~40.  Default 2
 *                    preserves the original "replace everything" behaviour.
 */
void repairFull(
    std::vector<uint16_t>& seq,
    std::vector<PairRule>&  btree,
    int&                    nextSymbol,
    int                     maxVocab,
    uint32_t                maxSymlen,
    uint32_t                minPairFreq = 2);

} // namespace QaplaRePair
