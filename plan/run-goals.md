# Runs and their goals

Every run that is going, paused or planned, with the goal it serves. The goal decides the follow-up, see
`.claude/skills/run-supervision/SKILL.md`. Updated when a run starts, ends or changes its goal.

Overall goal: Qapla's own net, at the level of the Stockfish net in Qapla - about 500-600 Elo over the
hand written evaluation.

## The programme: the best 8-bucket net out of HCE alone (Volker, 2026-10-04)

How do we get the best result for our 8-bucket net starting from the hand written evaluation? The steps
below build on each other; the single runs further down are parts of them.

**Principle: no external evaluation.** Until further notice no evaluation of another engine goes into
the training data of our net, although it would pay (Stockfish labels: about +150 Elo on set 1). We pull
ourselves out of the swamp by our own bootstraps. The Stockfish-label runs are for learning only.

Label rounds: round-0 labels come from the hand written evaluation; round-1 labels from the search with
the net trained on round-0 labels; and so on.

1. **Wider search for the labels** - in progress: set 1 relabelled by the low-selectivity HCE (Linux
   51-101, Windows 15-50, Mac 1-14 done), then trained like the old set-1 net and played against it.
   Trained 2026-10-04 17:38-19:07: best epoch 9 (held back 0.001941), `test/nnue/nnue-set1-lowsel.nnue`;
   Head to head against the old set-1 net (e08), 3000 games on Linux, done 19:48: set1-lowsel 54.17 %
   (+/-10 each), about +29 Elo. The wider search pays - step 4 goes ahead (sets 2 and 3 already being
   labelled).
2. **No captures, no check positions** in the training - the filter run below, against s8-e14 in the
   reference tournament. Loader filter `--skip-tactical` done 2026-10-04 (794a3d0, checked against
   python-chess, 24 % of the positions skipped); the run started on the Mac 2026-10-04 19:59 (`tmp/mac-s8-skip-train.sh`, nets to `test/nnue/nets-set1-to-6-s8-skip`).
3. **Stockfish-like training procedure**, on top of 2 (2 is kept even if it only holds level):
   - fixed epoch size of 10 M positions - Stockfish takes 100 M for a net about ten times as large
   - up to 800 epochs (8 G positions), measured every 100 epochs whether the net still improves
   - learning rate lowered every epoch as in Stockfish (gamma per epoch)
   - needs a fixed epoch size and a per-epoch lr schedule in `src/trainer/train.py`
4. **If the wider search promises an advantage (1), label all sets wider** and train them with the
   procedure found best in 2 and 3; otherwise train the current labels with it.
5. **Round-1 labels**: relabel all sets with the net from 4 and train again. If that pays, further
   rounds - it will saturate.

Depth-8 tournament on Windows, 2026-10-04 (Volker): lowsel without LMR (and with it without move count
pruning, which reads the same value) against lowsel, 3000 games - nolmr 64.98 % (+/-10 each), about
+107 Elo; nodes on three test positions 1.5x-3x of lowsel. Binary `Qapla-hce-lowsel-nolmr-win.exe` from
`tmp-lowsel-label` (2db3278), options lmrDivisor=100000 lmrPvDivisor=100000.

**Set 1 labelled with nolmr** since 2026-10-04 20:24 (Volker): Windows 1-50, Linux 51-101, concurrency 30,
`test/nnue/chunks-set1-nolmr`, binaries `Qapla-hce-lowsel-nolmr-{win.exe,linux}` (node counts identical).
About 57 min per chunk at concurrency 30 (lowsel: ~9), so ~48 hours for the full set - Volker:
the full set, no subset. Then trained like the old set-1 net and played against set1-lowsel. If clearly better, the lowsel
labels of sets 2 and 3 below are not needed.

Stopped 2026-10-04 20:20 for it (labels kept): Started on speculation (no idle machine): Windows labels set 2 with the low-selectivity HCE from
2026-10-04 ~17:15 (`test/nnue/chunks-set2-lowsel`, 101 chunks); thrown away if step 1 shows no advantage. Linux labels set 3 likewise since
2026-10-04 18:20 (`test/nnue/chunks-set3-lowsel`), paused 19:10-~20:10 for the head to head of step 1.

Planning stops there; by then we will have learned enough to plan the next part.

Filler for the Mac gpu whenever no training task is due: the Stockfish architecture on to epoch 100
(last entry of this file).

## Going

### Mac: Stockfish architecture (SF17) trained on sets 1-6, HCE labels, to epoch 10
Goal: find out whether it is our training data that must improve. If SF's architecture and procedure
on our data stay far below the Stockfish net, the data is the lever; if it comes close, the training is.
Done 2026-10-04 12:35: e10 scored 49.08 % against HCE (+/-12, 2000 games), e1 32.64 % (+/-13). Not
answered yet - the net is far from a comparable saturation, see the run to epoch 100 below.

### Set 1 labelled with Qapla's search and Stockfish's evaluation - labels done 2026-10-04 11:51
Goal: the Stockfish-eval test for set 1 - does a better label of the same positions make the net
clearly stronger, and by how much? (Order of the levers, 2026-10-01: capacity, better labels, new games.)

Pre-test: two nets from the same 26 chunks of set 1 (260k games, 1 head, 5 epochs), Stockfish labels
against HCE labels - SPRT H1 accepted after 544 games at bounds 0/+15, and H1 accepted after 2814 games
at bounds 40/50. Why the full set anyway: the pre-test had a quarter of the data and short training,
and the advantage of the set origin (learning 2) was clear early and could no longer be measured from
three to four sets on. Whether the label advantage stays, grows or vanishes in a fully trained net is
open; only a tournament gives its size with an uncertainty, i.e. how much of the ~600 Elo gap better
labels can close. Afterwards, idea 3 (low-selectivity HCE labels, paused below).

Procedure, exactly like the old net `nets-set1-nowdl` (2026-09-28), only the labels differ:
- data: chunks 1-101 joined in order, `gamefile --pgn2gam wdl=none` -> .gam
- `src/trainer/train.py`, `--stacks 1`, L1 256, lr 1e-3, batch 16384, seed 1, validation-every 100,
  max 20 epochs, patience 3; the net is the epoch with the lowest held back loss (old: e08)
- the held back loss does not compare the two (different labels); only games:
  new against old head to head at 10+0.05, book8ply.raw, until the error is below +/-10 Elo
  (~3000 games), and each against HCE in the same setting for the link to older figures
- one unavoidable difference: the trainer has had the compiled loader since 2026-09-28
- trained 2026-10-04 12:40-13:48: best epoch 6 (held back 0.001448), net `test/nnue/nnue-set1-sf.nnue`;
  tournament `tmp/set1-sf-vs-hce-labels.state` since 13:50
- on the Mac, after the e10 tournament (Volker, 2026-10-04). The tournament is one round robin of
  set1-sf, set1-hce (e08) and HCE (the nnue binary without a net), 3000 games per pairing, concurrency 9

## Paused

### Linux: tournament HCE against the set1234 net - done 2026-10-04 18:13
HCE 37.15 % against nnue-set1234 over 10000 games (+/-6). Reference field complete.
Goal: a reference figure of our best single-head net against HCE.

### Mac: set 1 relabelled by a low-selectivity HCE (factor 3 nodes) - 14 of 101 chunks
Linux labelled chunks 51-101 with it, 2026-10-04 09:14-15:36 (`Qapla-hce-lowsel-wide-linux`, node
counts identical to the Mac, concurrency 30). Windows labels chunks 15-50 since 2026-10-04 11:55
(`Qapla-hce-lowsel-wide-win.exe`, node counts identical, concurrency 30).

Situation: labelling of set 1 started on the Mac on 2026-10-02 22:33 and was interrupted at 14 of 101
chunks so that the Mac could train the Stockfish architecture with Stockfish's training (above). Not
cancelled: it is a task to resume by itself whenever the Mac has nothing else to do.

Goal: idea 3 - do labels from a less selective HCE search alone make the net stronger? At depth 8 the
low-selectivity setting (futility off, lmrDivisor 476, ~2.9x nodes) beat the default by +112 Elo
(+/-14, 3000 games). Volker planned it as the next try after the Stockfish-eval test of set 1. Once
labelled: train set 1 exactly like the old net `nets-set1-nowdl` (procedure as in the Stockfish-label
test above) and play it against that old net.

Resume with the identical call (chunks with a .done marker are skipped, the interrupted chunk is redone):

    R=$PWD; ( nohup sh src/pipeline/label-chunks.sh test/nnue/chunks-set1-lowsel test/nnue/labelled-set1-lowsel \
        $HOME/bin/qet $R/new-versions/Qapla-hce-lowsel-wide-mac $R 8 9 1 101 \
        ffDepthFactor=5000 futDepthFactor=5000 lmrDivisor=476 >> test/log/label-set1-lowsel.log 2>&1 < /dev/null & )

Runs on every machine. The widened option limits are on the temporary branch `tmp-lowsel-label`
(GitHub; never merge, delete it when the labelling is done). To build there: `git fetch`, check out
`tmp-lowsel-label`, `make Release -j` (plus `NATIVE=1` on x86), copy `build/Release/Qapla` to
`new-versions/Qapla-hce-lowsel-wide-<os>`, check out `nnue` again. Before labelling, the same few
positions at depth 8 with the three options set must give node counts identical to the Mac binary.
The chunks are in `test/nnue/chunks-set1-lowsel` on the Mac (generated data, copied with scp); split
the remaining range between machines with `[first] [last]` as for the Stockfish labels.

~37 min per chunk at concurrency 9 on the Mac. Not beside a training on the Mac: the training's data
loader needs the cpu.

## Planned

### Best HCE training again, without captures and without check positions (Volker, 2026-10-04)
Goal: can our best training so far be improved by leaving out positions whose label the net cannot
learn? Two filters, in the loader, nothing else changes:
1. no position from which the move played is a capture - the label there may carry a recapture the
   search sees and the board does not show
2. no position with the side to move in check - engines do not evaluate those, the search always
   extends them

Stockfish's trainer skips both too (nnue-pytorch's fen skipping: captures, in check).

Baseline: `nets-set1-to-6-s8`, best epoch 14 (s8-e14) - sets 1-6 nowdl with HCE labels, `--stacks 8`,
`--blend-start 0.8 --blend-end 0.7 --epochs 20 --patience 2 --workers 6 --seed 1 --validation-every
100`. The new run repeats that exactly, with `--skip-tactical`.
The held back loss does not compare the two (different positions); only games.

Test: the 10000-games reference tournament on Linux (`test/tournament/strength-reference.ini`,
gauntlet `nnue-set1234`), in which s8-e14 already played: +17 Elo (+/-7) against set1234. The new
best epoch joins that field (see `src/pipeline/join-gauntlet.py`).

Machine: wherever a trainer is free first; the old run took 16 epochs of ~70 min, 18.5 hours on
the Mac (fewer positions per epoch with the filters).

### Architecture A1: (2 x 512) x (8 x 16) x (8 x 32) x (8 x 1) (Volker, 2026-10-05)
Goal: does a more complex net play better? A wider accumulator (512 instead of 256) with a narrower
first layer (16 instead of 32, as Stockfish's 15+1) - the cost of an evaluation is dominated by the
first layer (2 x width x L1), so 512 x 16 costs about what 256 x 32 does. Taken over from Stockfish as one
step although it changes two things: Stockfish's shape is well tested; what the two parts contribute
separately can be found out later if needed.
Trained after the current training, with the procedure that turns out best (step 3 or s8-skip), on the
same data with the same filter, against the 256 x 32 net of that procedure.
Work: the sizes are compile time constants in `src/nnue/nnue-arch.h` and `src/trainer/netfile.py`
(the architecture id follows from them) - a build with 512/16, and the SIMD paths checked for it.

### Architecture A2: king mirrored left-right, HalfKA_hm (Volker, 2026-10-05)
A test of its own, after A1, so that nothing goes wrong unseen. With the own king on files e-h the
board is mirrored a<->h, 32 king squares instead of 64, each weight sees twice the positions. The engine
side is the harder part: feature indices mirrored without costing nps, the king caches (fewer of
them), and when the king crosses the d/e line the cached positions have to be updated with mirrored
bitboards.

### Architecture A3: the net chooses its bucket (Volker, 2026-10-05)
After A2. Today the bucket of a position is its piece count. Idea: a gating layer learns which of the
**8** buckets evaluates a position (mixture of experts) - same number of buckets, only the choice is
learned. Soft (softmax) in training, hard towards the end or straight-through; the engine always picks
one bucket, so it stays as fast. A load-balancing term against the choice collapsing onto few buckets.
Cheap gate input first (piece counts per type and colour, king squares) - from the accumulator it would
cost about a second first layer. Compared against the same net with buckets by piece count.
Only if the learned choice works: more buckets become an option, as they are cheap in the engine.

### Stockfish architecture on sets 1-6 with HCE labels, on to at least epoch 100 (Volker, 2026-10-04)
Goal: the same question as the run to epoch 10 - is it our data or our net and training? - answered at
a comparable saturation. The SF17 feature transformer is 12 times as wide as ours (3072 against 256), so
it needs about 12 times the positions before it can be compared with our net: our net was close to its
plateau at ~1 G positions, which makes ~12 G for the SF net, at least 90 more epochs of 100 M. At e10
(1.0 G positions) it scored 49 % against HCE and was still rising (e1: 32.6 %); that says nothing yet.
Measure every 10 epochs against HCE (2000 games, 10+0.05, like e1 and e10) and compare with our nets.

State 2026-10-04 19:59: 13 epochs done, paused for step 2. Resume with
`RESUME=<ckpt> ACCELS=mps sh tmp/sf-train-to100.sh` from
`tmp/sf-train-1/lightning_logs/version_3/checkpoints/epoch=12-step=97663.ckpt` (state after epoch 13;
`test/nnue/sf-train-1/sf-train-1-e10.ckpt` is the state after epoch 10, kept out of tmp/),
`--max-epochs 100` or more, otherwise the call of `tmp/sf-train-to10.sh`. ~50 min per epoch on the Mac
gpu, ~75 hours for 90 epochs - whenever the gpu is free, last on the list.

Against the hangs: `caffeinate -dimsu` running for the whole run (the night's two "hangs" were idle sleep
of the Mac), no pinned memory on MPS (fixed in nnue-pytorch qapla-sf17), stack dumps every minute, the
gpu watcher, memory log.
