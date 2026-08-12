# To do

Candidates that can gain Elo. Each one gets its own tag and its own SPRT, results go into
[version-log.md](version-log.md).

Priority 1 first, 3 last. The **Cleanups** section at the end carries no priority.

## 1. King attack: queen term on the right side

**Priority 1** — done: [x] — `0.4.0-046`, ≈ +5.7 Elo


[king-attack.h:170](../eval/king-attack.h#L170) counts the queen of the side whose king
is under attack instead of the attacking side:

```cpp
(position.getPieceBB(QUEEN + COLOR) != 0) * queenFactor
```
Tune all available king attack uci parameters with clop. 

## 2. ClockManager with RAMP_BEGIN_TIME = 2000

**Priority 1** — done: [x] — kept as `0.4.0-044`, Elo neutral

## 3. LMR / MCP: make every factor tunable

**Priority 1** — done: [x] — `0.4.0-047` ≈ +5.7, `0.4.0-048` ≈ +14.3, `0.4.0-051` ≈ +6.0 Elo

Every relevant factor of the late move reduction and of the move count pruning becomes tunable.

PV nodes and capture moves get a reduction of their own, loosing and winning captures separable.
Passed pawn pushes get a reduced reduction factor, not an exemption; promotions count as passed
pawn pushes.

Counter check at the end: take captures out of the reduction again, an early `return 0` for every
capture as it was before. SPRT only, no CLOP. The capture term only survives a clear H0 — on H1
*and* on undecided it goes out of the code entirely.

## 4. All futility margins tunable, then re-optimized

**Priority 1** — done: [x] — `0.4.0-052`, ≈ +7.1 Elo

Every futility margin becomes tunable: forward futility, the move based futility pruning often
called razoring, and the quiescence margin. Then one CLOP run over all of them — they interact.

## 5. Replace IID by IIR

**Priority 1** — done: [x] — `0.4.0-054`, ≈ +5.3 Elo over the closing run. All five experiments
run, the closing run confirms the winner.

The internal iterative deepening in [search.cpp:243](../search/search.cpp#L243) is replaced by an
internal iterative reduction.

Make sure that you check PV AND ttMove, if PV move is availabl it is also fine - no reduction
Make sure to not reduce ALL nodes, it is normal, that ALL nodes have no TT-Move

Topic to Sprt in IIR (multiple sprt, no clop):
1. Experiment: Extend the current IID to work also in CUT nodes, but never in ALL nodes.
2. Compare it with IIR in PV-Nodes And Cut-Nodes (the best version of point 1, so either den PV only or the PV+CUT node version)
3. Reduce PV nodes even more (PV nodes without ttMove are very expensive), try 1, 2, 3
4. Reduce Cut nodes more, if tt-Value is below alpha (upper bound) AND there is no tt-Move (no tt-move AND >= alpha -> reduce by 1, no tt-move AND UpperBound -> Reduce by 2 or 3)
5. Run IIR only for PV nodes, no CUT nodes
6. As so many SPRT increases the chance of false positive SPRT, run a final SPRT against the original IID version with the best result of all IIR tries to prove that it holds true in a single last SPRT run even, if the SPRT 5 was positive and you run the idential SPRT twice.


## 6. Quiescence: try the tt move first

**Priority 1** — done: [x] — `0.4.0-057` lost 4.6 Elo, `dead/qs-ttmove`

The quiescence never tries the tt move first, in neither of its two paths. It should.

## 7. Aspiration window: the delta term is always zero

**Priority 2** — done: [x] — both runs lost, `dead/aspiration-delta`; the term is out of the code

[aspirationwindow.h:98-99](../search/aspirationwindow.h#L98-L99) overwrites the previous position
value before the difference is taken, so the window never widens for a value that jumped between
iterations.

Add tunable parameters to the aspiration window and optimize them with clop

- Try with original values (a second sprt), if the clop values does not result in elo
- Fix the bug in any case, if bose sprt does not work by keeping the logic as is but without bug (so no value-based window widening but not in a buggy way)

## 8. Quiescence tt control flow with the isPV flag

**Priority 2** — done: [x] — `0.4.0-061` lost 5.8 Elo, `dead/qs-ttpv`

The early tt return in [quiescencese.cpp:102](../search/quiescencese.cpp#L102) fires in every
node, which leaves the stand pat refinement below it unreachable. Bind it to a null window again,
and additionally let it fire in PV nodes when the tt entry carries the `isPV` flag — a value from
a real window search may cut a PV node.

Assume the plain guard already lost an SPRT; the `isPV` part is what is new.

## 9. Normal search time for longer time controls

**Priority 2** — done: [ ]

Only the maximum search time has been tuned so far, not the normal one
([clockmanager.h:283](../search/clockmanager.h#L283)). Other engines spend clearly more time in
the opening phase at longer time controls.

Add tunable parameters to control the time usage. It should depend on the time available so shorter time available gives even relatively shorter time. 1 minute for a game is considered as long time, 20s as medium, 5s as short time, 1s very short (to give you a relation). Optimize it with clop in timecontrols 5s + 10ms, 20s + 100ms and 60s + 1s - I know this will take long. Organize the parameters in a way that some parameters influences short time controls more and some parameters longer timecontrols so that you are able to mix the result from the three clop runs. Be linear in the formular no jump even not in first and second derivatives

### Concept

The move time is a product of three independent factors. Each answers one question, none of
them is derived from another:

```
moveTime = fairSlice(timeLeft, movesPlayed) * clockShare(timeLeft) * modeFactor(searchFinding)
```

**fairSlice** — `timeLeft / (movesToGo + 2)`, what one move gets if the rest of the game is
played evenly. The `+ 2` is the safety against losing on time.

`movesToGo` is where the moves played enter, and today it is
`max(AVERAGE_MOVE_COUNT_PER_GAME - movesPlayed / 2, KEEP_TIME_FOR_MOVES)` — a kink at move 50,
where the slope jumps from −0.5 to 0. Replace it by the same saturating shape the share uses:

```
movesToGo = keep + (start - keep) * midpoint / (midpoint + movesPlayed)
```

`movesToGo` keeps its meaning, the forecast of the moves still to be played, and its role in
`timeLeft / (movesToGo + 2)` is unchanged. Only the course changes: instead of falling linearly
and clamping hard at move 50 it approaches `keep` asymptotically.

| | meaning | today |
|---|---|---|
| `start` | the forecast at move 1, so the length of a game the engine plans for | 60 (`AVERAGE_MOVE_COUNT_PER_GAME`) |
| `keep` | the forecast a long game settles on, so how many moves the engine always keeps time for, no matter how long the game has already run | 35 (`KEEP_TIME_FOR_MOVES`) |
| `midpoint` | where the transition sits: at `movesPlayed == midpoint` the forecast is exactly halfway between the two, so 47.5 for 60 and 35 | 25 to 30 reproduces today's line |

Monotone falling, saturating at `keep`, continuous in every derivative.

`keep` is the one that matters late: a game that is still running after move 80 gets
`timeLeft / 37` per move, so the engine never spends its last reserve on a single move. `start`
decides how fast the opening is played, `midpoint` how quickly the engine moves from the one
regime into the other.

`movesPlayed` here means full moves. `ClockSetting::getPlayedMovesInGame()` counts plies, today's
code divides by two for that reason and the new formula has to do the same.

**clockShare** — how much of that fair slice a move may take, as a function of the time still on
the clock. Already implemented:

```
share = shareMin + (shareMax - shareMin) * timeLeft / (timeLeft + halfTime)
```

`shareMin` governs short time controls, `shareMax` long ones, `halfTime` places the transition.
That separation is what lets three runs at three time controls be combined.

**modeFactor** — the situation the search reports. This one is discrete by nature and that is
fine: it is a function of the search finding, not of the clock, so it cannot make the time jump
as the clock runs down. Today `SearchState::modifyTimeBySearchFinding` has hard values 1, 4, 15
and 1/5; all of them become tunable coefficients.

### What Spike had and Qapla does not

Read from `C:\Development\SpikeEngine\src`, `TimeControl.cpp` and `TimeCalc.h`. Qapla already
inherited the mode machine including the factors 4 and 15, and the thresholds 0.7 and 0.8 of the
average time for starting another iteration or another root move. Missing:

- **A normal factor above one.** Spike's `cNormal` multiplies by 10/8 = 1.25 and its `cSilent`
  by 0.9. Qapla's normal mode multiplies by 1.0 and it has no silent mode. Since nearly every
  move is normal, Qapla spends about a fifth less than Spike did on ordinary moves — the most
  likely single reason why other engines look slower in the opening. The normal factor is the
  first coefficient to tune.
- **A forced move mode.** Spike cut the time to a fifth, capped at 333 ms, when the move was
  forced. Qapla plays a forced move at full price.
- **A hard abort at 1.2 × average.** Spike stopped there, capped by the maximum. Qapla only
  stops at the maximum, which is far larger, so a single iteration can eat the whole reserve.
- **A seeded start value.** Spike started a game with the previous value at −0.8 pawns so that
  the first move does not look like a collapse and trigger the critical mode.

### Order of the runs

One parameter per run, each at the time control where it has the most leverage, applying each
result before the next run starts:

1. `timeShareMin` at 5+0.01 — done, 80 → 135
2. `timeShareMax` at 60+1
3. `timeShareHalfTime` at 20+0.1
4. the normal mode factor at 20+0.1
5. the `movesToGo` coefficients

The closing proof is an SPRT at a long time control, not at 5+0.01 — a change aimed at long
games shows nothing there.

## 10. Opposite coloured bishops: scale the surplus

**Priority 3** — done: [x] — `0.4.0-084`, about +6 Elo ± 3

Hand written factors 45, 50, 65, 85, 100 percent by pawn surplus, applied at the end of `lazyEval`
after the tempo bonus and the fifty move damping. The first attempt `0.4.0-083` put them into the
piece signature hash, whose entries replace the value and therefore switch that damping off - two
opposite effects, flat result.

The start position set (`tools/gen-ocb-endgames.py`, `test/sprt/sprt-ocb.ini`) turned out to be the
wrong instrument and says so itself: 49.89 % there against 50.85 % on the standard book. Inside the
material class the scaling multiplies every leaf by the same factor and cannot change a move; what
it decides is whether to enter such an endgame, and that decision is not in a set that starts
inside one.

## 11. Pawn shield: activate and tune the weights

**Priority 3** — done: [x] — `0.4.0-086`, about +8 Elo ± 3

Activated as a term of the king attack value, and first given a shape a run can work on: index 7,
the full shield, pinned at 0, because a constant over all eight factors cancels between the two
kings and no run could have resolved it. CLOP over the remaining seven, 5000 samples, then 51.11 %
over 6783 games.

Tuned alone, not together with the other king attack parameters as written here — that would be
15 values in one run and a run takes at most 10. Re-tuning the attack weights against the shield
is still open.

## 12. Space weight

**Priority 3** — done: [x] — `0.4.0-085` lost about 20 Elo, `dead/space-weight`

CLOP over -40 to 40, 2000 samples, estimate -2.1, so the null point. The estimate was then set
aside and the ported weight of 100 measured on its own: 47.18 % over 2641 games. Both answers
point the same way and the game result is the harsher one.

Switched off again, code left in place with the measurement written at the call in `lazyEval` and
at the default weight — not removed, so that the number stands where the idea would come back.

## 13. Forward futility: honour ttValueIsLessOrEqualAlpha

**Priority 3** — done: [x] — `0.4.0-081` undecided at 50.0 %, `dead/ff-ttalpha`

The claim that `setFromParentNode` resets the flag before the line is reached no longer held, so
the guard was reachable and could be measured for the first time. It moves 2.8 % of the nodes and
lifts the EPD success rate by a point, but the full 20000 games came out at 50.02 %. The line is
back in the code, commented out, with the result above it.

## 14. SEE based capture ordering

**Priority 3** — done: [x] — `0.4.0-082` lost about 4 Elo, `dead/see-loosing-order`

Tried in the one shape the area had not seen: the loosing captures, which come back from the
deferral in generated order, sorted by their exchange value instead. Cheaper (3 % fewer nodes, a
faster EPD run) and still about 4 Elo worse. Fourth loss in the capture ordering, after `0.4.0-039`,
`-040` and `-042`.

Untried: a tie break by the capturing piece inside an equal captured value, so real MVV/LVA where
only the MVV half exists today. Not part of this item any more.

---

# Cleanups

No priority, no Elo expected. The EPD node count must come out **identical**, that is the proof.
No SPRT, no tag.

## 15. Attack tables into the ply, out of the position

The attack masks are computed by the position because move generation needs them. Move them into
the ply and hand them to the move generation by reference, to be created and updated there.

Today they are rebuilt in many places because a sub search can change them, and missing one of
those rebuilds has caused real bugs more than once.

## 16. Castling encoded as king to its own rook square

Castling is generated as the king moving to `g1` or `c1`. Encode it as the king moving onto its
own rook's square instead — that is what Chess960 needs and it is the simpler logic.

**Not node stable**, unlike the rest of this section: the butterfly history is indexed by piece
and destination, so castling moves into a different history slot.

## 17. Remove passedpawn.h

Find the best architecture to get the passed pawn information from eval (the bitboard with passed pawns) and remove passedpawn.h. Double check nps to see, if it is really faster than the current implementation that is quite simple.
One option is to have the EvalResults structure as ref parameter in eval and the real occurence is in ply. Then search can always access it anywhere. 
Double check, if there are other optimization opportunities by doing so.

# Features

## 18. Implement syzygy bases support

Based on stockfish code