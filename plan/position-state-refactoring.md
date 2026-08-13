# Position state — what the board keeps, and what it cost to change it

`Board` and `MoveGenerator` carry four different kinds of thing at once: the position, values
maintained incrementally, values rebuilt after every move, and configuration. Which of them is
valid at which moment was nowhere written down, so the search compensated with defensive
recomputations at six places and the quiescence search followed a different unwritten contract.

This document is the classification, plus the record of what was tried against it. Phase A and
B are in the code. Everything that followed was measured and taken back out — the numbers are
in section 5, and they are the more useful half of this file.

---

## 1. The four categories

### 1 — Undoables

The position itself. Lives in `Board`, and only there. Has real undo code.

`_board`, `bitBoardsPiece`, `bitBoardAllPiecesOfOneColor`, `bitBoardAllPieces`,
`kingSquares`, `_whiteToMove`.

`BoardState` — hash, pawn hash, en passant square, castling rights, halfmove counters
([boardstate.h](../basics/boardstate.h)) — sits here but is already handled the way category 2
wants it: `doMove` mutates it, `undoMove` restores it with a single assignment from the copy
the caller kept. It is the reference implementation of the principle.

### 2 — Iterative non-undoables

Values built up move by move, restored from a snapshot rather than reconstructed.

`_pstBonus`, `_materialBalance`, `_pieceSignature`, `_imbalance`.

**This is what phase B changed, and it is the only measured gain of the whole exercise.**
Before, `undoMove` ran the inverse piece operations, so the piece signature, the imbalance,
the material balance and the PST bonus were all computed a second time on the way back up.
Now the caller keeps an `IncrementalState` next to the `BoardState` it already kept, and
`undoMove` assigns both back.

There is no way around the copy. Whatever is not reconstructed by undo code has to be copied.
That trade is the point: one cheap copy on the way down instead of four incremental updates
per touched piece on the way back.

### 3 — Ply-computed

Rebuilt from the position after every move, valid for exactly one ply.

`attackMask[2]`, `pawnAttack[2]`, `pieceAttackMask[64]`, `pinnedMask[2]`
([movegenerator.h](../movegenerator/movegenerator.h)), and `EvalResults`
([evalresults.h](../eval/evalresults.h)), which the evaluation builds and throws away.

They still live in the board. Two attempts to move them out are in section 5; both cost more
than they were worth.

Two properties of these arrays are worth knowing and were not written down anywhere:

- **`pieceAttackMask` is only valid for squares carrying a piece other than a pawn.** The
  computation writes an entry only while walking knights, bishops, rooks, queens and kings.
  Empty squares and pawn squares keep a leftover from an earlier position.
- **`pinnedMask` has two writers.** Move generation computes it for the side to move,
  `Eval::initPlyData` for both colours at the start of every evaluation call. Reading the move
  generator alone suggests the evaluation works on a stale mask for one colour. It does not.

### 4 — Search steering

Needs access to other plies, so it cannot live in a per-ply block handed to `Board`. Stays in
`SearchNode`: window and result, `previousMove` (read from ply-1 and ply-2), `isImproving`
(compares against the evaluation two plies up), the PV store, the killers, the move provider
and the TT data.

---

## 2. Measuring

**Correctness** — the node counts have to match exactly:

```
~/bin/qet --settingsfile=test/epd/epd-wmtest-depth.ini \
    --engine name="Qapla current" cmd=<repo>/build/Release/Qapla
# expected: total nodes: 56158265
```

A change that moves the node count is not behaviour neutral, whatever it was meant to be.
`perft 5` from the start position (4865609) covers the paths the EPD run does not.

**Performance** — always interleaved, baseline / new / baseline / new, against an untouched
reference binary. Never a number compared against a number written down earlier:

```
for i in 1 2 3 4; do
  <run depth 16 with the reference binary>
  <run depth 16 with build/Release/Qapla>
done

~/bin/qet --concurrency=1 --each proto=uci \
    --epd file=test/epd/wmtest.epd depth=16 --engine name=x cmd=<binary>
# expected node count in every run: 118619321
```

Discard the first pair — the machine is cold and the baseline runs first, which flatters it.
Report all remaining pairs. A difference counts only if it has the same sign in every one.

**Why interleaved.** Absolute times are not comparable across sessions: the same reference
binary measured 25.01 s one day and 23.90 s the next on the same machine. A gain read off
yesterday's number is an artefact. Interleaving also cancels the thermal drift within a series.

`concurrency=1` is not a preference either. With 4 parallel tasks the M4 heats up within a
single series of three runs and the times rise monotonically — 7 % at depth 16, 8 % at depth
18. One core barely heats it: 1 % spread.

---

## 3. What is in the code

**Phase A — names.** `searchparameter.h` → `search-config.h` / `SearchConfig` for the feature
switches and fixed ply counts; `search-param.h` → `tunable.h` / `tunable<>()` /
`TunableParams` for the UCI tuning mechanism, which used to be called `SearchParams` right
next to `SearchParameter`. `SearchVariables` → `SearchNode`, because every variable of that
type was already called `node`, plus the member renames that followed from it.

Both numbers unchanged throughout.

**Phase B — the incremental values are restored, not recomputed.** `IncrementalState`,
snapshotted by the caller, restored by `undoMove`, which now uses placement-only piece
primitives: `addPieceToPosition`, `removePieceFromPosition`, `movePieceInPosition`. The hash
updates on the undo path were pure waste anyway — `undoMove` overwrote `_boardState` wholesale
right afterwards.

**2.0 % faster**, same sign in every pair.

One lesson from a broken attempt inside it: **`clear()` is the reliable definition of what a
class maintains incrementally.** The first version of `IncrementalState` carried
`Imbalance::_imbalance` but not `_whiteSignature` / `_blackSignature`, which `updateByDelta`
reads for both the delta and its early exit. The member list had been guessed from the
declarations; `Imbalance::clear()` names all three. `Imbalance` owns a `State` struct mirroring
its `clear()` now, and `PieceSignature` and `MaterialBalance` were checked the same way.

---

## 4. How to work on this

**Steps small enough that a wrong node count points at its own cause.** As soon as finding
the cause turns into a search, the step was far too big. Two consequences, both deliberate:

- **Throwaway code is cheap, debugging is not.** A step may add scaffolding the next step
  deletes: a second value computed in parallel with the old one, a debug-only comparison
  between them, an accessor that exists for one step.
- **A check that cannot fail proves nothing.** Falsify the scaffolding before trusting it —
  break the invariant on purpose once and see the check fire.

The standard tool is the **shadow comparison**: compute the new way next to the old way, keep
the old one as the source of truth, compare under `assert` in the debug build. The node count
cannot move, because nothing reads the new value yet, and when the assert fires it names the
field that diverged instead of leaving a wrong number at the end of a 25 second run.

---

## 5. What was tried and taken back out

Three attempts to get the attack tables out of the board or to make their staleness
impossible. All three were correct — node counts and perft matched throughout — and all three
were slower. The structure got simpler; the engine did not get faster.

### Attempt 1: a per-ply block threaded through everything

`EvalResults` renamed to `PlyData` and widened to carry the attack tables, since it is already
a parameter of 51 evaluation functions. Move generation and the evaluation read the block of
the ply they are in; `doMove` stopped computing anything; the six defensive recomputations in
[search.cpp](../search/search.cpp) were deleted.

**Result: 0.7 % slower than the reference**, i.e. the 2.0 % of phase B gone and a little more.
Moving `PlyData` to the end of `SearchNode` recovered about one point — the 728 byte block had
been inserted in the middle of the node's hot scalar fields — leaving **0.4 % faster**, still
1.5 points short of phase B alone.

The cost is in the shape, not in the count: the tables ended up computed once per node, fewer
times than before. What it bought was an `Attacks&` passed through every generation function
instead of a member whose address the compiler knew, and a `SearchNode` grown from 3.8 to
4.5 KB.

It also added complexity where none was wanted: the endgame evaluation, the bitbase generator
and the interface have no ply block and had to acquire one or build their own tables.

### Attempt 2: private tables behind a getter with a validity flag

The four arrays private, reachable only through `getAttacks()`. `undoMove` marks them invalid,
the getter rebuilds on the next access. Nobody outside has to know when they went stale and
nobody can read a stale one, which is exactly what the six recomputations were for. The
endgame and interface code stays untouched.

**Result: 2.6 % slower than the reference**, same sign in six pairs out of six.

Again not the count — with the six recomputations gone it computes less often than before.
`isInCheck` stops being a two-liner and becomes a branch plus a call that may write memory,
and that shows at its ~23 call sites.

### Attempt 3: the check state as a cached flag

Compute `inCheck[WHITE]` and `inCheck[BLACK]` inside the attack mask computation, so
`isInCheck()` is a bool read. Measured directly against phase B: **-0.14 / -0.01 / -0.09 /
+0.01 / +0.01 seconds** — no consistent sign, nothing measurable. Two loads from data that is
in cache anyway are not a cost worth removing.

### What this says

The attack tables are hot enough that any indirection around them costs more than the
recomputations it saves. That is a finding about this engine, not a general one, and it was
only obtainable by measuring — every one of the three looked like an improvement on paper.

The defensive recomputations in the search stay. They are a real wart, and none of the three
ways of removing them paid for itself.

---

## 6. Out of scope — this changes play

**`_tbHits` is never incremented.** Its only writer sits in the unreachable part of
`Search::hasBitbaseCutoff` ([search.cpp](../search/search.cpp)), which begins with
`return false;`. The UCI output reports 0 tablebase hits permanently. Reporting only, no
effect on play — but the whole function is disabled bitbase probing, not dead code, and
deleting it would throw away work that was meant to be resumed.
