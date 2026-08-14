# Syzygy tablebase support

Plan for item 18 of [todo.md](todo.md). Written in English like the rest of `plan/`.

Two things are built here, in this order:

1. **WDL as bitbase replacement** — the Syzygy `.rtbw` files answer win/draw/loss and are used in
   exactly the place Qapla's own bitbases are used today. The own bitbases are switched off
   completely behind a compile flag before this starts.
2. **DTZ at the root only** — the `.rtbz` files are probed at ply 0/1, only to pick the move that
   is actually played. Never inside the search.

The probing code comes from [syzygy1/probetool](https://github.com/syzygy1/probetool),
directory `regular/`. License there: *"do whatever you want with it"*.

---

## 0. The FEN objection does not apply

`probetool.c` takes a FEN, and that would indeed be far too slow — but `probetool.c` is only the
command line driver. The library is `tbprobe.c` / `tbprobe.h`, and it never sees a FEN. It works
through an abstract `TB_Position` whose struct **is not defined by the library**: the caller
defines it and implements 18 small functions on it (`tbprobe.h`, section *"Functions required by
tbprobe.c"*).

`tbinterface_bb.c` and `tbinterface_0x88.c` are two *example* implementations that carry their own
board and their own move generator — those are the ones with `TBitf_set_from_fen()`. Ronald de Man
writes in `tbinterface_bb.c` itself:

> For best performance, engine authors may consider re-implementing the `TB_Position` struct and
> the `TB_...()` functions required by `tbprobe.c` using their own board representation and move
> generation routines.

That is what we do: `TB_Position` becomes a thin wrapper around Qapla's
[MoveGenerator](../movegenerator/movegenerator.h), and no FEN, no second board and no second move
generator ever exist. `tbinterface_bb.c` and `tbinterface_0x88.c` are **not** taken into the
engine — they are only kept outside the repo, as the oracle for the cross-check in section 7.

---

## 1. Switch the own bitbases off

One flag in a central header, e.g. `bitbase/bitbase-config.h`:

```cpp
// Qapla's own bitbases. Undefined: the engine plays without any bitbase, the
// tablebase code of syzygy/ takes over. Define it to get them back.
// #define QAPLA_USE_OWN_BITBASES
```

Undefined by default, as requested. Every *probing* site becomes a no-op:

| site | change |
| --- | --- |
| [chessinterface.h:159](../interface/chessinterface.h#L159) | `registerBitbaseFromHeader()` — no KPK from the header |
| [boardadapter.h:100-119](../search/boardadapter.h#L100-L119) | `qaplaBitbasePath` / `…PathNL` / `…Cache` accepted and ignored |
| [uci.cpp:82-83](../interface/uci.cpp#L82-L83) | the two `qaplaBitbase…` options no longer announced |
| [winboard.cpp:73](../interface/winboard.cpp#L73) | `feature egt=qaplaBitbases` dropped |
| [evalendgame.cpp:149](../eval/evalendgame.cpp#L149) | `getFromBitbase` not compiled, `registerBitbase` empty |
| [search.cpp:83](../search/search.cpp#L83) | `hasBitbaseCutoff` — already a dead `return false`, gets the flag anyway |

What stays compiled regardless of the flag: the **generator and the verifier**
(`bitbase/bitbasegenerator.*`, `bitbase/verify.h`, the `bitgenerate` / `bitverify` commands in
[bitbase-interface.cpp](../interface/bitbase-interface.cpp)). They are offline tooling, they do
not touch how the engine plays, and the generated bitbases are needed as the reference in the
cross-check of section 7. `BitbaseReader` therefore keeps compiling too; only the calls into it
from eval, search and the interfaces disappear.

This step changes play: KPK and the whole `K*K*` set are gone. The EPD node count must differ, and
it is expected to be **weaker** until section 5 is in. It gets its own tag and its own entry in
[version-log.md](version-log.md), and the honest number for the whole item comes from the closing
run in section 9 — not from this one.

---

## 2. Vendor the probing code

New directory `syzygy/`:

```
syzygy/tbprobe.c        verbatim from probetool/regular/tbprobe.c
syzygy/tbprobe.h        verbatim from probetool/regular/tbprobe.h
syzygy/README.md        origin, upstream commit hash, license, "do not edit"
```

Taken **unmodified**, so an upstream fix is a file copy. `tbprobe.h` already carries
`extern "C"` guards.

It stays a `.c` file compiled by the C compiler: it is C11 and uses `<stdatomic.h>`
(`atomic_bool ready[3]` at line 227, acquire/release around the lazy table mapping at line 1294)
plus `pthread`/Win32 mutexes. Translating that to C++ would mean editing vendored code for no
gain. The `.c` path is already established — [bitbase/lz4.c](../bitbase/lz4.c) and
[bitbase/miniz.c](../bitbase/miniz.c) are built the same way.

`tbprobe.c` handles Windows and POSIX itself (`CreateFileMapping` vs. `mmap`), so nothing has to
be added for either platform.

### Build files

- **[Makefile](../Makefile)** — nothing to do. `SRC_C` globs `*.c` and `CFLAGS` already exist for
  both the Windows (`clang-cl`) and the Unix branch. Only check once that `clang-cl` accepts
  `<stdatomic.h>`; if it does not, add `-std:c11` to `CFLAGS_BASE` in the Windows section.
- **[CMakeLists.txt](../CMakeLists.txt)** — `file(GLOB_RECURSE SOURCES … "*.cpp")` misses `.c`
  entirely, so `lz4.c` and `miniz.c` are missing there today as well. Add a second glob for `*.c`
  and append it to `SOURCES`.
- **[Qapla.vcxproj](../Qapla.vcxproj)** — files are listed explicitly, so `syzygy/tbprobe.c` needs
  its own `ClCompile` entry (and `tbprobe.h` a `ClInclude` entry).

---

## 3. The interface layer — `TB_Position` on Qapla's board

New files, C++, compiled into the engine:

```
syzygy/tb-position.h      the TB_Position struct
syzygy/tb-interface.cpp   the 18 TB_* functions, in extern "C"
```

### The struct

`tbprobe.c` recurses: `probe_ab` generates captures, does a move, and inside that move generates
captures again. So the move list has to be a stack of frames, one per ply — exactly what
`tbinterface_bb.c` does with `state[idx].firstMove`.

```cpp
struct TB_Position {
    QaplaMoveGenerator::MoveGenerator* board;   // borrowed, never copied

    struct Frame {
        QaplaBasics::Move  moves[MAX_MOVES];    // captures first, then quiets
        int                numCaptures;
        int                numQuiets;
        bool               generated;
        Layout             layout;              // CAPTURES | QUIETS | ALL
        QaplaBasics::Move  playedMove;          // for undo
        QaplaBasics::BoardState boardState;     // for undo
    };
    Frame frames[MAX_DEPTH];                    // 8 is enough, use 16
    int   idx;
};
```

Depth: the recursion is bounded by the number of captures available, i.e. by the piece count
minus two. 16 frames are far beyond what a 7-piece tablebase can reach.

One instance is enough — the search is single threaded
([iterativedeepening.h](../search/iterativedeepening.h) holds exactly one `Search`, and
`ThreadPool` is only used by [perft.h](../search/perft.h)). It lives as a member of the object
that owns the probing (section 4), not as a global, so SMP later only needs one instance per
thread.

### The three traps in the move generation

**a) The capture/quiet split is not Qapla's silent/non-silent split.**
[movelist.h:88-95](../basics/movelist.h#L88-L95): `addPromote` puts the queen promotion into the
non-silent region and the rook, bishop and knight promotions into the *silent* region — including
when they are captures. Syzygy needs every capture, underpromotion captures included, in the
capture block. So: generate once with `genMovesOfMovingColor`, then partition the frame by
`move.isCapture()` ourselves. `nonSilentMoveAmount` must not be used.

**b) `TB_generate_quiets(pos, 0)` means quiets only.**
`tbprobe.c` calls it that way at lines 1502, 1564, 1691, 1729 — the capture list generated a
moment earlier at the same frame is thrown away. With `start == numCaptures` (line 1584) the
quiets are appended behind the captures instead. Hence the `layout` field: generate all moves once
per frame, then index

| layout | move `m` is | returned count |
| --- | --- | --- |
| `CAPTURES` (after `generate_captures`) | `moves[m]` | `numCaptures` |
| `QUIETS` (after `generate_quiets(0)`) | `moves[numCaptures + m]` | `numQuiets` |
| `ALL` (after `generate_quiets(numCaptures)`) | `moves[m]` | `numCaptures + numQuiets` |

**c) Castling must never appear.**
[movegenerator.cpp:648-656](../movegenerator/movegenerator.cpp#L648-L656) adds castling in
`genMoves`. Syzygy tables know nothing about castling rights. Probing is refused at the entry
point when any castling right is still set (section 4), and since castling rights only ever
decrease, no descendant can generate one either. The filter in the interface is an `assert`, not
a runtime branch.

### The 18 functions

| function | implementation on `MoveGenerator` |
| --- | --- |
| `TB_material_key` | sum of `matKey[]` over the pieces, table copied from `tbinterface_bb.c` |
| `TB_material_key_from_counts` | same table over the counts |
| `TB_material_string` | `KQPvKRP` from the piece bitboards |
| `TB_list_squares` | walk `pt[]`, read `getPieceBB(piece)`, `^0x38` on the square when `flip` |
| `TB_generate_captures` | generate frame, `layout = CAPTURES`, return `numCaptures` |
| `TB_generate_quiets` | set `layout`, return per the table above |
| `TB_move_is_legal` | `TB_do_move` + `TB_undo_move` |
| `TB_do_move` | save `getBoardState()`, `board->doMove(move)`, `++idx`; `board->isLegal()` false → undo, return false |
| `TB_undo_move` | `--idx`, `board->undoMove(playedMove, boardState)` |
| `TB_white_to_move` | `board->isWhiteToMove()` |
| `TB_bare_kings` | popcount of all pieces `== 2` |
| `TB_in_check` | `board->isInCheck()` |
| `TB_has_en_passant` | `board->getEP() != NO_SQUARE` — false positives are explicitly allowed |
| `TB_move_is_ep` | `move.isEPMove()` |
| `TB_move_is_pawn_move` | `isPawn(move.getMovingPiece())` |
| `TB_no_legal_moves` | generate frame, `TB_move_is_legal` over all |

Square encoding needs no conversion: Qapla's [types.h:53](../basics/types.h#L53) is `A1 = 0 … H8 =
63`, the same mapping Syzygy uses (`^0x38` flips the rank in both).

Piece encoding does need a table. Syzygy: `PAWN = 1 … KING = 6`, colour in bit 3. Qapla
[types.h:184](../basics/types.h#L184): `PAWN = 2 … KING = 12`, colour in bit 0. Two `constexpr`
arrays of 16 entries, one per direction.

`TB_do_move` verifies with `board->isLegal()`. That recomputes the attack masks a second time
(`doMove` already computed them) — correct but not free. It is the safe version for the first
build; if the probe turns out to be hot, the check can be narrowed later to "is the king of the
side that just moved attacked", which is what `isLegal()` does after the recompute.

---

## 4. Loading and the UCI options

New class `QaplaSyzygy::TablebaseReader` in `syzygy/tablebase-reader.h/.cpp`, the counterpart of
[BitbaseReader](../bitbase/bitbase-reader.h) and the only place that includes `tbprobe.h`.

```cpp
static bool setPath(const std::string& path);     // calls TB_init(), reports table counts
static void release();                            // TB_release() between games
static int  maxCardinality();                     // TB_MaxCardinality[TB_WDL]
static bool isProbeable(const MoveGenerator&);     // piece count, castling rights, cardinality
static WdlResult probeWdl(MoveGenerator&, bool& success);
static int  probeDtz(MoveGenerator&, bool& success);
```

`isProbeable` is the single guard, used by every caller:

- piece count `<= maxCardinality()`, and `maxCardinality() > 0` at all (no path set → always false)
- **no castling right set** — Syzygy assumes there are none
- the 50-move counter is *not* part of it: WDL ignores it, and the root code in section 5 handles
  it explicitly

UCI options, standard names so that qet, cutechess and every GUI set them without configuration:

```
option name SyzygyPath type string default <empty>
option name SyzygyProbeLimit type spin default 7 min 0 max 7
```

`SyzygyProbeDepth` and `Syzygy50MoveRule` are deliberately left out for now: nothing in phase A
reads a depth (section 5 probes the root only), and the 50-move rule is handled by the DTZ ranking
rather than by an option. They go in when there is something to switch.

Wiring, mirroring what `qaplaBitbasePath` does today:

- announce in [uci.cpp](../interface/uci.cpp#L82) next to the other options
- handle in [BoardAdapter::setOption](../search/boardadapter.h#L90) — note the `else` branch there
  only runs for values that are not `"true"`/`"false"`, which is where the existing string options
  already sit; `SyzygyPath` goes in the same place
- winboard: `feature egt=syzygy`, `egtpath syzygy <path>` in
  [winboard.cpp:340](../interface/winboard.cpp#L340)
- `TB_release()` from `BoardAdapter::newGame()`

Report the result of `TB_init` as `info string` in the same style
[boardadapter.h:103](../search/boardadapter.h#L103) uses for the bitbases: number of WDL and DTZ
files found, and the maximum cardinality.

---

## 5. Phase A — WDL where the bitbases were

`getValueFromBitbase` had two shapes and both get a Syzygy counterpart:

```cpp
// win/draw/loss, from white's point of view
WdlResult probeWdl(MoveGenerator& position);
// the eval flavour: currentValue ± WINNING_BONUS, or 1 for a draw
value_t   probeWdlValue(MoveGenerator& position, value_t currentValue);
```

The value mapping is taken over unchanged from
[bitbase-reader.cpp:218-225](../bitbase/bitbase-reader.cpp#L218-L225), so only the source of the
answer changes, not what eval does with it. Syzygy's five outcomes collapse onto the three Qapla
knows:

| `TB_probe_wdl` | meaning | Qapla |
| --- | --- | --- |
| `+2` | win | `Win` |
| `+1` | cursed win, drawn under the 50-move rule | `Draw` |
| `0` | draw | `Draw` |
| `-1` | blessed loss | `Draw` |
| `-2` | loss | `Loss` |

Mapping the cursed cases to `Draw` is the conservative choice and matches what the engine can
actually achieve inside a search that does not track DTZ. It is the one place worth a second
experiment later (map them to a small ± value instead).

Hook: `EvalEndgame::getFromBitbase` at [evalendgame.cpp:149](../eval/evalendgame.cpp#L149) gets a
Syzygy twin, registered through the existing `registerBitbase` mechanism. The registration
currently happens per loaded file; here it happens once when `SyzygyPath` is set, over the same
wildcard patterns [bitbase-reader.cpp:42-46](../bitbase/bitbase-reader.cpp#L42-L46) already lists,
cut off at `maxCardinality()`. That keeps the whole lookup path — the piece signature hash of
[evalendgame.h:279](../eval/evalendgame.h#L279) — exactly as it is.

### The cost of a probe — a trade-off with a crossover, not a verdict

Both designs keep the data in RAM: Qapla explicitly in its
[ClusterCache](../bitbase/cluster-cache.h), Syzygy through `mmap` and the OS page cache, which is
the documented and intended design there. Neither one goes to disk in steady state. What differs
is *what* is cached and what a probe costs on top of it.

**Qapla caches decoded bytes, and pays a lot for a miss.**
[bitbase.cpp:100-131](../bitbase/bitbase.cpp#L100-L131): `readCluster` runs the decompressor
*before* `cache.setEntry`, so a hit is a hash probe, one byte, a shift and a mask — nothing is
cheaper than that. A miss decompresses a whole 16 KB cluster with miniz or LZ4. This is the normal
path, not an edge case: `loadBitbase(name, true)` in
[bitbase-reader.cpp:113](../bitbase/bitbase-reader.cpp#L113) attaches the file and reads only the
header, so `_loaded` stays false and every probe goes through the cluster cache.

**Syzygy caches compressed pages and pays a small, bounded amount on every probe.** It never
decodes a whole block. `decompress_pairs` (line 1202) reads the sparse index — `numIndices =
(tb_size + (1 << idxBits) - 1) >> idxBits` at line 1043, one entry per 2^`idxBits` positions —
which pins the block directly; the walk over `sizeTable` skips whole blocks, and the decode loop
inside the block is bounded by that same sparse granularity before the descent through the
re-pair symbol tree reaches the byte. The whole thing is built for random access.

So it is hit-rate arithmetic, and the crossover is a question of table size:

- **Small tables** — clusters get revisited often, the 16 KB decompression amortizes over many
  probes, and Qapla's byte-and-mask hit beats a Syzygy probe. Qapla's design wins.
- **Large tables** — reuse falls, misses dominate, and a 16 KB decompression per miss is far more
  expensive than one bounded Syzygy decode. Syzygy's design wins.

Two things push the crossover further towards Syzygy than the raw hit rate suggests. It caches
*compressed* data, so the same RAM holds many more positions. And the page cache is neither
charged to the process nor capped by `qaplaBitbaseCache` — the default is `ClusterCache
cache{511}` × 16 KB, i.e. **8 MB**, while the page cache may use whatever is free. It is also
shared *between processes*, which counts directly in an SPRT where two engine instances probe the
same files.

The one Syzygy cost with no Qapla counterpart is capture resolution: `probe_ab` (line 1411)
recurses over the captures, because the WDL tables deliberately do not store positions that have
a capture available — part of why they compress as well as they do. In the quiet positions where
eval is called there is usually no capture at all, so the multiplier is near 1 and only bites in
capture-rich nodes.

None of this predicts a problem, and the earlier draft of this section was wrong to claim a
different cost class. It also should not be over-thought: Qapla already probes bitbases from the
eval leaf today, cluster-cache misses included, and that is the shipping configuration. The
measurement that settles it is the **runtime** of the EPD run, not the node count.

If it does bite, the fallback is the shape every other engine uses: probe in the *search* instead
of in eval, at [search.cpp:83](../search/search.cpp#L83) where `hasBitbaseCutoff` already sits
ready, guarded by a depth limit and with the result written to the TT — which turns the TT into
the cache of decoded results. Separate tagged version, separate SPRT, not a silent switch.

Measure both: EPD node count (must differ — proof the code is reached) **and** the runtime of the
EPD run, which is where the probe cost shows up.

---

## 6. Phase B — DTZ at the root

Only after phase A stands.

De Man's recommendation (README, *"Tablebase root probing code"*) is not "play the DTZ optimal
move": playing DTZ optimally produces the unnatural 85-move knight shuffles his own example
shows. It is: **use the tablebase to preselect the moves that are equivalent in outcome, then let
the normal search pick among them.**

That maps onto Qapla with almost no new machinery, because the root move list is already
filterable:
[IterativeDeepening::searchByIterativeDeepening](../search/iterativedeepening.h#L105) receives
`searchMoves` and passes it straight to
[Search::startNewSearch](../search/search.h#L67) → `RootMoves::setMoves`. So the root probe
produces a `std::vector<Move>` and the rest of the engine needs no change at all.

New file `syzygy/root-probe.h/.cpp`, ported from `probetool.c` lines 192-308 (`root_probe_dtz`,
`root_probe_wdl`), with the move type changed from `int` to Qapla's `Move`:

```cpp
struct RootTbResult {
    bool                    probed;      // false → search normally
    std::vector<Move>       bestMoves;   // the highest ranked group
    value_t                 score;       // for the UCI score line
};
RootTbResult probeRoot(MoveGenerator& position, const MoveHistory& history);
```

Sequence, exactly as the README prescribes:

1. `isProbeable(position)` → otherwise `probed = false`
2. `root_probe_dtz` — for every root move: zeroing move (capture or pawn move) → `-TB_probe_wdl`
   on the child mapped through `WdlToDtz`, otherwise `-TB_probe_dtz` on the child corrected by one
   ply. This is the "ply 0/1" probing: the root position and the position after each root move,
   nothing deeper.
3. any probe fails (a DTZ file missing) → `root_probe_wdl` as fallback
4. both fail → `probed = false`, normal search

The rank formula at `probetool.c:258` needs two inputs Qapla has to supply:

- **`cnt50`** — `position.getTotalHalfmovesWithoutPawnMoveOrCapture()`
  ([board.h:121](../basics/board.h#L121)). Without it the 50-move rule handling is wrong, and that
  is precisely what turns won endgames into draws.
- **`rep`** — probetool hardcodes `false` and says so at line 222. It means *"has any position
  repeated since the last zeroing move"*; when true the engine must play strictly DTZ optimally to
  break out of the repetition. [MoveHistory](../search/movehistory.h) has
  `isDrawByRepetition(position)`, which is a different question (current position repeated). Start
  with `rep = false` like probetool, and add a proper `hasRepeatedSinceZeroing()` to `MoveHistory`
  as a follow-up — with `rep = false` the engine can shuffle in a won endgame until the 50-move
  rule bites.

Then in `searchByIterativeDeepening`:

- `probed == false` → unchanged
- `probed == true` → intersect `bestMoves` with the caller's `searchMoves` (empty means all), pass
  the result down, count `_tbHits`, and report `score` on the UCI `info` line instead of the
  search value

`bestMoves` holds *only the highest ranked group*. Every move in it is equivalent in outcome, so
the search is free to choose, which is what avoids the unnatural moves. MultiPV keeps working
because the search still sees more than one move; if `multiPV` exceeds the group size, the next
rank group can be appended later — not part of the first build.

The search itself is not touched. No probing below ply 1 in this phase, as specified.

---

## 7. Verification

Three checks, cheapest first.

**a) The interface layer against Qapla's own bitbases.** Before section 1 removes them from play
they are still generated and still correct, and they cover everything up to five pieces. A new
debug command in [BitbaseInterface](../interface/bitbase-interface.h) (`tbcompare <signature>`)
walks the bitbase index, reconstructs each position and compares `probeWdl` against
`BitbaseReader::getValueFromSingleBitbase`. Every mismatch that is not a cursed/blessed case is a
bug in the interface layer. This exercises `TB_list_squares`, the material key and the capture
generation over millions of positions and needs no external reference at all.

**b) Against probetool itself.** Build `probetool` from the upstream repo unchanged (its own
Makefile, `tbinterface_bb`) — it links its own `TB_*` symbols, so it must stay a separate binary;
there is no symbol clash as long as it is never linked into Qapla. Feed both it and a Qapla debug
command the same FEN list ([wmtest.epd](../wmtest.epd) filtered to ≤7 pieces, plus positions from
finished games) and diff WDL, DTZ and the root ranking. This is the check that catches a wrong
`cnt50` or a wrong rank formula, which check (a) cannot see.

**c) Move generation.** For every position in (b): the move count out of the TB frame must equal
the number of legal moves from `genMovesOfMovingColor` minus castling moves, and the capture
count must equal the number of moves with `isCapture()`. A one line assert in the debug build,
run over the same list.

---

## 8. Order of work

| # | step | play changes | verification |
| --- | --- | --- | --- |
| 1 | `syzygy/` vendored, builds on all three build systems, nothing calls it | no | EPD node count **identical** |
| 2 | interface layer + `TablebaseReader` + UCI options, still nobody probes | no | EPD node count **identical**, plus check 7a |
| 3 | own bitbases off behind the flag | yes | node count differs, tag, SPRT, version-log |
| 4 | phase A — WDL in eval | yes | node count differs, **watch the EPD runtime**, tag, SPRT |
| 5 | phase B — DTZ at the root | yes | node count differs, tag, SPRT |
| 6 | closing run | — | winner of 3-5 against the version before step 3 |

Steps 1 and 2 are the only ones that are behaviour neutral, and for them the rule from
[CLAUDE.md](../CLAUDE.md) applies in full: identical total node count, runtime within 5%.

Step 3 is expected to *lose* Elo on its own — it takes KPK away and puts nothing in its place yet.
It is a prerequisite, not an idea, so the `H0 → dead/` rule does not decide it. It gets its number
in the version log for the record and the item as a whole is decided by the closing run in step 6:
**Syzygy on, own bitbases off** against **own bitbases**, on a machine that actually has the
tablebase files. That is the only honest number for the feature.

Note for the SPRT runs of steps 4-6: both engines in the run must get `SyzygyPath`, otherwise the
comparison measures nothing but a missing directory. Check the `info string` line in the pgn once
at the start of each run.

## 9. Open points

- **Where the WDL probe belongs** — eval leaf (section 5, as specified) or search node with a
  depth limit and TT store. Decided by the runtime of the EPD run and then by an SPRT, not up
  front.
- **Cursed win / blessed loss** — mapped to `Draw` in the first build. A small ± value instead is
  a candidate for its own experiment.
- **`rep` in the root ranking** — `false` for now; `MoveHistory::hasRepeatedSinceZeroing()` is the
  follow-up.
- **`SyzygyProbeLimit` below the available cardinality** — the option exists from the start so
  that a 7-piece set can be capped to 5 for a test; it only narrows `isProbeable`.
- **DTM** — `TB_probe_dtm` exists in the API but the tables were never released. Ignored.
- **Shatranj** — the `shatranj/` directory of probetool is irrelevant here.
