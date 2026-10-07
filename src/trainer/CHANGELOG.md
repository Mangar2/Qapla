# Trainer changelog

Every change to the trainer (`src/trainer`: train.py, gamedata.py, model.py, netfile.py, export.py,
ranger_lite.py, native/) gets an entry here and a new version in `version.py`, in the same commit.

## Versions: major.minor.patch

- **patch** - the trainer writes **bit-identical nets** under every setting. Proven by
  `test_reproducible.py` against `reproducible.json`: it must report `identical`. Refactorings,
  speed, logging, new checks that only stop a run, renamed files, fewer nets written.
- **minor** - the nets change under at least one setting: a new option (even off by default), a fix,
  another default, another order of the data. After the change `test_reproducible.py --record`
  writes the new record, and the entry says which runs of the test changed.
- **major** - the procedure changes in principle, or the net file format changes.

Every file a training writes carries the version in its name: `net-v2.2.1-epoch09.nnue`,
`state-v2.2.1-epoch100.pt`; `last.pt` holds it inside. `--resume` continues only a run of the same
major.minor.

The test runs on the CPU, where two trainings with the same seed give the same bits. On the GPU (mps)
they do not, so a patch is proven on the CPU and a GPU training has a spread of its own.

## The history, versioned afterwards (2026-10-07)

The versions up to 2.2.0 were assigned on 2026-10-07 from `compare_trainers.py`: every trainer commit
trained the fixture on the CPU against its predecessor, with the commands both understand; the results
are in `trainer-comparisons.md`. "identical"/"different" below quote that file.

**Before 0.9.0 nothing is reproducible.** Those trainers did not draw the initial weights from the
seed: a trainer compared with itself gives different nets (171fc0a, b396152 measured), so no two of
them can be compared either. Their versions follow the rules from the commit, not from a measurement.

| version | commit | date | change | evidence |
|---|---|---|---|---|
| 0.1.0 | 9a92c6b | 09-26 | the loader and the trainer | first |
| 0.1.1 | 2891065 | 09-26 | the loss of a batch read without the graph | meant to change nothing; not measurable |
| 0.2.0 | 9b9e406 | 09-26 | the game result always has a weight | changes the loss |
| 0.3.0 | 9db9560 | 09-26 | the value is a win probability | changes the target |
| 0.4.0 | 6172221 | 09-26 | the cache takes several game files | new option |
| 0.5.0 | 7f17f39 | 09-27 | a validation set and the stopping rule | new behaviour |
| 0.6.0 | 9c967d8 | 09-28 | reads the packed game file directly | new loader |
| 0.6.1 | 171fc0a | 09-28 | writes games back out as pgn (export-pgn.py only) | training code untouched |
| 0.7.0 | b6588b6 | 09-29 | several game files as one corpus | new behaviour |
| 0.8.0 | b396152 | 09-30 | --resume | new option |
| 0.9.0 | 52dc531 | 10-01 | compiled loader; initial weights from the seed | different from 0.8.0; **first reproducible** (identical to itself) |
| 1.0.0 | ca34193 | 10-01 | --stacks, net format QAPLANN2 | major: format; one head identical to 0.9.0 |
| 1.1.0 | a3f6128 | 10-02 | --neighbours | new option; identical to 1.0.0 with one and with eight heads |
| 1.1.1 | bded1e1 | 10-02 | pgn exporter: promotions | training code untouched |
| 1.2.0 | 794a3d0 | 10-04 | --skip-tactical | new option; identical to 1.1.0 without it |
| 1.3.0 | 2eb17be | 10-04 | --epoch-size: Stockfish's procedure as an option | new option; identical to 1.2.0 without it (also with 8 heads and skip) |
| 1.4.0 | 70ba1e9 | 10-05 | skipping captures and checks by default | different by default; identical with --skip-tactical |
| 1.4.1 | 5762614 | 10-05 | a net every 100 epochs by default | fewer nets written; the nets identical |
| 2.0.0 | 61ec5c1 | 10-05 | Stockfish's procedure the default | major: procedure; identical with --epoch-size 0, different with --epoch-size (RangerLite default) |
| 2.1.0 | 13fa48f | 10-05 | --skip-early | new option; identical without it |
| 2.2.0 | ad3792b | 10-05 | --accumulator, --l1 | new option; identical at the default sizes, same file format there |
| 2.2.1 | 6064f86 | 10-07 | versioned file names, version printed, --resume checks it; test_reproducible.py | identical to 2.2.0 (test, and compare_trainers) |

`1039c45` (09-28, convert.py) changes how a pgn becomes a game file - the data, not the trainer.

## Which nets came from which version

A net is ascribed to the last trainer commit before its training started; whether the working copy was
clean is not recorded for any of them.

| net | trained | version | learnings resting on it |
|---|---|---|---|
| nets-set1-nowdl e08 | 09-28 | 0.6.1 | 2, 4, 5, 6 (baseline) |
| set1234 best (e11) | 09-30 | 0.7.0 | 3 (baseline), reference field |
| set123456-s8 e14 | 10-01 | 1.0.0 | 3, 7 (baseline) |
| set1-sf | 10-04 | 1.1.1 | 4 |
| set1-lowsel e09 | 10-04 | 1.1.1 | 6 |
| set123456-s8-skip e09 | 10-04 | 1.2.0 | 7, 8 (baseline) |
| s8-sflike e300-e800 | 10-05 | 1.3.0 | 8 |
| A1, set1-nolmr, set1-hce-retrain | 10-06/07 | 2.2.0 | - |

What the comparisons say about the learnings: 7 (s8 1.0.0 against s8-skip 1.2.0, s8 command) and 8
(s8-skip 1.2.0 against sflike 1.3.0, s8-skip command) - the trainers are identical under the baseline's
command, so each compared exactly one factor. 3, 4, 5, 6 compared nets from a 0.x trainer with nets
from a 1.x trainer - two factors, and the 0.x side not even reproducible.

## 2.2.1 - 2026-10-07

First version kept by these rules (named 1.0.0 for a few hours on 2026-10-07 before the history was
numbered; the files written then carry `v1.0.0` and are the same trainer).

- The version is printed at the start and goes into every file name; `--resume` refuses a checkpoint
  of another major.minor, or one from before versioning. `test/nnue/nets-a1/last.pt` (A1, started
  2026-10-06 with 2.2.0) was stamped by hand; the unstamped original is `last.pt.before-1.0.0`.
- `test_reproducible.py` and its record `reproducible.json`; `compare_trainers.py` and its record
  `trainer-comparisons.md`.
