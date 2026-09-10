# Writing Qapla bitbases in the Syzygy format

The engine's own generator keeps producing what it produces; what changes is the *storage*, and
later what the generator *counts*. Target: `.rtbw` and `.rtbz` files that the Syzygy probing code
already in the engine - and any other Syzygy reader - opens, maps and probes without knowing where
they came from.

Nothing here asks for perfect compression. The files must be *correct* and *readable*; being a few
per cent larger than de Man's is irrelevant.

The authoritative description of what the stored values mean is [syzygy-probe.md](syzygy-probe.md).
This file describes the opposite direction and adds only what a writer needs.

## The route

Six steps for win/draw/loss, each one ending in something that can be run and measured. Distance
to mate is a decision to take afterwards, not a promise made now.

| # | step | ends with |
|---|---|---|
| 1 | conversion layer for everything Qapla does differently - perspective, piece codes, value set | a function that answers "Syzygy's value of this position" from Qapla's table, no file involved |
| 2 | the file: Syzygy WDL container, still without cursed win / blessed loss | `.rtbw` files the engine's own reader probes |
| 3 | comparison against real Syzygy files, folded: their win **or** cursed win must be our win | a number - how many positions differ, and how many differ only by the fold |

Steps 1 to 3 are done for pawnless material, see section 8.1.
| 4 | distance to zeroing move in the generator - the "move counter" | a DTZ value per position, verified by its own invariants |
| 5 | cursed win / blessed loss from it, in the WDL file | `.rtbw` files with all five values |
| 6 | comparison against real Syzygy files again, now exact | zero differences |

Steps 1-3 are worth doing on their own even if nothing follows: they prove the index, the layout
and the container, which is the part that is fiddly rather than deep. Step 4 is where real work
starts, and it is also the step that makes DTM cheap later, because both need the same
level-ordered iteration.

---

## 0. What already exists

| piece | where | state |
|---|---|---|
| WDL generator (retrograde, multi-threaded) | [bitbasegenerator.cpp](../bitbase/bitbasegenerator.cpp) | works, unchanged through step 3 |
| Qapla index / reverse index | [bitbaseindex.h](../bitbase/bitbaseindex.h), [reverseindex.h](../bitbase/reverseindex.h) | index -> position and position -> index both exist |
| Re-Pair + Huffman compressor | [recursive-pairing.h](../bitbase/recursive-pairing.h) | same algorithm class as Syzygy, **different serialisation** |
| Qapla container `.qwdl` / `.btb` | [bitbase-repairfile.h](../bitbase/bitbase-repairfile.h), [bitbase-file.h](../bitbase/bitbase-file.h) | own magic, own header |
| Syzygy reader (index, decompression, mapping) | [tbprobe.cpp](../src/syzygy/tbprobe.cpp) | complete, is the reference the writer must satisfy |
| Capture resolution, en passant, the whole probe protocol | `computeWdl` in [rootmoves.cpp](../search/rootmoves.cpp#L106-L199) | exists - the comparison harness of steps 3 and 6 reuses it |
| Generator front end | `bitgenerate` in [bitbase-interface.cpp](../interface/bitbase-interface.cpp) | the place an output-format option goes |

The Re-Pair compressor is a real head start: its btree packing is already the Syzygy `LR` layout
(3 bytes, two 12-bit children), its block/sparse-index concept is the same, and `probe()` was
written along `decompress_pairs()`. What differs is the *file* it writes, not the algorithm.

**One thing is not switched off.** The compiled-in KPK bitbase is registered at every start
([chessinterface.h:159](../interface/chessinterface.h#L159)) and read by the endgame evaluation
([evalendgame.cpp:174-181](../eval/evalendgame.cpp#L174-L181)). Only the *file* path is unused.
So anything that changes `BitbaseReader`, `BitbaseResult` or the stored perspective changes how
the engine plays and needs the node-count run of `CLAUDE.md`. Everything below is arranged so that
it does not: the conversions live in a new adapter between generator and file, and the generator's
own conventions stay as they are.

---

## 1. Step 1 - the conversion layer

Three differences, all mechanical, all cheap - and the reason to do them first is not that they are
hard but that having them behind you halves the search space when the first file comes out wrong.

### 1.1 Piece encoding

Three encodings are in play and the writer touches all three:

| encoding | value | used by |
|---|---|---|
| Qapla | `type << 1 \| colour`, `WHITE_PAWN`... | generator, `PieceList`, index |
| handover | white 0..5, black 6..11 | `TbPosition`, [tbposition-builder.h](../src/syzygy/tbposition-builder.h) |
| **format** | `colour << 3 \| type`, type `PAWN=1 .. KING=6` | the nibbles stored **in the file** |

A white rook is `4` in the file, a black rook `12`; `flipColour` is a plain `^ 8`
([tbprobe.cpp:96](../src/syzygy/tbprobe.cpp#L96), [tbprobe.cpp:744](../src/syzygy/tbprobe.cpp#L744)).
The material key is 4 bits per (colour, type) with `keyIndex = type - 1 + colour * 6`
([tbprobe.cpp:115](../src/syzygy/tbprobe.cpp#L115)). One conversion header, and do not reuse the
handover code by accident - it looks close enough to be dangerous.

### 1.2 Perspective

Qapla stores from **white's** point of view - `Win` means white wins, in white-to-move and
black-to-move entries alike (see the encoding comment at
[bitbasegenerator.cpp:505-522](../bitbase/bitbasegenerator.cpp#L505-L522)). Syzygy stores from the
**side to move**. Black-to-move entries are negated on the way out.

This negation belongs in the adapter, not in the generator. Moving it into the generator would
change `.btb` / `.qwdl` and with them the live KPK path, for no gain.

### 1.3 Value set

Stored byte = `wdl + 2`: `0 = loss, 1 = blessed loss, 2 = draw, 3 = cursed win, 4 = win`
(`mapScore` for WDL is `value - 2`, [tbprobe.cpp:661](../src/syzygy/tbprobe.cpp#L661)). Qapla's
`BitbaseResult` is `Draw = 0, Win = 1, Loss = 2, Unknown = 3` - a table maps it, with `1` and `3`
unreachable until step 5.

`Unknown` marks positions the index can express but the board cannot reach. They are don't-care;
the compressor already treats them as jokers
([recursive-pairing.h:29-32](../bitbase/recursive-pairing.h#L29-L32)). Keep that.

### 1.4 What step 1 delivers

`syzygyValueOf(position)` - Qapla's table, read through the adapter, answering in Syzygy's value
set from the side to move. **And with it the first comparison, before a single file exists:** run
it against real Syzygy files through `computeWdl` ([rootmoves.cpp](../search/rootmoves.cpp#L106-L199))
over the whole index space of a small material, folded as in step 3.

That test is the whole point of doing step 1 separately. It separates two failure classes that are
otherwise indistinguishable: *my values are wrong* and *my file is wrong*. Once it is green, every
later difference is a format bug.

Two paths cannot be loaded at once - `setPath` and the table registry are global
([tbprobe.cpp:1241](../src/syzygy/tbprobe.cpp#L1241)). Probe the whole list with one path, buffer
the answers, switch, probe again, compare.

**The piece list is not part of this step.** Which order `pieces[]` has in the file is a layout
choice of the container, not a property of the generator - it belongs to step 2, section 3.

---

## 2. The index problem, and the cheap way around it

A Syzygy file stores its values in *Syzygy index order*. The generator produces them in *Qapla
index order*. The two orders share nothing.

The expensive route is writing the inverse of `doProbeTable` (index -> position). Do not take it.

**Take the forward direction, which already exists.** `doProbeTable`
([tbprobe.cpp:724-878](../src/syzygy/tbprobe.cpp#L724-L878)) computes, from a position, exactly the
`idx` the value has to be written at. Split it:

```
encodeIndex(layout, position) -> { stm, file, idx }     // pure, no file access
probe(...)                    -> encodeIndex + decompressPairs + mapScore
```

Then the writer becomes a scatter pass:

```
for every Qapla index i of the table:
    position = ReverseIndex(i, pieceList)          // already exists
    if illegal -> skip
    (stm, file, idx) = encodeIndex(layout, position)
    slot[stm][file][idx] = syzygyValueOf(i)        // step 1's adapter
```

Two properties make this sound:

- `encodeIndex` canonicalises internally (colour flip, rank/file flip, diagonal flip, group
  sorting), so *any* representative of a symmetry class lands on the same `idx`. It does not matter
  that Qapla's canonical representative differs from de Man's.
- Every legal position has a Qapla index, so every reachable slot is written.

Slots that stay unwritten are positions no legal placement reaches (broken positions). They are
don't-care and get the joker treatment.

**Coverage is the first test, not an afterthought:** count written slots per `(stm, file)` and count
double writes with conflicting values. A conflicting double write means the two index schemes
disagree about a symmetry - find that before compressing anything.

### 2.1 What `encodeIndex` needs from the layout

`groupIdx[]` / `groupLen[]` come from `setGroups`
([tbprobe.cpp:892-930](../src/syzygy/tbprobe.cpp#L892-L930)), which is driven by `pieces[]` and
`order[]` - both of which the *writer* chooses and then stores in the file. So the writer runs the
identical `setGroups` on its chosen layout and hands the resulting `PairsData` to `encodeIndex`.
Refactor `setGroups` out of the reader instead of copying it; two divergent copies of that loop is
how this project would lose a week.

---

## 3. Layout decisions the writer has to make

Free parameters of the format. Chosen once, stored in the file, used by `encodeIndex`.

- **Which colour is "white" in the file.** The file name is `<strong>v<weak>`, the table is
  generated with the reference colour as the first key. Qapla's piece string (`KRPK` -> `KRPvK`)
  already carries that convention.
- **`hasPawns`, `hasUniquePieces`, `pawnCount[]`** are *not* choices - they follow from the
  material ([tbprobe.cpp:454-479](../src/syzygy/tbprobe.cpp#L454-L479)). Derive them with the same
  code, or the reader disagrees about the group sizes.
- **`pieces[]` order per `(side, file)`.** Constraints, not preferences:
  - with pawns, the reference colour's pawns come first - the index computation reads `pieces[0]`
    to find the pawn colour and asserts it
    ([tbprobe.cpp:751-758](../src/syzygy/tbprobe.cpp#L751-L758));
  - without pawns, the leading group is 3 pieces when `hasUniquePieces`, else the 2 kings;
  - **the third entry of the leading group must be a piece that occurs exactly once.** The
    leading-group formula treats `squares[0..2]` as distinguishable; putting one of two identical
    rooks there makes the encoding ambiguous. `KRRvKN` has `hasUniquePieces == true` (the knight),
    so the layout must read `K K N R R`, never `K K R R N`.
  - pieces of the same type and colour must be adjacent - `setGroups` builds groups by equality of
    neighbours.
- **`order[]`** (leading group / remaining pawns in the multiplication chain). Any self-consistent
  value works; start with the simplest and do not tune it.
- **Sides.** `Sides == 2` for WDL: when `key != key2` the file holds a white-to-move and a
  black-to-move table and the `Split` flag is set. When `key == key2` (`KRvKR`, `KQPvKQP`, ...) only
  the white-to-move table exists and the reader mirrors. Then the writer must *check*, not assume:
  the black-to-move value of a position must equal the white-to-move value of its mirror. A free
  consistency test of the whole pipeline.
- **Files.** With pawns there are four tables per side, split by `edgeDistance(file)` of the leading
  pawn. `encodeIndex` returns which one.

---

## 4. Step 2 - compression and container

### 4.1 Keep the algorithm, replace the serialisation

`QaplaRePair::compress()` stays. `QaplaRePair::serialize()` cannot be used - the byte stream is
Qapla's own. The deltas, all mechanical:

| item | Qapla today | Syzygy needs |
|---|---|---|
| symbol numbering | original grammar ids plus a `symOrder[]` indirection | ids **renumbered** so that every Huffman length class is a consecutive id range; `lowestSym[len]` is the lowest id of that class, no indirection table exists |
| decode table | `symCount[]` written, `base64[]` derived | `lowestSym[]` written, `base64[]` derived from it ([tbprobe.cpp:975-990](../src/syzygy/tbprobe.cpp#L975-L990)) |
| symbol count | implicit | explicit `uint16` before the btree |
| terminals | `NUM_TERMINALS` fixed leaves | a leaf is a rule with `right == 0xFFF`; its `left` is the value ([tbprobe.cpp:934-947](../src/syzygy/tbprobe.cpp#L934-L947)) |
| sparse index | derived at load | **stored**, 6 bytes per entry (`uint32` block, `uint16` offset), `ceil(tbSize / span)` entries |
| block lengths | `blocksNum` entries | `blocksNum + padding` entries, `padding` stored as its own byte |
| block data | packed | aligned to **64 bytes** before each `(side, file)` block array |
| header | own magic + entry count | section 4.2 |

The renumbering is the only part that is more than bookkeeping: once the Huffman code lengths are
known, symbols are sorted by length, given new consecutive ids in that order, and the btree children
plus the encoded stream are rewritten through the permutation. A separate, testable pass
(`renumberForSyzygy`), verified by round-tripping through the *reader's* `setSizes` +
`decompressPairs` before a single file is written.

Block size and span stay at the Syzygy convention (`log2` bytes each in the header). The reader
wants `blockLength[b] + 1` terminals per block and each block padded to `sizeofBlock` - the shape
the compressor already produces.

### 4.2 The container, byte for byte

Both file types share everything except the magic and the map section. Order as `setTable`
([tbprobe.cpp:1033-1094](../src/syzygy/tbprobe.cpp#L1033-L1094)) walks it:

```
  4  magic                     WDL: 71 E8 23 5D      DTZ: D7 66 0C A5
  1  flags                     bit0 Split (key != key2), bit1 HasPawns,
                               upper nibble the piece count
     for f in 0..maxFile:                       maxFile = hasPawns ? 3 : 0
  1    order                   low nibble side 0, high nibble side 1
 (1)   order2                  only when pawns on both sides (remaining-pawn group)
  n    pieces[pieceCount]      low nibble side 0, high nibble side 1, format encoding
     pad to even
     for f, for side:          setSizes block
  1      flags                 STM | Mapped | WinPlies | LossPlies | Wide | SingleValue
         if SingleValue: 1 byte value, nothing else
  1      log2(sizeofBlock)
  1      log2(span)
  1      padding               blockLengthSize - blocksNum
  4      blocksNum             uint32 LE
  1      maxSymLen
  1      minSymLen
  2*k    lowestSym[]           k = maxSymLen - minSymLen + 1, uint16 LE
  2      symbol count          uint16 LE
  3*s    btree                 LR, 12 bits left / 12 bits right
         pad to even
     DTZ only: map sections    4 per f, each length-prefixed (byte, or uint16 when Wide)
     for f, side: sparseIndex  6 bytes * ceil(tbSize / span)
     for f, side: blockLength  2 bytes * (blocksNum + padding)
     for f, side: pad to 64, then blocksNum * sizeofBlock bytes of block data
```

`tbSize` is `groupIdx[]` at the zero terminator of `groupLen[]` - the writer gets its own table size
out of `setGroups`, another reason to share that function rather than copy it.

File names: `KRPvK.rtbw`, `KRPvK.rtbz`. The reader registers a DTZ file only when the matching WDL
file exists ([tbprobe.cpp:571](../src/syzygy/tbprobe.cpp#L571)).

### 4.3 Acceptance for step 2

1. **Coverage** - every slot written exactly once, no conflicting double write.
2. **Self round-trip** - map the file with the engine's own reader, probe *every* index, compare
   against the in-memory array. Same shape as the existing `.qwdl` verification loop at
   [bitbasegenerator.cpp:820-846](../bitbase/bitbasegenerator.cpp#L820-L846). Jokers skipped.

Step 2 is *not* proven by these two. They compare the file with the array it was written from; an
index scheme that is self-consistent but not de Man's passes both. That is what step 3 is for.

Test material, cheapest first: `KQvK`, `KRvK` (pawnless, unique pieces), `KRvKR` (symmetric key,
single side), `KPvK` (pawns, file split), `KRRvKN` (identical pieces plus a unique one - the layout
constraint of section 3), `KPPvKP` (pawns on both sides, `order2`).

---

## 5. Step 3 - the folded comparison

Both tables answer the same question - the game-theoretic value ignoring the 50 move rule - so the
relation is exact, not approximate:

```
sgn(ours) == sgn(theirs)        for every legal position, always
```

Written out: their `win` or `cursed win` must be our `win`, their `loss` or `blessed loss` our
`loss`, their `draw` our `draw`. **Any sign difference is a bug, not a tolerance.** There is no
"mostly agrees" outcome to accept here.

Both sides of the comparison are resolved values, not raw entries: a Syzygy entry is a lower bound,
so it must go through capture resolution before it means anything - `computeWdl` in
[rootmoves.cpp](../search/rootmoves.cpp#L106-L199) is that protocol and is reused as it stands.

Two numbers come out and both belong in the log:

- positions with a sign difference - must be zero;
- positions where they say cursed win / blessed loss and we say win / loss - the exact size of the
  50-move gap, and the work list for step 5.

A third check that costs nothing and proves the format rather than the index: read the same file
with `python-chess`, which parses `.rtbw` in pure Python and shares no line of code with this
engine. If it agrees, "drop-in" is proven rather than asserted.

---

## 6. Step 4 - the distance to the zeroing move

This is the step where the generator changes, and it is bigger than "add a counter".

**What cursed win means.** Win, but drawn under the 50 move rule - and the criterion is
`DTZ > 100`, the distance to the next *zeroing* move (capture or pawn move), not a count of moves
made during generation. So the value needed is a distance per position, computed like the table
itself: retrograde, over the whole index space.

**Why DTZ and not DTM.** Distance to zero stops at the next capture or pawn move, where the sub
position is looked up fresh and the counter restarts. So the iteration needs the **WDL** of the
subordinate tables and nothing more - exactly what already exists. Distance to mate would need the
*DTM* of the subordinate tables, which do not exist yet. DTZ is therefore the cheaper and
self-contained one, and it is the right one for this purpose.

**Two properties of the current loop are in the way** ([bitbasegenerator.cpp:442-473](../bitbase/bitbasegenerator.cpp#L442-L473)):

- **Passes are not ply-synchronised.** A candidate resolved by one thread in pass *n* is visible to
  another thread in the *same* pass (`_computedResults` is written immediately,
  [bitbasegenerator.cpp:392-432](../bitbase/bitbasegenerator.cpp#L392-L432)). For a win/draw/loss
  bit that is harmless, it only converges faster. For a distance it is fatal: the same position gets
  a different number depending on thread interleaving. The pass has to become **double-buffered** -
  read the previous level, write the next one, apply at the end of the pass.
- **A zeroing move is a boundary, not a step.** A winning capture or pawn move ends the count at the
  move itself; the position after it starts a new count in its own table. The initial pass already
  consults the subordinate tables through captures and promotions
  ([bitbasegenerator.cpp:493-540](../bitbase/bitbasegenerator.cpp#L493-L540)) - that is the right
  place, it now has to record a distance of one instead of a bit.

Do the double-buffering as its own change and prove it changes nothing: the WDL result of every
generated table bit-identical to today's. Only then add the distance.

**Invariants that cost nothing and catch nearly everything:** a won position must have a move to a
lost position with `DTZ - 1`, or be a zeroing move with `DTZ == 1`; a lost position's `DTZ` is the
maximum over its moves; `DTZ == 0` exactly for draws.

**The by-product.** After this step the values of a real `.rtbz` file exist. Writing them is the
container of section 4.2 with the DTZ magic and the map sections - small, next to what step 5 needs
anyway, and it gives the engine the root ranking of [syzygy-probe.md](syzygy-probe.md) for its own
tables. Worth taking, but it is a separate decision, not part of the WDL route.

---

## 7. Step 5 - the two remaining values

`win && dtz > 100 -> CursedWin`, `loss && dtz < -100 -> BlessedLoss`, everything else unchanged.
The stored byte set of section 1.3 is then complete.

One honest caveat: whether a table that is built by this rule matches de Man's *everywhere* is not
something to assume in advance. The rule is what the format documents, and the propagation case - a
win whose continuations are all cursed - is where the format's own approximation lives. Implement
the documented rule, then let step 6 say whether it agrees. If it does not, the diff list is small
and concrete, which is the best position to investigate from.

## 8. Step 6 - the exact comparison

Same harness as step 3, fold removed: `ours == theirs` for every legal position, all five values.
Zero differences, or a list of positions to explain. Then the WDL part is done.

---

## 8.1 What the first table taught

`KRvK` is written and compared. What had to be found out on the way, so that the next material
does not have to find it again:

- **`initMaps()` was reader-only.** The map tables are filled when a path is set, and a writer
  never sets one - every index came out zero. Both directions now go through `ensureMaps()`.
- **The `.qwdl` file cannot say which entries are illegal.** The compressor treats
  `BitbaseResult::Unknown` as a joker and returns a neighbour's value instead
  ([recursive-pairing.h:29-32](../bitbase/recursive-pairing.h#L29-L32)), so an illegal entry comes
  back looking like an ordinary one. The writer re-establishes legality the way the generator does -
  `ReverseIndex::isLegal()`, the position legal, and the index the canonical one of its class
  ([bitbasegenerator.cpp:713-729](../bitbase/bitbasegenerator.cpp#L713-L729)). Without that test
  the joker values land in the file: 85 slots of `KRvK` were filled twice with values that
  contradicted each other, every one of them from a non-canonical index.
- **The double-write check earned its place.** It is what turned both faults above from a wrong
  file into a message naming two positions - and the two it named were mirror images of each
  other, which said immediately that the fault sat on the Qapla side of the index, not in the
  format.
- **Read the reference file before writing one.** `KRvK.rtbw` is 208 bytes and shows the whole
  shape: the white to move table is a single value (every position a win, flag `SingleValue`), the
  black to move table is two terminals with a one bit code each, and the file ends in a 16 byte
  trailer the reader never looks at - a checksum. It also settles the free choices: de Man puts the
  kings first in one of the two tables and not in the other, so the piece order really is free
  within the constraints of section 3.
- **The flags byte carries the piece count** in its upper nibble. This prober ignores it, others
  may not, so it is written.

Numbers of the run: 57288 indexed positions, 7273 of them illegal, 50015 values placed in 62664
slots, 12649 slots never reached and filled with a neighbour. The file is 5648 bytes against de
Man's 208 - that difference is the recursive pairing grammar, which is not built here. All 399112
legal positions of the material give the same resolved value as the reference, and the value
distribution matches entry for entry: 201700 losses, 22244 draws, 175168 wins, no cursed win in
this material.

Still open from the ladder of section 4.3: the file has not been read by a foreign
implementation - `python-chess` is not installed here.

## 9. Afterwards: distance to mate

Only worth starting once step 6 is green, and cheaper then than it looks now, because the machinery
it needs - the double-buffered level iteration - was built in step 4.

**What is different from DTZ.** A conversion does not cost one ply: a winning capture is worth
`1 + DTM(child)`, which can be 40 ply, not 1. So the subordinate tables must be **DTM** tables, and
a position seeded from a conversion belongs in the bucket `1 + DTM(child)`, not on level 1. The
loop becomes value iteration in increasing DTM order:

```
level 0 : mates (side to move is mated)   -> DTM 0, a loss for the side to move
level n : WIN in n   if some move reaches a LOSS in n-1
          LOSS in n  if every move reaches a WIN in <= n-1, max = n-1
seeds   : conversions enter bucket 1 + DTM(child)
```

Storage grows to 16 bits per entry, which is the dominant memory cost and decides how far the
generator reaches. Sanity checks: DTM of a won position is odd, of a lost position even; every won
position has a move to a lost child with `DTM - 1`.

**Storing it in the DTZ container.** Same file shape, different meaning:

- **Value classes.** The map has four sections selected by the WDL value: `win, loss, cursed win,
  blessed loss` (`WDLMap = {1,3,0,2,0}`, [tbprobe.cpp:665](../src/syzygy/tbprobe.cpp#L665)). With
  step 5 done, all four can be filled.
- **Plies or moves.** `WinPlies` / `LossPlies` decide whether the reader doubles the value, and it
  adds one unconditionally: `returned = (mapped [*2]) + 1`
  ([tbprobe.cpp:663-684](../src/syzygy/tbprobe.cpp#L663-L684)). With both plies flags set,
  `stored = DTM - 1`.
- **The byte domain is the real limit.** The value out of the compressed stream indexes the map
  section, so at most 256 *distinct* values per class in the standard reading. Five-piece DTM maxima
  sit around 130 moves, right at that edge; six-piece maxima are far past it. The writer measures
  the maximum first and picks the encoding from it - and refuses rather than truncating silently.
  Escapes, in order: `Mapped | Wide` (map entries become `uint16`, enough while the number of
  distinct values is ≤ 256); or unmapped values up to 4095, which the 12-bit terminals allow
  ([tbprobe.cpp:191-201](../src/syzygy/tbprobe.cpp#L191-L201)) but `rtbgen` never emits, so a
  foreign reader may not follow.
- **One side only.** `TBTable<DTZ>::Sides == 1` ([tbprobe.cpp:429](../src/syzygy/tbprobe.cpp#L429)):
  a distance table stores one side to move, announced by the `STM` flag, and the reader answers
  `OtherSideToMove` for the other. A property of the format, not something the writer works around.

**What "compatible" cannot mean.** The *file* is compatible - the same `setTable`, `setSizes`,
`decompressPairs` and `mapScore` read it. The *corrections* a reader applies on top of a DTZ entry
are not DTM semantics:

- the zeroing shortcuts (a winning capture or a quiet pawn move ends the probe at `±1`) are true for
  distance-to-zero and false for distance-to-mate;
- the `OtherSideToMove` recovery by a one-ply search is structurally right for DTM, but its pawn-move
  part is not;
- the 50 move accounting (`dtz + cnt50 <= 99`, rank 900, the `±100` band) has no meaning for DTM;
- the documented off-by-one tolerance of DTZ is not acceptable for a mate distance.

So the engine needs its own thin DTM layer next to the DTZ one in
[rootmoves.cpp](../search/rootmoves.cpp) - for the stored side no correction at all, for the other
one ply of search that follows captures and pawn moves into the sub-table instead of short-cutting
them.

**Where the files live.** A DTM file in a `.rtbz` is indistinguishable from a real DTZ file by name,
and mixing them in one directory silently poisons a Syzygy installation - the more so once step 4
produces real DTZ files of our own. Own directory, and a second path option in the engine. A
distinct extension is a one-line change in `mapped()`
([tbprobe.cpp:1130](../src/syzygy/tbprobe.cpp#L1130)) but costs the drop-in property for foreign
readers, which is the point of the exercise: directory, not extension.

---

## 10. Open decisions

- Whether the WDL writer should fold exact values down to lower bounds where a capture already
  reaches them. It compresses better and is legal - the reader takes `max(entry, best capture)`, so
  an exact entry can never be raised. It also makes the step 3 comparison harder to read.
  Recommendation: not before the files are proven correct.
- Whether to write real `.rtbz` DTZ files as the by-product of step 4 (section 6), and when.
- Which side to store per distance table - the format allows one; the stronger side to move is the
  natural choice, but the one-ply recovery for the other side has to be written either way.
- Whether DTM above five pieces is worth doing at all, given the value range of section 9 and 16
  bits per entry.
