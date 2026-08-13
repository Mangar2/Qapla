# Position state refactoring — four categories of state

Today `Board` and `MoveGenerator` carry everything: the position, values that are updated
incrementally, values that are rebuilt after every move, and configuration. Which of them is
valid at which moment is nowhere written down, so the search compensates with defensive
recomputations at six places and the quiescence search silently follows a different contract.

The way out is not a better `undoMove`. It is a classification that every piece of state has
to fit into, and one struct that carries the parts a ply produces.

**The result has to be simpler than what it replaces, not only faster.** Speed alone is not a
success criterion here. If a step ends up complicated, the step is wrong and gets rethought
rather than finished. Two things count as complicated and are not accepted as an outcome:

- changing something and changing it back afterwards — which is what the attack tables do
  today;
- needing a value that might not be computed yet, and papering over it with a flag that
  computes it on demand. That is a stopgap, not a solution.

---

## 1. The four categories

### 1 — Undoables

The position itself. Lives in `Board`, and only there. Has real undo code.

`_board`, `bitBoardsPiece`, `bitBoardAllPiecesOfOneColor`, `bitBoardAllPieces`,
`kingSquares`, `_whiteToMove` ([board.h:394-489](../basics/board.h#L394-L489)).

`BoardState` — hash, pawn hash, en passant square, castling rights, halfmove counters
([boardstate.h](../basics/boardstate.h)) — belongs here by location but is already handled
the way category 2 wants it: `doMove` mutates it, `undoMove` restores it with a single
assignment from the copy the caller kept. **It is the existing reference implementation of
the principle and stays exactly as it is.** Moving it into `PlyData` for the sake of
uniformity would touch every `getEP()`, `computeBoardHash()` and castling reader in the tree
and buy nothing.

### 2 — Iterative non-undoables

Values built up move by move. Copied from ply-1 at the start of the ply, adjusted by
`doMove`, never undone — the parent still holds its own copy.

`_pstBonus`, `_materialBalance`, `_pieceSignature`, `_imbalance`
([board.h:478-481](../basics/board.h#L478-L481)).

Today these live in `Board` and are undone by running the inverse piece operations
([board.cpp:257-277](../basics/board.cpp#L257-L277)), so the piece signature, the imbalance,
the material balance and the PST bonus are all recomputed a second time on the way back up.

**There is no way around the copy.** Whatever is not reconstructed by undo code has to be
copied. That trade is the whole point: one cheap copy on the way down instead of four
incremental updates per touched piece on the way back.

### 3 — Ply-computed

Rebuilt at this ply from the position, valid only for this ply.

- `attackMask[2]`, `pawnAttack[2]`, `pieceAttackMask[64]`
  ([movegenerator.h:333-344](../movegenerator/movegenerator.h#L333-L344))
- `pinnedMask[2]` ([movegenerator.h:339](../movegenerator/movegenerator.h#L339))
- the check-giving squares, today `SearchVariables::checkingBitmaps`
- `EvalResults` ([evalresults.h:40](../eval/evalresults.h#L40)) including `passedPawns` and
  the midgame factor — today a local built inside the evaluation and thrown away

This is the category that produces today's mess, so it gets the strictest rules. Note what
the normal answer would be and why it is rejected: encapsulation would put the attack tables
inside `Board` and let `Board` keep them current. That costs performance, which is why it was
not done — and the price for not doing it was a recomputation scattered over six call sites
and a class of bug that is easy to introduce and hard to see.

The way out is not to hide the update better. It is to make it impossible to miss.

**Rule 1 — one visible writer, at the top.** Nothing in category 3 is computed as a side
effect somewhere down the call hierarchy. `doMove` stops computing derived state altogether.
`updateAttacks(position, plyData)` stands explicitly in the search and in the quiescence
search, and `computePinnedMask` leaves the move generator and is called where moves are
actually generated. What everybody needs is shown offensively, not tucked away.

**Rule 2 — readers take `const PlyData&`.** Move generation and the evaluation's consumers
cannot write into the block at all, and the compiler enforces it. This replaces a validity
flag with an assert: a flag that says "not computed yet, so compute it now" is a stopgap, not
a solution, and it hides exactly the ordering question we want visible.

**Rule 3 — order is explicit and commented.** Because there is no lazy fallback, the update
calls in the search and in the quiescence search have to stand in the right order, and each
one carries a comment saying why it stands at that point and not earlier or later. Computing
something before it is needed costs time: `pinnedMask` is computed today only when moves are
really generated, so it belongs immediately before the move generation, not at the start of
the ply where every node that cuts early would pay for it.

**Consequence for anything the evaluation produces.** On a TT cutoff the evaluation never
runs, so no `EvalResults` exist at that node. Under these rules a reader in the search cannot
ask for them — that would need exactly the flag rule 2 rejects. The passed pawns survive a
pawn hash hit, because the pawn TT stores them
([pawntt.h:38](../eval/pawntt.h#L38), [pawn.h:90](../eval/pawn.h#L90)), but they do not
survive a node where the evaluation was skipped. So the search keeps its own passed-pawn
detection; the duplication is the smaller evil.

### 4 — Search steering

Needs access to other plies, so it cannot live in a per-ply block that is handed to `Board`.
Stays where it is, in `SearchVariables`.

Window and result (`alpha`, `beta`, `bestValue`, `bestMove`), `previousMove` — read from
ply-1 and ply-2 —, `isImproving`, which compares against the evaluation two plies up
([search.cpp:58](../search/search.cpp#L58)), the PV store, the killers, the move provider and
the TT data.

---

## 2. `PlyData`

`PlyData` carries categories 2 and 3. It is passed by reference into `doMove`, into move
generation and into the evaluation. `Board` stores neither category — not as a copy and not
as a pointer; it receives what it needs on every operation.

```
struct PlyData {
    Incremental incremental;   // category 2, copied from ply-1, adjusted by doMove
    Attacks     attacks;       // category 3, written by updateAttacks / updatePins
    EvalResults eval;          // category 3, written by the evaluation
};
```

In the search a `PlyData` lives inside `SearchNode`. The quiescence search has no
`SearchNode` — it owns its `PlyData` directly, which is the reason the type exists separately
from the search node instead of being folded into it.

Each block names its writer, and only that writer takes a non-const reference. Everyone else
— move generation, the evaluation's consumers, the pruning decisions — sees
`const PlyData&`. Long methods are not the enemy here; unannounced side effects on a shared
block are. Splitting the search into many small calls is only safe as long as it is visible
from the search itself which of them writes.

The test of whether the design holds is not "`undoMove` takes no `PlyData`" — it may well need
one. Undoing a capture requires knowing which piece was taken, and an engine that does not
encode the captured piece in the move has to read it from somewhere; that information is not
itself undoable. Qapla encodes it today ([board.cpp:261](../basics/board.cpp#L261)), so the
question stays open until a step actually needs it.

The invariant is narrower and holds either way:

> `undoMove` restores category 1. It may **read** whatever it needs to do so, but it must
> never roll back or recompute category 2 or 3.

The undo path gets placement-only piece primitives for exactly that reason.

---

## 3. Naming

| today | suggestion | reason |
|---|---|---|
| `searchparameter.h`, `class SearchParameter` | `search-config.h`, `class SearchConfig` | a bag of feature switches and fixed ply counts, not parameters in the tuning sense |
| `search-param.h`, `param<>()`, `class SearchParams` | `tunable.h`, `tunable<>()`, `class TunableParams` | the UCI tuning mechanism; `SearchParams` next to `SearchParameter` is the worst name collision in the tree |
| `SearchVariables` | `SearchNode` | every variable of that type is already called `node` |
| `searchvariables.h` | `search-node.h` | follows the type |
| `computeAttackMasksForBothColors()` | `updateAttacks()` | describes the purpose, not the implementation |
| `positionHashSignature` | `positionHash` | "signature" already means the piece signature |
| `checkingBitmaps` | `checkGivingSquares` | squares *from which* a piece type would check |
| `SearchVariables::computeMoves` | `generateMoves` | and it should stop setting `bestValue` as a hidden side effect |
| `moveNumber` | `movesTried` | it is a counter, not an ordinal |
| `pvMovesStore` | `pv` | |
| `Board::updateStateOnDoMove` | private, folded into `doMove` | public today for no reason |
| `MoveGenerator` | `Position` (optional, at the very end) | once the derived state is out, it is a position with move generation on it |

---


## 4. Steps

### How small a step has to be

**If the node count is wrong at the end of a step, it has to be obvious where the problem
is.** That is the sizing rule, and it is stricter than "one topic per step". As soon as
finding the cause turns into a search, the step was far too big — not slightly, massively.

Two consequences follow, and both are deliberate:

- **Throwaway code is cheap, debugging is not.** It is perfectly fine for a step to add
  scaffolding that the next step deletes: a second copy computed in parallel with the old one,
  a debug-only comparison between the two, an accessor that exists for one step. Writing and
  deleting that costs an hour. Hunting a wrong node count through a large diff costs a day.
- **The plan is not frozen.** Every step teaches something about what the code actually does.
  After each one, this plan may be adapted, steps split further, reordered or dropped. It is a
  working document, not a contract.

The standard tool for the risky steps is the **shadow comparison**: compute the new way next
to the old way, keep the old one as the source of truth, and compare the two under `assert` in
the debug build only. The node count cannot move, because nothing reads the new value yet —
and when the assert fires it names the field that diverged instead of leaving a wrong number
at the end of a run. Then, and only then, the source of truth is switched over in a step of
its own, and the scaffolding is deleted in a third.

That is why the sequence below has three steps where one would seem to do. The middle one is
the only one that can break, and it changes one thing.

### Measurement for every step

```
# correctness — must match exactly
~/bin/qet --settingsfile=test/epd/epd-wmtest-depth.ini \
    --engine name="Qapla current" cmd=<repo>/build/Release/Qapla
# expected: total nodes: 56158265

# performance — 3 runs, report median and spread
~/bin/qet --concurrency=1 --each proto=uci \
    --epd file=test/epd/wmtest.epd depth=16 \
    --engine name="Qapla ref" cmd=<repo>/build/Release/Qapla
# expected: total nodes: 118619321, ~25.0 s per run
```

`concurrency=1` is not a preference. With 4 parallel tasks the M4 heats up within a single
series of three runs and the times rise monotonically (7 % at depth 16, 8 % at depth 18). One
core barely heats it: 1 % spread. The untouched reference binary is kept at
`test/epd/ref/Qapla-ref`; do not delete it.

A step that changes the node count is a failed step. Not every step has to be faster, but the
sequence as a whole has to be. A debug build has to run the shadow asserts of the current step
at least once before the step counts as done — the release measurement alone does not exercise
them.

---

### Phase A — Names (4 steps, all mechanical)

Renames come first so that every later structural diff is readable. Each is its own step
because a mechanical rename that breaks the node count means the rename was not mechanical,
and that is worth seeing on its own.

**A1** — `searchparameter.h` → `search-config.h`, `class SearchParameter` → `SearchConfig`.
Includes and call sites only.

**A2** — `search-param.h` → `tunable.h`, `param<>()` → `tunable<>()`,
`class SearchParams` → `TunableParams`.

**A3** — `searchvariables.h` → `search-node.h`, `SearchVariables` → `SearchNode`. Type and
file only, no member touched.

**A4** — the member renames inside `SearchNode`: `boardState`, `positionHashSignature`,
`checkingBitmaps`, `moveNumber`, `pvMovesStore`.

Expect for each: both numbers identical.

---

### Phase B — Category 2, the incremental evaluation state (5 steps)

**B1 — group** — `struct Incremental { EvalValue pstBonus; MaterialBalance material;
PieceSignature pieceSignature; Imbalance imbalance; }` becomes a single member of `Board`.
All accessors keep their signatures and forward into it. Nothing moves out.

The step exists to surface every place that touches these four members.

Expect: both numbers identical.

**B2 — snapshot, unused** — `SearchNode` gets an `Incremental` field next to the `BoardState`
it already keeps. `doMove` fills it. Nothing reads it. Scaffolding.

Expect: both numbers identical — by construction, since no reader exists.

**B3 — shadow compare** — after `undoMove`, a debug-only `assert` compares `Board`'s
`Incremental` against the snapshot the node kept. Confirms the snapshot is complete and
correct *before* anything depends on it. Scaffolding; deleted in B5.

Expect: both numbers identical, debug build silent.

**B4 — switch the source of truth** — `Board` gets placement-only piece primitives (bitboards,
`_board`, `kingSquares` only) and `undoMove` uses them, restoring `Incremental` from the
node's snapshot instead of recomputing it.

The one step in Phase B that can break, and it changes exactly one thing. The B3 assert is
still in place and fires with a field name if the snapshot was incomplete.

Expect: node counts identical, **time lower** — the undo path loses four incremental updates
per touched piece.

**B5 — delete the scaffolding** — the B3 comparison goes; `Incremental` moves from `Board`
into `PlyData`.

Expect: node counts identical, time within noise.

---

### Phase C — Category 3, the attack tables (6 steps)

**C1 — group** — `struct Attacks { array<bitBoard_t,2> attackMask, pawnAttack, pinnedMask;
array<bitBoard_t,BOARD_SIZE> pieceAttackMask; }` becomes a member of `MoveGenerator`; the
existing public arrays stay reachable as references or accessors so the evaluation compiles
unchanged.

Expect: both numbers identical.

**C2 — a writer that takes its target** — `updateAttacks(position, Attacks&)` writes into a
caller-supplied block. `MoveGenerator::doMove` calls it with its own member, exactly as
before. Signature only; nothing moved.

Expect: both numbers identical.

**C3 — second block, written but unread** — `SearchNode` and the quiescence search each own an
`Attacks`. Explicit `updateAttacks` calls are placed in the search and in the quiescence
search where the masks are needed, each with a comment saying why it stands at that point.
`Board` still computes its own; all readers still read `Board`'s. Both are computed, which is
pure duplication for one step.

A debug-only `assert` compares the two blocks at every read point. This is the step that
proves the update calls sit in the right places, and it proves it with an assert that names
the position, not with a wrong node count at the end of a 25 second run.

Expect: node counts identical — no reader changed — and time worse, since everything is
computed twice. That is expected and temporary.

**C4 — switch the readers** — move generation, `isInCheck` and the evaluation read the ply
block and take `const PlyData&`. `Board`'s block is still computed and still compared.

The step where correctness is decided. If the node count moves, the cause is one of the update
calls placed in C3 — nothing else changed.

Expect: node counts identical.

**C5 — stop computing twice** — `MoveGenerator::doMove` stops calling `updateAttacks`, the
`Attacks` member and the C3 comparison leave `Board`.

Expect: node counts identical, **time lower**.

**C6 — remove the six defensive recomputations** — the calls at
[search.cpp:184](../search/search.cpp#L184), [192](../search/search.cpp#L192),
[389](../search/search.cpp#L389), [563](../search/search.cpp#L563),
[579](../search/search.cpp#L579), [626](../search/search.cpp#L626) are already writing into
nothing after C5. Deleting them is bookkeeping.

If the node count moves anyway, remove them one at a time — six one-line removals, and the
failing one names itself.

Expect: node counts identical, time lower.

---

### Phase D — Pin masks (3 steps)

**D1 — writer that takes its target** — `computePinnedMask` writes into a passed-in `Attacks`;
the move generator still calls it with `Board`'s block.

**D2 — move the call site** — the call leaves the move generator
([movegenerator.cpp:596-608, 636](../movegenerator/movegenerator.cpp#L596-L608)) and becomes
explicit in the search and in the quiescence search, **immediately before the move
generation** — not at the start of the ply. Today it only runs when moves are really
generated, and every node that cuts earlier would otherwise start paying for it.

Careful: it is computed for the side to move only. D2 reproduces that exactly, wrong as it is.
The fix is section 5, item 1.

**D3 — readers switch** to the ply block; `pinnedMask` leaves `Board`.

Expect for each: node counts identical.

---

### Phase E — Category 3, the evaluation results (2 steps)

**E1** — `EvalResults` ([evalresults.h:40](../eval/evalresults.h#L40)) is filled in the ply
block instead of in a local. The evaluation is the named writer; everything else sees it
const.

**E2** — delete the local and any leftover plumbing.

The passed pawns do **not** become a search input. On a TT cutoff the evaluation never runs,
so the block does not exist, and letting the search ask for it would need exactly the
on-demand flag the rules reject. [extension.h:88](../search/extension.h#L88) keeps its own
passed-pawn detection. The duplication is the smaller evil — recorded here as a decision, not
an oversight.

Expect: node counts identical, time within noise.

---

### Phase F — Finish (3 steps)

**F1** — `evalVersion` and `randomBonus` ([board.h:476-477](../basics/board.h#L476-L477)) are
engine configuration on a position object. Move them to where they are configured.

**F2** — decide about the remaining copies. Only now, with the structure in place, check
whether any copy introduced in B4 or C3 is worth removing, and whether `BoardState` should
follow into `PlyData` after all. Measurement decides; the default answer is "leave it".

**F3** — full comparison against `test/epd/ref/Qapla-ref` at depth 14 and depth 16, entry in
[version-log.md](version-log.md), tag only if the sequence is faster overall and the result is
simpler than what it replaced.

---

## 5. Out of scope — these change play

Found while reading. Each needs its own tag and its own SPRT and belongs in
[todo.md](todo.md), not here.

1. **`pinnedMask` is stale for the side not to move.** `computePinnedMask<COLOR>()` runs only
   for the moving side, but the evaluation reads `position.pinnedMask[COLOR]` for both colours
   ([queen.h:100](../eval/queen.h#L100), [rook.h:142](../eval/rook.h#L142),
   [bishop.h:99](../eval/bishop.h#L99)). The opponent's mask is left over from an unrelated
   position, so that part of the pin term is noise. Computing both changes the evaluation.

2. **`_tbHits` is never incremented.** Its only writer sits in the unreachable part of
   `Search::hasBitbaseCutoff` ([search.cpp:83](../search/search.cpp#L83)), so the UCI output
   reports 0 tablebase hits permanently. Reporting only, no effect on play.
