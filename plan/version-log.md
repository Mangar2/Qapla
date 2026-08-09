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

## Notes on reading this log

Seven SPRTs ran in sequence over the same code area, keeping whatever passed. At alpha = 0.05
each single run has a 5 % chance of accepting a change that is worth nothing, and over a chain
that adds up. A closing SPRT of the head against 0.4.0-035 is the honest number for the two
survivors, not the sum of the two individual gains.
