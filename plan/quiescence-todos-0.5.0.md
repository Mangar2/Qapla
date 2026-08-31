
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
