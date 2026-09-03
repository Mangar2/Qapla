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

One CLOP over all seven values including `qsAlphaSafetyMargin`, 5000 samples, 114 min. They are all
summands of the same threshold, so none of them can cancel another.

| | old | new |
|---|---|---|
| `ffDepthFactor` | 75 | 83 |
| `ffBase` | 75 | 69 |
| `ffImprovingBonus` | 100 | 101 |
| `futDepthFactor` | 75 | 43 |
| `futBase` | 75 | 80 |
| `futImprovingMalus` | 100 | 77 |
| `qsAlphaSafetyMargin` | 47 | 50 |

The forward futility becomes more conservative — it cuts at `eval - margin >= beta`, so a larger
margin cuts less. The move loop futility becomes clearly more aggressive: it cuts at
`eval + captured + margin < alpha`, where a smaller margin cuts more, and its depth slope nearly
halves. `qsAlphaSafetyMargin` confirms itself, it had been tuned not long ago.

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

## In progress — normal search time (todo item 9)

`computeAverageTime` multiplied the fair time slice by a factor clamped to 1.0 below five minutes
of remaining time, reaching 2.0 only beyond two hours — at every time control the engine actually
plays it did nothing. Below it sat a hard halving at 10000 ms with an increment of one millisecond
or less. Both replaced by

```
share = shareMin + (shareMax - shareMin) * timeLeft / (timeLeft + halfTime)
```

monotone, saturating, continuous in every derivative.

The tables below list the time one move gets, every fifth move, under the assumption that every
move takes exactly the normal time and no mode factor applies. The clock is simulated: each move
costs its own time and gains the increment.

**5 s + 10 ms**

| move | 054 | 062 | 064 / 065 |
|---:|---:|---:|---:|
| 1 | 90 ms | 115 ms | 135 ms |
| 6 | 90 ms | 112 ms | 129 ms |
| 11 | 90 ms | 110 ms | 124 ms |
| 16 | 90 ms | 107 ms | 117 ms |
| 21 | 90 ms | 104 ms | 109 ms |
| 26 | 91 ms | 100 ms | 103 ms |
| 31 | 80 ms | 85 ms | 84 ms |
| 36 | 71 ms | 72 ms | 70 ms |
| 41 | 63 ms | 63 ms | 59 ms |
| 46 | 57 ms | 54 ms | 50 ms |
| 51 | 51 ms | 46 ms | 42 ms |
| 56 | 45 ms | 40 ms | 36 ms |
| 61 | 41 ms | 35 ms | 31 ms |
| 66 | 37 ms | 31 ms | 27 ms |
| 71 | 33 ms | 27 ms | 23 ms |
| 76 | 30 ms | 24 ms | 20 ms |
| 81 | 28 ms | 22 ms | 19 ms |
| 86 | 26 ms | 20 ms | 17 ms |
| 91 | 24 ms | 19 ms | 16 ms |
| 96 | 22 ms | 16 ms | 14 ms |
| 101 | 20 ms | 16 ms | 14 ms |
| 106 | 19 ms | 15 ms | 13 ms |
| 111 | 18 ms | 14 ms | 13 ms |
| 116 | 17 ms | 14 ms | 11 ms |
| 121 | 16 ms | 12 ms | 11 ms |
| 126 | 15 ms | 12 ms | 11 ms |
| 131 | 15 ms | 12 ms | 11 ms |
| 136 | 14 ms | 11 ms | 11 ms |
| 141 | 13 ms | 11 ms | 11 ms |
| 146 | 13 ms | 11 ms | 11 ms |
| 151 | 13 ms | 11 ms | 10 ms |
| 156 | 12 ms | 11 ms | 10 ms |
| 161 | 12 ms | 11 ms | 10 ms |
| 166 | 12 ms | 11 ms | 10 ms |
| 171 | 11 ms | 11 ms | 10 ms |
| 176 | 11 ms | 10 ms | 10 ms |
| 181 | 11 ms | 10 ms | 10 ms |
| 186 | 11 ms | 10 ms | 10 ms |
| 191 | 11 ms | 10 ms | 10 ms |
| 196 | 11 ms | 10 ms | 10 ms |

**20 s + 100 ms**

| move | 054 | 062 | 064 / 065 |
|---:|---:|---:|---:|
| 1 | 422 ms | 508 ms | 618 ms |
| 6 | 422 ms | 500 ms | 592 ms |
| 11 | 422 ms | 492 ms | 562 ms |
| 16 | 422 ms | 481 ms | 535 ms |
| 21 | 422 ms | 472 ms | 503 ms |
| 26 | 422 ms | 458 ms | 472 ms |
| 31 | 381 ms | 402 ms | 400 ms |
| 36 | 345 ms | 355 ms | 340 ms |
| 41 | 314 ms | 313 ms | 293 ms |
| 46 | 286 ms | 278 ms | 255 ms |
| 51 | 263 ms | 250 ms | 224 ms |
| 56 | 242 ms | 225 ms | 199 ms |
| 61 | 224 ms | 205 ms | 181 ms |
| 66 | 208 ms | 187 ms | 165 ms |
| 71 | 194 ms | 173 ms | 152 ms |
| 76 | 182 ms | 161 ms | 141 ms |
| 81 | 171 ms | 150 ms | 134 ms |
| 86 | 162 ms | 142 ms | 127 ms |
| 91 | 154 ms | 136 ms | 123 ms |
| 96 | 147 ms | 129 ms | 118 ms |
| 101 | 141 ms | 124 ms | 115 ms |
| 106 | 136 ms | 120 ms | 112 ms |
| 111 | 131 ms | 117 ms | 109 ms |
| 116 | 127 ms | 114 ms | 107 ms |
| 121 | 124 ms | 112 ms | 106 ms |
| 126 | 121 ms | 109 ms | 106 ms |
| 131 | 118 ms | 108 ms | 104 ms |
| 136 | 116 ms | 106 ms | 103 ms |
| 141 | 114 ms | 105 ms | 100 ms |
| 146 | 112 ms | 105 ms | 100 ms |
| 151 | 111 ms | 104 ms | 100 ms |
| 156 | 109 ms | 104 ms | 100 ms |
| 161 | 108 ms | 101 ms | 100 ms |
| 166 | 107 ms | 100 ms | 100 ms |
| 171 | 106 ms | 100 ms | 100 ms |
| 176 | 105 ms | 100 ms | 100 ms |
| 181 | 105 ms | 100 ms | 100 ms |
| 186 | 104 ms | 100 ms | 100 ms |
| 191 | 104 ms | 100 ms | 100 ms |
| 196 | 103 ms | 100 ms | 100 ms |

**60 s + 1 s**

| move | 054 | 062 | 064 / 065 |
|---:|---:|---:|---:|
| 1 | 1967 ms | 2189 ms | 2585 ms |
| 6 | 1967 ms | 2176 ms | 2502 ms |
| 11 | 1967 ms | 2150 ms | 2406 ms |
| 16 | 1967 ms | 2122 ms | 2318 ms |
| 21 | 1968 ms | 2101 ms | 2219 ms |
| 26 | 1968 ms | 2066 ms | 2125 ms |
| 31 | 1844 ms | 1904 ms | 1895 ms |
| 36 | 1736 ms | 1767 ms | 1712 ms |
| 41 | 1641 ms | 1643 ms | 1571 ms |
| 46 | 1559 ms | 1544 ms | 1454 ms |
| 51 | 1488 ms | 1460 ms | 1363 ms |
| 56 | 1425 ms | 1385 ms | 1292 ms |
| 61 | 1371 ms | 1325 ms | 1233 ms |
| 66 | 1323 ms | 1271 ms | 1188 ms |
| 71 | 1282 ms | 1229 ms | 1151 ms |
| 76 | 1246 ms | 1191 ms | 1121 ms |
| 81 | 1214 ms | 1161 ms | 1098 ms |
| 86 | 1187 ms | 1134 ms | 1079 ms |
| 91 | 1163 ms | 1112 ms | 1063 ms |
| 96 | 1142 ms | 1094 ms | 1051 ms |
| 101 | 1124 ms | 1078 ms | 1009 ms |
| 106 | 1108 ms | 1065 ms | 1000 ms |
| 111 | 1094 ms | 1054 ms | 1000 ms |
| 116 | 1082 ms | 1045 ms | 1000 ms |
| 121 | 1072 ms | 1038 ms | 1000 ms |
| 126 | 1062 ms | 1000 ms | 1000 ms |
| 131 | 1054 ms | 1000 ms | 1000 ms |
| 136 | 1047 ms | 1000 ms | 1000 ms |
| 141 | 1041 ms | 1000 ms | 1000 ms |
| 146 | 1036 ms | 1000 ms | 1000 ms |
| 151 | 1032 ms | 1000 ms | 1000 ms |
| 156 | 1027 ms | 1000 ms | 1000 ms |
| 161 | 1000 ms | 1000 ms | 1000 ms |
| 166 | 1000 ms | 1000 ms | 1000 ms |
| 171 | 1000 ms | 1000 ms | 1000 ms |
| 176 | 1000 ms | 1000 ms | 1000 ms |
| 181 | 1000 ms | 1000 ms | 1000 ms |
| 186 | 1000 ms | 1000 ms | 1000 ms |
| 191 | 1000 ms | 1000 ms | 1000 ms |
| 196 | 1000 ms | 1000 ms | 1000 ms |

Two things the tables show. The old code is flat across all three time controls, which is the
point of the item — the factor that was supposed to make a long game slower never left its lower
clamp. And the increment carries most of the load: the times barely fall until the forecast stops
shrinking around move 26, because each move gets its own increment back.

`timeShareMin` came out at 135 in the first run, 5+0.01, 2000 samples, 45 min, tagged
**`0.4.0-062`**. That is a quarter more time per move than the old code takes and the opposite of
what the item assumed — the expectation was that a short time control should be played relatively
faster.

`timeShareMax` came out at 168 in the second run, 60+1, stopped at 1700 of 2000 samples for a
pending reboot, tagged **`0.4.0-063`**. The estimate had plateaued: after rising from 153 at 600
samples the last twenty readings all sit between 165 and 169.

So the share runs from 135 % of the fair slice at an empty clock to 168 % at a full one, 160 % at
60 seconds left. The old code was flat at 100 %. The direction the item expected does hold — more
clock, more time per move — it just sits an entire level higher than assumed.

- SPRT `0.4.0-062` vs `0.4.0-054` at 5+0.01: **H1 accepted**, 52.86 %, ≈ **+19.9 Elo**, 2602 games

The largest single gain of the whole series, and it comes from spending 35 % more time per move.
Every SPRT of this item runs against `0.4.0-054`, at all three time controls, because the goal is
a version that is better at each of them and above all at 60+1.

### What the engine really spends

The ini now writes a pgn with the clock and nothing else; `test/tools/movetime-stats.py` turns it
into the blocks of five above. From the 7705 moves `0.4.0-062` played in that run:

| moves | formula | mean | median | stddev | max |
|---:|---:|---:|---:|---:|---:|
| 1-5 | 115 ms | 118 ms | 100 ms | 47 ms | 400 ms |
| 11-15 | 110 ms | 111 ms | 90 ms | 47 ms | 420 ms |
| 21-25 | 104 ms | 105 ms | 90 ms | 46 ms | 410 ms |
| 31-35 | 85 ms | 75 ms | 70 ms | 34 ms | 340 ms |
| 41-45 | 63 ms | 55 ms | 50 ms | 25 ms | 220 ms |

The mean tracks the formula almost exactly for the first thirty moves, so the normal time is
really what the engine aims at. The median sits well below it: the distribution is skewed to the
right, most moves stop early at the 0.7 or 0.8 threshold and single moves reach four times the
normal time — which is exactly the critical mode factor of 4. From move 30 the practice falls
below the formula, there the maximum time caps it.

### 0.4.0-064 — both share values tuned at 20+0.1

CLOP over `timeShareMin` and `timeShareMax` at 20+0.1, 2500 samples, 5 h: 135 → 155 and 168 → 168.
`shareMax` did not move, which was expected — at 20 seconds left the two sit at 50:50 in the share
and the weight shifts towards `shareMin` as the clock runs down, so that run can barely see
`shareMax`.

- SPRT vs `0.4.0-054` at 20+0.1: **H1 accepted**, 51.82 %, ≈ **+12.7 Elo**, 3702 games

`shareMin` has now risen from 100 to 135 at 5 seconds and to 155 at 20. Together with 168 the
share is nearly flat between 160 and 168 percent, so what is left of the time dependence is little
more than a constant. The engine was simply playing too fast, at every time control.

**A run thrown away, worth recording.** The first attempt at this SPRT passed `tc=20+0.1` after the
second `--engine` block, where it is an option of that engine alone: 064 played with 20 seconds
against 054 with the 5 from the ini and won 87.69 % of 337 games. Nothing in the output says so —
the only signal is the absurd number. The pgn showed it in two lines, 0.5 to 0.8 seconds a move
against 0.07 to 0.09. A time control override belongs in `--each`, before the engines; the rule is
in CLAUDE.md now. The two CLOP runs are not affected, they define a single engine both sides are
derived from, and their runtimes match the time control they were given.

Still open for the item: the formula rebuild — a saturating `movesToGo` instead of the kinked
maximum and the mode factors as coefficients — plus the pieces Spike had that Qapla lacks.

## 0.4.0-065 — time formulas reformulated

`movesToGo` was `max(start - movesPlayed, keep)`, a kink where the slope jumps from −1 to 0. It is
a soft maximum now, `keep + s * ln(1 + exp((start - played - keep) / s))`, which at s = 0.5 gives
the same integer for every ply count from 0 to 400. The four mode factors became coefficients in
percent with the defaults the hard values had. Behaviour identical to `0.4.0-064`.

- SPRT vs `0.4.0-054` at 5+0.01: **H1 accepted**, 52.38 %, ≈ **+16.6 Elo**, 3105 games

## 0.4.0-066 — normal time capped against the maximum

The normal time is the budget the search plans against: it starts another iteration while below
0.7 of it and another root move while below 0.8. Nothing tied that budget to the maximum, so the
room between the deepening decision and the hard cut shrank over the game — at 60+1 from eleven
times the gate down to 1.2 by move 80, and with the critical factor of 4 from move 42 on there is
none left at all. Every iteration begun there dies mid tree and buys nothing for the move choice.

`normalTime = min(normalTime, maxTime * 100 / timeAvgCapDivisor)`, the divisor never below 100 so
the normal time stays under the maximum by construction. CLOP at 20+0.1, stopped at 950 of 2000
samples: 220.

- SPRT vs `0.4.0-065` at 20+0.1: **H0 accepted**, 49.16 %, ≈ −5.8 Elo, 7228 games

Reverted, `dead/time-avg-cap`.

The reasoning was sound and the measurement says no. What it means is that the work in an aborted
iteration is not lost after all: the transposition table keeps it, and the next move starts from a
tree that is already searched deeper. Cutting the budget to hold every iteration inside the maximum
gives up more than the aborted iterations cost. Spike has the same construction and the same
missing cap, which is worth knowing — it is not an oversight there either.

## 0.4.0-067 — the same cap with divisor 150

Second attempt with a milder cap, at 5+0.01 because it decides faster.

- SPRT vs `0.4.0-065` at 5+0.01: **H0 accepted**, 49.59 %, ≈ −2.9 Elo, 15462 games

Reverted, `dead/time-avg-cap-150`. Much closer than 220 and it still needed 15462 games to reach
the bound, but on the wrong side of it. The cap is out of the code for good, and so is
`timeIncrementShare`, which never had an effect at 100 percent.

Two runs on the same idea, both negative, and the milder one closer to neutral: the loss scales
with how hard the budget is cut. That is the shape of a change that costs something real and buys
nothing — the aborted iterations were never the waste they looked like, the transposition table
carries their work into the next move.
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

## 0.4.0-089 — the two lines merged

Branch `todo-10-14` merged into the time management line. The two touch different code: items 10
to 14 sit in the eval, item 9 in the ClockManager, and nothing had to be decided by hand outside
the two plan files.

The node count is the proof. Nothing in the time management can be seen at a fixed depth —
`isInfiniteSearch` short circuits the whole computation — so the merge has to give exactly what the
branch end gave: **56158265**, and it does.

What the merged version carries against `0.4.0-054`:

| | measured at | |
|---|---|---|
| item 9, normal search time | 5+0.01 | ≈ +16.6 Elo, 3105 games |
| item 9, normal search time | 20+0.1 | ≈ +12.7 Elo, 3702 games |
| items 10 to 14, eval | 5+0.01 | ≈ +10 Elo ± 3, 8002 games pooled |

Both figures rest on self play and neither has been measured at 60+1, which is still open for
item 9.

## Notes on reading this log

Seven SPRTs ran in sequence over the same code area, keeping whatever passed. At alpha = 0.05
each single run has a 5 % chance of accepting a change that is worth nothing, and over a chain
that adds up. A closing SPRT of the head against 0.4.0-035 is the honest number for the two
survivors, not the sum of the two individual gains.

## 0.5.0-002 — quiescence ToDo 4, the `doFutilityOnCapture` guard removed

Reverted. SPRT against `0.5.0-001` at 5+0.01, standard bounds: **H0 accepted** after 6532 games. EPD nodes 56158265 → 56158290, different.

The guard turns futility pruning off for a capture whose owner is down to two pieces or fewer,
where the evaluation is no longer a smooth function of material. That the EPD run barely reaches
such positions and the games reject the removal is the useful part of this entry: a node
count difference near zero says the test set does not reach the change, not that the change is
harmless. Code on `dead/qs-todo4`.

## 0.5.0-003 — quiescence ToDo 3, only queen promotions exempt from the capture pruning

Reverted, and it never was a change. `MoveList::addPromote` files the queen promotion as the only
non silent promotion; rook, bishop and knight promotions are silent, and quiescence iterates the
non silent moves alone. Inside `computePruneForewardValue`, `move.isPromote()` and
`getPieceType(move.getPromotion()) == QUEEN` are therefore the same predicate, for every position.

Identical EPD node count, 56158265, confirming the argument. No SPRT was run — there were not two
programs to compare. Code on `dead/qs-todo3`.

## 0.5.0-004 — the two mate distance cutoffs at the quiescence node entry removed

Reverted. SPRT against `0.5.0-001` at 5+0.01, standard bounds: **H0 accepted** after 12540 games. Identical EPD node count, 56158265.

The pair of entries this log should be read together with `0.5.0-002`. Both changes were invisible
in the fixed depth run and both were rejected in games; this one needed 12540 games to settle, twice
the usual. The checks fire when the search window already carries a mate score, which the
aspiration window produces in games and the depth 14 EPD run rarely reaches. Code on
`dead/qs-todomate`.

## 0.5.0-005 — quiescence ToDo 2, node level delta pruning

Reverted. SPRT against `0.5.0-001` at 5+0.01, standard bounds: **H0 accepted** after 6399 games. EPD nodes 56158265 → 54558890, different.

Returning `standPat + queen + margin` when that stays below alpha, with the margin at 200. The
bound is not safe against this evaluation: a capturing promotion gains more than a queen, and the
stand pat value is a full eval whose positional terms move with the capture — only the winning
bonus is guarded against. The ToDo made the CLOP of the margin conditional on H1, so no tuning run
was started, and the failure points at a margin far above the proposed upper limit of 400, not at
the negative values the item expected. Code on `dead/qs-todo2`.

## 0.5.0-006 — quiescence ToDo 5, the beta side probe on the pruning estimate

Reverted, and untestable as written. `SEE::computeExchangeValue` searches a window of ±1 around a
threshold derived from alpha and clamps its result into it, so the pruning estimate is at most
`alpha + 1` and can never exceed beta — for any position and any value of `qsBetaSafetyMargin`.
The CLOP run the item asked for would have tuned a parameter with no effect.

Identical EPD node count, 56158265. The ToDo's own note names the prerequisite, a SEE that cuts at
the beta side, and marks it as not a ToDo yet; that is where the item has to restart. Code on
`dead/qs-todo5`.

## 0.5.0-007 — ToDo 1, no tt cutoff on mate values

Reverted. SPRT against `0.5.0-001` at 10+0.01 with the bounds the item asked for, H0 = −6 and
H1 = −1, maxgames 50000: **H0 accepted** after 6272 games. EPD nodes
56158265 → 54860445, success rate 22 % → 18 %.

`abs(value) < MIN_MATE_VALUE` on the quiescence cutoff and on the one in `SearchNode::probeTT`,
the two places that cut on a tt value. The item was willing to pay a small loss for mate search
stability and set H1 to −1 for exactly that; the run rejects even that, and the drop on `wmtest`,
a mate and tactics set, had already pointed the same way. Blocking the cutoff does not stabilise
the mate search, it discards mate scores the search had proven and makes it prove them again.
Code on `dead/qs-todo1`.

### The round as a whole

Six quiescence ToDos, six reverts, no closing SPRT — the rule for a series exists to check a chain
of accepted changes, and this chain has none. Four SPRTs, 31743 games, about 6½ hours at
concurrency 16 with two runs at a time. Two of the six items were settled by reading the code
instead of running anything. Details in `plan/quiescence-todos-0.5.0.md`.

## 0.5.0-009 — quiescence futility margin with an evaluation dependent part, CLOP values

Reverted. SPRT against `0.5.0-001` at 5+0.01, standard bounds: **H0 accepted** after 6522 games. EPD nodes 56158265 → 56595020, different.

The forward futility margin gets a second term, `(material - eval) * weight / 100`, both from the
view of the side to move, so it widens exactly when the opponent stands better than the material
alone justifies. The reasoning behind it: what the evaluation grants beyond the material sits on
pieces — an advanced passed pawn, the piece carrying a king attack — and capturing such a piece
takes the bonus with it, so the capture gains more than the exchange value says.

The reformulation itself was clean: the weight defaults to 0 and reproduces the old expression
exactly, identical node count with the group flag both false and true. A CLOP run over 3000 samples
estimated `qsAlphaSafetyMargin` 56.19 and `qsEvalMarginWeight` 19.63, rounded to 56 and 20.

The confirming SPRT rejects them. The node count says the term widens the margin on average, and
the older measurements in the code comment at that line (`100: 49%, 35: 50,3%, 40: 49,3%`) say the
engine wants the margin *smaller*, not larger. Two changes pushed that way at once — the fixed
margin from 50 to 56 and the new term — so this run cannot say which one caused the rejection.
`0.5.0-010` isolates the term by putting the fixed margin back. Code on `qs-evalmargin`.

## 0.5.0-010 — the evaluation dependent margin alone, fixed margin back at 50

Reverted. SPRT against `0.5.0-001` at 5+0.01, standard bounds: **H0 accepted** after 5936 games. EPD nodes 56158265 → 58956880, different.

`0.5.0-009` moved two things at once, the fixed margin and the new weight, so it could not say
which one caused it. This run holds the fixed margin at the 50 it always had and changes only
the weight. It loses on its own, by slightly more than the pair did, so the fixed margin move was
not the cause.

The idea behind the term is sound — a captured piece takes its
positional bonus with it — but protecting those captures costs pruning on every other capture in
the position, and that trade is negative here. Code on `qs-evalmargin`.

## 0.5.0-011 — the same weight with the fixed margin at 30

Built, tagged and measured, **SPRT not run**. EPD nodes 55459078, different from the baseline.

Third point of a scan over the fixed margin at weight 20, after 56 and 50, both of which lost. If
it is ever run and wins, it needs a control at fixed margin 30 with weight 0 before the gain is
credited to the new term: two things differ from the baseline in it, and the older measurements at
that line (`35: 50,3%`) suggest a smaller fixed margin may win on its own.

## 0.5.0-008 — mate distance cutoff with `>=` and `<=`

Not decided. SPRT against `0.5.0-001` at 5+0.01 with H0 = −5 and H1 = 0, the bounds of a non
regression test: **inconclusive at the 40000 game limit**. EPD nodes
56158265, identical.

`Search::nonSearchingCutoff` tested `alpha > MAX_VALUE - ply` while `Quiescence::search` tested
`alpha >= MAX_VALUE - ply`. The boundary itself is unreachable — a mate at this ply is worth
exactly `MAX_VALUE - ply` — so `>=` is the correct comparison and the two places now agree.

Undecided is the answer: 40000 games did not separate the correction from the baseline against
bounds of −5 and 0. It is a tidy-up whose effect is provably confined to one boundary case; it is
kept on `qs-matecmp` and not merged, because nothing here argues for it beyond correctness.

## 0.5.0-012 — evaluation dependent margin, author's version, term only in the threshold

Not decided. SPRT against `0.5.0-001`, standard bounds: **inconclusive at 20000 games**, the LLR
stopping just short of the H0 bound. EPD nodes 56053705, different.

The term reached only the SEE threshold, not the returned value. Because `computeExchangeValue`
clamps its result at `threshold + 1`, that caps the return at `alpha - evalMargin + 1`, so in every
node where the term was active *all* captures were pruned — the exact opposite of the intent, which
was to protect them. The version is therefore a different heuristic than the one it was meant to
be: prune harder when the opponent's advantage is positional rather than material. As that
heuristic it was not rejected and not accepted; the run ran out of games near the H0 bound.

The EPD success rate of 29 % against the baseline's 22 % is what a hundred positions can do; the
run over 20000 games says the opposite, and the run is what counts.

## 0.5.0-013 — the same term, corrected into both the threshold and the returned value

Not decided. SPRT against `0.5.0-001`, standard bounds: **inconclusive at 20000 games**. EPD nodes 57257112, different.

The first version of the idea that acts in the direction it was designed for: the node searches
*more*, so the captures carrying a positional bonus are protected from the forward pruning instead
of being cut. Neither accepted nor rejected at the guessed values 30 and 100.

Repeated with the same bounds against the same baseline, `013b`: **H0 accepted** after 15345 games.
Undecided once and rejected once, so unlike `0.5.0-018` the repeat does not overturn the first run,
it sharpens it. No version of this idea ever reached H1, and this is the one that carried it in its
intended form.

The shape is right and free, the coefficients were never fitted - but two CLOP runs and six SPRTs
found nothing to fit them to. Code on `qs-evalmargin-fixed`; the author's revision on
`release0.5` decouples the two coefficients so that the offset sets where the term starts and the
factor how steeply it grows, which is what a CLOP run needs.

## 0.5.0-014 — alpha safety margin 35 instead of 50

Reverted. SPRT against `0.5.0-001`, standard bounds: **H0 accepted** after 18238 games. EPD nodes 56158265 → 56261698, different.

This settles an old comment that used to sit above the parameter, `100: 49%, 35: 50,3%, 40: 49,3%`.
Read as a curve it said 35 was the better value; measured against its own decision it is worse than
50. The three percentages were three indistinguishable numbers without an error range, and carrying
them in the source as prior information for a tuning run was worse than having nothing there. They
are gone from the code, and this entry replaces them with a decision.

`qsAlphaSafetyMargin` stays at 50, and the CLOP run for the evaluation dependent term therefore
searches around that value.

## 0.5.0-015 — the evaluation dependent margin with its CLOP values

Reverted. SPRT against `0.5.0-001`, standard bounds: **H0 accepted** after 11641 games. EPD nodes 55197564 → 54750353, different.

CLOP over both coefficients, 3000 samples, `qsAlphaSafetyMargin` held at 50 because `0.5.0-014`
had just shown 50 is not to be moved. Estimates: offset 144.25, factor 31.38, rounded to 144 and
31. The factor did not move — 30 to 31 is inside the noise of the fit — so the run's whole content
is that the term should start earlier, at 144 instead of 200.

Starting earlier is what the confirming run rejects.

**This closes the item.** Everything tried on it:

| version | shape | result |
|---|---|---|
| `0.5.0-009` | tapered material, weight 20, fixed margin 56 | H0 after 6522 games |
| `0.5.0-010` | same, fixed margin back at 50 | H0 after 5936 games |
| `0.5.0-012` | author's version, term only in the SEE threshold | undecided, 20000 games |
| `0.5.0-013` | the same, corrected into both terms | undecided, 20000 games |
| `0.5.0-015` | decoupled shape with CLOP values | H0 after 11641 games |

No closing run is needed: every one of these ran against `0.5.0-001` directly, none was built on
its predecessor, so `0.5.0-015` already is the run the rule asks for.

The best the idea ever reached is `0.5.0-013`: costs nothing, gains nothing. The mechanism is
sound — a captured piece takes its positional bonus with it — but protecting those few captures is
paid for with pruning on every other capture in the position, and that trade does not come out
positive at any value tried.

Two CLOP runs were made on this term and both produced points that their confirming SPRT rejected.
On a surface this flat, four games per sample fit noise; the CLOP estimate is not evidence of
anything on its own, which is what the confirming run is for.

## 0.5.0-016 — node level delta pruning, all three exemptions mirrored

Reverted. SPRT against `0.5.0-001`, standard bounds: **H0 accepted** after 7401 games. EPD nodes 56158265 → 54818593, different.

The second attempt at the old ToDo 2, after the first one (`0.5.0-005`, also rejected) turned out to have
skipped two of the three exemptions of `computePruneForewardValue`. This version mirrors all three
at node level: no winning bonus, no pawn on the seventh rank, `doFutilityOnCapture` true for the
opponent's colour. It fires only where the move loop would prune every capture anyway, because the
exchange value can never exceed the captured piece and that is at most a queen.

What makes this run worth more than its verdict is what was established before it. A diagnostic
build, delta3, ran the identical condition but kept the move loop and replaced only the returned
value — and gave the same node count to the node, 54818593. Skipping the loop changes nothing at
all. So the only difference this SPRT measures is the returned fail low value:
`standPat + queen + 200` instead of `max(standPat, the pruned forward values)`, at most 1260
centipawns looser.

**That alone is enough to be rejected.** A quiescence node that fails low reports a bound, and the
search is sensitive to how tight that bound is — the run reached H0 after 7401 games, faster than
any other version of this item. Worth remembering before the next idea that trades bound precision
for speed in the quiescence. How much it costs is a separate question and would need its own run
with the bounds placed there.

The saving itself is real but small: the move generation and the SEE calls of a node whose every
move is pruned. It does not come close to paying for the loss.

## 0.5.0-017 — the fixed margin lowered to 10, the term left to cover the critical cases

Reverted. SPRT against `0.5.0-001`, standard bounds: **H0 accepted** after 4702 games. EPD nodes 57289722, different.

`qsAlphaSafetyMargin` 50 → 10, offset 100, factor 20. The sharpest formulation of the item so far:
a margin is only needed where there is more to win than material, so a flat margin pays in every
position for a danger that exists in few. Lower the flat part and let the term raise it where it
matters. Crossover at 300 centipawns of difference — below that the margin is smaller than the old
50, above it larger.

The run reached its bound after 4702 games, fewer than any other version of this item, and
`0.5.0-014` had already rejected 35 in place of 50. Untested and the obvious control if the item is
picked up again: `qsAlphaSafetyMargin` 10 with the term switched off, which would say whether the
flat margin alone is responsible.

## 0.5.0-018 — quiescence cleanup plus the mate bound tested with `>=` and `<=`

**Three runs, the same two binaries, and not the same answer.** All against `0.5.0-001` with
H0 = -10 and H1 = 0:

| run | opening seed | result |
|---|---|---|
| 018 | default | **H0** after 9887 games |
| 018b | 20260901 | **H1** after 4253 games |
| 018c | default, fresh state file | **H1** after 2489 games |

The first run is the outlier, and 018c rules out the openings as the cause - same seed, same
positions, opposite decision. At alpha = 0.05 a run adopts the wrong hypothesis one time in twenty,
and this series ran about fifteen SPRTs. One of them landing wrong was to be expected; this was it.

That matters beyond this entry. **A single SPRT is one decision at its error rate, not a fact.**
Where a result is surprising relative to what the code can plausibly do, repeat it before building
an explanation on it - repeating is cheaper than the explanation, and far cheaper than the wrong
conclusion drawn from it.

The change itself is correct, and the argument is short. Quiescence scores a mated side at ply *p*
as `-MAX_VALUE + p`, so the parent at *p-1* gets at most `MAX_VALUE - p`; the best value reachable
at ply *q* is `MAX_VALUE - q - 1`. Once alpha has reached `MAX_VALUE - ply` the window cannot be
reached any more, and returning that bound is a correct upper bound. Mirrored for beta. `>=` would
even be safe one ply earlier. So `Search::nonSearchingCutoff` may test the bound the way
`Quiescence::search` does.

The cleanup half cannot affect play at all: built without the operator change it produces **byte
identical object files** for both changed translation units, and with the same `QAPLA_VERSION` the
whole binary is byte identical to `0.5.0-001` - same SHA-256.

What stood here before, that the cutoff prevents the search from re-deriving a mate it had already
found, was invented to explain the first run. It is wrong: the cutoff only removes work that could
not have changed the value.

## 0.5.0-019 — the quiescence cleanup alone

The version to keep. Against `0.5.0-001` it changes nothing but comments and dead code:

- every `ToDo` comment replaced by the decision of the SPRT that answered it, bounds and outcome,
  nothing else — no Elo figures, no percentages
- the commented out node level delta cutoff and the commented out beta probe deleted, together
  with the comments that described them

**No SPRT.** Both changed translation units compile to object files byte identical to those of
`0.5.0-001`, which is a stronger statement than any run could make. The EPD node count is
56158265 as expected.

## 0.5.0-020 — the state for release 0.5.0

The end of the quiescence work. Against `0.5.0-001` it carries exactly two things:

- the cleanup of `0.5.0-019` — every `ToDo` comment replaced by the decision of the SPRT that
  answered it, bounds and outcome and nothing else, and the commented out node level delta cutoff
  and beta probe deleted together with the comments describing them. Byte identical object code.
- `Search::nonSearchingCutoff` testing the mate bound with `>=` and `<=`, so it agrees with
  `Quiescence::search`. Correct, because the best value reachable at ply *q* is `MAX_VALUE - q - 1`
  and the bound itself is therefore out of reach too. SPRT with H0 = −10 and H1 = 0: H0 accepted
  once, H1 accepted in both repeats, see the `0.5.0-018` entry.

Everything else tried between `0.5.0-002` and `0.5.0-017` was rejected or left undecided and is
reverted; the code is on the `dead/qs-todo*`, `qs-evalmargin*`, `qs-delta*`, `qs-margin35` and
`qs-matecmp` branches with its tags.

Node count is expected to differ from `0.5.0-001` — the operator change is reached, at depth 18 but
not at depth 14, which is why the EPD ini now runs depth 18.

## 0.5.0-021 — the tt move ordered first among the quiescence captures

Reverted. SPRT against **`0.5.0-020`**, standard bounds: **H0 accepted** after 11587 games. EPD
nodes 243290670 → 243758240 at depth 18, different.

A second attempt at what `0.4.0-057` had already rejected, this time by the cheap route: a new
`MoveProvider::computeCaptures` overload that gives the tt move `MAX_CAPTURE_WEIGHT` right after
the capture weights are computed, so `findNextBestCaptureMove` picks it first. One weight
assignment, no extra selection stage, no separate list pass.

Making it cheaper does not make it right. The answer is the same as in 0.4.0-057.

The overload stays in `moveprovider.h` with the result recorded on it, unused; the quiescence uses
the two argument version again. Removing the call restores the node count of `0.5.0-020` exactly,
243290670.

This is also the first entry measured against `0.5.0-020` rather than `0.5.0-001` — from here on
that is the version a change has to beat.

## 0.5.0-022 — fail soft SEE with a threshold parameter

Kept as it is. SPRT against `0.5.0-020`, standard bounds: **undecided** at the 20000 game limit.
EPD nodes 243290670 → 251447508 at depth 18, different.

`computeSEEValue` is replaced by `computeSEEFailSoft`, which takes the threshold as a parameter and
exits early against it, instead of the caller setting a ±1 window on the member `alpha`/`beta`
before the call. The window members are gone from `clear()`, the bounds are local to the
computation.

It looked at first as though it bought throughput. It does not, and the way that came out is worth
keeping.

The EPD run at depth 18 showed `0.5.0-022` finishing in 12.40 s against 13.26 s, with more nodes -
apparently a tenth more nodes per second. Measured properly, one position at a time and single
threaded, the two are the same speed: 2.21 against 2.19 million nodes per second, `0.5.0-022` a
shade slower, and the same in every phase group.

The wall clock difference is load balancing. The EPD ini runs concurrency 10 over 100 positions, so
the wall time is a makespan, not a sum. Summed over the single position runs, `0.5.0-020` needs
110.05 s of computing time and `0.5.0-022` needs 114.86 s - more, as it must, since it searches
more nodes at the same rate. Scheduling those per position times greedily onto ten workers predicts
12.91 s and 12.10 s, within 0.3 s of what the runs actually took. `0.5.0-020` loses 17 % of the
wall clock to idle workers, `0.5.0-022` only 5 %, because its packets happen to divide better.

**A wall clock time from the EPD run is not a speed measurement when the node counts differ.** The
runtime criterion in `CLAUDE.md` applies to behaviour neutral changes, where the counts are
identical and the work per worker is the same; outside that case it measures how the positions
happened to distribute.

The per position counts also put a number on the chaos the node count rule describes: 44 positions
searched more, 56 searched fewer, single positions moved by factors from 0.44 to 1.77, and the
twenty largest deviations sum to 146 % of the total difference - they nearly cancel. The net
difference between the two totals is a residue, not a trend. There is no phase pattern behind it
either: the opening and middlegame group moved by a factor of 1.06, the late middlegame by 0.89,
and those two groups pull against each other.

So: the restructuring is free, not faster. Undecided over 20000 games is the whole of what the
SPRT says about it.

## 0.5.0-023 — SEE skips the exchange when the opponent cannot recapture

Kept. SPRT against the state before the change, standard bounds: **H1 accepted**, +6 Elo from 9051
games, LLR 2.96, winrate 50.85 % (W:2374 D:4456 L:2221), 50 minutes.
EPD nodes 243290670 -> 251565903 at depth 18, different.

`computeExchangeValue` tests the destination square against `attackMask[opponent]` before it sets
up the exchange. Is the square undefended, the captured piece is the whole result and the sweep
over pawn, knight, bishop, rook and queen - three magic lookups among them - never runs. In the
quiescence that case is the bread and butter, not the exception.

The mask is a lower bound on the attackers of the square. During an exchange pieces are only
removed from `allPiecesLeft`, and the capturing piece lands on the target square itself, where it
blocks no ray to that square. A defender that the departing piece was covering is therefore
missed. The value comes out too positive, so a move is searched that would have been pruned - the
error can cost nodes, never a move.

That the masks are built without the enemy king does not reach SEE. The distortion needs a
checking slider whose ray continues past the king, and the squares behind the king are the only
ones affected. Capturing the checker happens on the near side of the ray; the king capturing there
would stay in check; and any other capture behind the king neither takes the checker nor blocks
the ray, so it is not a legal evasion and never generated.

The 3.4 % extra nodes are the missed defenders, the 5.2 % shorter runtime the saved probes. Both
numbers come from three interleaved pairs against the baseline binary, node counts identical
within each engine, so the wall clock comparison of `0.5.0-022` does not apply here - it was the
same binary shape and the same worker packing on both sides.

Still open: presetting `nextPiece` from per piece type attack masks, so the probes for types that
cannot reach the square are skipped as well. `computeAttackMask` already builds those masks and
discards them. A rook can never hide a bishop attack on the same square - only the queen sits on
both ray kinds - and the `QUEEN` branch already falls back to `BISHOP`, so the preset stays
complete.

## 0.5.0-023 — the quiescence capture handling moved out of MoveProvider

Untested against `0.5.0-022`, and **not** node neutral: EPD nodes 251447508 → 251565903 at depth 18.
It is the baseline the three quiescence tests below were measured against, so what it is worth on
its own is still open.

The capture generation, weighting and selection move from `MoveProvider` into `Quiescence`, which
owns a local move list per node. One thing changed with them: `doFutilityOnCapture` took the
captured piece and derived the colour from it, and for a move that captures nothing — a plain
promotion — `getPieceColor(NO_PIECE)` is WHITE whatever the side to move. It now takes the
opponent's colour directly. That is the node count difference.

## 0.5.0-024 — no recapture bonus in the capture ordering

Reverted. SPRT against `0.5.0-023` with the bounds the ToDo asked for, H0 = −4 and H1 = +1:
**undecided** at the 20000 game limit. EPD nodes 251565903 → 245850456, different.

The ordering added 10 to the weight of a capture on the square the previous move captured on, so
recaptures come ahead of equal captures. Removing it does not separate from keeping it at these
bounds. `0.4.0-039` had already rejected the stronger form of the same idea, every recapture ahead
of every other capture.

## 0.5.0-025 — the selected capture swapped to the front instead of dragged

Kept or reverted at will. SPRT against `0.5.0-023`, standard bounds: **undecided** at the 20000
game limit. EPD nodes 251565903 → 253591386, different.

`selectNextCapture` moved the chosen capture to the front with `dragMoveToTheBack`, shifting
everything between it and the front one step back to keep the relative order of the rest. The order
is not needed — the next move is chosen by weight — so a swap does the same job in constant time
instead of linear.

It is not free of play effects: `findNextBestCaptureMove` takes the first move of equal weight, so
reordering the remainder changes the tie breaks, which is why the node count differs. The run does
not separate the two at these bounds. As a simplification with no measurable price it can stay;
as an improvement it is not one.

Note that the runtime criterion cannot be used here: the node counts differ, and a wall clock from
the EPD run is a makespan, see the `0.5.0-022` entry.
