
---

## Node counts of all six variants

Every variant was built alone, from `70b0565`, in its own worktree, so no item carries another
one's change. The EPD run at fixed depth 14 is the reachability proof.

| tag | item | EPD nodes | vs. baseline | success |
|---|---|---|---|---|
| — | baseline `0.5.0-001` | 56158265 | — | 22 % |
| 0.5.0-002 | 4, no `doFutilityOnCapture` guard | 56158290 | +25 | 22 % |
| 0.5.0-003 | 3, only queen promotions exempt | 56158265 | **0** | 22 % |
| 0.5.0-004 | —, no mate distance cutoffs | 56158265 | **0** | 22 % |
| 0.5.0-005 | 2, node level delta pruning | 54558890 | −1599375 | 22 % |
| 0.5.0-006 | 5, beta probe on the pruning estimate | 56158265 | **0** | 22 % |
| 0.5.0-007 | 1, no tt cutoff on mate values | 54860445 | −1297820 | 18 % |

Three of the six change nothing at all, and one of those three cannot change anything in any
position. That result is worked out per item below.

---

## ToDo 5 — beta probe on the pruning estimate: dead code by construction

The probe cannot fire, and no value of `qsBetaSafetyMargin` can make it fire. The CLOP run the
ToDo asks for would have tuned a parameter that has no effect.

`computePruneForewardValue` gets its exchange value from `SEE::computeExchangeValue` with

```
threshold = alpha - standPatValue - margin
```

and that function is a windowed search, not an exact one. It sets `alpha = threshold - 1`,
`beta = threshold + 1` and `computeSEEValue` clamps `gain` into that window on every exit
([see.h:216-255](../search/see.h#L216-L255)). Two cases, no third:

- the captured piece alone is already below the threshold — `computeExchangeValue` returns that
  raw value early, and `standPat + margin + rawValue < alpha` follows from the definition of the
  threshold, so the move is pruned by the `continue` and never reaches the probe;
- otherwise the returned value is clamped at `threshold + 1`, so the estimate is at most
  `standPat + margin + threshold + 1 = alpha + 1`.

The probe therefore sees at most `alpha + 1 - qsBetaSafetyMargin`, and it tests that against
`beta > alpha`. It is false for every position and every non-negative margin. The identical node
count of `0.5.0-005` is the measurement that confirms it.

This is what the ToDo's own note anticipates: *"we currently cut SEE, once we found anything above
beta. If this here is successful, we might adapt SEE to cut only for beta + margin."* The
prerequisite is not optional, it is the whole thing — without a beta side SEE the probe has no
input that could ever exceed beta. The note also says that adapting SEE is explicitly not a ToDo
yet, so the item ends here.

**Decision: reverted.** The code stays commented out at its place with this finding, no CLOP and
no SPRT were run — there was nothing to measure.

---

## ToDo 3 — only queen promotions exempt: the distinction does not exist

Same picture as ToDo 5, and provable in one place. `MoveList::addPromote`
([movelist.h:88-95](../basics/movelist.h#L88-L95)) puts the queen promotion into the non silent
moves and the rook, bishop and knight promotions into the *silent* ones:

```cpp
addNonSilentMove(Move(move).setPromotion(QUEEN + COLOR));
addSilentMove(Move(move).setPromotion(ROOK + COLOR));
addSilentMove(Move(move).setPromotion(BISHOP + COLOR));
addSilentMove(Move(move).setPromotion(KNIGHT + COLOR));
```

Quiescence generates with `genNonSilentMovesOfMovingColor` and iterates with `selectNextCapture`,
which walks the non silent part of the list only. An under promotion therefore never reaches
`computePruneForewardValue`, and inside that function `move.isPromote()` and
`getPieceType(move.getPromotion()) == QUEEN` are the same predicate. The identical node count of
`0.5.0-003` is the confirmation of the argument, not a weak sample.

**Decision: reverted.** No SPRT — the two versions are the same program. The ToDo comment is
replaced by a note that the capture generation already answers the question.

---

## ToDo 2 — node level delta pruning: H0

The one item of the six that changed the search substantially, and it loses.

| | |
|---|---|
| tag | `0.5.0-005` |
| change | after the stand-pat beta cutoff, return `standPat + queen + margin` when that stays below alpha |
| margin | 200, queen taken as `getPieceValueForMoveSorting(WHITE_QUEEN)` = 1060 |
| EPD nodes | 54558890, −2.85 % — clearly wired in |
| SPRT | 5+0.01, H0 = −2, H1 = +3, alpha = beta = 0.05, concurrency 16 |
| result | **H0 accepted**, LLR −2.95 |
| score | 48.94 % over 6399 games, W 1606 / D 3051 / L 1742, ≈ −7 Elo |
| runtime | 66:33 |

Why it loses, as far as the code says: the cutoff fires when the stand-pat value is more than
1260 below alpha, and it then claims no capture can close that gap. Two things can:

- a capturing promotion gains the promotion *and* the captured piece, up to about 1500, and
  `computePruneForewardValue` exempts promotions from pruning for exactly that reason — the node
  level cutoff bypasses that exemption;
- the stand-pat value is a full evaluation, not a material count, so a capture moves the
  positional terms with it. Only the winning bonus is guarded against here, everything below it —
  king attack, passed pawns, threats — swings freely.

A queen plus 200 is a safe bound for a material-only stand pat. It is not one for this eval.

**Decision: reverted**, commented out at its place with the result. The ToDo makes the CLOP of the
margin conditional on H1 (*"if a margin of 200 results in h1, If so, CLOP the value"*), and H0
closes that gate. Note the direction the failure points: the margin would have to grow far beyond
the 400 upper limit of the proposed range, not shrink into the negative values the ToDo expected,
and at that size the cutoff stops firing at all.

---

## ToDo 1 — no tt cutoff on mate values: H0

The guard was added in both places at once, as the ToDo asks: the quiescence tt cutoff and the
main search cutoff in `SearchNode::probeTT`, which is the "same check below" the comment means —
it is the only other place that cuts on a tt value.

| | |
|---|---|
| tag | `0.5.0-007` |
| change | `abs(value) < MIN_MATE_VALUE` on both tt cutoffs |
| EPD nodes | 54860445, −2.31 % |
| EPD success | 22 % → **18 %** |
| SPRT | 10+0.01, H0 = −6, H1 = −1, alpha = beta = 0.05, maxgames 50000, concurrency 16 |
| result | **H0 accepted**, LLR −2.98 |
| score | 48.57 % over 6272 games, W 1425 / D 3242 / L 1605, ≈ −10 Elo |
| runtime | 122:36 |

The ToDo set the bounds to accept a small loss in exchange for mate search stability, and named
−1 Elo as the price it was willing to pay. The measured price is ten times that, and the EPD run
had already pointed the same way: the success rate on `wmtest`, a set built around mates and
tactics, dropped by four points. Blocking the cutoff does not stabilise the mate search here, it
takes away the mate scores the search had already proven and makes it prove them again.

Only 6272 of the 50000 games were needed; the run reached its bound on its own.

**Decision: reverted.** The ToDo comment is replaced by the result at the cutoff itself.

---

## The unnumbered ToDo — the two mate distance cutoffs: H0

*"test if we loose any elo, remove the following two checks"* — we do.

| | |
|---|---|
| tag | `0.5.0-004` |
| change | the two `alpha >= MAX_VALUE - ply` / `beta <= -MAX_VALUE + ply` returns at the node entry removed |
| EPD nodes | 56158265, identical to the baseline |
| SPRT | 5+0.01, H0 = −2, H1 = +3, concurrency 16 |
| result | **H0 accepted**, LLR −2.98 |
| score | 49.50 % over 12540 games, W 3008 / D 6398 / L 3134, ≈ −3 Elo |
| runtime | 129:02 |

This is the item worth keeping in mind for later work. The EPD run said the two checks never
fire — identical node count over 56 million nodes — and on that evidence alone they look like two
comparisons per quiescence node paying for nothing. The games say otherwise: they cost 3 Elo when
removed, and the run needed 12540 games, twice as many as any other item here, to reach its bound.

The reason is what the fixed depth run cannot contain. The checks fire when the search window
already carries a mate score, which is what the aspiration window does once a mate is found in a
real game. `wmtest` at depth 14 stops before that state is reached often enough to show up.

An identical node count proves a change is not reached *by that test set*. It does not prove the
code is dead — that needs an argument from the code, as in ToDo 3 and ToDo 5, and there the
argument exists and holds for every position.

**Decision: reverted**, the checks stay. The ToDo comment is replaced by the result.

---

## ToDo 4 — the `doFutilityOnCapture` guard: H0

| | |
|---|---|
| tag | `0.5.0-002` |
| change | the guard removed, so captures are pruned in thin material too |
| EPD nodes | 56158290, +25 against the baseline |
| SPRT | 5+0.01, H0 = −2, H1 = +3, concurrency 16 |
| result | **H0 accepted**, LLR −2.95 |
| score | 48.97 % over 6532 games, W 1630 / D 3137 / L 1765, ≈ −7 Elo |
| runtime | 72:22 |

Twenty-five nodes out of fifty-six million, and 7 Elo. `futilityOnCaptureMap` is false exactly
when the side owning the captured piece is down to two pieces or fewer, which is where the
evaluation stops being a smooth function of material and a futility bound stops meaning anything.
`wmtest` at depth 14 barely reaches those positions; games at 5+0.01 reach them constantly.

**Decision: reverted**, the guard stays.

---

## Summary

All six items are decided and all six are negative. Nothing from this round goes into the engine.

| # | item | evidence | outcome |
|---|---|---|---|
| 1 | no tt cutoff on mate values | SPRT H0, −10 Elo, 6272 games | reverted |
| 2 | node level delta pruning | SPRT H0, −7 Elo, 6399 games | reverted |
| 3 | only queen promotions exempt | dead code, proven from the move generation | reverted |
| 4 | no `doFutilityOnCapture` guard | SPRT H0, −7 Elo, 6532 games | reverted |
| 5 | beta probe on the pruning estimate | dead code, proven from the SEE window | reverted |
| — | no mate distance cutoffs | SPRT H0, −3 Elo, 12540 games | reverted |

Machine time: four SPRTs, 31743 games, about 6 hours 30 minutes of wall clock at concurrency 16,
two runs at a time on a 32 core machine. The two dead code items cost nothing — they were settled
by reading the code, and the identical node counts confirmed the reading.

Since every item was reverted, the working branch ends where it started, plus the comments that
record the results. No closing SPRT is needed: the rule exists to check a chain of *accepted*
changes, and this chain has none. `70b0565` remains the strongest known version.

### What the round is worth

Four of the six ToDos asked for an SPRT on code that had never been checked for reachability, and
two of those four could not have produced a result at all. Reading the code first — the move
generation for ToDo 3, the SEE window for ToDo 5 — settled both in minutes and saved two runs of
several hours each. That check is cheap and belongs before the tag, not after the run.

The opposite lesson comes from ToDo 4 and the mate distance cutoffs. Both looked inert in the EPD
run, 25 nodes and 0 nodes of difference, and both cost real Elo in games. A near zero node count
difference is not evidence that a change is harmless; it only says the test set does not reach it.
The two claims that *were* safe to make from the node count were the ones backed by an argument
from the code.

The direction of the results is also worth noting on its own: the quiescence search as it stands
resists every one of the four loosenings tried here. Its pruning is not obviously too conservative
in the places these items suspected.

---

# Nachtrag — three follow-up items

Asked for after the first round was closed. The baseline stays `0.5.0-001`; the working branch had
only comments on top of it at that point, so the saved baseline binary is still the right opponent.

## A — ToDo 2 with `abs(eval) < WINNING_BONUS`: the guard was already in the tested version

The proposal was to gate the node level delta pruning on `abs(eval) < WINNING_BONUS`, on the
grounds that once a winning bonus is in the evaluation, the gain a capture can produce is to take
that bonus away. `0.5.0-005` already carried exactly that gate:

```cpp
if (standPatValue > -WINNING_BONUS && standPatValue < WINNING_BONUS) {
    const value_t maxGain = position.getPieceValueForMoveSorting(WHITE_QUEEN) + margin;
    if (standPatValue + maxGain < alpha) return standPatValue + maxGain;
}
```

It was taken over from `computePruneForewardValue`, which guards its own pruning the same way.
The −7 Elo therefore have a different cause, and the two named in the ToDo 2 section above still
stand: a capturing promotion gains about 1500, more than the queen the bound allows, and the
evaluation terms *below* the winning bonus move with the capture.

One leak in the guard is worth writing down for whoever picks this up again. `evalendgame.cpp`
adds the bonus as `WINNING_BONUS - knightDistance` and `WINNING_BONUS + distanceValue * 2` in
places, so a position that carries a winning bonus can evaluate just under 10000 and slip through
`abs(standPat) < WINNING_BONUS`. The same leak exists in `computePruneForewardValue` today.

**No new run.** Repeating `0.5.0-005` unchanged would have measured the same thing twice. What the
proposal is really after — captures gaining more than material when the evaluation says the
position is worth more than its material — is item C, applied per move instead of per node.

## B — mate distance cutoff, `>=` and `<=`

The quiescence entry already used `>=` and `<=`. The two that differed were in
`Search::nonSearchingCutoff` ([search.cpp:453](../search/search.cpp#L453)):

```cpp
if (TYPE != SearchRegion::PV && alpha > MAX_VALUE - value_t(ply)) {
else if (TYPE != SearchRegion::PV && beta < -MAX_VALUE + value_t(ply)) {
```

`>` and `<` leave out the boundary itself, and the boundary is already unreachable: a mate at this
ply is worth exactly `MAX_VALUE - ply`, so `alpha == MAX_VALUE - ply` cannot be improved either.
Corrected to `>=` and `<=`, which also makes the two places agree.

| | |
|---|---|
| tag | `0.5.0-008` |
| EPD nodes | 56158265, identical — the boundary case does not occur at fixed depth 14 |
| SPRT | 5+0.01, **H0 = −5, H1 = 0**, maxgames 40000, concurrency 16 |

The bounds are the point here: this is a correctness tidy-up whose effect should be near zero, so
the run asks whether it *loses* Elo, not whether it wins any. H1 accepted means the correction is
free and stays.

*Result pending — the run is at 17495 games, 49.93 %, LLR 1.28 against the 2.94 bound.*

## C — an evaluation dependent part of the forward futility margin

The forward pruning skips a capture when `standPat + margin + see <= alpha`. The margin was one
fixed number. The idea behind the second term: everything the evaluation grants beyond the plain
material sits on pieces — an advanced passed pawn, the piece that carries a king attack. Capturing
such a piece takes the bonus with it, so the capture gains more than the exchange value says, and
the margin that decides whether it may be pruned has to grow with that part of the evaluation.

```cpp
const value_t fixedMargin = tunable<SearchConfig::optimizeQS, "qsAlphaSafetyMargin", 56, 0, 100>();
const value_t evalWeight  = tunable<SearchConfig::optimizeQS, "qsEvalMarginWeight", 20, -100, 100>();
return fixedMargin + (Eval::materialValue(position) - standPatValue) * evalWeight / 100;
```

The difference is taken as **material minus evaluation**, both from the view of the side to move,
so it is positive exactly when the opponent stands better than the material alone justifies — and
a positive weight then widens the margin, which is the direction the idea wants.
`Eval::materialValue` was added for it: the tapered material balance on the evaluation's own
scale, so the difference really contains nothing but the non material part.

Two things about the shape. The margin does not depend on the move, so it is computed once per
node now and handed to `computePruneForewardValue` — that also keeps the two places that use it
in step, which the existing comment there demands. And the weight starts at 0, which reproduces
the old expression exactly: node count 56158265 with the group flag false *and* true, as the rule
for a reformulation requires.

### The CLOP run

| | |
|---|---|
| build | `make Release` with `optimizeQS = true` |
| settings | `test/clop/clop-standard.ini`, 3000 samples, 4 games per sample, concurrency 16 |
| ranges | `qsAlphaSafetyMargin` 0..100 (centred on 50), `qsEvalMarginWeight` −100..100 (centred on 0) |
| runtime | 126:37 |

| parameter | estimate | rounded |
|---|---|---|
| `qsAlphaSafetyMargin` | 56.19 | 56 |
| `qsEvalMarginWeight` | 19.63 | 20 |

The weight comes out clearly positive, which is the sign the idea predicted. Twenty percent of the
distance between evaluation and material, on top of a fixed margin that moved from 50 to 56.

Both parameters were tuned in one run, as asked. They are additive terms over different inputs,
not two halves of one quotient, so they do not cancel each other out — but they do trade off
partially, and that is the reason the closing SPRT tests the pair, not each on its own.

### The SPRT

| | |
|---|---|
| tag | `0.5.0-009` |
| EPD nodes | 56595020, +436755 against the baseline — the new term is reached |
| EPD success | 19 % |
| SPRT | 5+0.01, H0 = −2, H1 = +3, concurrency 16, against `0.5.0-001` |
| result | **H0 accepted**, LLR −2.95 |
| score | 48.96 % over 6522 games, W 1616 / D 3154 / L 1752, ≈ −7 Elo |
| runtime | 67:46 |

CLOP found a point that loses. That happens — the estimate is a fit through noisy samples — but
the direction is readable and it is not noise.

The node count is the clue: 56595020 against 56158265, **+0.78 %**. The new term widens the margin
on average, so less gets pruned and more gets searched. And a wider futility margin is exactly
what the old measurements in the code comment say the engine does not want:
`100: 49%, 35: 50,3%, 40: 49,3%` — the fixed margin was tested below 50 and scored better there,
above 50 and scored worse.

So two things pushed the same wrong way at once: the fixed margin moved 50 → 56, and the new term
widened it further. Which of the two did the damage cannot be read off this run, and that is the
warning CLAUDE.md gives about parameters that trade off partially — they were tuned together
because the item asked for it, and the result cannot be split afterwards.

### Isolating the new term

`0.5.0-010`: the weight stays at 20, the fixed margin goes back to the 50 it had before the CLOP
run. One variable instead of two.

| | |
|---|---|
| tag | `0.5.0-010` |
| EPD nodes | 58956880, +2798615 against the baseline, **+5.0 %** |
| EPD success | 18 % |
| SPRT | 5+0.01, H0 = −2, H1 = +3, concurrency 14, against `0.5.0-001` |
| result | **H0 accepted**, LLR −2.95 |
| score | 48.86 % over 5936 games, W 1443 / D 2915 / L 1578, ≈ −8 Elo |
| runtime | 70:27 |

So the term does not carry the loss of `0.5.0-009` — it loses on its own, and by slightly more.
With the fixed margin held at the value it always had, adding the evaluation dependent part costs
8 Elo, and the node count says why it is not a small effect: **+5.0 %**, the margin widens
substantially and the forward pruning stops doing its work.

The mechanism the item describes is sound — a captured piece takes its positional bonus with it —
but the price of protecting those captures is paid on every other capture in the position, and
that trade comes out negative here.

Worth noting that the node count does not move monotonically with the margin: the fixed 56 build
searched *fewer* nodes than the fixed 50 build at the same weight. The margin enters twice, once
in the returned value and once in the SEE threshold, and the two pull against each other — which
is what the comment at that line has always warned about.

### A note on the build

`make ReleaseOpt`, which CLAUDE.md prescribed for search parameter runs, does not compile:
`PARAM_OPTIMIZE` turns `MaterialBalance::PAWN_VALUE_EG` into a runtime variable and
`eval/tablebase-value.h:50` initialises the `constexpr TB_CURSED_BONUS` from it. It is not needed
either — `tunable<>` never reads `PARAM_OPTIMIZE`, the group flag alone exposes the options, so a
normal `Release` build is the correct one and avoids the hundreds of eval options ReleaseOpt would
add. CLAUDE.md is corrected accordingly.

---

## D — ToDo 2 re-examined: the move loop was never the point

Objection raised after the first round: if `standPat + queen + 200 < alpha`, then the loop below
must forward prune every capture anyway, because the exchange value can never exceed the captured
piece and the captured piece is at most a queen. The cutoff should therefore be a pure saving of
the move generation and change nothing. So −7 Elo needs a different explanation than the one given
in the ToDo 2 section.

The objection is right. `computePruneForewardValue` returns at most
`standPat + margin + queen = standPat + 50 + 1060 = standPat + 1110`, and the cutoff condition
asks for `standPat + 1260 < alpha`, so every move it covers is below alpha and gets pruned.

Two variants, both built from `70b0565`, separate the possible causes.

**delta2** — the same cutoff, but with all three exemptions of `computePruneForewardValue`
mirrored at node level, not just the winning bonus: no promoting pawn on the seventh rank, and
`doFutilityOnCapture` true for the opponent's colour. That last one is one query, not one per
move, because the map is indexed by the colour of the captured piece alone.

**delta3** — a diagnostic build. The identical condition, but the move loop runs **unchanged** and
only the returned value is replaced at the end. It isolates the looser bound from the skipped loop.

| build | EPD nodes | vs. baseline |
|---|---|---|
| baseline `0.5.0-001` | 56158265 | — |
| `0.5.0-005`, as tested, no promotion / futility exemption | 54558890 | −1599375 |
| delta2, all exemptions mirrored, loop skipped | 54818593 | −1339672 |
| delta3, all exemptions mirrored, loop **kept**, value replaced | **54818593** | −1339672 |

delta2 and delta3 agree to the node. Skipping the loop changes nothing whatsoever — the objection
is confirmed exactly, not approximately. What is left splits like this:

| cause | nodes |
|---|---|
| the replaced fail low value | 1339672 |
| the two missing exemptions | 259703 |
| skipping the move loop | **0** |

So the dominant effect, by five to one, is the one the ToDo 2 section named last and treated as
secondary: the returned value. A node with no capture at all returns `standPat` in the old code and
`standPat + 1260` in the new one. Both are sound upper bounds for a fail low, the new one is just
1260 centipawns looser, and it travels into the parent's `bestValue` and from there into the
transposition table.

That reframes the item. The cutoff is not a pruning decision at all — it is a *value* change with a
free move generation saving attached. Whether it is worth anything therefore depends on one
question only: does the looser fail low bound cost more than the skipped move generation gains.
delta2 is the version that asks it cleanly, and it is the one to run.

*SPRT pending — queued behind the two follow-up runs.*

### The fixed margin scan, and where the item stands

`0.5.0-011` holds the weight at 20 and puts the fixed margin at 30, below the 50 it always had.
It is built, tagged and measured, but **its SPRT has not been run** — the machine is needed
elsewhere.

| version | fixed margin | weight | EPD nodes | vs. baseline | EPD success | SPRT |
|---|---|---|---|---|---|---|
| baseline `0.5.0-001` | 50 | 0 | 56158265 | — | 22 % | — |
| `0.5.0-009` | 56 | 20 | 56595020 | +0.8 % | 19 % | H0, −7 Elo, 6522 games |
| `0.5.0-010` | 50 | 20 | 58956880 | +5.0 % | 18 % | H0, −8 Elo, 5936 games |
| `0.5.0-011` | 30 | 20 | 55459078 | **−1.2 %** | **22 %** | not run |

`0.5.0-011` is the only one of the three whose node count falls below the baseline and whose EPD
success rate stays at 22 %. Both of the measured ones lose, and both search *more* nodes than the
baseline — the same direction the old comment at that line already recorded for a widened margin.

If `0.5.0-011` is ever run and looks good, it needs a control at **fixed margin 30 with weight 0**
before anything is attributed to the new term. Two things differ from the baseline in it, and the
old data (`35: 50,3%`) says a smaller fixed margin may well win on its own.

## Open at the end of the first session

Built and tagged, not run:

- `0.5.0-011` — fixed margin 30, weight 20. State file `test/log/sprt-qs-fixed30.state` holds a few
  hundred games from a run that was stopped; the same call continues it.
- **delta2** — branch `qs-delta2`, the node level delta pruning with all three exemptions mirrored.
  Proven to leave the search identical apart from the returned fail low value; the SPRT asks
  whether that looser bound costs more than the saved move generation gains.
- **0.5.0-008**, the mate distance `>=` / `<=` correction, is still running at the time of writing.

---

# The three follow-up points, all decided

Run in the order agreed: the fixed margin first, because it sits inside every other variant, then
the tuning of the new term around it, then the node level cutoff.

| # | version | change | result |
|---|---|---|---|
| 1 | `0.5.0-014` | `qsAlphaSafetyMargin` 35 instead of 50 | H0, −2 Elo, 18238 games |
| 2 | `0.5.0-015` | evaluation dependent margin, CLOP values 144 / 31 | H0, −4 Elo, 11641 games |
| 3 | `0.5.0-016` | node level delta pruning, all exemptions mirrored | H0, −6 Elo, 7401 games |

Nothing survives. Details per version in `plan/version-log.md`.

## What the three add up to

**Point 1 retired a number that had been guiding decisions.** The comment `100: 49%, 35: 50,3%,
40: 49,3%` above the margin read as a measured curve pointing at 35. Measured properly, 35 loses
two Elo. Three percentages without an error range are three indistinguishable numbers, and in the
source they were worse than nothing, because they suggested a direction. They are gone.

**Point 2 closed the evaluation dependent margin.** Five versions, two CLOP runs, and the best
outcome the idea ever reached is "costs nothing, gains nothing" (`0.5.0-013`). Both CLOP runs
produced points their confirming SPRT rejected — on a surface this flat, four games per sample fit
noise, and the estimate is worth nothing until the SPRT has spoken.

**Point 3 put a number on bound precision.** Because delta3 had already proven that skipping the
move loop changes nothing, this run measured one thing in isolation: a fail low value up to 1260
centipawns looser than necessary. That alone is −6 Elo, far more than the move generation it saves.

## Loop invariance, measured

The margin computation was suspected of being loop invariant waste, recomputed for every capture
although it depends only on the position and the stand pat value. Hoisting it to the node is
behaviour neutral — identical node count, 55197564 — so the runtime criterion decides. Three
interleaved pairs:

| | run 1 | run 2 | run 3 |
|---|---|---|---|
| in the loop | 3.065 | 3.063 | 3.086 |
| hoisted | 3.077 | 3.070 | 3.095 |

The hoisted version is slower in every pair, by about 0.3 %. `computePruneForewardValue` has three
early returns in front of the computation — winning bonus, promotion, `doFutilityOnCapture` — so in
the loop it runs only for moves that pass all three, and never at all in a node above the winning
bonus. Hoisted it runs once per node unconditionally. Below roughly one surviving move per node
that is a loss, and quiescence is below it.

The computation stays in the loop. The compiler cannot hoist it either — `doMove` writes the
material balance inside the same loop — but here that is the better outcome, not a missed
optimisation.
