---
name: nnue-trainer
description: How to work with Qapla's NNUE trainer in src/trainer - running a training, changing the trainer, versioning it (major.minor.patch, CHANGELOG.md), proving a change with the reproducibility test, naming of trained files, and comparing two nets so that they differ in one factor only. Use before touching any file in src/trainer, before starting a training, and before comparing two nets.
---

# The NNUE trainer

The trainer is `src/trainer` (train.py, gamedata.py, model.py, netfile.py, export.py, ranger_lite.py,
native/batcher.cpp). It runs on the Mac, from `src/trainer`, with `.venv/bin/python`. What the data
is, how it is labelled and how nets are measured in games is in the skill `nnue-training`.

## The trainer is versioned - from 2026-10-07 on, no exceptions

`src/trainer/version.py` holds `VERSION = 'major.minor.patch'`, `src/trainer/CHANGELOG.md` the history
and the rules. **Every change to a file of the trainer raises the version and gets a CHANGELOG entry
in the same commit.**

| level | when | proof |
|---|---|---|
| patch | the nets are bit-identical under every setting | `test_reproducible.py` reports `identical` |
| minor | the nets change under at least one setting - a new option even if off by default, a fix, another default, another data order | the test reports which runs differ; then `test_reproducible.py --record`, and the entry names the changed runs |
| major | the procedure changes in principle, or the net file format | as minor |

A change that "should not change anything" is a patch only once the test says `identical` - the same
rule as the node count for the engine. If it says `different`, it is a minor, whatever was intended,
and the cause is understood before it is committed.

### The reproducibility test

```
cd src/trainer && .venv/bin/python test_reproducible.py            # ~2.5 min, CPU
cd src/trainer && .venv/bin/python test_reproducible.py --record   # minor/major only
```

It trains the first 2000 games of set 1 on the CPU in six runs that together reach every option that
changes a net (both procedures, 1 and 8 heads, both loaders, filters, sizes, worker counts, non-default
numbers, a resumed run) and compares the md5 of every net with `reproducible.json`.

- It runs on the **CPU** because only there are two runs with one seed bit-identical (measured
  2026-10-07, workers included). On the GPU (mps) they differ even without workers.
- A **new option that changes nets** gets a place in one of the runs (or a run of its own) in the same
  commit, otherwise the test no longer covers the trainer.
- **Keep the options few.** Every option multiplies what the test has to cover. An option that is no
  longer used is removed (a minor) rather than kept "just in case".
- The number of workers changes the order of the data; it belongs to the command like any option.

### Comparing two states of the trainer

```
cd src/trainer && .venv/bin/python compare_trainers.py <commit A> <commit B> -- <train.py options>
```

Takes both states out of git (never the working copy), trains the fixture with the same command on the
CPU and reports `identical` or `different from epoch n on`; appends the result to
`trainer-comparisons.md`. It refuses an option one of the states does not know. Use it to check
whether two nets came from trainers that work alike under the command used, and to find the commit
where a behaviour changed. Trainers before 0.9.0 (52dc531) are not reproducible at all - their initial
weights do not come from the seed - so they compare as `different` even with themselves.
Run it with `nice -n 19` while a training runs; a forgotten `--epoch-size 0` on a version from 2.0.0
on trains epochs of 10 M positions on the CPU.

### File names carry the version

Every file a training writes: `net-v2.2.1-epoch09.nnue`, `net-v2.2.1-epoch09.pt`,
`state-v2.2.1-epoch100.pt`; `last.pt` holds it as `version`. Keep the version in the name when a net
is copied elsewhere (`test/nnue/nnue-set1-hce-v2.2.1-e09.nnue`), so that every net in a tournament can
be traced to its trainer. `--resume` refuses a checkpoint of another major.minor.

Nets from before 2026-10-07 have no version; CHANGELOG.md lists which commit was current when.

## Running a training

```
.venv/bin/python -u train.py <gam> [<gam> ...] --out <dir> --workers 6 --seed 1 --validation-every 100
```

Defaults (2.2.1): Stockfish's procedure (`--epoch-size 10000000`, RangerLite, lr 8.75e-4, gamma 0.992,
800 epochs, a net every 100), captures and checks skipped, `--accumulator 256 --l1 32`, one head.
The old procedure (one pass an epoch, Adam 1e-3, stop on the held back loss) is `--epoch-size 0`.
Write the whole command into the run's script or log; the defaults are part of the version, the
command is not.

## Comparing two nets - one factor only

**Test with today's standard, never with captures and checks** (Volker 2026-10-10): from 3.1.0 the trainer
always skips them (the option is gone); an old run with them can only be repeated with the trainer of its time.
The old `--epoch-size 0` procedure only to repeat an old run 1:1.

Before a head to head, write down every difference between the two sides: trainer version, every
option, the data, the labels, the engine binary. **Exactly one entry is allowed.**

- A baseline trained with an older trainer or another command is not a valid opponent. Retrain it
  with the current version and the identical command first.
- Prove that the factor is in effect: labels compared with and without the option, the option visible
  on the command line, a changed chunk time or node count.
- The GPU spread: two GPU trainings of the same command give different nets. How large that is in
  games must be measured (two runs of one command, head to head) before a small difference between
  two nets is read as an effect.

Why: on 2026-10-07 the "+29 Elo for lowsel labels" turned out to compare a net with only 14 % lowsel
labels (options dropped by an old label script) and a trainer 818 lines newer against the old e08 -
two factors, neither checked. A week of work rested on it. Forensics in
`test/nnue/forensics-set1-lowsel/`.
