# Eval: to try

Ideas for the hand-crafted eval, mostly found by comparing Qapla's eval with Spike's on positions from
`test/eval-weakness/` (`compare-eval.py`). Each item is an idea, not a result; it is decided by SPRT like any
other change.

Spike's internal unit is half a centipawn (`cVCPFact = 2` in Spike's `Consts.h`): divide every Spike value by 2
to compare it with Qapla. The output of `compare-eval.py` shows Spike's values unconverted.

## Strong penalty for low mobility of range pieces, mainly in the endgame

Spike adds a "cramped" penalty on top of its linear mobility when a piece has fewer than 4 squares
(`cMobilityMg/Eg` in Spike's `EvalBase.h`): knight and bishop in midgame and endgame, rook and queen in the
endgame only. Spike's values in cp, cramped penalty plus linear mobility (`cValMob*`); Spike's per-square field
values (`mFldAttack*`) depend on the squares and are left out:

| squares | bishop mg / eg | rook mg / eg | queen mg / eg |
|---|---|---|---|
| 0 | -64 / -96 | 0 / -96 | 0 / -96 |
| 1 | -31 / -48 | 1 / -48 | 0 / -47 |
| 2 | -14 / -23 | 2 / -23 | 0 / -21 |
| 3 | -5 / -11 | 3 / -11 | 0 / -8 |
| n >= 4 | n / n/2 | n / n/2 | 0 / 1.5 n |

What counts as a square in Spike:
- bishop: empty, not attacked by an enemy pawn; the ray passes own bishop/queen and enemy rook/queen.
- rook: empty, not attacked by an enemy pawn or minor; the ray passes own rook/queen and the enemy queen. Not
  evaluated at all in the pure midgame (phase 64).
- queen: empty, not attacked by an enemy pawn, minor or rook; no pass-through. Endgame only.

To try: high negative endgame values for the low entries of the mobility tables of the range pieces (bishop,
rook, queen), then tune the tables (`PARAM_OPTIMIZE_BISHOP`, ... with the low entries in range). Qapla needs no
new term, the tables already have one entry per square count. Spike penalises cramped knights as well; the
knight table is a candidate too.

## Blocked d/e pawn on its home rank, midgame only

Spike's development term (`evalWhiteDevelopment` in Spike's `Eval.h`): a pawn on d2/e2 (d7/e7 for black) whose
square in front is occupied by any piece costs `cValBlockedPawn_D_E` = -30, i.e. -15 cp. Spike applies it in the
midgame and the endgame alike; in the endgame that looks like a small bug in Spike.

To try: the same penalty for the midgame only, -15 cp as the starting value, then tune it.

## Isolated pawn by square, heavier in the endgame the further back it stands

Spike scores an isolated pawn by its square (`aIsolatedPawn_Mg/Eg` in Spike's `EvalBase.h`), Qapla with one value
for every square. Spike's penalty in cp, from white's point of view (files a..h):

| rank | mg | eg |
|---|---|---|
| 7 | 14 15 16 17 17 16 15 14 | 14 15 16 17 17 16 15 14 |
| 6 | 14 15 16 17 17 16 15 14 | 15 16 17 18 18 17 16 15 |
| 5 | 14 15 16 17 17 16 15 14 | 16 17 18 19 19 18 17 16 |
| 4 | 13 14 15 16 16 15 14 13 | 17 18 19 20 20 19 18 17 |
| 3 | 12 13 14 15 15 14 13 12 | 18 19 20 21 21 20 19 18 |
| 2 | 12 13 13 15 15 13 13 12 | 19 20 21 22 22 21 20 19 |

On top of that in Spike: isolated on a half-open file (no enemy pawn in front) -10 / -5 cp, more than one isolated
pawn on the file -5 / -15 cp.

To try: an isolated pawn penalty by rank (and possibly file), with the endgame penalty growing towards the pawn's
home rank, then tune it.

## Single bishop and pawns it cannot defend (endgame, not in Spike)

In the endgame a side whose only minor piece is one bishop has a problem with every pawn that stands on a square
of the other colour and cannot move on (blocked): the bishop can never defend it, so only the king and the rooks
can. Against more rooks (two against one, for example) those pawns are lost one after the other, passed pawns
included.

To try: an endgame penalty per such pawn for the side with exactly one bishop, possibly scaled by how many of the
opponent's heavy pieces can attack it compared with the own ones.
