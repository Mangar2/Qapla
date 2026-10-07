# Trainer changelog

Every change to the trainer (`src/trainer`: train.py, gamedata.py, model.py, netfile.py, export.py,
ranger_lite.py, native/) gets an entry here and a new version in `version.py`, in the same commit.

## Versions: major.minor.patch

- **patch** - the trainer writes **bit-identical nets** under every setting. Proven by
  `test_reproducible.py` against `reproducible.json`: it must report `identical`. Refactorings,
  speed, logging, new checks that only stop a run, renamed files.
- **minor** - the nets change under at least one setting: a new option (even off by default), a fix,
  another default, another order of the data. After the change `test_reproducible.py --record`
  writes the new record, and the entry says which runs of the test changed.
- **major** - the procedure changes in principle, or the net file format changes.

Every file a training writes carries the version in its name: `net-v1.0.0-epoch09.nnue`,
`state-v1.0.0-epoch100.pt`; `last.pt` holds it inside. `--resume` continues only a run of the same
major.minor.

The test runs on the CPU, where two trainings with the same seed give the same bits. On the GPU (mps)
they do not, so a patch is proven on the CPU and a GPU training has a spread of its own.

## 1.0.0 - 2026-10-07

First versioned state. The nets are bit-identical to those of the unversioned trainer as of ad3792b
(the last trainer commit before versioning): `test_reproducible.py` recorded the six runs with that
code and then confirmed them with this one.

- The version is printed at the start and goes into every file name; `--resume` refuses a checkpoint
  of another major.minor, or one from before 1.0.0. `test/nnue/nets-a1/last.pt` (A1, started
  2026-10-06 with ad3792b) was stamped 1.0.0 by hand; the unstamped original is `last.pt.before-1.0.0`.
- `test_reproducible.py` and its record `reproducible.json`.

## Before 1.0.0 (unversioned)

Nets trained before 2026-10-07 carry no version. The trainer that made them is known only from the
date and the commit then current; a net is ascribed to the last trainer commit before its training
started, with no proof that the working copy was clean. The commits that changed nets:

| commit | date | change |
|---|---|---|
| 9a92c6b | 09-26 | the loader and the trainer of the net |
| 9b9e406, 9db9560 | 09-26 | game result always weighted; the value is a win probability |
| 6172221, 7f17f39 | 09-26/27 | several game files; a validation set and the stopping rule |
| 9c967d8 | 09-28 | reads the packed game file directly |
| 171fc0a | 09-28 | (pgn export only) - the trainer of the set-1 net e08 (`nets-set1-nowdl`) |
| b6588b6 | 09-29 | several game files as one corpus |
| b396152 | 09-30 | --resume |
| 52dc531 | 10-01 | compiled loader, "verified bit for bit against the python one" |
| ca34193 | 10-01 | --stacks |
| a3f6128 | 10-02 | --neighbours |
| 794a3d0 | 10-04 | --skip-tactical |
| 2eb17be | 10-04 | fixed epochs, lr decay, RangerLite (--epoch-size) |
| (f6ee940) | 10-04 | current when set1-lowsel was trained |
| 70ba1e9 | 10-05 | --skip-tactical on by default |
| 5762614, 61ec5c1 | 10-05 | Stockfish's procedure the default; a net every 100 epochs |
| 13fa48f | 10-05 | --skip-early |
| ad3792b | 10-05 | --accumulator, --l1 |
