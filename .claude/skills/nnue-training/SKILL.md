---
name: nnue-training
description: Everything about building and measuring Qapla's own NNUE - generating and labelling game sets, converting them, training nets on the Mac, building the engine binaries that play them, running epoch and reference tournaments on the Linux and Windows machines, and reporting. Use whenever the work touches nnue data, a training run, a net, a labelling run or a tournament between nets.
---

# NNUE training for Qapla

The goal: a net of our own that is clearly stronger than the hand written evaluation (HCE). The
target bar is Stockfish's net in Qapla, roughly 500-600 Elo above HCE. Stockfish details (PSQT
accumulator, a second small net) only come after our net has gained about 500 Elo - do not propose
them earlier.

Background documents in the repository - read the one that matters before acting:

| what | where |
|---|---|
| the data sets, their recipes and sizes | `test/nnue/dataset/DATASET.md` |
| how the data was generated, loader, GPU instance | `src/nnue-data/generated-data.md` |
| trainer overview | `src/trainer/README.md` |
| the pipeline steps (play, label, convert, train) | `src/pipeline/pipeline.toml` |
| machine config (hosts, paths, AWS) - **not in git** | `src/pipeline/local.toml`, shape in `local.example.toml` |

## Machines and what each is for

| machine | reach | repo | used for |
|---|---|---|---|
| Mac (M4, local) | - | `/Users/volkerbohm/dev/qapla/qapla` | training (MPS GPU), small tournaments/SPRTs at concurrency 9 |
| Linux `qapla` | `ssh mangar@qapla` | `~/dev/qapla` | tournaments (14 pairs), labelling (15 engines). 16 real cores |
| Windows `Ryzen9` | `ssh mangar@Ryzen9` | `C:\development\Qapla2` | tournaments, labelling. 16 cores (5950X, Zen 3, AVX2, no AVX-512) |
| AWS | `aws login --remote` | - | spot instances; currently nothing runs there, data removed |

- Code reaches the other machines **only through GitHub** (`git pull` there). Generated data -
  nets, game files, pgns, chunks - is copied directly with `scp`.
- Nothing machine specific goes into git: `local.toml` is ignored, scripts stay neutral.
- Linux: never put extra load on it while a tournament runs there. Kill qet with `pkill -x qet`,
  never with a pattern - a pattern matches the ssh command line itself.
- Windows over ssh: the default shell is cmd. For anything non-trivial pipe a script into the msys
  bash: `printf '...' | ssh mangar@Ryzen9 "C:/msys64/usr/bin/bash.exe -s"` (put
  `export PATH=/usr/bin:$PATH` first). A process started by an ssh session on Windows dies with the
  session: start long runs through WMI with a `.cmd` file written locally and copied over:
  `powershell -NoProfile -Command "Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{CommandLine='cmd /c C:\development\Qapla2\test\log\<name>.cmd'; CurrentDirectory='C:\development\Qapla2'}"`.
  Never build escaped cmd/powershell strings inline - write the file locally, `scp` it.
- Tools: Mac/Linux `~/bin/qet`, Windows `C:\development\bin\qet.exe`. The qet source is
  `~/dev/qapla/engine-tester` (branch 0.7.0); on Windows `C:\development\qapla-engine-tester`,
  `cmake --preset release && cmake --build --preset release`, then copy to `C:\development\bin`.

## Where things go

Run outputs - tournament and SPRT state, pgns, run logs, training checkpoints, speed tests, chunk
copies - go to `tmp/` (see CLAUDE.md), which may be emptied at any time. What is worth keeping is
moved out explicitly: nets to `test/nnue/`, binaries to `new-versions/`. Check free disk space before
anything that writes gigabytes: a Stockfish-architecture checkpoint is 1.6 GB per epoch.
Old checkpoints of Stockfish-architecture training go to the external SSD of the Mac,
`/Volumes/T7/qapla/<run>/` (copy, compare, then remove locally); the `.nnue` nets stay in the repo.

## The data path

```
play (qet, depth 6)  ->  template pgn  ->  label (qet analysis, depth 8, direction=reverse)
  ->  labelled pgn (lan, values in comments)  ->  convert  ->  .gam (QAPLAGM2)  ->  train
```

- **.gam format**: 3 bytes per ply - 11 bit move, 2 bit result, 11 bit value code (win
  probability, sigmoid(v/400)). Defined in `src/trainer/format.py`.
- Sets 1-6 are a million games each (set 1 = HCE self play). Each exists as `<set>.gam` (with
  results) and `<set>-nowdl.gam` (result RESULT_NONE). **nowdl trains better** - use it.
- The template pgn of a set is not kept; it is derivable from the .gam.
- **Converters** - use the C++ tool, byte-identical to the Python scripts and ~60x faster:
  `~/dev/qapla/chess-tools/build/release/gamefile --pgn2gam in=<pgn> out=<gam> wdl=none`
  and `--gam2pgn in=<gam> out=<pgn>`. (Python originals: `src/trainer/convert.py`,
  `src/trainer/export-pgn.py`.) Build: `cmake --preset release && cmake --build --preset release`
  in `~/dev/qapla/chess-tools`.
- A subset of a .gam by game index: `format.read_games` / `format.write_games` in a few lines of
  Python, run from `src/trainer`.

### Labelling (relabelling a set with another engine)

1. `gamefile --gam2pgn` the set's .gam (with results!) to a template pgn.
2. Split into chunks of 10000 games with `split_into_chunks` from `src/pipeline/pipeline.py` -
   chunk k holds games (k-1)*10000 .. k*10000-1 of the set.
3. Copy the chunks to each machine and run
   `sh src/pipeline/label-chunks.sh <chunks> <out> <qet> <engine> <engine dir> 8 <concurrency> [first] [last] [Option=value ...]`.
   It writes a `.done` marker per finished chunk and skips marked chunks, so it can be stopped and
   restarted. `[first] [last]` splits a set between machines. Everything after the range becomes a
   UCI option of the engine. Use a separate chunk directory per labelling engine (hard links), so
   the markers do not mix.
4. qet analysis may write games in a different order; the game set is the same. Check with a
   multiset comparison of the move sequences before comparing two labellings.

Throughput at depth 8 (one chunk = 10000 games): Stockfish eval ~43 min on Linux (15 engines),
~46 min on Windows (15); low-selectivity HCE ~37 min on the Mac (9).

## Engine binaries

The Makefile release build is `make Release -j`. Variants needed here:

| binary | build |
|---|---|
| nnue engine (plays a net) | `make Release -j EXTRA_DEFINES="-DQAPLA_USE_NNUE"` plus `NATIVE=1` on x86 |
| HCE opponent | the same source **without** `QAPLA_USE_NNUE` (or the nnue build without a net) |
| Stockfish eval | branch `nnue-branch`, `EXTRA_DEFINES="-DUSE_STOCKFISH_EVAL"`; nets `nn-1111cefa1111.nnue` and `nn-37f18f62d772.nnue` are read from `nnue/` under the working directory - run it with qet `dir=<repo>/test/nnue/stockfish-eval` |

- **Without `QAPLA_USE_NNUE` the engine silently plays HCE whatever `NnueFile` says.**
  `nnueeval` still loads the net, so it is no proof. The proof is a short search with and without
  the net: the node counts must differ. `epoch-tournament.sh` checks this before every epoch.
- `NATIVE=1` on Linux: without it the nnue takes the 128 bit path and loses ~22 % speed.
  Windows: `-arch:AVX2` is in the Makefile already; `NATIVE=1` works there too.
- Stockfish eval on Windows needs `USE_SSE2 USE_SSSE3 USE_SSE41` besides `USE_AVX2` (fixed in the
  nnue-branch Makefile) - without them the values are deterministic but wrong. Check any new
  platform build against Linux or the Mac: identical node counts on a few positions.
- `nnue-branch` converts Stockfish's value with its own `to_cp` before the search sees it.
- Build one version at a time in the one working copy; copy binaries to `new-versions/` under a
  telling name. Never commit search flags switched on for an experiment.
- The Makefile compiles **every `.cpp` under the repo** - never leave scratch `.cpp` files in
  `test/log`.

## Training

Run on the Mac, from `src/trainer`, with the venv: `src/trainer/.venv/bin/python` (system python
has no torch). Create the output directory first.

```
.venv/bin/python -u train.py <gam> [<gam> ...] --out <dir> --blend-start 0.8 --blend-end 0.7 \
    --epochs 20 --patience 2 --workers 6 --seed 1 --validation-every 100 --stacks 1
```

- `--stacks 8` = 8 layer stacks (heads by piece count, one shared accumulator). Needs a lot of
  data - for small corpora (a single set or less) train **one head** (`--stacks 1`).
- `--neighbours` trains head i on buckets i-1, i, i+1 as well; it only reaches the heads (~1 % of
  the weights), the accumulator sees the same data - expect little.
- `--resume` continues from the newest checkpoint; `--loader native` (default) is the compiled
  loader `src/trainer/native/batcher.cpp`.
- Nets are written per epoch as `net-epochNN.nnue`; the log's `best epoch` line names the one with
  the lowest held back loss. Single-head nets are written as QAPLANN1, stacked as QAPLANN2.
- The pipeline wraps this: `python3 src/pipeline/pipeline.py run --only <step>`; steps carry
  `done = true` when they must not run again.
- **Held back loss compares runs only with the same labels.** Nets trained on different labels
  (HCE vs Stockfish) have different targets - only games decide.
- Strength follows positions processed, not distinct positions; without heads the plateau was
  ~4-6 G positions at ~62-63 % against HCE.

## Measuring

- **Epoch tournament** - every epoch against HCE, 1500 games, only the best epoch so far, one at a
  time: `sh src/pipeline/epoch-tournament.sh <run> <nets dir> <training log> <binary> <host>`
  (host from local.toml: `qapla` or `ryzen9`). One state file per epoch:
  `test/log/epochs-<run>-eNN.state`. Stop it with `pkill -f "epoch-tournamen[t].sh"`.
- **Reference tournament** on Linux: `test/tournament/strength-reference.ini`, 10000 games per
  pairing, reference `nnue-set1234` (gauntlet). A new engine joins by an `[engine]` section in the
  state file before `[tournament]` (see `src/pipeline/join-gauntlet.py`), then qet is restarted with
  `--tournament file=<state>` alone. Paused tournaments resume with the identical call.
- **SPRT** for "is A better than B": `test/sprt/sprt-standard.ini`, `--each tc=10+0.05 option.Hash=64`
  **before** the engine blocks, challenger `gauntlet=true`, own state file. Exit 14 = H1, 15 = H0,
  16 = undecided.
- **Depth-8 comparison** of search settings: `test/tournament/depth8-selectivity.ini`.
- `python3 src/pipeline/status.py` shows the Mac training, the Linux tournament with Elo per engine,
  and AWS. It reads only Linux; Windows has to be asked directly.

### How to state results - non-negotiable

- Every strength statement carries its uncertainty as a number: 1500 games ~ +/-15 Elo per value,
  the difference of two such ~ +/-21; 5000 ~ +/-11; 10000 ~ +/-8. No "plateau", "better",
  "equal" unless the gap is clearly outside it. A consistent series of measurements counts for more
  than any single one. The "best" of several noisy values is biased upward.
- No hedging phrases ("this is no proof") - the number and a plain classification are enough.
- An SPRT yields H0, H1 or undecided - never quote its win rate or an Elo figure from it.

## Results so far (2026-10-03) - extend this table

| what | result |
|---|---|
| set1234 (1 head) vs HCE | 63.18 % (5000 games) |
| set123456 with 8 heads, best e14, vs HCE | ~ +118 Elo (+/-15, 1500 games) |
| s8-e14 vs set1234 | +17 Elo (+/-7, 10000 games) |
| HCE vs set1234 | about -91 Elo (+/-11, 3900 games, paused) |
| low-selectivity HCE (futility off, lmrDivisor 476, ~2.9x nodes) vs default, depth 8 | +112 Elo (+/-14, 3000 games) |
| nets from 26 chunks of set 1 (260k games, 1 head, 5 epochs), Stockfish labels vs HCE labels | SPRT H1 accepted after 544 games, bounds 0/+15 |
| the same two nets, second SPRT | H1 accepted after 2814 games, bounds 40/50 |

Open work: Stockfish relabelling of set 1 (Windows chunks 1-50, Linux 51-101), low-selectivity
HCE relabelling of set 1 on the Mac (paused at 14 of 101, `test/log/label-set1-lowsel.log`),
HCE-vs-set1234 reference pairing paused on Linux at round 44.

## Reporting while running unattended

Volker often follows only through the app and sees only the chat. Report every 30 minutes in the
chat: a table machine | task | state, the relevant tournament figures with uncertainty, and only
what changed or needs attention. Use a background `sleep 1800` as the timer - cron schedules do not
fire in the VS Code extension.
