# Syzygy tablebases — implementation plan

Item 18 of [todo.md](todo.md). The reasoning behind every decision here is in
[syzygy.md](syzygy.md); this file is only what has to be done, in the order it has to be done.

Six stages, lettered so they never collide with the numbered steps in the source comments of
[search.cpp](../search/search.cpp). Stages A to D do not change how the engine plays and are
verified by an identical node count. Stages E and F change play and each get a tag, a version-log
entry and an SPRT.

| stage | result |
| --- | --- |
| A | a format layer that turns a position descriptor into a raw table entry |
| B | tablebases can be loaded and configured, nothing probes yet |
| C | the file-based bitbases are retired, the compiled-in KPK stays |
| D | the dead bitbase cutoff is removed |
| E | positions inside the tables are decided in the search |
| F | the root plays tablebase technique instead of tablebase evaluation |

---

## Stage A — the format layer

**Goal:** a directory `syzygy/` that turns the handover structure into a raw table entry and knows
nothing else about the engine.

This is what we have to produce and hand over — a piece list, because that is what the indexing
works on. It holds at most seven entries, sorted ascending by piece so the layer can take the pawns
of either colour off the front or the back without scanning. The piece encoding here is the
interface's own, neither Qapla's nor the one the layer uses internally: filling the structure
translates from Qapla's, and the layer translates into its own behind the call.

```cpp
struct TbPosition {
    uint8_t square[7];      // A1 = 0 ... H8 = 63
    uint8_t piece[7];       // 0 = WhitePawn, 1 = WhiteKnight ... 11 = BlackKing, ascending
    uint8_t pieceCount;     // 2 ... 7
    bool    whiteToMove;
};

enum class Wdl    { Loss = -2, BlessedLoss = -1, Draw = 0, CursedWin = 1, Win = 2 };
enum class Status { Ok, NoTable, OtherSideToMove };   // OtherSideToMove: Dtz only

struct WdlEntry { Status status; Wdl     value;    };
struct DtzEntry { Status status; int32_t distance; };

WdlEntry probeWdlEntry(const TbPosition& pos);
DtzEntry probeDtzEntry(const TbPosition& pos, Wdl wdl);

struct LoadResult { uint32_t wdlFiles; uint32_t dtzFiles; uint32_t maxCardinality; };

LoadResult setPath(const std::string& path);
void       release();
uint32_t   maxCardinality();
```

The material key is not handed over. The format layer builds it itself, because it also has to
build it from a piece string when setting up a table, and both ways must hit the same scheme.

Both probe calls return the stored entry unchanged. The two corrections the format demands on top
of it need moves and are therefore Qapla code, written in stage F.

**Steps**

1. Take out of the foreign source exactly the part that consumes this structure and returns the
   entry — table discovery and file mapping, the table descriptors and their setup, the index
   computation, the decompressor. Leave behind everything that makes or unmakes a move. Rewrite its
   twelve accesses to the foreign position class as reads of `TbPosition`, keep the material key
   internal, replace the option lookups by function parameters and the borrowed helpers by the
   project's. Note that the result carries five states, not three.
2. Adapt [piecelist.h](../bitbase/piecelist.h) into the filler. Its board constructor and
   `addPiecesFromBitBoard` can be lifted almost as they are. Drop what belongs to the bitbase index
   — piece-string parsing and accessors, promotion, removal, mirroring, the pawn counter, the
   ten-piece maximum — translate the encoding to 0..11 while inserting, and change the order, which
   forces both kings to the front there. Walking in ascending piece order makes the list come out
   sorted, so the sorting pass goes away. The filler lives outside `syzygy/`.
3. Write a small test program: build a piece list for a position with one pawn per side, look it up
   against the real tables in `C:\Chess\syzygy\tables`, and print the state. This is the proof
   that structure, filler and format layer fit together, and it is worth having before anything in
   the engine calls the layer. The copy of the tables is still running — pick a signature whose
   `.rtbw` has arrived.
4. Add a README recording origin, upstream version, the licence note, and what was taken versus
   left. The adaptation is a permanent divergence: every upstream fix has to be re-applied through
   it, and the README is what makes that possible.
5. Add the files to [Qapla.vcxproj](../Qapla.vcxproj); the Makefile and CMake pick them up by glob.

**Done when:** the test program returns the correct state for a position whose answer is known
independently; `syzygy/` includes no engine header and no function in it makes or unmakes a move;
all three build systems build and the EPD comparison run reports an identical total node count with
runtime within 5 %, since nothing in the engine calls the layer yet.

---

## Stage B — loading and options

**Goal:** a user can point the engine at a tablebase directory and see what was found. Nothing
probes yet.

1. Add the engine side of the interface next to the position builder, not inside `tbprobe.*`.
   Finding and loading the files is done — `setPath` probes every possible table name, registers
   what exists and maps a file on first use. What is missing is only what the format layer must not
   know: a place for the option values, the guard below, printing what `setPath` reports as an
   `info string`, and forwarding `release`.
2. Declare the four options through [UciOptionProvider](../interface/uci-option-provider.h): path,
   probe depth, fifty-move-rule switch, probe limit. All four are load-bearing; none is optional.
   Nothing has to be touched in `uci.cpp` or in the board adapter for this.
3. Build the guard `isProbeable(board)`, used by every later caller: piece count within both the
   maximum cardinality that was loaded and the configured probe limit, and no castling right set.
   It needs the board, so it cannot live in the format layer, and it should not be rebuilt at each
   call site. It does not check whether the table for this exact material is present - a lookup for
   a missing one simply comes back as no table. The optimisation section makes that check cheap
   once the whole thing works.
4. Winboard: announce the tablebase feature and accept the path command.
5. Release the mapped files when a new game starts.
6. Add the debug command for check 1 below.

**Done when:** the options appear in the UCI handshake, setting the path reports plausible file
counts, check 1 passes for every signature the own bitbases cover, and the EPD node count is
identical.

---

## Stage C — retire the file bitbases, keep KPK

**Goal:** the engine ships with the compiled-in KPK bitbase and gets everything else from the
tablebases.

1. Add a compile flag in a bitbase configuration header, undefined by default.
2. Behind the flag: setting the bitbase path, all file loading and attaching in the reader, the two
   `qaplaBitbase…` UCI options, their branches in `setOption`, and the winboard tablebase feature
   for the own format.
3. Unconditional and untouched: the KPK registration from the compiled-in header at startup, the
   signature map and its lookup, both value queries, the index computation, and the eval
   registration and hook. KPK has no fallback — the endgame heuristic for the same signature does
   nothing when only one pawn is on the board — so removing it would leave that ending with no
   knowledge at all.
4. Untouched regardless of the flag: the generator, the verifier and their commands. They are
   offline tooling, they produced the compiled-in KPK data, and they are the reference for check 1.

**Done when:** the EPD node count is identical. Nothing loads bitbase files in the test
configuration today, so anything else means something that was believed dead was not.

---

## Stage D — remove the dead bitbase cutoff

**Goal:** clear the site stage E will occupy.

1. Delete `hasBitbaseCutoff` and its call in `nonSearchingCutoff`. The function returns false
   unconditionally today, so the search never reaches its body.

**Done when:** the EPD node count is identical.

---

## Stage E — win/draw/loss cutoff in the search

**Goal:** a position that the tables already answer is cut instead of searched.

1. Add a cutoff step in `negaMax` directly after the move generation and before the node-wide
   extension step. Not earlier: the move generation supplies the guard, the hash probe above it
   keeps repeats cheap, the remaining depth is in scope, and the eval bookkeeping that the
   grandchild reads has already been done.
2. Guard — all of these must hold: `isProbeable` from stage B; the halfmove counter since the last
   pawn move or capture is zero; nothing zeroing is available, which is exactly a non-silent move
   count of zero and needs only a passthrough accessor on the search node; and either the piece
   count is strictly below the loaded cardinality or the remaining depth reaches the configured
   probe depth, so the depth limit only bites on the most expensive level. With no tables loaded
   the cardinality is zero and the guard fails on its first test.
3. Call the raw win/draw/loss entry point of stage A. Under the guard above no correction applies,
   so this stage needs none of the move-driven format corrections and generates no moves of its
   own — it reads the list the search generated anyway.
4. Value: reuse the mapping the eval hook uses today — current evaluation plus the winning bonus,
   and the small fixed value for a draw. A flat constant for every won position would tell the
   engine that it wins but not how to make progress, and would trade a working conversion gradient
   for none.
5. Bound: a win is a lower bound and may only cut when the value reaches beta; a loss is an upper
   bound and may only cut at alpha; a draw is exact. Both conditions already exist, commented out,
   at the cutoff site removed in stage D.
6. Store the result in the transposition table with the matching precision and an inflated draft,
   so a probed position is not probed again on every revisit.
7. Count the hit in the existing tablebase-hit counter. It reads zero today, which makes it the
   direct proof that the new code is reached.
8. Add a distinct cutoff enum value so traces tell the tablebase apart from the bitbase.

**Done when:** tagged; the EPD run with the tablebase path set shows a different node count and a
non-zero hit count; check 2 passes; the SPRT decides.

**Why it stands on its own:** because the value keeps the evaluation gradient instead of being a
flat constant, this stage adds knowledge without taking any away, and it does not depend on stage F
to be worth anything. What it cannot do is convert endings whose technique the evaluation does not
already encode - there the gradient points nowhere useful, and only the distance ranking of stage F
helps. So the two are independent improvements, each with its own SPRT, not a pair that has to be
run together.

---

## Stage F — distance-to-zero ranking at the root

**Goal:** the engine converts won endings instead of merely knowing they are won.

1. Write the two move-driven format corrections as Qapla code, using Qapla's move generator and
   `doMove`/`undoMove`, next to the root code that needs them — not inside `syzygy/`. First: a
   win/draw/loss answer for a position that has a capture available is only a bound, so the
   captures are played out and the best of them and the raw entry is taken. Second: a distance
   entry holds the answer for one side to move only, so when it reports the other side, the legal
   moves are walked one ply to find the shortest distance. The rules come from the foreign source,
   the code does not.
2. Add two fields to the root move: a rank and a tablebase score.
3. Rank every root move: play the move; if the child is a draw by repetition or by the fifty-move
   rule, the distance is zero; if the move resets the halfmove counter, derive the distance from
   the win/draw/loss answer; otherwise take the distance answer for the child and correct it by one
   ply. Force a mating move to the shortest distance.
4. The rank formula additionally needs the current halfmove counter. Its second input, whether a
   position has already repeated, starts as a constant false - see the open points.
5. If any distance answer is missing, fall back to a ranking from win/draw/loss alone. If that
   fails too, leave the move list untouched and search normally.
6. Sort the root moves stably by rank. **Do not shorten the list** — every move stays searchable,
   which keeps MultiPV working and keeps the tie-break information.
7. When the distance ranking succeeded and the best move is not losing, switch the in-search probe
   of stage E off for the rest of the search. The root ranking and the in-search cutoff answer the
   fifty-move question differently, and mixing them makes the engine abandon a won ending.
8. Report the tablebase score on the info line instead of the search value, and add the root move
   count to the hit counter.

**Done when:** tagged; the node count differs; check 2 passes including the root ranking; the SPRT
decides.

---

## Checks

**Check 1 — against the own bitbases.** Walk a signature's index, reconstruct each position and
compare the tablebase answer against the bitbase answer. Every mismatch that is not a
cursed-win or blessed-loss case is a bug. This exercises the material key, the index computation
and the guard over millions of positions and needs no external reference. Run it before stage C is
merged, while the generated files are still trivially loadable, and run it again with the stage E
guard active — it is the empirical confirmation that a guarded direct read is exact.

**Check 2 — against an external reference.** Feed the same position list to both — [wmtest.epd](../wmtest.epd)
filtered to seven pieces or fewer, plus positions from finished games — and diff the win/draw/loss
answer, the distance answer and the root ranking. This catches a wrong halfmove counter or a wrong
rank formula, which check 1 cannot see.

---

## Rules for the test runs

- Stages A to D: the EPD comparison run from [CLAUDE.md](../CLAUDE.md) applies in full — identical
  total node count, runtime within 5 %.
- Stages E and F only change play when the tablebase path is set, so **both engines in those SPRT
  runs must get it**, and the EPD comparison run needs it on the command line too. Otherwise the
  run measures a missing directory and proves nothing. Check the `info string` line in the pgn once
  at the start of each run.
- Every tagged version gets its [version-log.md](version-log.md) entry, in a commit of its own.
- If stages E and F turn into a series of more than two SPRTs, the closing-run rule applies.

---

## Once it works — optimisations and follow-ups

Nothing here is needed to make the feature correct. Each item is measurable on its own, and none of
them should be started before stages A to F are in and behaving.

**Skip the handover structure when the table is missing.** The stage B guard only knows the maximum
cardinality, so with a partial set a caller builds `TbPosition` for a lookup that then reports no
table. It can answer the whole question instead: Qapla maintains `getPiecesSignature()`
incrementally, so testing whether this exact material is registered is one lookup on a value the
board already has, with no walk over the pieces. Fill the set at `setPath` time from the registered
table names, adding each signature and its `changeSide()` variant, because one file covers both
colour distributions - the format layer has to hand the names out for that, today `LoadResult` only
counts them. The signature saturates non-pawn counts at three, so from six pieces on `KQQQQvK`
looks like `KQQQvK`; that can only produce a false positive, where the structure is built and the
probe then reports no table. False negatives, the dangerous direction, cannot occur.

**Resolve the cardinality once per search.** Stage E reads it per node. Computing it once before the
search starts - the configured probe limit clamped to what was loaded - turns the common case into a
single integer test.

**Switch the in-search probe off after a successful root ranking.** Stage F already does this, but
only for the winning case. Whether the losing case is worth the same treatment is a separate
question.

**Capture-resolving probe.** Stage E gives up where a capture is available, on the argument that the
search resolves it one ply deeper at lower cost. The alternative resolves it inside the probe and
hits more often. Higher hit rate against higher cost per probe - a clean single-change SPRT.

**Repetition input of the rank formula.** With it false, every move that keeps the win inside the
fifty-move budget gets the same top rank and the search picks freely among them. Setting it once a
position has repeated switches the ranking to distance order and thereby demands progress instead of
mere preservation - the engine is circling at that point, and while the search does prevent the
threefold itself, it can burn the fifty-move counter on non-repeating idle moves. It needs a query
on [MoveHistory](../search/movehistory.h) asking whether any position has repeated since the last
capture or pawn move: same history window as the existing repetition check, different evaluation,
threshold two instead of three.

**Root principal variation extension.** Walking the distance tables to extend the displayed
variation to mate is cosmetic and affects no move choice.

---

## Open points

- **Fifty-move rule default.** The option exists from stage B; whether it should default to on is
  not worth an experiment before stage F is in.
- **Distance-to-mate tables.** The interface exists, the tables were never published. Ignored.
