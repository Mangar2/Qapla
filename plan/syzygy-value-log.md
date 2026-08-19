# Syzygy: turning a table answer into a search value

What a win/draw/loss answer is worth inside the search is not obvious, and the first shape we
tried lost 60 Elo. This file records every shape that was measured, so that a later reader can
see what was already tried instead of trying it again.

## How the runs are done

Every run pits **the same executable against itself**, one side with `SyzygyPath` set and one
without. That isolates the question: does asking the tables help, with this mapping? The
no-tables side behaves identically across all runs, so the numbers are comparable.

```
champion   Qapla-noTB    build/Release/Qapla.exe, no SyzygyPath
challenger Qapla-TB      the same file, SyzygyPath = C:/Chess/syzygy/tables
tc         5+0.05        openings test/opening/book8ply.raw
bounds     H0 = -2, H1 = 3, alpha = beta = 0.05, normalized, pentanomial
maxgames   20000         concurrency 30
adjudication  none - neither draw nor resign, deliberately
```

Started through the GUI's remote control (`qcg.exe --remote-control`), results kept as
`test/log/sprt-syzygy-<id>.qsprt`.

Tables installed: 3, 4 and 5 pieces complete, 145 win/draw/loss and 145 distance files.

## What the search does with the answer

The probe sits in `Search::hasTablebaseCutoff`, one call site in the inner node. It only runs
where the answer is unconditionally valid: piece count within the loaded cardinality, halfmove
clock at zero, no castling rights, and no capture or promotion available. The mapping under
test is `ChessEval::tablebaseWdlToValue` in `eval/tablebase-value.h`.

Two limitations apply to every run below and are not part of what is being measured:

- The root is never probed, so an endgame already on the board is played on the evaluation
  alone. Only trading *into* a table position is covered. Fixing this is the root ranking.
- A win or loss is used as a bound, so it only cuts at beta or alpha. A draw is exact and cuts
  in any window.

## Runs

### V0 - evaluation plus winning bonus, unguarded

```
Win  -> currentValue + WINNING_BONUS
Loss -> currentValue - WINNING_BONUS
Draw -> DRAW_VALUE
CursedWin/BlessedLoss -> currentValue +/- one pawn
```

**843 games, +130 =438 -275, score 0.4140, -60.4 Elo. H0 accepted.**

Broken, and the run only confirmed it. `currentValue` was `node.adjustedEval`, which is
`NO_VALUE` (-30001) whenever the side to move is in check - `search-node.h` does not evaluate
in check, and `search.cpp` then falls back to the evaluation from two plies earlier, which is
itself `NO_VALUE` near the root. A win therefore became `-30001 + 10000 = -20001`: not merely
wrong but sign-inverted, and the cutoff test read it as a loss and cut on alpha. **Won endgames
were cut as lost**, and the value went into the transposition table with the tablebase depth
bonus on top.

The condition that a probe needs the halfmove clock at zero means every probed node was reached
by a capture or a pawn move, and those give check often, so this was not a rare corner.

A second defect of the same line, independent of the first: the sum was unbounded. Qapla's
evaluation reaches five figures in endgames - a measured case gave `adjustedEval = -10303` - so
`eval + WINNING_BONUS` can reach the mate band at 29000 and make a table win indistinguishable
from a mate.

Saved as `test/log/sprt-syzygy-v0-broken.qsprt`.

### V1 - the same, guarded and clamped

```
gradient = currentValue == NO_VALUE ? DRAW_VALUE : currentValue
Win  -> clamp(gradient + WINNING_BONUS, TB_WIN_MIN, TB_WIN_MAX)   =  10000 .. 27999
Loss -> -clamp(WINNING_BONUS - gradient, TB_WIN_MIN, TB_WIN_MAX)  = -27999 .. -10000
Draw -> DRAW_VALUE
CursedWin/BlessedLoss -> no cutoff at all
```

The two defects of V0 and nothing else. The gradient is still the evaluation, so the shape
under test is unchanged - only the missing value and the missing bound are repaired.

Cursed win and blessed loss stop cutting here and stay that way for V2 and V3, so that the
win and loss mapping can be read on its own. They get their own runs afterwards.

Binary kept as `c:/Development/qapla-tb-variants/v1-eval-clamped.exe`.

**3113 games, +795 =1664 -654, score 0.5226, +15.7 Elo. H1 accepted.**

So the tables are worth something once the value reaching the search is not nonsense, and the
whole of V0's -60 Elo was the missing guard, not the shape. Saved as
`test/log/sprt-syzygy-v1.qsprt`.

What is established is the decision, not the number: through this mapping and through the inner
node alone, the tables are worth having. The root is still never probed, so whatever Syzygy can
give in total, this run does not reach it.

### V2 - winning bonus plus material difference

```
gradient = position.getMaterialValue(position.isWhiteToMove()).endgame()
```

V1 with a different gradient, nothing else changed. The evaluation carries endgame terms that
reach five figures, which is what forced the clamp in the first place; the material difference
stays within the few pieces a table position can hold and says the one thing that converts a
won ending - do not hand material back.

**12981 games, +3256 =6618 -3107, score 0.5057, +4.0 Elo. H1 accepted.**

Same decision as V1 at the same bounds, so the same thing is established and nothing more.

Binary kept as `c:/Development/qapla-tb-variants/v2-material.exe`, saved as
`test/log/sprt-syzygy-v2.qsprt`.

### V3 - a fixed value, no gradient

```
Win  ->  TB_WIN_VALUE   (28000)
Loss -> -TB_WIN_VALUE
```

`TB_WIN_VALUE` flat, the way the distance mapping already works. Costs the gradient entirely:
every won position is worth the same, so the engine has nothing telling it how to make
progress and has to find that in the search below the cutoff. The cleanest shape if it holds
up - and after V2 the expectation is that it will not, since it removes what V2 showed to be
the valuable part.

**5516 games, +1399 =2858 -1259, score 0.5127, +8.8 Elo. H1 accepted.**

Same decision again, at the same bounds.

Binary kept as `c:/Development/qapla-tb-variants/v3-fixed.exe`, saved as
`test/log/sprt-syzygy-v3.qsprt`.

## Where the series stands

| run | gradient under the win | Elo | games | decision |
| --- | --- | --- | --- | --- |
| V0 | evaluation, unguarded | -60.4 | 843 | H0 |
| V1 | evaluation, guarded and clamped | +15.7 | 3113 | H1 |
| V2 | material difference | +4.0 | 12981 | H1 |
| V3 | none, flat 28000 | +8.8 | 5516 | H1 |

**The Elo column ranks nothing.** An SPRT decides between its two hypotheses and says nothing
beyond that. V1, V2 and V3 ran against the same bounds and all three accepted H1, so all three
established exactly the same statement - with this mapping the tables are not worth less than
-2 Elo - and after these runs the three are to be judged as equal. The point estimates are
recorded for completeness only. They are not measurements of the difference: a run stops the
moment it crosses a bound, so a run that happened to fluctuate upwards ends early with an
inflated estimate, and V1 ended earliest of the three.

Separating two mappings needs a run whose bounds separate them - one where the weaker mapping
would accept H0 and the stronger one H1. That is a different run, not a comparison of these
numbers.

### V4 - closing run, V1 against V3 head to head

Both with tables, playing each other, so the question is purely which mapping is better rather
than what each is worth against a table-less opponent. Bounds as before, H0 = -2, H1 = 3, read
as: H1 accepted means V1 beats V3 by roughly three Elo or more; H0 accepted means it does not,
and the two stay indistinguishable.

**20000 games, +4874 =10267 -4859, 50.0%, LLR -0.09 against bounds of +/-2.94. No decision.**

The run went the full distance without approaching either bound, so the two mappings are
indistinguishable within the three Elo the bounds were set to detect - and with 20000 games
behind it, an LLR of -0.09 is not a run that was about to decide either way. Neither the
gradient nor the clamp is worth anything measurable on top of the cutoff itself.

Saved as `test/log/sprt-syzygy-v4-v1-vs-v3.qsprt`.

## Decision

The statistics do not choose, so the code does. **V3 is kept**, on grounds that are engineering
and not measurement:

- it does not read the evaluation at all, so the `NO_VALUE` trap that cost 60 Elo in V0 cannot
  come back through this path
- no clamp is needed, because a constant cannot leave its band
- a table win is one named constant in one band, next to the distance mapping that already
  works that way

V1 stays recorded above rather than deleted: it is not worse, and if a later change gives the
tables more to say - a root probe above all - the question of a gradient underneath the win is
open again, and the run that answered it once is here.

## Cursed win and blessed loss

Through V0 to V4 these two never cut at all, so that the win and loss mapping could be read on
its own. A run that gave them their own value - one pawn, cutting as a bound - was started
against the version that does not cut them and **stopped without a decision**: after V4 there
was no reason left to expect one. A cursed win is rare enough in five piece tables at this time
control that no run of this length can see it, and the two candidate values are a pawn apart.

They are therefore scored as **plain draws**, which is what they are under the fifty move rule
that the engine plays by, and which lets them cut exactly like a draw. With
`Syzygy50MoveRule` switched off they are remapped to win and loss before this point and never
reach the mapping, which is the whole meaning of that option.

## Final mapping

```
Win         ->  TB_WIN_VALUE   (28000)
Loss        -> -TB_WIN_VALUE
Draw        ->  DRAW_VALUE
CursedWin   ->  DRAW_VALUE
BlessedLoss ->  DRAW_VALUE
```

No parameters, no dependence on the evaluation, nothing that can carry an unset value into the
search.

## F1 - the whole feature against the version before it

Everything above measured one mapping against another. This run asks the only question that
matters for keeping the feature: is Qapla with Syzygy support stronger than Qapla was before
any of it was written?

```
champion   Qapla-preSyzygy  built from d63afd8, the last commit before the first Syzygy code
challenger Qapla-Syzygy     the final mapping above, SyzygyPath set
bounds     H0 = 2, H1 = 7
```

The bounds are raised deliberately. Everything else in this file was measured at H0 = -2, which
only ever established "not worse"; here the question is whether the feature earns its place, so
H0 is set at two Elo and the run has to clear seven to accept.

Note what is inside the challenger besides the probe: the retired file bitbases, the UCI option
provider, the option list that replaced `qaplaBitbasePath` and `qaplaBitbaseCache` with the four
`Syzygy*` options. The run measures the whole change, not the probe alone.

**Result: pending.**

