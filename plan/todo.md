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

## 10. Opposite coloured bishops: scale the surplus

**Priority 3** — done: [ ]

A material advantage with bishops on opposite colours and nothing else but pawns is usually a
draw and has to be scaled down.

**Hand written factors, no CLOP** — the position type is too rare for a signal. For the same
reason measure it on a start position set of opposite coloured bishop endgames, not on the
standard book.

## 11. Pawn shield: activate and tune the weights

**Priority 3** — done: [ ]

`computePawnShieldValue` is not part of the king attack evaluation. Activate it and tune its
weights together with the other king attack parameters.

Already tried once.

## 12. Space weight

**Priority 3** — done: [ ]

`spaceWeightMg` defaults to 0, the space evaluation is off. Tune it. If it fails again, remove
the space code.

Already tried once, hence the 0.

## 13. Forward futility: honour ttValueIsLessOrEqualAlpha

**Priority 3** — done: [x] — `0.4.0-081` undecided at 50.0 %, `dead/ff-ttalpha`

The claim that `setFromParentNode` resets the flag before the line is reached no longer held, so
the guard was reachable and could be measured for the first time. It moves 2.8 % of the nodes and
lifts the EPD success rate by a point, but the full 20000 games came out at 50.02 %. The line is
back in the code, commented out, with the result above it.

## 14. SEE based capture ordering

**Priority 3** — done: [x] — `0.4.0-082` lost 3.5 Elo, `dead/see-loosing-order`

Tried in the one shape the area had not seen: the loosing captures, which come back from the
deferral in generated order, sorted by their exchange value instead. Cheaper (3 % fewer nodes, a
faster EPD run) and still 3.5 Elo worse. Fourth loss in the capture ordering, after `0.4.0-039`,
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