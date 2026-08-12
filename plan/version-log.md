# Version log

One entry per tagged version. Node counts come from the fixed depth EPD run
(`test/epd/epd-wmtest-depth.ini`), Elo from the SPRT winrate. A behaviour neutral change
keeps the node count and gets no SPRT; every other change gets its own tag and its own run.

qet exit codes: 14 = H1 accepted, 15 = H0 accepted, 16 = undecided within maxgames.

## 0.4.0-035 — ClockManager: maxTime near the reserve

`b9088be`. The old code capped `maxTime` at `timeLeft - MIN_REMAINING_TIME` and collapsed to
50-200 ms per move once the clock approached the 2000 ms reserve. The new lower bound of
`timeLeft / 5` stops that; above the reserve the new code is in fact more frugal than the old
one (1000 vs 1666 ms at 5000 ms left).

- EPD nodes: 111958440
- SPRT vs 0.4.0-034, run at two time controls:

| TC | Result | Winrate | Elo | Games |
|---|---|---|---|---|
| 5+0.01 | H1 accepted | 65.69 % | ≈ +113 | 548 |
| 20+0.01 | H1 accepted | 51.06 % | ≈ +7.4 | 6809 |

The gap between the two time controls is the point of this entry: the flaw only bites below
the 3000 ms mark, and at 20 s base that is a much smaller share of the game. Kept.

## 0.4.0-036 — History sorting of the silent moves

`03bb135`, preceded by the behaviour neutral rename `9d8beb9`.

`sortFirstSilentMoves` never updated its running best weight, so the comparison ran against
the constant 0 for every candidate. The result was the *last* move with a positive history
instead of the best one — the history values had no influence on the order at all. Fixed by
tracking the best weight, plus an early exit once nothing above the floor is left.

- EPD nodes: 111958440 → **109978598** (−1.8 %)
- SPRT vs 0.4.0-035 at 10+0.01: **H1 accepted**, 53.01 %, ≈ +21 Elo, 2396 games

Kept.

## 0.4.0-037 — Sort 14 instead of 7 silent moves

`AMOUNT_OF_SORTED_NON_CAPTURE_MOVES` 7 → 14. The old value predates the fix above, so it was
never chosen against a sorting that actually orders by history value.

- EPD nodes: 109978598 → **106629546** (−3.0 %)
- SPRT vs 0.4.0-036 at 10+0.01: **inconclusive**, 49.96 %, LLR −0.73 after the full 20000 games

Reverted (`6d6332f`), kept as branch `dead/sort14`. Three percent fewer nodes at exactly equal
strength means the extra sorting work eats the search gain it produces. The LLR is nowhere near
either bound, so continuing with a raised game limit is not indicated — the result is flat, not
undecided for lack of games.

This also cancels the two experiments that depended on it: sorting *all* silent moves was to be
tried only if 14 beat 7, and the CLOP on the amount only if the amount mattered at all. It does
not.

## 0.4.0-038 — Sort silent moves with a negative history too

`SORT_WEIGHT_FLOOR` from 0 to `-MAX_VALUE`. Only moves with a positive history were sorted so
far; the split at 0 had no reason beyond the constant the old broken loop compared against.

Caveat worth remembering: the butterfly values are not bounded by `MAX_VALUE` — `MAX_HIST` is
0x70000000 — so a move whose history has dropped below −30000 still keeps its generated
position. The floor is a category mix of a search value and a history value; it covers the
overwhelming majority of moves but not all of them.

- EPD nodes: 109978598 → **105543179** (−4.0 %)
- SPRT vs 0.4.0-036 at 10+0.01: **H1 accepted**, 51.55 %, ≈ +11 Elo, 4674 games

Kept.

## 0.4.0-039 — Recapture ordering with real piece values

With a queen worth 9 the bonus of 10 put every recapture ahead of every other capture. When
the simple piece values were replaced by real ones the constant stayed, and the bonus degraded
to a tie break between captures of equal value. Now the queen value plus one is subtracted from
the non recaptures, read from the piece values rather than hard coded, so taking back a pawn
outweighs winning a queen elsewhere again.

- EPD nodes: 105543179 → **120686976 (+14.3 %)**, success rate 19 % → 27 %
- SPRT vs 0.4.0-038 at 10+0.01: **H0 accepted**, 49.35 %, ≈ −4.5 Elo, 10018 games

Reverted (`f0f3739`), kept as branch `dead/recapture-priority`.

The node count called this one before the SPRT did: +14.3 % at fixed depth means the ordering
got worse, and putting every recapture ahead of a higher valued capture is exactly the kind of
rule that fights MVV/LVA. The success rate jumped from 19 % to 27 % at the same time and pointed
the other way — 8 positions out of 100, and wrong. Worth remembering the next time the EPD
success rate looks tempting as a shortcut.

The magic 10 is back in the tree with the revert, still a tie break between captures of equal
value. Renaming it into a documented constant is a behaviour neutral change and still open.

## 0.4.0-040 — Queen promotion in front of the captures

A promotion that captures nothing weighed 0, so it ranked behind every capture including pawn
takes pawn. It now gets one more than the most valuable piece that can be taken, on top of what
it captures itself.

Only the promotion to queen is affected. `addPromote` puts it into the non silent list while the
under promotions are generated as *silent* moves — they never reach `computeCaptureWeight` and
never appear in the quiescence capture list at all.

- EPD nodes: 105543179 → **111245334** (+5.4 %)
- SPRT vs 0.4.0-038 at 10+0.01: **stopped and recorded as not successful** at 11867 games,
  49.84 %, LLR −1.04

Reverted, kept as branch `dead/queenpromo-front`. The run was cut short by hand rather than
carried to a bound; at −1.04 after nearly 12000 games it was drifting towards H0, not towards a
gain. Sorting the promotion ahead of *everything* repeats the pattern that already failed in
0.4.0-039. The follow up is to place it at the end of the winning captures instead.

## 0.4.0-041 — Sort 14 silent moves, second attempt

`AMOUNT_OF_SORTED_NON_CAPTURE_MOVES` 7 → 14 again. The first attempt (0.4.0-037) was flat, but
back then only moves with a positive history took part in the sorting at all. Since 0.4.0-038
the negative ones are sorted too, so the additional slots have more to work with.

- EPD nodes: 105543179 → **106648627** (+1.0 %)
- SPRT vs 0.4.0-038 at 10+0.01: **inconclusive**, 49.99 %, LLR −0.50 after the full 20000 games

Reverted, kept as branch `dead/sort14b`. Identical to the first attempt, so the amount of sorted
moves does not matter — not with a positive history filter and not without one. Worth noting
where the node counts landed: 0.4.0-037 saved 3.0 % against its base with the extra slots, this
one costs 1.0 % against its base. Sorting the negative histories in 0.4.0-038 had already
collected what the extra slots used to bring. The parameter is settled at 7.

## Promotions — closed without a change

Both parts of the promotion plan turned out to need no code:

- **Under promotions** are already ordinary silent moves. `addPromote` adds them through
  `addSilentMove`, `sortNonCaptures` weights them by history like any quiet move, and they never
  appear in the quiescence list at all.
- **The queen promotion already sits at the end of the winning captures.** Without a capture its
  weight is 0, below every capture, and the loosing captures leave the stage through the deferral
  malus. It is the last move `GOOD_CAPTURES` hands out.
- A suspected SEE defect on capturing promotions **does not exist**. `isLoosingCapture` only runs
  the exchange when the moving piece is more valuable than the captured one, which for a pawn is
  never true — the cheapest capturable piece is a pawn and the test is strict. No pawn capture is
  ever rated loosing, so no capturing promotion is ever deferred.

## 0.4.0-042 — Full exchange value as capture weight

The weight was the value of the captured piece, so the capturing piece never took part and a
queen taking a defended pawn ranked by the pawn. It is now the net result of the whole exchange,
from a new `computeExactExchangeValue` with a fully open window — `computeExchangeValue` cannot
serve, its result is a bound outside the threshold window and a bound carries no order.

A loosing capture is now simply one with a negative weight, so the separate `isLoosingCapture`
call in the selection loop is gone.

- EPD nodes: 105543179 → **114313553 (+8.3 %)**, EPD runtime 6.2 s → 7.9 s
- SPRT vs 0.4.0-038 at 10+0.01: **H0 accepted**, 49.03 %, ≈ −6.7 Elo, 6939 games

Reverted (`b7253e9`), kept as branch `dead/see-weight`. Node count and runtime both pointed the
wrong way before the SPRT confirmed it, and the reason holds up: exchange values collapse every
equal trade to 0 and throw away the discrimination the value of the captured piece provided —
rook takes rook and pawn takes pawn become indistinguishable. On top of that the SEE runs for
every capture in the preparation stage instead of only for the captures the selection reaches.

The untried variant is to keep the exchange value for the good/bad split only and order the
winning captures by the captured piece as before, which is what most engines do: MVV/LVA for the
order, SEE for the split.

## Where this left the engine

Two of seven changes survived. The head carries 0.4.0-038 behaviour.

| Tag | Change | Verdict |
|---|---|---|
| 036 | History sorting repaired | **kept**, ≈ +21 Elo |
| 037 | 14 instead of 7 sorted moves | flat, `dead/sort14` |
| 038 | Negative histories sorted too | **kept**, ≈ +11 Elo |
| 039 | Recapture ahead of every capture | −4.5 Elo, `dead/recapture-priority` |
| 040 | Queen promotion ahead of everything | stopped at 49.84 %, `dead/queenpromo-front` |
| 041 | 14 sorted moves, second attempt | flat, `dead/sort14b` |
| 042 | Exchange value as capture weight | −6.7 Elo, `dead/see-weight` |

The head EPD node count is back at 105543179, identical to 0.4.0-038, which confirms the reverts
are complete.

Every change that pushed a rule ahead of the material order lost — the recapture priority, the
promotion priority, and the exchange value replacing the captured piece value. The two that won
did not reorder anything by decree, they made an existing mechanism work as it was meant to.

## Still open

- [ ] Rename the recapture bonus of 10 into a documented constant. Behaviour neutral, verifiable
      by node count. It is a tie break between captures of equal value today, not the priority it
      was when a queen was worth 9.
- [ ] The comment on `GOOD_CAPTURES` still names `SILENT_MOVES`, renamed to `REMAINING_MOVES` in
      `9d8beb9`.
- [ ] A closing SPRT of the head against 0.4.0-035, see below.

## 0.4.0-043 — Smooth ramp for the maximum move time

The share of the remaining time a move may spend was `max((timeLeft - 2000) / 3, timeLeft / 5)`.
The second term dominated everywhere below 10 seconds left, so the share sat at a flat fifth and
only started growing above that. Replaced by a ramp: a fifth below one second left, a third from
three seconds on, a steady climb in between. Sampling every millisecond from 0 to 10000 the curve
has no step larger than a millisecond and stays monotone, where an intermediate stepped variant
jumped by 251 ms at 3000.

- EPD nodes: unchanged at 105543179 — and meaningless here. A depth limit makes
  `isInfiniteSearch()` true, so `computeMaxTime` is never reached in that test. The number only
  confirms the search itself was not touched.
- SPRT vs 0.4.0-038 at 10+0.01: **stopped undecided** at 18359 games, 49.78 %, LLR −2.11

Not reverted. The ramp stays in the tree and 0.4.0-044 varies its foot point instead; the run was
close to the H0 bound and drifting there, so the ramp starting at 1000 ms is not the version to
keep.

## 0.4.0-044 — Ramp starting at 2000 ms

`RAMP_BEGIN_TIME` 1000 → 2000. The ramp keeps its 2000 ms length, so it moves up as a whole: the
share stays at a fifth below 2000 ms and reaches the full third at 4000 instead of 3000. The more
cautious of the two.

- SPRT vs 0.4.0-038 at 10+0.01: **inconclusive**, 49.99 %, LLR −0.46 after the full 20000 games

Measured against 038, not against 043, so the run decided the whole ramp change rather than the
parameter alone — and the answer is that at 10+0.01 the ramp is worth nothing measurable. 043 was
at 49.78 % and drifting to H0, 044 is flat at 49.99 %, so the higher ramp foot is the better of
the two without being a gain.

Worth putting next to the 0.4.0-035 result: the same kind of change was worth ≈ +113 Elo at
5+0.01 and ≈ +7.4 at 20+0.01. Time management pays where the clock actually gets tight. A ramp
that only alters the share between 2000 and 4000 ms left may simply never bind often enough at
10 seconds base to show up.

**Kept anyway**, by decision, not by measurement: the ramp costs nothing, and the time management
it produces is the preferred behaviour. This is the baseline from here on.

## Time control change

From here on both `test/sprt/sprt-standard.ini` and `test/clop/clop-standard.ini` run at
**5+0.01** instead of 10+0.01. Results before this point were measured at the old control and are
not directly comparable — see 0.4.0-035, where the same change was worth ≈ +113 Elo at 5+0.01 and
≈ +7.4 at 20+0.01.

## 0.4.0-045 — King attack: queen term on the attacking side

`computeAttackValue` raised the attack index when the side *whose king is examined* had a queen.
It has to be the attacker.

- EPD nodes: 105543179 → 104822381, success rate 19 % → 25 %
- SPRT vs 0.4.0-044 at 5+0.01: **stopped at 18450 games**, 50.11 %, LLR 0.46, heading nowhere

Not reverted. The `attackWeight` curve was fitted with the error in place, so the fix alone was
always going to be measured against weights that compensate for it. 0.4.0-046 is the same change
with the weights re-tuned, and that is what the SPRT decided.

## 0.4.0-046 — King attack weights re-tuned for the corrected queen term

CLOP over all eight king attack parameters, ranges centred on the current values and deliberately
narrow — a re-optimization, not a first fit. The seven support points of the curve move from
0, −5, −15, −35, −105, −230, −710 to 0, −3, −13, −32, −85, −234, −658: the middle softens, the
region around index 20 hardens, the far end eases by about 7 %.

Index 0 means no attack at all. CLOP proposed 0.51 there; it is kept at 0 by hand, a standing
bonus for nothing makes no sense.

`queenFactor` came out at 2.65 and rounds back to the 3 it already had. Worth recording: an
earlier CLOP on this parameter had produced 2, and 2 lost in play, which is why 3 had been
restored. That run tuned the term while it still counted the defender's queen. With the term on
the correct side the optimum agrees with the value that had proven itself.

- EPD nodes: 104822381 → **107064904**, success rate 25 % → 29 %
- SPRT vs 0.4.0-044 at 5+0.01: **H1 accepted**, 50.82 %, ≈ +5.7 Elo, 9613 games

Kept, new baseline. The fix alone was flat, the fix plus the re-tuning is worth ≈ +5.7 Elo —
which is the whole argument for tying a re-tuning to a change that shifts what the weights were
fitted to.

## 0.4.0-047 — LMR reshaped, divisors tuned

`computeLMR` is now one ramp product with tunable coefficients instead of a hand written ladder.
The move number ramp and the depth ramp are shared by all node types; PV nodes differ only in the
divisor. Captures are no longer exempt: a capture that wins material is never reduced, a loosing
one keeps the late move term and only gets a larger divisor. `lmrNotImprovingAdd` raises the
numerator when the position is not improving, but only where a reduction already happens.

Values that can only take a handful of integers — the minimum ply, the ramp offsets and slopes,
the divisor of the flat branch — are compile time constants. A CLOP run gets no signal out of a
range of three or five values, and a UCI option it cannot move only clutters the list.

The first attempt put all 17 values into one run. Two things were wrong with that: CLOP cannot
separate that many parameters, and numerator and denominator of the same division were in the same
run, where countless pairs give the same quotient and neither side can be estimated. Split into a
denominator run (this version) and a numerator run.

CLOP over `lmrMoveBreak`, `lmrPvDivisor`, `lmrDivisor`, `lmrCaptureDivisorAdd`, 4000 samples,
91 min: 7 → 6, 448 → 512, 192 → 261, 256 → 245. The divisors go back to exactly where they stood
before `lmrNotImprovingAdd` existed. Lowering them by half of that term, as they were seeded, was
wrong — the extra reduction for a non improving position is wanted on top, not compensated away.

- EPD nodes: 107064904 → **92945122**, success rate 29 % → 25 %
- SPRT vs 0.4.0-046 at 5+0.01: **H1 accepted**, 50.82 %, ≈ +5.7 Elo, 9591 games

Kept, new baseline. The success rate falling while the engine plays measurably better is the
familiar picture — the EPD set rewards depth on tactical positions, and this version searches
13 % fewer nodes for the same time.

## 0.4.0-048 — LMR numerator coefficients tuned

Second CLOP over the same function, this time the numerator side: `lmrMoveBase` 16 → 11,
`lmrMoveHighBase` 32 → 51, `lmrDepthBase` 16 → 11, `lmrRampMin` 16 → 15, `lmrRampMax` 48 → 55,
`lmrNotImprovingAdd` 128 → 133. 5000 samples, 114 min.

The move number ramp becomes far steeper. Below the break point it is flatter than before, above
it the base jumps to 51 — at the break the ramp steps from 23 to 51. Almost all of the reduction
now hangs on crossing move number 6. `lmrNotImprovingAdd` lands on its seed value, which is a
second, independent confirmation of the term.

- EPD nodes: 92945122 → **90963468**, success rate 25 % → 29 %
- SPRT vs 0.4.0-047 at 5+0.01: **H1 accepted**, 52.06 %, ≈ +14.3 Elo, 3690 games

Kept, new baseline. By far the largest single step of this series, and it came from splitting one
unusable 17 parameter run into two clean ones.

## 0.4.0-049 — counter check: captures are not reduced at all

`computeLMR` returns 0 for every capture again, the exemption it had before 0.4.0-047. The
question was whether reducing loosing captures, with a divisor tuned for exactly that, buys
anything.

- EPD nodes: 90963468 → **91160837**
- SPRT vs 0.4.0-048 at 5+0.01: **undecided after 20000 games**, 50.21 %, LLR 1.30

Kept. Undecided counts against the capture term here, and the small positive LLR is if anything a
hint the other way. `lmrCaptureDivisorAdd` and the `isLoosingCaptureLight` call are out of the
code, not merely bypassed — node count after the removal identical at 91160837.

The term was not badly tuned, it simply has almost nothing to work on: only loosing captures were
affected and the whole difference is 0.2 % of the nodes.

## 0.4.0-050 — move count pruning with divisors of its own

The pruning had been derived from the reduction: skip the move once `depth - lmr` fell below zero,
so it carried no value that a run could move. It now reads the same ramp weight and compares it
against `(depth + 1) * mcpDivisor`, with `mcpDivisor` / `mcpPvDivisor` independent of the reduction
divisors. Defaults equal to the reduction divisors reproduce the derived form exactly — the
restructuring itself came out at 91160837 nodes, unchanged.

CLOP over the two, 2500 samples, wide ranges: 512 → 592 and 261 → 354, both clearly larger, so the
run wanted less pruning.

- EPD nodes: 91160837 → 102983953, success rate 26 % → 23 %
- SPRT vs 0.4.0-049 at 5+0.01: **H0 accepted**, 48.57 %, ≈ −10 Elo, 4946 games

Reverted, `dead/mcp-divisors`. Only the two values went back, the restructuring stays — the
pruning keeps its own coefficients.

A CLOP optimum losing 10 Elo is worth recording as a warning: 2500 samples over a two dimensional,
flat landscape produce an argmax that is mostly noise. Where CLOP and SPRT disagree this clearly,
the SPRT is the answer. The direction below the default is untested; the run explored 61 to 461 and
found nothing there either.

## Not tagged — the separated move count pruning taken back out

`0.4.0-050` had kept its structure after the value revert: `computeLateMoveDecision` returned both
the reduction and the pruning decision, the pruning reading the ramp weight against `mcpDivisor` /
`mcpPvDivisor`.

Taken back out. The two are not independent in the way the split assumed: with divisors that differ,
the pruning can skip a move the reduction would still have searched, or search one the reduction has
already reduced past the remaining depth — in that second case with a negative depth, answered out
of the quiescence. `computeLMR` returns a plain `ply_t` again and the pruning is `depth - lmr < 0` in
the move loop, where `lmr > depth` cannot survive.

Node count after the rollback 91160837, identical to `0.4.0-049`.

Worth keeping in mind for the tuning: the `0.4.0-047` and `0.4.0-048` runs were made on exactly this
derived logic, `mcpDivisor` did not exist yet. Their values are valid for the code as it stands. What
the derived form does mean is that `lmrDivisor` moves the pruning threshold along with the reduction,
so those two effects cannot be told apart in a run.

## 0.4.0-051 — passed pawn pushes are reduced less

Rebuilt on `0.4.0-049` after the first attempt had been made on the discarded structure. The
divisor gets `lmrPassedPawnDivisorAdd` for a pawn no opponent pawn can stop; a promotion counts as
such a push. Since the move count pruning reads the reduction, the same value also makes those
moves survive the pruning longer — one coefficient, two effects, and that is what was tuned here.

The eval keeps its passed pawns in the pawn hash and the move loop never sees them, so
`search/passedpawn.h` carries a front span table of its own, one lookup and one AND per quiet pawn
move.

CLOP over the single value, 2000 samples: seed 128 → 112. The first, discarded run had said 141 for
the weaker version of the same parameter.

- EPD nodes: 91160837 → **90280196**
- SPRT vs 0.4.0-049 at 5+0.01: **H1 accepted**, 50.86 %, ≈ +6.0 Elo, 8883 games

Kept, new baseline.

## Not tagged — passed pawn pushes, first attempt

`lmrPassedPawnDivisorAdd` on the divisor, a promotion counting as such a push, front span table in
`search/passedpawn.h` because the eval keeps its passed pawns in the pawn hash and the move loop
never sees them.

CLOP over the single value, 2000 samples, 46 min: seed 128 → 141.

Not tested. The run was made on the build with the separated pruning, where the larger divisor left
the pruning threshold untouched. Under the derived form it moves the threshold as well, so the
estimate does not carry over. Redone from scratch on `0.4.0-049`.

- EPD nodes with the term at 141: 88402616, at 0: 91160837 — the second is the control that proves
  the rest of the rollback is neutral.

## 0.4.0-052 — all futility margins tunable and re-optimized

Both margins had been fixed expressions of the same shape, `factor * (depth + 1)` with a hard 100
for improving — one slope that dragged the constant part along and an improving term without a
coefficient of its own. They now sit at their call sites with three independent values each, and
nothing is shared between the forward futility and the move loop futility. The depth limits stay
constants, they are ply counts. The restructuring came out at 90280196 nodes, unchanged, with the
flags off and on.

One CLOP over all seven values including `qsSafetyMargin`, 5000 samples, 114 min. They are all
summands of the same threshold, so none of them can cancel another.

| | old | new |
|---|---|---|
| `ffDepthFactor` | 75 | 83 |
| `ffBase` | 75 | 69 |
| `ffImprovingBonus` | 100 | 101 |
| `futDepthFactor` | 75 | 43 |
| `futBase` | 75 | 80 |
| `futImprovingMalus` | 100 | 77 |
| `qsSafetyMargin` | 47 | 50 |

The forward futility becomes more conservative — it cuts at `eval - margin >= beta`, so a larger
margin cuts less. The move loop futility becomes clearly more aggressive: it cuts at
`eval + captured + margin < alpha`, where a smaller margin cuts more, and its depth slope nearly
halves. `qsSafetyMargin` confirms itself, it had been tuned not long ago.

- EPD nodes: 90280196 → **86564295**, success rate 26 % → 24 %
- SPRT vs 0.4.0-051 at 5+0.01: **H1 accepted**, 51.02 %, ≈ +7.1 Elo, 7624 games

Kept, new baseline.

## 0.4.0-053 — IID also in cut nodes

First of the four experiments of todo item 5. The pre search ran in PV nodes only; it now runs in
cut nodes as well, all nodes stay out. The node type existed already but was only set by
`setFromParentNode`, which runs after the pre search — `childNodeType` computes it from the parent
instead, and `setFromParentNode` uses the same function.

- EPD nodes: 86564295 → 91108889, success rate 24 % → 21 %
- SPRT vs 0.4.0-052 at 5+0.01: **H0 accepted**, 49.00 %, ≈ −7.0 Elo, 7049 games

Reverted, `dead/iid-cut`. A pre search costs a whole sub tree and pays only with a better move
order; a cut node is done as soon as one move fails high, so there is little order left to improve.
`childNodeType` stayed, 0.4.0-054 needs it.

## 0.4.0-054 — IID replaced by an internal iterative reduction, in PV and cut nodes

Second experiment, and the same idea as 0.4.0-053 in the other form: the pre search produces a move
and is then thrown away, the reduction just searches such a node one ply shallower and costs
nothing extra. PV and cut nodes, never all nodes. A move from the previous iteration counts like a
tt move, so a node on the former primary variant is not reduced — `hasPVMove` on the move provider,
which is exactly the move the ordering would offer first.

- EPD nodes: 86564295 → **56240905**, success rate 24 % → 23 %
- SPRT vs 0.4.0-052 at 5+0.01: **H1 accepted**, 50.46 %, ≈ +3.2 Elo, 17559 games

Kept, new baseline. 35 % fewer nodes for +3.2 Elo, and the run needed almost the full distance —
the extra depth bought back nearly everything the shallower nodes gave away.

The pair 0.4.0-053 and 0.4.0-054 is worth remembering: the same insight, pre search versus
reduction, −7.0 against +3.2 Elo.

## 0.4.0-055 — IIR: PV nodes reduced by two plies

Third experiment. PV and cut nodes got separate reductions and the PV one went from 1 to 2, on the
argument that a PV node without any move to try first is the more expensive of the two.

- EPD nodes: 56240905 → 53840068, success rate 23 % → 19 %
- SPRT vs 0.4.0-054 at 5+0.01: **H0 accepted**, 48.91 %, ≈ −7.6 Elo, 6255 games

Reverted, `dead/iir-pv2`. The value 3 was not run: the direction is monotone and 2 already loses
clearly.

## 0.4.0-056 — IIR: cut nodes with an upper bound tt value reduced by two

Fourth experiment. A cut node with no move to try first whose tt value did not reach alpha last
time is discouraging twice over, so it was searched two plies shallower instead of one.

- EPD nodes: 56240905 → 45214777, success rate 23 % → 18 %
- SPRT vs 0.4.0-054 at 5+0.01: **H0 accepted**, 49.51 %, ≈ −3.4 Elo, 13138 games

Reverted, `dead/iir-upperbound`. Together with 0.4.0-055 this settles the depth question of the
item: one ply is right in both node types, and every attempt to reduce further loses. `hasPVMove`
and `childNodeType` stay, 0.4.0-054 uses them.

Winner of todo item 5 is 0.4.0-054, four runs in.

## Closing run for todo item 5

`0.4.0-054` against `0.4.0-052`, the version the item started from: **H1 accepted**, 52.41 %,
3148 games.

It is the same pairing as the run under 0.4.0-054 itself, which gave 50.46 % over 17559 games, and
the two numbers are a good illustration of why the closing rule exists — this time in the other
direction. A run that reaches its bound early does so because it was fluctuating upwards, and 3148
games at 52.41 % overstates the case as much as the long run understates nothing.

Pooled over both runs: 10510.5 of 20707 points = 50.76 %, **≈ +5.3 Elo**. That is the number for
the item.

## 0.4.0-057 — quiescence tries the tt move first

Neither path offered it. The capture list is selected by weight only, so the tt move got a weight
above every capture; the evades entered the stage machine behind the stage that offers the tt move
and now start at that stage.

Which tt moves actually took part differs between the two paths. In the capture path only
capturing ones: `computeCaptures` generates the non silent moves alone, and `selectProposedMove`
searches exactly that list, so a quiet tt move is not found and gets no weight. In the evades path
any move of the evade list can be the tt move, quiet king moves included — which is correct there,
the quiescence searches all evades when in check.

- EPD nodes: 56240905 → 56326178 (+0.15 %), success rate unchanged at 23 %
- SPRT vs 0.4.0-054 at 5+0.01: **H0 accepted**, 49.34 %, ≈ −4.6 Elo, 9826 games

Reverted, `dead/qs-ttmove`.

The node count already said the reach is small: the early tt return above the move loop cuts on
most tt hits, so a tt move is rarely available down there at all — that is todo item 8.

What is left is the same pattern that sank 0.4.0-039 and 0.4.0-040: a rule placed ahead of the
material order. In the quiescence the order is the value of the captured piece, and pulling a tt
move that may be a pawn capture in front of a queen capture costs more than the hash knowledge is
worth. Three attempts now, all lost, always in that shape.

A split into the two halves was not run. If it is worth a retry, the capture half is the suspect —
the evades half only adds a stage that offers a move which is in the evade list anyway.

## 0.4.0-058 — aspiration window: delta term repaired and tuned

`setSearchResult` overwrote `_positionValue` before the difference was taken, so the delta term of
the window size was always zero and a value that jumped between iterations never widened the
window. Repaired, and the window size given a coefficient per influence — the two delta cases
independent of each other instead of one being the other divided by ten.

CLOP over the six coefficients, 5000 samples, 114 min. All of them land close to where they stood:
15 → 12, 10 → 10, 100 → 106, 10 → 12, 5 → 4, 30 → 25.

- EPD nodes: 56240905 → 57370149, success rate unchanged at 23 %
- SPRT vs 0.4.0-054 at 5+0.01: **undecided**, 50.01 %, LLR −0.33 after the full 20000 games

## 0.4.0-059 — the same fix with the original values

Since the tuned values came out flat, the second run tested the repaired delta term on its own.

- EPD nodes: 56240905 → 55953846
- SPRT vs 0.4.0-054 at 5+0.01: **H0 accepted**, 49.43 %, ≈ −4.0 Elo, 11827 games

Both reverted, `dead/aspiration-delta`.

The term is now **out of the code** rather than silently computing zero: the window has no delta
part any more, `calculateWindowSize` does not take the argument, and a comment records why. Node
count after the removal 56240905, identical to 0.4.0-054 — the bug had made the term inert, so
deleting it changes nothing.

Why a wider window on a jumping value hurts is worth a thought: the delta is large exactly when the
last iteration moved a lot, and a window opened wide there searches the whole node with a real
window instead of failing fast and re-searching. The re-search is apparently the cheaper of the
two. The four remaining coefficients stay tunable, they were confirmed at their hand written
values.

## 0.4.0-060 — IIR in PV nodes only

Fifth experiment of todo item 5, the counter check to 0.4.0-054: cut nodes out of the reduction
again, to see what the cut node half is actually worth.

- EPD nodes: 56240905 → 82676622, success rate 23 % → 26 %
- SPRT vs 0.4.0-054 at 5+0.01: **H0 accepted**, 48.79 %, ≈ −8.4 Elo, 5597 games

Reverted, `dead/iir-pvonly`. The cut nodes carry the item: without them the reduction saves almost
nothing (82.7 against 56.2 million nodes) and loses 8.4 Elo. `0.4.0-054` stands.

## 0.4.0-061 — quiescence tt return bound to a null window, plus the isPV flag

The early return fired in every node, which left the stand pat refinement below it unreachable in
nodes with a real window. Bound to a null window again, and additionally firing in a pv node when
the entry carries the `isPV` flag — a value from a real window search may cut a pv node.

- EPD nodes: 56240905 → 56052024 (−0.3 %), success rate 23 % → 24 %
- SPRT vs 0.4.0-054 at 5+0.01: **H0 accepted**, 49.17 %, ≈ −5.8 Elo, 8214 games

Reverted, `dead/qs-ttpv`. The plain guard had already lost once, and the `isPV` addition does not
rescue it. Nothing is left to vary here: the return is either unconditional, which is the current
code, or guarded, which loses in both forms tried. Item closed.

The node count says why the stand pat refinement is not the prize it looked like: making it
reachable moves 0.3 % of the nodes. What the guard costs instead is every cheap tt cutoff it gives
up in a pv node.

## Items 10 to 14, worked in parallel on branch `todo-10-14`

The branch starts at `373bb20b`, right before the ClockManager work of item 9, which runs on
another machine at the same time. Its versions take the block from `0.4.0-081` upwards so that
the two machines cannot collide on a tag. Every run in this block measures against the state at
the branch point, which is `0.4.0-054` behaviour - `0.4.0-057`, `-060` and `-061` were all
reverted, and the EPD node count of 56240905 at the branch point confirms it.

## 0.4.0-081 - forward futility honours ttValueIsLessOrEqualAlpha

Todo item 13. The guard stood in the code as a commented out line, with the note that it never
had any effect because `setFromParentNode` reset the flag between `probeTT` and the call. That
note is out of date: the tt bound flags are deliberately left standing there today, so the line
is reachable and could be measured for the first time.

- EPD nodes: 56240905 -> 54663722 (-2.8 %), success rate 23 % -> 24 %
- SPRT vs the branch point at 5+0.01: **undecided**, 50.02 %, LLR -0.21 after the full 20000
  games (5216 / 9578 / 5206), 98 min

Reverted, `dead/ff-ttalpha`. Node count after the revert 56240905, identical to the baseline.

Two things are worth keeping. The first is that the guard is anything but inert: it moves 2.8 %
of the nodes and lifts the EPD success rate by a point. Skipping the pruning searches the node
properly instead of guessing, and the better tt entry that comes out of it apparently saves more
nodes elsewhere than the skipped cutoff costs - yet none of that turns into Elo.

The second is the shape of the run. At 4208 games the LLR stood at -1.99 of the -2.94 bound and
the winrate at 48.9 %; it looked decided. It ended at -0.21 and 50.02 %. With bounds 5 Elo apart
the swing over thousands of games is larger than the effect being measured, which is exactly why
the log records the final number and nothing else.

## 0.4.0-082 - the loosing captures ordered by their exchange value

Todo item 14, in the one shape the area had not been tried in. The captures the SEE rates as
loosing are deferred out of the capture stage and come back in `REMAINING_MOVES` in the order the
move generator produced them - the only place in the move ordering where no order exists at all.
`computeExactExchangeValue` is back from `0.4.0-042`, but it runs only for a capture the selection
actually reaches and has just found loosing, not for every capture in the preparation stage.

- EPD nodes: 56240905 -> 54575345 (-3.0 %), success rate 23 % -> 22 %, EPD runtime 3.39 s -> 3.08 s
- SPRT vs the branch point at 5+0.01: **H0 accepted**, 49.49 %, about -4 Elo +- 2, 12721 games

Reverted, `dead/see-loosing-order`. Node count after the revert 56240905.

The interesting part is that it is cheaper and still worse: 3 % fewer nodes, a faster run, and
-3.5 Elo. Whatever the generated order of the loosing captures is, it beats sorting them by how
much material the exchange loses. A plausible reading is that the exchange value answers the wrong
question down there - a capture that loses a rook for a bishop may still be the move that saves the
game, and the generator order at least keeps the cheap pieces first by accident of the piece loop.

This is the fourth change to the capture ordering that lost: recapture priority (`0.4.0-039`,
-4.5), queen promotion priority (`0.4.0-040`, flat), exchange value as the capture weight
(`0.4.0-042`, -6.7) and now the order of the loosing captures. The one variant still untried is a
tie break by the capturing piece inside an equal captured value - real MVV/LVA, where today only
the MVV half exists. Item closed without it.

## 0.4.0-083 - opposite coloured bishops, scaled through the signature hash

Todo item 10, first attempt. `KBP*KBP*` registered as an endgame pattern, scaling the value by
45, 50, 65, 85 and 100 percent for a pawn surplus of 0 to 4 and more. Hand written, no CLOP - the
material is far too rare in a normal game for a run to find a signal on it.

- Activation shown on the static evaluation, not on the EPD node count: the wmtest positions do
  not reach this material. Eight positions of the set, baseline against new: -94/-51, 241/116,
  178/85, -435/-287, 359/175, 106/63, -446/-295, -140/-74
- SPRT on the opposite coloured bishop start set at 5+0.01: **undecided**, 50.02 %, LLR 0.05
  after the full 20000 games

The version has a construction fault, found afterwards by comparing the moves the two engines
choose on the set: 9 of 30 differed, which is far more than a scaling that is order preserving
inside its own material class should produce. The reason is the contract of the signature hash.
An entry there **replaces** the value, so `lazyEval` takes the endgame branch and skips the tempo
bonus and the fifty move damping, `result -= result * (halfmoves - 20) / 250`. That damping pulls
towards the draw as well - the version switched it off in exactly the positions it wanted pulled
towards the draw, and the flat result is two opposite effects measured against each other.

A second lesson sits in the start position set. Inside `KBP*KBP*` with a fixed surplus every leaf
is scaled by the same factor, so the minimax result is too and the move choice cannot change. What
the scaling really decides is whether to *enter* such an endgame - and a set that starts inside one
does not contain that decision. The set is the wrong instrument for this change, however well it
covers the material.

## 0.4.0-084 - the same scaling, applied last

The scaling moved out of the signature hash to the end of `lazyEval`, after the tempo bonus and
the fifty move damping, which is where a scaling of the finished value belongs. The material test
is one and plus one compare on the piece signature instead of a hash lookup. The factors are
unchanged.

- EPD nodes: 56240905 -> 55970930, success rate unchanged at 23 %
- Static evaluation now baseline times the factor exactly: -94/-47, 241/120, 178/89, -435/-282,
  359/179, 106/68, -446/-289, -140/-70
- SPRT vs the branch point at 5+0.01: **H1 accepted**, 50.85 %, about +6 Elo +- 3, 8623 games

- SPRT on the opposite coloured bishop start set: **H0 accepted**, 49.89 %, 15324 games

Kept. Two thirds of an hour of runtime separates it from `0.4.0-083`, and the whole difference is
where the multiplication happens.

The two runs together are the interesting part. On the start position set the change is worth
nothing, slightly negative even; in normal games it is worth six Elo. That is precisely what the
mechanism predicts: inside `KBP*KBP*` the scaling multiplies every leaf by the same factor and
cannot change a move, it only decides whether such an endgame is worth entering - and that
decision does not occur in a set that starts inside one. The item asked for the special set
because the material is too rare in the standard book. It is too rare there, and the standard book
is still the only place the change can be measured.

## 0.4.0-085 - the space evaluation switched on

Todo item 12. `spaceWeightMg` stood at 0, which made the term inert, and the call in `lazyEval`
was commented out on top of that. Both switched on, at a weight of 100 - a factor of 1.0 on the
raw bonus, so the term exactly as the port defines it.

A CLOP run came first: 2000 samples over -40 to 40, centred on the 0 the value stood at, estimate
**-2.1**. That is the null point. The run was then deliberately set aside and the ported weight
measured instead, on the argument that a run cannot learn from an inert term what the term would
be worth switched on. Activating it at weight 0 is behaviour neutral, the node count stayed at
55970930, which confirms the CLOP engine only ever differed in the weight.

- EPD nodes: 55970930 -> 56980971, success rate 23 % -> 22 %
- SPRT vs 0.4.0-084 at 5+0.01: **H0 accepted**, 47.18 %, about -20 Elo +- 5, 2641 games

Switched off again. The two measurements agree and the game result is the harsher of the two: the
term is not merely worthless at its ported weight, it costs twenty Elo. CLOP pointing at the null
point was the correct answer, not a failure to see a signal.

The code stays in `eval/space.h` and `eval/space.cpp`, with the number written at the call in
`lazyEval` and at the default weight. It is measured now rather than suspected, which is worth
more standing in the code than the same lines would be worth deleted. Kept as `dead/space-weight`.

## 0.4.0-086 - the pawn shield activated and tuned

Todo item 11. `computePawnShieldValue` had been written but nothing ever called it; the index
lookup already carried a `kShield` entry for it. It is now added to the attack value of the side
whose king is examined, so it shares the midgame scaling of the king attack term and disappears
with it towards the endgame.

Before tuning, the eight factors had to be given a shape a run can work on. Both kings get the
same term and the two are subtracted from each other, so a constant added to all eight cancels
out - up to the rounding of the midgame scaling it changes nothing at all. That is a degree of
freedom carrying no information, and no run could have resolved it. Index 7, the full shield, is
pinned at 0 and the other seven are measured against it; the original set shifted by -10 is the
same evaluation.

CLOP over the seven, 5000 samples, 115 min:

| index | pawns | before | estimate | taken |
|---|---|---|---|---|
| 0 | none | -18 | -12.93 | -13 |
| 1 | east | -19 | -19.06 | -19 |
| 2 | west | -19 | -24.29 | -24 |
| 3 | west+east | -15 | -17.49 | -17 |
| 4 | front | -19 | -15.10 | -15 |
| 5 | front+east | -14 | -7.69 | -8 |
| 6 | front+west | -5 | -9.96 | -10 |

Indices 5 and 6 are mirror images of each other and had stood 9 apart, which was always suspect.
The run pulled them to -8 and -10. Agreement of that kind between two values it optimises
independently is not what noise produces, and it is the best evidence the run found something.

Not tuned together with the other king attack parameters as the item asked: seven shield values
plus seven attack support points plus the queen factor are 15, and a run takes at most 10.

- EPD nodes: 55970930 -> 56158265, success rate 23 % -> 22 %
- SPRT vs 0.4.0-084 at 5+0.01: **H1 accepted**, 51.11 %, about +8 Elo +- 3, 6783 games

Kept.

## Closing run for items 10 to 14

Two of the five items survived, and each of them was accepted at alpha = 0.05 against whatever
the head was at the time. Their numbers must not be added up, so the head was measured against the
branch point once more.

- SPRT `0.4.0-086` vs the state at `373bb20b` at 5+0.01, the standard bounds: **H1 accepted**,
  51.19 %, 6340 games

A second closing run followed, with the bounds placed where the question actually is instead of at
the standard +-2/+3:

- SPRT `0.4.0-086` vs the state at `373bb20b`, H0 = 0, H1 = 10: **H1 accepted**, 52.68 %, 1662
  games, 8 min

### What the closing runs say, and what the first version of this entry got wrong

They say the two survivors together are worth at least 10 Elo.

The first version of this entry said something else. It put the summed single runs, about 14 Elo,
against the first closing run, about 8, and explained the gap as the price of stopping at a bound.
That explanation was invented. The gap is 5 Elo against a standard error of the difference of
about 5 - one sigma, nothing to account for.

Two things went wrong and both are worth naming.

The first is arithmetic dressed up as precision. Writing 8.3 claims the value is known to better
than a tenth. With 6340 games and this draw rate the variance per game is 0.129, the standard
error of the score is 0.45 %, and that is about 3 Elo at one sigma. The correct way to write that
number is "about 8 Elo", or "8 +- 3". The spurious decimal made two indistinguishable figures look
like two different results, and the story followed the decimal.

The second is reading an SPRT as a measurement. It stops the moment the LLR reaches a bound, and
the score at that moment is biased in the direction that caused the stop. The second closing run
shows it plainly: same two engines, and it came out at 52.68 % after 1662 games where the first
had 51.19 % after 6340. The short run is the hotter one, which is why it was short.

Taken together - they are independent samples of the same comparison - 8002 games at 51.50 %, so
**about 10 Elo, +- 3**. The sum of the two single runs, about 14, sits inside that. Whatever the
first version of this entry claimed about the sum being inflated has no support in the data, and
if anything they lean the other way.

The Elo below is the score at the stopping point converted to Elo, with one sigma from the game
count and the draw rate of that run. It is a label on a decision, not a measurement - and the
error bars are the size of the effects being looked for.

| Item | Tag | Result |
|---|---|---|
| 13 forward futility honours the tt bound | `0.4.0-081` | undecided at 50.02 %, 20000 games |
| 14 loosing captures by exchange value | `0.4.0-082` | H0, 49.49 %, about -4 +- 2 Elo |
| 10 opposite bishops through the signature hash | `0.4.0-083` | flat, construction fault |
| 10 opposite bishops scaled last | `0.4.0-084` | **H1, 50.85 %, about +6 +- 3 Elo, kept** |
| 12 space evaluation at its ported weight | `0.4.0-085` | H0, 47.18 %, about -20 +- 5 Elo |
| 11 pawn shield activated and tuned | `0.4.0-086` | **H1, 51.11 %, about +8 +- 3 Elo, kept** |

Three of the six versions were measured on an instrument that first had to be repaired: item 10
needed a start position set that then turned out to be unable to see the effect, item 12 needed a
CLOP run whose answer was set aside, and item 13 needed the code to be reachable at all. The
runtime went into that as much as into the games.

## 0.4.0-087 - material balance re-tuned, and dropped again

Not a todo item. The piece values had been optimized once as `0.4.0-026` (`f830fb9`) and rolled
back in `464ba8e` with the note that they lost the final tournament against `0.4.0-025a`. That
tournament was broken: `025a` won 150 games on time and came out rated far too high, so every
version measured against it afterwards was judged against a number that did not exist. Nothing
had been tuned on the material balance since, so the rollback stood on a comparison worth
nothing - reason enough to run it again.

CLOP over 5000 samples, 117 min, the pawn left fixed at 80/95 as the scale. A common factor over
all material values is a degree of freedom a run resolves badly, and the pawn is what every other
value is read against; `0.4.0-026` had left it alone for the same reason.

| value | 025a | CLOP | taken | 0.4.0-026 had |
|---|---|---|---|---|
| knight mg | 360 | 363 | 365 | 365 |
| knight eg | 310 | 308 | 310 | 310 |
| bishop mg | 360 | 363 | 365 | 365 |
| bishop eg | 330 | 331 | 330 | 340 |
| rook mg | 560 | 576 | 575 | 550 |
| rook eg | 570 | 564 | 565 | 570 |
| queen mg | 1035 | 1072 | 1070 | 1150 |
| queen eg | 1085 | 1060 | 1060 | 1150 |

- EPD nodes: 56158265 -> 55782709, success rate 22 % -> 23 %
- SPRT vs `0.4.0-086` at 5+0.01: **H0 accepted**, 49.23 %, about -5 Elo +- 3, 8666 games

Reverted, `dead/material-retune`. Node count back at 56158265.

Two things are worth keeping. The two runs, four months apart and on different code, both put the
minor pieces at 365 in the midgame and both move the midgame queen up - independently, and from
different starting points. That is the part of the picture that reproduces. What does not
reproduce is the size: 026 wanted the queen 115 higher, this run 35, and the endgame queen moves
the other way entirely.

The other is what the run does not settle. CLOP optimises in self play, where both sides carry the
same shifted values, and material values are the classic case where that says least about play
against another engine. The SPRT says these particular values lose; it does not say the 025a
values are right. Answering that needs the final tournament against the field, not another run of
the engine against itself.

## 0.4.0-088 - end of branch `todo-10-14`, for the final tournament

Not a change of its own. It is the end of the branch that worked todo items 10 to 14, tagged so
that the tournament against the field has a clean version number to run.

What is in it, against the branch point `373bb20b`:

- **0.4.0-084**, opposite coloured bishops scaled towards the draw, at the end of `lazyEval`
- **0.4.0-086**, the pawn shield activated and its seven factors tuned

Everything else that was tried is reverted and present only as a comment at the place where the
idea sits: the futility guard on the tt bound (`0.4.0-081`), the exchange value order of the
loosing captures (`0.4.0-082`), the space evaluation (`0.4.0-085`) and the re-tuned material
values (`0.4.0-087`). Their code is on the `dead/` branches.

- EPD nodes: 56240905 at the branch point -> 56158265, success rate 23 % -> 22 %
- SPRT vs the branch point at 5+0.01, standard bounds: H1 accepted, 51.19 %, 6340 games
- SPRT vs the branch point, H0 = 0 and H1 = 10: H1 accepted, 52.68 %, 1662 games
- Both pooled, 8002 games at 51.50 %: about 10 Elo +- 3

The final tournament against the field is still to run. Self play is what all of the above rests
on, and this is the version that has to prove itself somewhere else - the more so because the last
time a version was judged against that field, the field measurement itself was broken and the
error stood for four months, see `0.4.0-087`.

## Notes on reading this log

Seven SPRTs ran in sequence over the same code area, keeping whatever passed. At alpha = 0.05
each single run has a 5 % chance of accepting a change that is worth nothing, and over a chain
that adds up. A closing SPRT of the head against 0.4.0-035 is the honest number for the two
survivors, not the sum of the two individual gains.
