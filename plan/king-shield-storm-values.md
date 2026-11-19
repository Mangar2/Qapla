# King shield and pawn storm: value sets

Values of the king shield (`PAWN_INDEX_FACTOR_DEFAULT`, `eval/king-attack.h`) and the pawn storm
(`PAWN_STORM_RANK_DEFAULT`), midgame cp, from the side whose king is examined. Shield index: bit 4 = a pawn in front
of the king (F), bit 2 = one to the west (W), bit 1 = one to the east (E); for a king on g1 that is g, f and h.

- **original**: 0.5.0-043, the shield before the re-tuning, no storm term.
- **CLOP**: joint CLOP of shield and storm on Linux (5000 samples at 5+0.01, 30 engines, storm rank 4 fixed at 10,
  2026-11-19), tested as 0.5.0-045.
- **Claude**: the CLOP values made monotone - one shield pawn more never makes it worse; the frequent cases (one pawn
  missing) keep their CLOP value, the rarer ones are set just below; storm rising with the rank (rank 5 changed its
  sign between two CLOPs and is not determined).
- **Volker**: Volker's set (2026-11-19). He gave no value for "no shield pawn"; it is set to -60, as bad as the worst
  case with two pawns missing (it must not be better).

| term | meaning (king on g1: W = f, F = g, E = h) | original | CLOP | Claude | Volker |
|---|---|---|---|---|---|
| shield 7 | all three present | 0 | 0 (fixed) | 0 | 0 |
| shield 6 | h missing | -10 | -19 | -19 | -25 |
| shield 5 | f missing | -8 | -28 | -28 | -25 |
| shield 3 | g missing | -17 | -34 | -34 | -35 |
| shield 4 | only g present | -15 | -33 | -33 | -50 |
| shield 2 | only f present | -24 | -23 | -35 | -60 |
| shield 1 | only h present | -19 | -31 | -36 | -60 |
| shield 0 | none present | -13 | -26 | -38 | -60 |
| storm rank 4 | per storming pawn on the attacker's rank 4 | - | 10 (fixed) | 10 | 10 |
| storm rank 5 | per storming pawn on rank 5 | - | -15 | 15 | 20 |
| storm rank 6 | per storming pawn on rank 6 | - | 19 | 20 | 40 |

Array order of `PAWN_INDEX_FACTOR_DEFAULT` (index 0..7):
- original { -13, -19, -24, -17, -15, -8, -10, 0 }
- CLOP { -26, -31, -23, -34, -33, -28, -19, 0 }
- Claude { -38, -36, -35, -34, -33, -28, -19, 0 }
- Volker { -60, -60, -60, -35, -50, -25, -25, 0 }

## Tournament

Round robin on the M4 (2026-11-19): original (0.5.0-043), Claude, Volker - 50 rounds of 50 games per pairing, 2+0.004
(the 5+0.01 of the CLOP scaled to the M4), 8 engines, 7500 games:

| engine | Elo | +/- | games | score | draws |
|---|---|---|---|---|---|
| original (0.5.0-043) | 2605.0 | 7 | 5000 | 51.08 % | 45.5 % |
| Claude | 2601.6 | 7 | 5000 | 50.35 % | 47.0 % |
| Volker | 2593.4 | 7 | 5000 | 48.57 % | 46.1 % |

The CLOP set itself (0.5.0-045) lost its SPRT against 043 at 5+0.01 on Linux: H0 accepted after 10027 games, bounds
-2/+3. At these time controls a stricter shield does not pay; the stricter the set, the lower it finished.
