
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
