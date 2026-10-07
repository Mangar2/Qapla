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
- Sets 1-3 are 1 M games each, sets 4-6 2 M each - 9 M games, 1.04 G positions in all (counted
  2026-10-05). Who played them: set 1 HCE-HCE (1/9 of the games), set 2 nnue1-nnue1, set 3 nnue2-nnue2,
  set 4 nnue1-HCE, set 5 nnue2-HCE, set 6 nnue1-nnue2. Positions with a value: 139 M, 124 M, 138 M,
  190 M, 252 M, 197 M. Each exists as `<set>.gam` (with
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
   **Pull the working copy of every machine before a labelling with options, and prove the options
   reach the engine.** A label-chunks.sh older than 239020f takes the range but drops every option
   after it without a word: on 2026-10-04 Linux and Windows (both at fe600b3) labelled set1-lowsel
   15-101, set 2 1-20 and set 3 1-9 with plain HCE; it showed only as half the chunk time, found three
   days later. The proof: label ~50 games of the chunk with and without the options and compare with
   the run's output - it must be identical to the one with options (`tmp/cmp2.py` style, matched by
   move sequence). The chunk time is a hint too: the wide lowsel options cost a factor ~2.2.
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
- **A change of EXTRA_DEFINES needs `make BUILD_TYPE=Release clean` first.** The defines are no make
  dependency: without the clean the old objects are linked and the binary is the previous shape or
  variant, silently (2026-10-05: an "A1" build on the Mac was 256 x 32, a "before" build on Windows
  was A1 and refused the 256 net, so its node check ran without a net). After every build check what
  it is: `printf 'stat\nnew\nnnueeval net <net>\nquit\n' | <binary>` - the `new` is needed, without
  it nnueeval evaluates the empty board.
- **A new net shape is checked across the SIMD paths**: the same net, the same positions at a fixed
  depth on the Mac (NEON) and on Linux or Windows (AVX2) must give identical node counts. The AVX2 dot
  product once stepped by 32 bytes and read past a 16-wide layer (fixed in 2527211); nnueeval at one
  position did not show it, a tournament did (-175 Elo).
- Net shapes: `-DQAPLA_NNUE_ACCUMULATOR=512 -DQAPLA_NNUE_L1=16` and `train.py --accumulator 512 --l1 16`
  (architecture test A1); the shape number of the file refuses a net of another shape.
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

- **Captures and checks are skipped by default** (`--skip-tactical`, learning 7: about +75 Elo).
  `--no-skip-tactical` only to reproduce an old result trained without it. The held back loss of a
  filtered run compares only with filtered runs.
- **Stockfish's procedure is the default** (learning 8, about +44 to +53 Elo over our old way): epochs of
  10 M positions out of an endless stream, RangerLite at lr 8.75e-4 falling by 0.992 an epoch, up to 800
  epochs, no stopping rule, a net every 100 epochs (`--save-every`), held back loss every 10
  (`--validate-every`). `--epoch-size 0` gives the old way (one pass an epoch, Adam, `--patience`).
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

## Training the Stockfish architecture on the Mac (nnue-pytorch, local branch qapla-sf17)

`~/dev/qapla/nnue-pytorch`, branch `qapla-sf17` (base e215624), venv `.venv` (Python 3.12 via uv).
Local changes, see `QAPLA-SF17.md` there:
- `ft_quantized_one = 127`: Stockfish 17 reads the accumulator with 1.0 = 127; with 255 the nets
  evaluate wrong in Stockfish 17 / Qapla's nnue-branch (wrong sign even).
- Metal kernels for the sparse feature transformer (`model/modules/feature_transformer/metal.py`):
  ~37,000 positions/s on the M4 GPU against ~18,000 on the CPU and ~4,900 with the stock MPS path.
- No pinned memory without CUDA: on MPS the prefetch thread's pinning deadlocks the training
  (sometimes after hours, sometimes at the first batch).
- Is it running? Check `ioreg -r -d 1 -c IOAccelerator | grep -o '"Device Utilization %"=[0-9]*'`
  (99 while training) right after the start - do not wait for the progress bar, it prints every 20 %.
- Data: `gamefile --gam2binpack` (chess-tools), the score written so that nnue-pytorch's target is our
  probability. Export: `serialize.py <ckpt> <net> --features "HalfKAv2_hm^" --l1 3072 --l2 15 --l3 32`.
- Play: nnue-branch built with `-DUSE_STOCKFISH_EVAL -DQAPLA_SF_BIG_NET_ONLY`; the net goes in as
  `nnue/big.nnue` under the engine's `dir=` (with the small net file beside it, unused).

## Measuring

- **Epoch tournament** - every epoch against HCE, 1500 games, only the best epoch so far, one at a
  time: `sh src/pipeline/epoch-tournament.sh <run> <nets dir> <training log> <binary> <host>`
  (host from local.toml: `qapla` or `ryzen9`). One state file per epoch:
  `test/log/epochs-<run>-eNN.state`. Stop it with `pkill -f "epoch-tournamen[t].sh"`.
- **Reference tournament** on Linux: `test/tournament/strength-reference.ini`, 10000 games per
  pairing, reference `nnue-set1234` (gauntlet). A new engine joins by an `[engine]` section in the
  state file before `[tournament]` (see `src/pipeline/join-gauntlet.py`), then qet is restarted with
  `--tournament file=<state>` alone. Paused tournaments resume with the identical call.
- **Building a temporary branch on another machine**: `git checkout -B <branch> origin/<branch>`
  after the fetch. A plain checkout of a branch that exists there already takes the old local tip -
  2026-10-04 a Windows build silently came out of the previous commit.
- **A third engine on the command line of a finished two-engine tournament** (same state file) plays
  the missing pairing against the gauntlet engine first, but afterwards also the pairing of the two
  non-gauntlet engines, `gauntlet=true` notwithstanding (2026-10-04, e10 started against e1). Watch for
  the end of the wanted pairing and stop qet then, or give the newcomer a state file of its own.
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
