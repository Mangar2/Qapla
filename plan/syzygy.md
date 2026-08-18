# Syzygy tablebase support

Plan for item 18 of [todo.md](todo.md). Written in English like the rest of `plan/`.

---

## Summary — read this, skip the rest unless you want the details

**The source is Stockfish's [`syzygy/tbprobe.cpp`](file:///C:/Chess/Engines/stockfish-windows-x86-64-avx2/stockfish/src/syzygy/tbprobe.cpp),
not probetool.** One self-contained C++ file, ~1770 lines, that talks to the engine's own board
class directly. Everything a probetool-based port would need — an abstraction layer, a C compiler
path, a second move list — does not exist there and is not built here either.

**What the port actually is:** copy `tbprobe.cpp` + `tbprobe.h` into `syzygy/`, then rewrite the
~15 places where they call `Position` so they call `MoveGenerator` instead. Roughly:

| Stockfish | Qapla |
| --- | --- |
| `pos.pieces()`, `pos.pieces(c, pt)`, `pos.piece_on(s)` | `getAllPiecesBB()`, `getPieceBB(piece)`, board lookup |
| `pos.count<ALL_PIECES>()`, `popcount` | `popCount(getAllPiecesBB())` |
| `pos.material_key()` | `getPiecesSignature()` ([piecesignature.h](../basics/piecesignature.h)) |
| `pos.set("KQvKR", c, &st).material_key()` | `PieceSignature(code)` — `set(string)` already exists |
| `MoveList<LEGAL>(pos)` | `genMovesOfMovingColor(moveList)` — Qapla generates fully legal moves |
| `pos.do_move/undo_move` | `doMove(m)` / `undoMove(m, boardState, incremental)` |
| `pos.rule50_count()`, `pos.checkers()`, `pos.can_castle()` | `getTotalHalfmovesWithoutPawnMoveOrCapture()`, `isInCheck()`, castling accessors |
| `pos.is_draw()`, `pos.has_repeated()` | `MoveHistory` — the only genuinely missing piece, see section 2c |

That is the whole interface work — a table of renames plus one addition to `MoveHistory`.
**Three of the old plan's nine sections disappear**, and one is cut to a paragraph.

**Four corrections to the old plan, in order of importance:**

1. **No `TB_Position` adapter.** The old section 3 built an 18-function callback layer with a
   per-ply frame stack, a `CAPTURES`/`QUIETS`/`ALL` layout enum and two piece-encoding tables —
   all of it required by probetool's engine-agnostic C API, none of it by Stockfish's. Dropped
   entirely. Same for the C-compiler discussion (`.c` file, `<stdatomic.h>`, `-std:c11`,
   `extern "C"`, CMake `*.c` glob): it is a `.cpp`, and `CMakeLists.txt:11` globs `*.cpp` already.

2. **WDL belongs in the search, not in eval.** The old plan hooked Syzygy into
   `EvalEndgame::getFromBitbase` because that is where the bitbases sit. That is wrong for Syzygy
   for reasons that have nothing to do with speed — a WDL probe is only valid at `rule50 == 0`,
   its result is a *bound* and not a score, and it needs a TT store to be affordable. See
   section 3. Qapla's `Search::hasBitbaseCutoff` ([search.cpp:83](../search/search.cpp#L83)) is
   the right place, with one move: it must run **after** the TT probe, not before.

3. **The file-based bitbases go, KPK stays — and that costs nothing.** The old plan switched *all*
   bitbases off, accepted a knowingly-losing tagged version and then needed a whole closing-run
   apparatus to repair the damage. That was based on a wrong premise. KPK is not a file: it is
   compiled into the binary as `KPK.h` and registered by
   [`registerBitbaseFromHeader`](../bitbase/bitbase-reader.cpp#L64) at startup, with no path and no
   I/O. The `.btb` bitbases *are* files, they need the same kind of download Syzygy needs, and
   Qapla's own format has no reason to exist next to Syzygy — so they go, and KPK stays. Since
   nothing loads them unless `qaplaBitbasePath` is set (and neither
   [epd-wmtest-depth.ini](../test/epd/epd-wmtest-depth.ini) nor
   [sprt-standard.ini](../test/sprt/sprt-standard.ini) sets it), **removing them is
   behaviour-neutral: identical node count, no SPRT, no lost Elo.** See section 5.

4. **The root ranks moves, it does not filter them.** The old plan intersected `searchMoves` with
   the best DTZ group. Stockfish assigns every root move a `tbRank` and `tbScore`, stable-sorts,
   and lets the search run on all of them — which keeps MultiPV intact and keeps the tie-break
   information. It also disables in-search probing when DTZ ranking succeeded and we are winning,
   which the old plan had no equivalent of. See section 4.

One fact that matters for stage D and that the old plan misread: `Search::hasBitbaseCutoff` starts
with an unconditional `return false` ([search.cpp:84](../search/search.cpp#L84)), so **the
search-side bitbase cutoff is dead code today** and KPK reaches the engine only through the eval
hook `EvalEndgame::getFromBitbase`. The old plan called this "already a dead `return false`, gets
the flag anyway" and moved on; in fact it is the site the whole Syzygy integration hangs on, and
its being dead is what lets it be moved behind the TT probe for free.

One simplification over Stockfish itself, not just over the old plan: **the in-search probe does
not generate moves of its own.** Syzygy's "don't care" freedom is tied solely to the existence of
captures, so a raw table read is exact whenever nothing zeroing is available — and the search
already knows that, because the probe sits right after `computeMoves` and reads
`getNonSilentMoveAmount() == 0`. Stockfish's `probe_wdl` degenerates to exactly that call in the
same case; it differs only in also resolving captures by recursion, which the search does better
one ply deeper with its own TT. See section 2a.

Smaller things the old plan got wrong or missed: `undoMove` takes three arguments, not two;
`Syzygy50MoveRule` and `SyzygyProbeDepth` are needed, not optional; the per-root-move draw check
and the mate-in-1 DTZ correction were missing; `has_repeated` is required for correct DTZ ranking
and is ~10 lines on `MoveHistory`, not a follow-up; and the 40-line cost analysis of ClusterCache
vs. mmap is moot once the probe sits behind a depth guard and a TT store.

**License:** Stockfish is GPL-3.0. Qapla's [LICENSE](../LICENSE) is AGPL-3.0 with GPL-3.0-or-later
file headers. GPLv3 §13 explicitly permits combining GPLv3 code with an AGPLv3 work, so this is
clean — keep Stockfish's copyright header in the vendored files unchanged and record the origin
in `syzygy/README.md`.

**Work order and expected results:** section 7, lettered A-F so the stages never collide with
`negaMax`'s own numbered steps. A-D are behaviour-neutral; only E and F, which actually add
Syzygy, need a tag and an SPRT.

---

## 1. Vendor and adapt

New directory `syzygy/`:

```
syzygy/tbprobe.cpp      from Stockfish src/syzygy/tbprobe.cpp, adapted
syzygy/tbprobe.h        from Stockfish src/syzygy/tbprobe.h, adapted
syzygy/README.md        origin, Stockfish version, license note, list of the adaptations
```

Unlike the old plan these are **not** taken verbatim — the whole point of Stockfish's design is
that the file is engine-specific. Keep the diff against upstream minimal and documented, so a
later upstream fix can be re-applied by hand.

What stays untouched: the entire decoder (`decompress_pairs`, the re-pair symbol tables, the
sparse index), the file mapping (`TBFile`, which handles `CreateFileMapping` and `mmap` itself),
`TBTables`, `probe_table`, `probe_ab`, `probe_wdl`, `probe_dtz`. That is ~90 % of the file and it
never touches a board except through the accessors in the summary table.

What has to be replaced:

- `Stockfish::` namespace → `QaplaSyzygy::`
- `Position` → `QaplaMoveGenerator::MoveGenerator`, per the table above
- `MoveList<LEGAL>(pos)` → `genMovesOfMovingColor(moveList)`, see below
- `Options["…"]` / `OptionsMap` → plain function arguments; the option values live in the
  `TablebaseReader` of section 2
- `Search::RootMoves` → `QaplaSearch::RootMoves` ([rootmoves.h](../search/rootmoves.h))
- a handful of Stockfish helpers from `misc.h` / `bitboard.h` (`sync_cout`, `popcount`, `lsb`,
  `msb`, `flip_rank`) → Qapla's equivalents in [bits.h](../basics/bits.h) and plain `std::cout`

Square encoding needs no conversion: Qapla's [types.h:53](../basics/types.h#L53) is
`A1 = 0 … H8 = 63`, the same mapping Syzygy uses.

### Build files

- **[Makefile](../Makefile)** — nothing to do, sources are globbed.
- **[CMakeLists.txt:11](../CMakeLists.txt#L11)** — nothing to do, `GLOB_RECURSE … "*.cpp"` picks
  it up. (Note in passing: that glob still misses `bitbase/lz4.c` and `bitbase/miniz.c`, so the
  CMake build is already incomplete — unrelated to this item, worth a separate fix.)
- **[Qapla.vcxproj](../Qapla.vcxproj)** — files are listed explicitly, so `tbprobe.cpp` needs a
  `ClCompile` and `tbprobe.h` a `ClInclude` entry.

---

## 2. Move generation, and the one place Qapla differs from Stockfish

### a) The in-search probe does not generate moves — it is guarded instead

Unlike Qapla's own bitbases, which store every position and answer with an index computation and a
bit, **Syzygy deliberately stores wrong values for some positions** — that is where its compression
comes from. `tbprobe.cpp:1262-1270` states exactly which ones:

> For a position where the side to move has a winning capture it is not necessary to store a
> winning value so the generator treats such positions as "don't care" […] if the side to move has
> a drawing capture, then the position is at least drawn. If the position is won, then the TB needs
> to store a win value. But if the position is drawn, the TB may store a loss value if that is
> better for compression.

**The don't-care freedom is tied solely to the existence of captures.** With no capture available
the table must store the true value, and the tables model no en passant either — so the condition
under which a raw table read is exact is *"nothing zeroing is available"*, which is exactly what
the non-silent region answers (section 2b):

```cpp
node.getNonSilentMoveAmount() == 0    // no capture, no e.p. capture, no promotion
```

It is **free and exact**: `computeMoves` generates the full legal list eagerly
([moveprovider.h:143](../search/moveprovider.h#L143)), and
`MoveProvider::getNonSilentMoveAmount()` ([moveprovider.h:278](../search/moveprovider.h#L278))
already exposes the count — only a passthrough on `SearchNode` is missing. Unlike an attack-mask
test it needs no `getEP()` fallback: an available e.p. capture is a non-silent move and is counted.

**Stockfish's `search<false>` degenerates to exactly this call in that case.** With `moveCount == 0`
the loop body never runs, `noMoreMoves` is false, control goes straight to `probe_table<WDL>`, and
`bestValue` stays `WDLLoss`, so the table value is returned with `result = OK`. The guarded version
is not an approximation of `probe_wdl` — it is the same code path with "give up" instead of
"resolve the captures".

What that gives up is **hit rate, not correctness**, and barely even that: in a capture-rich
position the search plays the capture itself, lands one ply deeper in a *smaller* table where
usually no capture remains, hits there, and stores the result in the TT. `probe_ab` is a mini
alpha-beta over captures — work the search already does better, with killers, ordering and a real
transposition table.

So the in-search probe of section 3 calls `probe_table<WDL>` behind the guard above and generates
no moves of its own — it reads a list the search generated anyway. Whether the full `probe_wdl`
with capture resolution buys enough extra hit rate to pay for itself is a legitimate follow-up
experiment (section 8), not the first build.

**Where move generation is genuinely unavoidable is the root DTZ path.** DTZ is stored for only one
side to move per material configuration, so the `CHANGE_STM` branch
(`tbprobe.cpp:1553-1581`) must do a 1-ply search to find the minimum DTZ; `search<true>` must also
see pawn moves to detect zeroing best moves, and mate detection needs `dtz == 1 && in check && no
legal moves`. That is once per played move, with the root move list already in hand, so the cost is
irrelevant there. The full machinery is vendored either way — it comes with the file — it is simply
not called from the hot path.

### b) Move generation maps one to one

`MoveList<LEGAL>(pos)` becomes `genMovesOfMovingColor(moveList)`. Nothing else. Qapla generates
**fully legal moves only**: `genMoves<COLOR>`
([movegenerator.cpp:594-623](../movegenerator/movegenerator.cpp#L594-L623)) computes the pinned
mask, dispatches to `genEvades<COLOR>` when the side to move is in check, and restricts pinned
pieces to their ray. There is no pseudo-legal stage to filter. `isLegal()`
([movegenerator.h:70](../movegenerator/movegenerator.h#L70)) exists for the bitbase generator,
which reconstructs positions from an index rather than reaching them by a move — it has no place
in this port.

`tbprobe.cpp` uses `MoveList<LEGAL>` at four places — the move loop of `search<>` (1281), the move
loop of `probe_dtz` (1556) and its mate check (1569), and the mate check of `root_probe` (1638).
`root_probe_wdl` generates nothing; it iterates the root move list. All four become the plain call,
and mate/stalemate is `moveList.getTotalMoveAmount() == 0`.

**The non-silent region is an exact "nothing zeroing available" test**, and the old plan's warning
about underpromotion captures was misplaced. `addPromote`
([movelist.h:88-95](../basics/movelist.h#L88-L95)) does put rook/bishop/knight promotions into the
*silent* region even when they capture — but it always emits the queen promotion into the
non-silent region alongside them, and every promotion in the generator goes through it
([movegenerator.cpp:273](../movegenerator/movegenerator.cpp#L273),
[:348](../movegenerator/movegenerator.cpp#L348)). E.p. captures use `addNonSilentMove`
([movegenerator.cpp:305](../movegenerator/movegenerator.cpp#L305)), ordinary captures reach it via
`addMove`'s `isCaptureOrPromote()` test ([movelist.h:49-58](../basics/movelist.h#L49-L58)), and
castling is silent. Therefore:

```
getNonSilentMoveAmount() == 0  ⟺  no capture, no e.p. capture, no promotion of any kind
```

There is no way to produce an underpromotion without its queen counterpart, so the region boundary
cannot lie about *whether* something zeroing exists. It only lies about *which* moves they are —
so `getNonSilentMoveAmount()` must not be used as a capture *count* or as the end of a capture
*enumeration*; `move.isCapture()` per move is the right test there, exactly as Stockfish's
`pos.capture(move)`. Neither the guard of section 2a nor the root path enumerates by region, so
this never comes up in practice.

Worth noting as the exact counterpart of the section 2a guard: where a move list happens to exist
already, `getNonSilentMoveAmount() == 0` decides the same question without the `getEP()`
approximation. The search has no move list at the point it probes, so the attack-mask form is what
gets used there.

One property remains:

- **Castling must never reach the tables.** Syzygy knows nothing about castling rights.
  `genMoves` adds castling at
  [movegenerator.cpp:614-619](../movegenerator/movegenerator.cpp#L614-L619), but every probe entry
  point refuses positions with any castling right set (section 3), so
  `isKingSideCastleAllowed` / `isQueenSideCastleAllowed` are false and no castling move is
  produced; castling rights only ever decrease, so no descendant can produce one either. An
  `assert`, not a runtime branch.

For `do_move` / `undo_move` note the signature `undoMove(Move, BoardState, const IncrementalState&)`
([movegenerator.h:113](../movegenerator/movegenerator.h#L113)) — three arguments, so both the
`BoardState` and the `IncrementalState` have to be saved before the move.

### c) Repetition and draw detection

Stockfish's `root_probe` needs two things Qapla's board does not have:

- **`pos.is_draw(1)` per root move** — after each root move, is the child a draw by repetition or
  by the 50-move rule? Stockfish sets `dtz = 0` for such a move. Qapla:
  `MoveHistory::isDrawByRepetition` ([movehistory.h:79](../search/movehistory.h#L79)) plus
  `getTotalHalfmovesWithoutPawnMoveOrCapture() >= 100`
  ([board.h:151](../basics/board.h#L151)). Root only, so `MoveHistory` is available.
- **`pos.has_repeated()`** — *has any position repeated since the last zeroing move?* This is the
  `rep` flag in the rank formula, and without it the engine may shuffle a won endgame into the
  50-move draw. It is a different question from `isDrawByRepetition` (which asks about the
  *current* position), but it walks the same history and reuses the same loop bound at
  [movehistory.h:84](../search/movehistory.h#L84). Add
  `bool hasRepeatedSinceZeroing() const` to `MoveHistory`. Not a follow-up — the rank formula is
  wrong without it.

---

## 3. Loading, options, and where the probe goes

### `TablebaseReader`

New class `QaplaSyzygy::TablebaseReader` in `syzygy/tablebase-reader.h/.cpp`, the counterpart of
[BitbaseReader](../bitbase/bitbase-reader.h) and the only place outside `tbprobe.cpp` that knows
about Syzygy:

```cpp
static void setPath(const std::string& path);   // Tablebases::init(), reports counts
static int  maxCardinality();
static bool isProbeable(const MoveGenerator&);  // piece count, castling rights, cardinality
```

`isProbeable` is the single guard used by every caller:

- `maxCardinality() > 0` and piece count `<= min(maxCardinality(), SyzygyProbeLimit)`
- **no castling right set**
- the 50-move counter is *not* part of it — the two call sites check it differently, see below

`setPath` reports the result as `info string` in the style
[boardadapter.h:103](../search/boardadapter.h#L103) uses for the bitbases: WDL and DTZ file counts
and the maximum cardinality. Re-calling `Tablebases::init` frees the mapped files, which is what
Stockfish does on `ucinewgame` ([engine.cpp:140](file:///C:/Chess/Engines/stockfish-windows-x86-64-avx2/stockfish/src/engine.cpp)).

### UCI options

Standard names, so qet, cutechess and every GUI set them without configuration:

```
option name SyzygyPath type string default <empty>
option name SyzygyProbeDepth type spin default 1 min 1 max 100
option name Syzygy50MoveRule type check default true
option name SyzygyProbeLimit type spin default 7 min 0 max 7
```

All four, not two. The old plan dropped `Syzygy50MoveRule` and `SyzygyProbeDepth` as "nothing
reads them yet" — but `Syzygy50MoveRule` is exactly what makes cursed wins and blessed losses
come out right (it is the `useRule50` that produces the `±1` draw score in the search and the
`bound` in the root ranking), and `SyzygyProbeDepth` is the knob that keeps the in-search probe
affordable. Both are load-bearing from the first build.

Wiring, mirroring `qaplaBitbasePath`:

- announce in [uci.cpp:82-83](../interface/uci.cpp#L82-L83) next to the other options
- handle in [BoardAdapter::setOption](../search/boardadapter.h#L90) — `SyzygyPath` goes into the
  same `else` branch as the existing string options; `Syzygy50MoveRule` arrives as `"true"` /
  `"false"` and therefore takes the `intValue` path at the top of that function
- winboard: `feature egt=syzygy`, `egtpath syzygy <path>` in
  [winboard.cpp:340](../interface/winboard.cpp#L340)

### Where the WDL probe goes — and why not into eval

Qapla's [`Search::hasBitbaseCutoff`](../search/search.cpp#L83) is structurally the same thing as
Stockfish's step 5 tablebase probe, so it is the hook. Four reasons the eval leaf is not:

1. **A WDL result is only valid at `rule50 == 0`.** Stockfish guards on exactly that
   (`search.cpp:643`). Eval is called at every leaf regardless of the halfmove clock, i.e. in
   precisely the positions where the 50-move rule decides the game.
2. **The result is a bound, not a score.** WDL win means "at least a TB win", not "equal to". In
   the search that becomes `BOUND_LOWER` / `BOUND_UPPER` / `BOUND_EXACT` written to the TT with
   `depth + 6`. An eval hook can only return an exact number, and that number then propagates into
   every TT entry above it.
3. **Cursed wins need `useRule50`, not a collapse to Draw.** Stockfish scores them
   `VALUE_DRAW + 2 * wdl * drawScore`, i.e. `±1` — a real win that is merely unreachable under the
   50-move rule keeps a positive sign and the search still steers towards it. Mapping them to
   `Draw`, as the old plan proposed, throws that away.
4. **Cost.** Even the guarded raw read of section 2a is not a byte-and-mask: it walks the sparse
   index and descends the re-pair symbol tree. Paid once per node in the search and served from the
   TT afterwards, that is fine; paid at every eval leaf it is not.

### The exact site: `negaMax` step 7b, right after `computeMoves`

The probe is a **cutoff in `negaMax`, immediately after
[`node.computeMoves`](../search/search.cpp#L515)** — a new step between the move generation and
step 8. Not in `nonSearchingCutoff`, where `hasBitbaseCutoff` sits today, and not in eval. Four
things are all true only at that point:

1. **The move list exists**, so the exact guard of section 2a (`getNonSilentMoveAmount() == 0`) is
   free. `computeMoves` generates eagerly ([moveprovider.h:143](../search/moveprovider.h#L143)), so
   the count is filled, not lazily deferred.
2. **It is after the TT probe** (step 3, [search.cpp:479](../search/search.cpp#L479)), so a
   position whose TB result is already cached costs nothing. This is why `nonSearchingCutoff` —
   which runs at [search.cpp:457](../search/search.cpp#L457), *before* the TT — is the wrong place:
   for a byte-lookup bitbase that ordering was harmless, for Syzygy it defeats the caching.
3. **`depth` is in scope**, so `SyzygyProbeDepth` works. Copy Stockfish's rule:
   probe when `pieceCount < cardinality || depth >= probeDepth` — below the maximum cardinality
   always, at the maximum only from the given depth. That is the knob for trading hit rate against
   probe cost.
4. **Eval bookkeeping is intact.** `checkEvalReleatedCutoffsAndSetEval` (step 7) has already set
   `node.adjustedEval`, which the grandchild reads for `isImproving`
   ([search.cpp:59](../search/search.cpp#L59)). Cutting before step 7 would leave it at `NO_VALUE`
   and corrupt that signal two plies down.

The price of sitting after step 7 is that futility and nullmove get first refusal, so a node they
cut never reaches the tablebase, and the eval was computed even when the TB would have answered.
Both are hit-rate losses, not correctness problems, and both are visible as the `tbhits` count in
the EPD run.

`hasBitbaseCutoff` and its call in `nonSearchingCutoff` are deleted outright rather than moved: the
function is dead today (`return false`), and KPK goes on reaching the engine through
`EvalEndgame::getFromBitbase` exactly as it does now. The dead
`BitbaseReader::getValueFromBitbase` body below the `return false` goes with the rest of the
file-bitbase code in section 5.

### The returned value: a bound, and it needs a gradient

Both points below apply to *any* WDL source, Syzygy and Qapla's own bitbases alike. They are not
new with Syzygy — `hasBitbaseCutoff` already carries the first one, commented out.

**A win is a lower bound, not a value.** The table says "won", not "mate in 7", so the number the
search returns is a placeholder and the true value is somewhere above it. Returning it is only
sound when it fails high: with `placeholder >= beta` the parent learns "at least beta", which is
true whatever the exact distance to mate. When `placeholder < beta` — which happens once beta sits
in the mate range, i.e. after a mate was already found higher up — returning it claims an exact
value that is too low, and the parent may prefer a move that scores slightly higher but wins slower
or not at all. The mirrored condition holds for a loss at alpha; a draw is exact.

Those are precisely the conditions already written down and commented out in
[search.cpp:94](../search/search.cpp#L94) and [:98](../search/search.cpp#L98)
(`// && curPly.beta <= MIN_MATE_VALUE`, `// && curPly.alpha >= -MIN_MATE_VALUE`). Reinstate them
for the tablebase cutoff.

**A flat value has no gradient, and that is the bigger risk.** `MIN_MATE_VALUE` for every won
position tells the engine *that* it wins but not *how to make progress* — every winning move scores
the same, so it can shuffle until the 50-move rule ends it. The current eval hook does not have
this problem: `getValueFromBitbase(position, value)` returns `currentValue + WINNING_BONUS`
([bitbase-reader.cpp:221](../bitbase/bitbase-reader.cpp#L221)) and therefore keeps the eval's own
gradient — king proximity, pawn advance — underneath the bonus.

Placing the probe after step 7 preserves that option: `node.adjustedEval` is already computed at
that point, so the cutoff can reuse the same mapping instead of a flat constant. Do that. It keeps
the conversion behaviour the engine has today and makes stage E a pure addition rather than a
trade — a flat cutoff would replace a gradient that currently works with one that does not, and
would show up as won endgames drawn on the 50-move rule.

Real winning technique comes from DTZ at the root (section 4), not from the search. That is why
**stage E on its own is the stage of this plan most likely to fail its SPRT**, and why stage F
should follow it quickly.

### TT store

Qapla stores nothing for cutoffs coming out of `nonSearchingCutoff` or step 7 — both simply
`return node.bestValue`. So there is no existing entry to corrupt; the point is the opposite one,
that without a store the same position is re-probed on every revisit. The precision field exists
already ([ttentry.h:137-139](../search/ttentry.h#L137-L139)): win → `GREATER_OR_EQUAL`, loss →
`LESSER_OR_EQUAL`, draw → `EXACT`.

Stockfish stores with `depth + 6`. That is a heuristic, not a requirement: a tablebase answer is
true independently of depth, so it gets an inflated draft to survive replacement and to stay
acceptable at greater remaining depth. Worth copying, worth measuring, not worth treating as part
of the correctness argument.

Count `_computingInfo._tbHits` ([computinginfo.h:276](../search/computinginfo.h#L276)) — the field
and the `tbhits` UCI output exist and read `0` today, which makes the counter direct proof that the
new code is reached. `Syzygy50MoveRule` decides whether cursed wins and blessed losses come back as
`±1` or as a plain draw.

---

## 4. The root — rank, do not filter

Port `root_probe`, `root_probe_wdl` and `rank_root_moves` from `tbprobe.cpp:1595-1765`.

`RootMove` ([rootmoves.h](../search/rootmoves.h)) gets two fields, mirroring Stockfish's
`search.h:101-102`:

```cpp
int     _tbRank  = 0;
value_t _tbScore = 0;
```

`RootMoves::setMoves` ([rootmoves.cpp:109](../search/rootmoves.cpp#L109)) already builds the list
and already honours `searchMoves`, so `rank_root_moves` runs right after it and then
`std::stable_sort`s by `_tbRank` descending. **The move list is not shortened.** Every root move
stays searchable — MultiPV keeps working, and the ranking rather than a filter is what breaks DTZ
ties without producing the unnatural shuffling moves that pure DTZ-optimal play produces.

Three parts of `root_probe` the old plan did not have:

- **`dtz = 0` for a root move that leads to a draw** by repetition or 50-move rule (`is_draw(1)`,
  section 2c). Without it a won position can be ranked into a repetition.
- **the mate-in-1 correction**: `if (checkers && dtz == 2 && no legal moves) dtz = 1` — otherwise a
  mating move is not ranked as the fastest win.
- **`cnt50` and `rep`** as inputs to the rank formula: `cnt50` is
  `getTotalHalfmovesWithoutPawnMoveOrCapture()`, `rep` is `hasRepeatedSinceZeroing()` from
  section 2c. The old plan hardcoded `rep = false`, which is what probetool's *demo driver* does,
  not what an engine does.

And one piece of control flow the old plan missed entirely: after a successful DTZ ranking
Stockfish sets `config.cardinality = 0` unless the best move is losing
(`tbprobe.cpp:1753`). That switches the in-search WDL probe **off** for the rest of the search.
The reason is that DTZ ranking at the root and WDL cutoffs inside the search answer different
questions about the 50-move rule, and mixing them makes the engine abandon a won endgame. Port
this as-is.

Reporting: when the root ranking succeeded, the UCI `info` line shows `_tbScore` instead of the
search value (Stockfish `search.cpp:2048-2049`) and adds the root move count to `tbhits`
(`search.cpp:2033`).

---

## 5. Retire the file-based bitbases, keep KPK

Syzygy replaces Qapla's own `.btb` / `.qwdl` files: both need a download, and there is no reason to
maintain a second format for the same information. KPK is a different thing and stays — it is 6 KB of
`constexpr uint32_t` in [KPK.h](../bitbase/KPK.h), decompressed into RAM at startup by
`Bitbase::loadFromEmbeddedData` ([bitbase.cpp:342](../bitbase/bitbase.cpp#L342)), and it works in
a binary that has no data files at all.

One flag, e.g. in `bitbase/bitbase-config.h`, undefined by default:

```cpp
// Loading Qapla's own .btb bitbase files from disk. Undefined: the engine ships with the
// compiled-in KPK only and gets everything else from Syzygy. Define it to get file loading back.
// #define QAPLA_USE_BITBASE_FILES
```

**Compiled out when undefined** — the file path and its configuration:

| site | change |
| --- | --- |
| [bitbase-reader.cpp:39](../bitbase/bitbase-reader.cpp#L39) | `loadBitbase()`, `loadBitbaseRec`, `loadBitbase(name, onlyHeader)`, `tryLoadBitbaseFile`, `registerQwdlFile`, `setBitbasePath` |
| [uci.cpp:82-83](../interface/uci.cpp#L82-L83) | `qaplaBitbasePath` and `qaplaBitbaseCache` no longer announced |
| [boardadapter.h:100-119](../search/boardadapter.h#L100-L119) | the `qaplabitbasepath` / `…pathnl` / `…cache` branches of `setOption` |
| [winboard.cpp:73](../interface/winboard.cpp#L73) | `feature egt=qaplaBitbases` dropped |
| [search.cpp:83](../search/search.cpp#L83) | the dead body below `return false` deleted; the function becomes the Syzygy hook of section 3 |

**Always compiled** — everything KPK needs, which is the probe path minus the file I/O:
`KPK.h`, `registerBitbaseFromHeader` ([chessinterface.h:159](../interface/chessinterface.h#L159)),
the `_bitbases` map and `getBitbase`, both `getValueFromBitbase` overloads, `BitbaseIndex`,
`BoardAccess::getIndex`, and the eval hook `EvalEndgame::registerBitbase` /
`getFromBitbase` ([evalendgame.cpp:149](../eval/evalendgame.cpp#L149)). The eval-side endgame
*heuristics* in the same file (`KQPsKRPs`, `KPsK`, …) are not bitbases and are untouched.

**Also always compiled: the offline tooling.** `bitbase/bitbasegenerator.*`, `bitbase/verify.h`,
the compressors, and the `bitgenerate` / `bitverify` commands in
[bitbase-interface.cpp](../interface/bitbase-interface.cpp) keep full file access regardless of the
flag. They are not on the play path, they are how `KPK.h` was produced in the first place, and they
are the reference for check 6a. The flag guards what the *engine* does, not what the tooling can do.

**This is behaviour-neutral and must be proven so.** Without `qaplaBitbasePath` nothing is ever
loaded, so `_bitbases` holds KPK and nothing else — which is already the case in every test run,
since neither test ini sets the option. The EPD node count must therefore be **identical**. If it
differs, something that was supposed to be dead was not.

**KPK has no fallback, which is why it must stay.** `registerBitbase("KPK")` runs after static
initialisation and `insert` overwrites on an equal key
([hashed-lookup.h:87-91](../basics/hashed-lookup.h#L87-L91)), so the bitbase entry replaces the
`KP+K` → `KPsK` heuristic for that signature — and `KPsK` returns `value` unchanged whenever there
is exactly one pawn ([evalendgame.cpp:174-176](../eval/evalendgame.cpp#L174-L176)) anyway. Dropping
the header registration would therefore leave KPK with no endgame knowledge at all, not with a
heuristic. Verified on the current binary: the drawn KPK `8/8/8/8/8/1k6/1P6/1K6 w` scores `cp ±1`,
which is the `Draw → 1` mapping of [bitbase-reader.cpp:223](../bitbase/bitbase-reader.cpp#L223),
and a won KPK scores eval + `WINNING_BONUS`. The `tbhits 0` on the same output line is the
independent confirmation that the *search* hook is dead.

---

## 6. Verification

Two checks, both cheap, both using things that already exist.

**a) Against Qapla's own bitbases.** They cover everything up to five pieces and they are correct.
A debug command in [BitbaseInterface](../interface/bitbase-interface.h) (`tbcompare <signature>`)
walks the bitbase index, reconstructs each position and compares `probe_wdl` against
`BitbaseReader::getValueFromSingleBitbase`. Every mismatch that is not a cursed/blessed case is a
bug. This exercises the material key, the index computation and the capture resolution over
millions of positions and needs no external reference. It lives in the offline tooling, so it keeps
working after section 5 — but run it **before** section 5 is merged, while the generated files are
still trivially loadable.

**b) Against Stockfish.** It is installed, it takes FENs, and it is the exact code this port comes
from — so any difference is a porting bug, which is a far sharper signal than the old plan's
"build probetool as a separate binary" would have been. Feed both engines the same FEN list
([wmtest.epd](../wmtest.epd) filtered to ≤ 7 pieces, plus positions from finished games) and
compare WDL, DTZ and the root ranking. This is what catches a wrong `cnt50` or a wrong rank
formula, which check (a) cannot see.

Move generation is verified implicitly by both: a wrong capture set or a stray castling move
changes the WDL answer.

---

## 7. Order of work

Lettered on purpose: `negaMax` has its own numbered steps in the source comments (step 5 there is
the singular extension, step 7 the eval-related cutoffs), and reusing digits here would collide
with them. **Stage A-F below, step N always means the numbering in
[search.cpp](../search/search.cpp).**

| stage | work | play changes | verification |
| --- | --- | --- | --- |
| **A** | `syzygy/` vendored and adapted, builds everywhere, nothing calls it | no | EPD node count **identical**, runtime within 5 % |
| **B** | `TablebaseReader` + the four UCI options + `MoveHistory::hasRepeatedSinceZeroing`, still nobody probes | no | EPD node count **identical**, plus check 6a |
| **C** | file bitbases retired behind the flag, KPK kept (section 5) | no | EPD node count **identical** |
| **D** | `hasBitbaseCutoff` and its call in `nonSearchingCutoff` deleted | no | EPD node count **identical** |
| **E** | WDL cutoff in `negaMax` after step 7, per section 3 | yes, with `SyzygyPath` set | node count differs, tag, SPRT |
| **F** | root ranking with DTZ | yes | node count differs, tag, SPRT |

**Stages A-D are all behaviour neutral**, and the rule from [CLAUDE.md](../CLAUDE.md) applies to
each of them in full: identical total node count, runtime within 5 %. That is the payoff of the
correction in the summary — the old plan had a deliberately Elo-losing prerequisite here, and there
is none. C and D are free because the file bitbases are never loaded in the test configuration and
`hasBitbaseCutoff` returns `false` today.

**Stage E is the one with real H0 risk.** It replaces a working eval gradient with a search cutoff
and brings no winning technique of its own — see the gradient discussion in section 3. If it comes
back H0, run F on top of it before moving it to `dead/`: DTZ at the root is what supplies the
technique, and the pair may pass where E alone does not.

**E and F are the only stages needing a tag and an SPRT**, and they only change play when
`SyzygyPath` is set — so **both engines in those runs must get it**, otherwise the run measures a
missing directory. Check the `info string` line in the pgn once at the start of each run. The EPD
comparison run likewise needs the path on the command line, or its node count will come out
identical and prove nothing.

If E and F turn into a series of more than two SPRTs, the closing-run rule of
[CLAUDE.md](../CLAUDE.md) applies as usual.

Every tagged version gets its entry in [version-log.md](version-log.md), in its own commit. Steps
1-4 are not tagged versions — they are behaviour-neutral commits and belong in the log only if one
of them turns out not to be neutral after all.

---

## 8. Open points

- **Full `probe_wdl` in the search instead of the guarded raw read** — section 2a probes only where
  the stored value is exact and gives up otherwise, on the argument that the search resolves the
  captures itself one ply deeper. The alternative is Stockfish's `probe_wdl`, which resolves them
  inside the probe and therefore hits in capture-rich nodes too. Higher hit rate, higher cost per
  probe; a clean single-change SPRT once stage E is in, and the only honest way to settle it.
- **Verify the no-capture guard empirically.** That a table read is exact whenever no capture is
  available follows from the generator's don't-care rule, but the plan should not rest on a source
  comment. Check 6a compares against Qapla's own bitbases over millions of positions and would
  expose a counterexample immediately — run it with the guard in place, not only with full
  `probe_wdl`.
- **`Syzygy50MoveRule = false`** — the option exists from the start; whether the default should be
  `true` (Stockfish's) is not worth an experiment before stage F is in.
- **Root PV extension into mate** — Stockfish walks the DTZ table to extend the PV to mate
  (`search.cpp:1915-2000`). Cosmetic, affects only the displayed PV, not play. Skip it in the
  first build.
- **SMP** — `tbprobe.cpp` is read-only after `init` and thread-safe by design, so nothing here
  blocks a later multi-threaded search. This is not a constraint that has to be planned around.
- **DTM** — the tables were never released. Ignored.
