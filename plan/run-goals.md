# Runs and their goals

Every run that is going, paused or planned, with the goal it serves. The goal decides the follow-up, see
`.claude/skills/run-supervision/SKILL.md`. Updated when a run starts, ends or changes its goal.

Overall goal: Qapla's own net, at the level of the Stockfish net in Qapla - about 500-600 Elo over the
hand written evaluation.

## The strategy (Volker 2026-10-18): all methodology with HCE first, then generation 2

Generation 2 (games and labels from the best net) is the larger lever, but it is planned only once everything is
known that can be learned faster with HCE data: how much data pays, how to train it (epoch size, length, blend and
its distribution, book), with which parameters. With HCE the sets are fastest to make; generation 2 should then need
only fine adjustment. Open with HCE, among others: the data quantity curve (a second and further 10 M sets from the
100 M book), training length against data size, the WDL weight by bucket.

## The programme: the best 8-bucket net out of HCE alone (Volker, 2026-10-04)

How do we get the best result for our 8-bucket net starting from the hand written evaluation? The steps
below build on each other; the single runs further down are parts of them.

**Principle: no external evaluation.** Until further notice no evaluation of another engine goes into
the training data of our net, although it would pay (Stockfish labels: about +150 Elo on set 1). We pull
ourselves out of the swamp by our own bootstraps. The Stockfish-label runs are for learning only.

Generations (Volker, 2026-10-05), a line of descent: the hand written evaluation is the root generation
(gen 0); a net trained on labels of gen 0 is a gen-1 net - what we train now; relabelling with a gen-1
net and training on those labels gives a gen-2 net; and so on. Labels are named by the generation that
produced them (gen-0 labels = HCE labels).

1. **Wider search for the labels** - in progress: set 1 relabelled by the low-selectivity HCE (Linux
   51-101, Windows 15-50, Mac 1-14 done), then trained like the old set-1 net and played against it.
   Trained 2026-10-04 17:38-19:07: best epoch 9 (held back 0.001941), `test/nnue/nnue-set1-lowsel.nnue`;
   Head to head against the old set-1 net (e08), 3000 games on Linux, done 19:48: set1-lowsel 54.17 %
   (+/-10 each), about +29 Elo. The wider search pays - step 4 goes ahead (sets 2 and 3 already being
   labelled).
   **Correction 2026-10-07:** only chunks 1-14 (Mac) of set1-lowsel carry lowsel labels; 15-101 (Windows,
   Linux) were labelled with plain HCE - both machines still had a label-chunks.sh before 239020f, which
   dropped the options (proven: labels identical, position for position, to a run without the options).
   So the +29 above is not a measurement of the wider search, and set1-nolmr (47.98 % in the head to head
   below) was played against a net with 86 % HCE labels. Redone: set 3 chunks 1-9 and set 2 chunks 1-20
   go into the queue again; the old labels are in Linux `tmp/hce-not-lowsel/`.
   **Volker 2026-10-07: everything built on it is void until set 1 is right.** Sets 2-6 stopped (set 3
   chunks 10-16 with options are valid and kept). Set 1 chunks 15-101 relabelled with lowsel on Linux
   since ~10:45 (`test/nnue/chunks-set1-lowsel-fix` -> `labelled-set1-lowsel-fix`, options checked on the
   qet command line), ~1100 s a chunk, ~27 h. Then: chunks 1-14 (Mac, correct) + 15-101 -> .gam, trained
   exactly like the old set-1 net, head to head against it (e08, HCE labels). If the full lowsel set is
   not better, find out why the set with 14 % lowsel labels was (+29).
   15:16-16:00 the Mac helped from chunk 101 downwards (chunks 101 and 100, 2055 s a chunk with 9 engines);
   stopped by Volker, the Mac trains A1 again (resumed at epoch 446). Linux does the rest alone, done
   ~2026-10-08 11:30.
   All traces of the +29 secured in `test/nnue/forensics-set1-lowsel/` (README there): it compared 14 %
   lowsel labels **and** a trainer 818 lines newer (f6ee940, compiled loader) against e08 (171fc0a, Sep 28).
   Control (Volker 2026-10-07): set 1 HCE labels retrained 16:58 with today's trainer (851de77) and exactly
   the nolmr command -> `nnue-set1-hce-retrain.nnue`; A1 paused after epoch 462. Then on Linux, 3000 games
   each: hce-retrain vs nolmr (labels only differ), hce-retrain vs e08 (trainer only differs); the set-1
   relabelling pauses for it (`tmp/linux-hce-retrain-h2h.sh`). Third: hce-retrain vs set1-lowsel (the +29
   test with labels as the only factor). On the CPU f6ee940 (lowsel) = 1.0.0 bit for bit, 171fc0a (e08)
   differs. Then the GPU spread: the retrain once more (`tmp/mac-set1-hce-retrain2.sh`), both against
   each other. Trainer versioned from now on (1.0.0, skill nnue-trainer).
   Evening 2026-10-07 (Volker): first a starting point. The tournament of 10-04 repeated with the originals:
   set1-lowsel 53.57 % (+/-10 each, 10-04: 54.17 %) - reproduced. Now both nets retrained with their
   original trainers (e08: 0.6.1, lowsel: 1.1.1, `tmp/mac-repro-damals.sh`), then head to head. The trainer
   history is numbered (CHANGELOG, now 2.2.1): learnings 7 and 8 compared one factor; 3 to 6 did not.
   **Do the buckets pay today?** s8-sflike e800 (1.3.0 = 2.2.1 under its command, CPU-proven) against the
   same command with `--stacks 1` (`tmp/mac-sflike-1head.sh`, ~10 h after the reproduction), then head to head.
   2026-10-08 00:37: both nets retrained with their original trainers, played as on 10-04: HCE (0.6.1, e10)
   57.12 % against lowsel (1.1.1, e06), +/-10 each, 3000 games - the old 54.17 % for lowsel does not come
   back. It was those two nets, not the trainer. Details in the forensics README.
   **Labelling with lowsel/nolmr ended for good (Volker 2026-10-08):** with the same trainer HCE labels give
   the stronger net (hce-retrain 55.08 % against nolmr; retrained 0.6.1 HCE 57.12 % against 1.1.1 lowsel).
   The relabelled set-1 chunks (`labelled-set1-lowsel-fix`, 29 of 87) and the set 2-6 chunks stay on Linux
   untouched until Volker decides what to delete.
   Except one point (Volker 2026-10-08): no net was ever trained on pure lowsel labels (set1-lowsel and its
   retrain: 14 % lowsel). So set 1 is relabelled with lowsel to the end (Linux, from 03:3x, ~17 h), then
   trained with 2.2.1 and exactly the hce-retrain command, and played against hce-retrain.
   **Buckets, 2026-10-08 09:03:** s1-sflike e800 (2.2.1, one head) against s8-sflike e800 (1.3.0 = 2.2.1 under
   the command, eight heads), only --stacks different: one head 50.92 %, +/-9 each, 3000 games - no
   difference measurable (difference about +6 for one head, its error about +/-13). Held back loss e800:
   0.000909 against 0.000907. The buckets pay nothing measurable today. Logs in test/nnue/results-buckets/.
   **Set 1b - more data (Volker 2026-10-08):** HCE against HCE from the same book at depth 6, but 2 threads per
   engine. Tested on the Mac with the original set-1 binary, 200 leaves: 1 thread plays the old set-1 games
   move for move; 2 threads give 200 of 200 different games, from right after the book, and a second 2-thread
   run differs again. On Linux after the lowsel labelling (`tmp/linux-newset.sh`: first the same 1-thread check
   with the Linux binary, then the whole book, concurrency 15 x 2 threads), then labelled at depth 8 as set 1.
   In parallel the pure-lowsel set 1 is assembled and trained on the Mac (`tmp/mac-lowsel-pure.sh`).
2. **No captures, no check positions** in the training - the filter run below, against s8-e14 in the
   reference tournament. Loader filter `--skip-tactical` done 2026-10-04 (794a3d0, checked against
   python-chess, 24 % of the positions skipped); the run started on the Mac 2026-10-04 19:59 (`tmp/mac-s8-skip-train.sh`, nets to `test/nnue/nets-set1-to-6-s8-skip`).
3. **Stockfish-like training procedure** - done, the new standard (learning 8). Run 2026-10-05 from 04:52,
   nets every 100 epochs in `test/nnue/nets-s8-sflike`. Against s8-skip, 2000 games each: e100 46.67 %,
   e200 55.62 %, e300 56.35 %, e420 57.57 %, e500 58.33 % (+/-11-12). Reference field 2026-10-05: e800
   69.32 % against set1234 (+/-5, 10000 games), s8-skip 63.03 % - about +49, +233 against HCE. Volker: no worsening expected
   as the learning rate falls; e800 (~15:00) goes into the 10000-games reference tournament on Linux.
   The plan as it was:
   - fixed epoch size of 10 M positions - Stockfish takes 100 M for a net about ten times as large
   - up to 800 epochs (8 G positions), measured every 100 epochs whether the net still improves
   - learning rate lowered every epoch as in Stockfish (gamma per epoch)
   - needs a fixed epoch size and a per-epoch lr schedule in `src/trainer/train.py`
4. **If the wider search promises an advantage (1), label all sets wider** and train them with the
   procedure found best in 2 and 3; otherwise train the current labels with it.
5. **Generation 2**: relabel all sets with the gen-1 net from 4 and train a gen-2 net. If that pays,
   further generations - it will saturate.

Depth-8 tournament on Windows, 2026-10-04 (Volker): lowsel without LMR (and with it without move count
pruning, which reads the same value) against lowsel, 3000 games - nolmr 64.98 % (+/-10 each), about
+107 Elo; nodes on three test positions 1.5x-3x of lowsel. Binary `Qapla-hce-lowsel-nolmr-win.exe` from
`tmp-lowsel-label` (2db3278), options lmrDivisor=100000 lmrPvDivisor=100000.

**Set 1 labelled with nolmr** since 2026-10-04 20:24 (Volker): Windows 1-50, Linux 51-101, concurrency 30,
`test/nnue/chunks-set1-nolmr`, binaries `Qapla-hce-lowsel-nolmr-{win.exe,linux}` (node counts identical).
About 57 min per chunk at concurrency 30 (lowsel: ~9), so ~48 hours for the full set - Volker:
the full set, no subset. Then trained like the old set-1 net and played against set1-lowsel.
Volker 2026-10-06: the test starts only when Linux and Windows have both finished. Training exactly as
set1-lowsel was trained (old procedure: `--epoch-size 0 --no-skip-tactical --stacks 1 --patience 3`,
seed 1, validation-every 100, full loop until the stop), then head to head against set1-lowsel. When
Windows has finished, its labels are copied to the Mac and Windows is hibernated (`tmp/win-end-nolmr.sh`).
Meanwhile the Mac trains A1 on from epoch 293.
2026-10-07: Windows done 04:51 (copied, hibernated 04:57), Linux done 05:06; .gam 417339651 bytes as
set 1. A1 paused after epoch 415. Trained 05:15-06:52: best epoch 10 (held back 0.002042),
`test/nnue/nnue-set1-nolmr.nnue`. Head to head against set1-lowsel on Linux since 06:54, 3000 games
(`tmp/set1-nolmr-vs-lowsel.state`); A1 resumed at epoch 416, paused again after epoch 445 (08:2x, Volker needs the Mac); resume with --resume.
Stopped 07:28 by Volker after 2600 games: set1-lowsel 52.02 %, set1-nolmr 47.98 % (+/-11 each). nolmr is
not clearly better, so lowsel stays the label search (nolmr costs ~6x the labelling time).
**Next (Volker 2026-10-07): label sets 2-6 with lowsel on Linux**, concurrency 30, binary
`Qapla-hce-lowsel-wide-linux`, options ffDepthFactor=5000 futDepthFactor=5000 lmrDivisor=476: set 3
resumed 07:29 (9 of 101 chunks done before), then 2, 4, 5, 6 (`tmp/linux-lowsel-queue.sh`; chunks made
on the Mac by `tmp/make-lowsel-chunks.sh`). Chunks 1-20 of set 2 were labelled by Windows on 2026-10-04 with the
same binary and options; input chunks md5-identical, copied to Linux 2026-10-07 with .done markers.

Stopped 2026-10-04 20:20 for it (labels kept): Started on speculation (no idle machine): Windows labels set 2 with the low-selectivity HCE from
2026-10-04 ~17:15 (`test/nnue/chunks-set2-lowsel`, 101 chunks); thrown away if step 1 shows no advantage. Linux labels set 3 likewise since
2026-10-04 18:20 (`test/nnue/chunks-set3-lowsel`), paused 19:10-~20:10 for the head to head of step 1.

Outlook, not planned in detail (Volker, 2026-10-05) - WDL in two phases:
- phase 1: generations without the game result until they saturate. Search-based labels can only fix
  errors that show inside the search horizon; a misjudgement that pays off later (e.g. a king attack)
  is a stable fixpoint of "eval = search over eval" and stays in every generation.
- phase 2: new games played by the best net, trained with the game result, again generations until
  they saturate - the result plays such errors out and corrects the fixpoint of phase 1. Early WDL from
  weak games risks blurring rather than correcting (set 1: HCE game results cost 79 Elo). The results of
  the old games stay as weak as the evaluation that played them, so phase 2 needs new games.
- one measurement that fits the hardware: the first new games of phase 2 trained once with and once
  without the result.
- open (Volker, 2026-10-05): play the phase-2 games deeper than the labels (e.g. depth 12) and relabel
  every position at depth 8, or play at depth 8? Deeper games make the result a statement about the
  position rather than about a blunder a few moves later, at a multiple of the cost per game. Test:
  the same compute in depth-8 games and in deeper games, both relabelled at depth 8, both with WDL.

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

### Fewer early positions in the training (Volker, 2026-10-05)
Our games start from the initial position and their first 8 plies come from book8ply, so the same
opening positions recur in thousands of games, each with a label - the net sees them a hundredfold.
Stockfish skips early positions with a probability falling over the ply (soft_early_fen_skipping:
keeps ~10 % at ply 0, ~15 % at 6, ~25 % at 10, ~75 % at 18, all later). To build into the loader like
the capture/check filter, Stockfish's curve first. Expected effect small: the positions concerned have
30-32 pieces, i.e. the last head (bucket 7, 32 pieces -> (32-1)/4 = 7).
Test in the Stockfish-like mode against the nets of the running step 3 at epochs 100 and 200.
Loader filter `--skip-early` done 2026-10-05 (13fa48f); the run starts on the Mac when step 3 has
written e800 (`tmp/after-e800.sh`, nets to `test/nnue/nets-s8-sflike-early`), stopped after epoch 200.
Result 2026-10-05, head to head on Windows, 2000 games each: early-e100 against step-3 e100 51.73 %
(+/-11), early-e200 against e200 50.72 % (+/-11) - no effect measurable either way.
SPRT on Windows (Volker, 2026-10-05 19:22): early-e200 against e200, H0 -2, H1 +3, max 10000 games. Meanwhile
the early run goes on on the Mac to e800 from its state after e200; stopped if the SPRT accepts H0.
Stopped 2026-10-05 21:18 at 7875 games, 50.00 %, LLR -0.14 - undecided, no decision expected before
10000 games. Volker: the old procedure stays, skipping early positions is not pursued for now (the start
book may be varied enough). The early run was stopped at e300 (`test/nnue/nets-s8-sflike-early`).

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

Result so far (A1 trained in the Stockfish-like mode from 2026-10-05 ~20:50, nets in `test/nnue/nets-a1`):
held back loss below the 256 x 32 net at every epoch (e100 0.000996 against 0.001047, e200 0.000911
against 0.000949), but head to head on Windows, 2000 games each: A1-e100 against step-3 e100 50.85 %,
A1-e200 against e200 48.93 % (+/-11) - no difference in play. The A1 binary searches about 9 % fewer
nodes per second. (A first A1 match at 27.5 % was void: the AVX2 dot product read past the 16-wide
layer, fixed in 2527211.)

Restarted 2026-10-06 ~03:00 with 20 M positions an epoch (Volker): with the learning rate falling per epoch,
a net twice as wide got its high-rate phase with half the positions it needs, which a longer run of the
same schedule cannot make up. Nets in `test/nnue/nets-a1`; the 10 M run (to e270) kept in
`test/nnue/nets-a1-10m`. ~35 hours to e800.
Result 2026-10-06: A1 (20 M) e100 against step-3 e100 57.57 % (+/-11, 2000 games), about +53 - against
50.85 % for the 10 M run at the same epoch. With the data scaled to its size the wider net plays better.
A1 (20 M) e200 against step-3 e200 55.00 % (+/-11), about +35. Against step 3 at the same amount of
data (A1 eN has seen what step-3 e2N has): about -9 at e100, about +21 at e200 (+/-22 each) - not shown
better, no sign of the advantage melting either. Volker: go on to e800, then A1-e800 into the reference
field (10000 games against set1234, where step-3 e800 stands at 69.32 %) - that decides.
Paused 2026-10-06 15:58 after epoch 292 (Volker needs the Mac). Resume: the same train.py call with
`--resume` (`--out ../../test/nnue/nets-a1 --stacks 8 --accumulator 512 --l1 16 --epoch-size 20000000
--workers 6 --seed 1 --validation-every 100`, sets 1-6 nowdl), log to `tmp/train-a1.log`.

### Architecture PSQT: a learned piece-square part per bucket - next topic (Volker, 2026-10-06)
Taken ahead of the earlier rule (Stockfish details only after +500 Elo): cheap, it improves the
evaluation substantially and carries over into every generation. Every feature gets, besides its
accumulator column, 8 PSQT values, one per bucket; they are summed and added straight to the output of
the chosen head. Training as in nnue-pytorch: output = head + (own psqt - opponent psqt) / 2, no
weighting; the PSQT weights start from the material values of the pieces. Stockfish blends 125/128 psqt
with 131/128 positional in the engine, tuned after training - for us an engine parameter for later, 1:1
to start. Cost: ~3 % more accumulator update, 1.4 MB, a new net format; checked across the SIMD paths.

State 2026-10-09: implemented (d3cb308, trainer 3.0.0 `--psqt`, net format QAPLANN3; checks in the
CHANGELOG entry). Training since 06:16 on the Mac: the command of s1-sflike e800 with `--psqt` the only
difference (`tmp/mac-sflike-psqt.sh`, ~34 s an epoch, done ~14:00); then head to head against s1-sflike e800
on Linux (the engine built there from GitHub, d3cb308).

**A1 is the new standard (Volker 2026-10-09):** reference field, 10000 games each: A1 e800 2695.1 +/-6,
s1-sflike e800 2678.6 +/-6, s8-sflike e800 2674.8 +/-5, HCE 2441.8 - A1 +253 over HCE. Defaults of trainer
and engine are switched only after the control below.
**Queued, top priority after psqt (Volker 2026-10-09): 256 x 32, 8 heads, 20 M an epoch, 800 epochs** - the A1 command with the default
sizes, so that A1 against it differs in the size only (A1 in the reference field also had twice the positions
of the sflike e800 nets). On the Mac after the psqt training (`tmp/mac-s8-20m.sh`).

**PSQT, 2026-10-09 18:40:** s1-sflike-psqt e800 (3.0.0) against s1-sflike e800 (2.2.1), --psqt the only
difference, one binary (d3cb308): psqt 49.12 %, +/-9 each, 3000 games - no measurable difference (about
-6, error of the difference about +/-13). Held back loss e800 0.000901 against 0.000909. So A1 + psqt is
not trained; the control goes on. Logs in test/nnue/results-psqt/.
**Data quantity, 2026-10-09 19:15:** set 4 whole (2 M games) against every second game of set 4 (1 M), same
source, same steps (8 heads, sflike, 200 epochs of 10 M, 2.2.1), one binary: whole 59.87 %, +/-10 each,
3000 games - about +70 for twice the distinct games. More distinct data pays clearly; the data is not
saturated. Held back loss e200 0.001090 against 0.001306. Logs in test/nnue/results-data/.
**Control, 2026-10-10 10:33:** 256 x 32, 8 heads, 20 M an epoch, e800 (3.0.0) against A1 e800 (512 x 16, same
training), head to head, 3000 games: 256 x 32 50.70 %, +/-9 each - no measurable difference (about +5, error of
the difference about +/-13). A1's lead in the reference field came from the doubled training, not from the size.
Current training: 256 x 32, 8 heads, 20 M an epoch, 800 epochs. Logs: Linux tmp/control-vs-a1*.
**Plan (Volker 2026-10-10): optimise the amount of training first, by binary search on the epoch size.**
256 x 32, 8 heads, 800 epochs, everything else the control's command. 40 M against 20 M (the control); if 40 is
better, then 80 against 40, else 30 against 20; and so on halving the step. Each step a head to head, 3000 games;
only the optimum goes into the reference field. Cost on the Mac: 40 M ~36 h, 80 M ~72 h.
**Step 1, 40 M against 20 M** (Mac CPU, 10+0.05, `tmp/s8-40m-vs-20m.state`): after 1800 games 40 M slightly
ahead, inside the error bars. **Volker 2026-10-12: the search stops here - the gap is too small for 80 M to pay,
and 30 M will not be clearly better.** If after 3000 games 40 M is ahead but still inside the bars, the same
tournament is continued to 5000 games (`tmp/mac-40m-h2h-extend.sh`).
**Result after 5000 games (2026-10-12):** 40 M 2605 +/-7, 20 M 2595 +/-7, 51.45 % for 40 M, draws 52.3 % - 40 M
ahead, the two intervals still touch (2598-2612 against 2588-2602). Nets on the T7 (`test/nnue/nets-s8-40m`, e800
also as `test/nnue/nnue-s8-40m-v3.1.0-e800.nnue`).
**SPRT on the Mac Studio (2026-10-12): H1 accepted after 7650 games, bounds 0/5** (6+0.03, 12 parallel games,
low power mode). 40 M is the new standard. It joined the reference field on Linux the same day (the depth-9
play paused for it, `tmp/linux-40m-reference.sh`).
Reference field (10000 games, 10+0.05): nnue-s8-40m-256x32-e800 2686.3 +/-6, A1 512x16 e800 2682.8 +/-6,
s1-sflike 2666.3, s8-sflike 2662.4, HCE 2429.5 +/-6 - 40 M at the top, its interval overlapping A1's.
**Safeguarding lowsel (Volker 2026-10-10)** - lowsel labelling costs days, so its gain must be secured under
today's training: (1) the control against A1 decides which training is current; (2) set 3 lowsel finished,
copied, converted; (3) HCE sets 1+3 against lowsel sets 1+3, today's training, skip, one command - labels the
only difference - then head to head. Only that decides whether sets 2, 4, 5, 6 are labelled with lowsel.
The set-1 test running on the Mac (A1 method, 50 epochs of 20 M) is an early signal.
**Set-1 label test under today's training, 2026-10-10 14:38:** set 1 pure lowsel against set 1 HCE, A1 method with
skip (512 x 16, 8 heads, 20 M an epoch, 50 epochs, trainer 3.0.0), head to head 3000 games: lowsel 50.78 %,
+/-9 each - no measurable difference (about +5, error about +/-13). The +27 of the old procedure without skip
does not carry over.
**Safeguard result, 2026-10-10 19:38:** lowsel sets 1+3 against HCE sets 1+3, 256 x 32, 8 heads, 20 M an epoch,
100 epochs, trainer 3.1.0, labels the only difference, 3000 games: 50.00 % each, +/-9 - no difference. Together
with the set-1 test (50.78 %): under today's training lowsel labels bring nothing measurable. The lowsel
labelling stays ended; the time goes into more data (the two 10 M sets). Logs in test/nnue/results-lowsel-safeguard/.
Order on the Mac (Volker 2026-10-10): the safeguard first - it decides whether the time goes into more lowsel
or into more training data - then the epoch-size search. `tmp/mac-safeguard-lowsel.sh`: 256 x 32, 8 heads,
20 M an epoch, 100 epochs (2 G positions), HCE sets 1+3 then lowsel sets 1+3 (~2 x 2.3 h).
**More data (Volker 2026-10-10) - two sets of 10 M games to compare:**
- old book 10x, mixed: sets 1-6 (9 games per leaf) + set 1b (HCE-HCE, 2 threads) = 10 M games; set 1b labelled
  on Linux after set 3 (lowsel labelling ends with set 3).
- new book, HCE only: the book extended from 1 M to 10 M leaves (same generator and settings, seed 2,
  `new-versions/Qapla-bookgen-mac`, built from c03335a without the uncommitted search files), one game per leaf
  HCE-HCE with 1 thread. **Changed (Volker 2026-10-11): played at depth 9 (as Stockfish), the evaluations of
  the play written into the pgn (`eval=true`) are the labels - no separate labelling; WDL from the results to be
  used too.** Depth 10 too risky as labels. Measured on 100 games: depth 10 costs ~8.5x depth 6, depth 9 ~0.65x
  depth 10, so ~8 days on Linux alone (Windows not used). 20 test games: the play values carry the same perspective
  as a separate depth-9 labelling and the converter reads them (`/9` suffix, mates); book plies get no value.
  The 1.68 GB book pgn made qet exit silently, so it is played in 11 parts of 1 M leaves
  (`tmp/book10m-d9/run-linux.sh`, out `test/nnue/games-book10m-d9/part-NN.pgn`).
Allows e.g.: HCE-only set against the mixed set of the same size.

**Test programme once the new set is played (Volker 2026-10-14)** - replaces the list below where they differ.
Training on the Mac Studio, and from 2026-10-15 (Volker) the SPRTs there too, beside the training, at 12 games,
`tc=2.6+0.013`, `maxgames=15000` - an SPRT finishes within one 2.2 h training, so a test round closes every
2.2 h with the next net training ahead (see the nnue skill); step 2 still runs on the M4. Every test is an SPRT of two versions against each
other, `test/sprt/sprt-standard.ini` (H0 -2, H1 3, maxgames 20000), nets at `--each tc=10+0.05 option.Hash=64`.
1. Make the trainer use the Mac Studio as fully as possible (the GPU idles at ~20 W, the main thread at ~22 %).
2. Old set (sets 1-6 nowdl), 20 M x 800, with the faster trainer, SPRT against the 20 M control
   (`nnue-s8-20m-v3.0.0-e800`). H0 -> ask Volker; H1 or undecided -> the new trainer is fine.
   Trainer 3.2.0 (fastft, 1.9x on the Mac Studio). Net `nnue-s8-20m-v3.2.0-e800` (Mac Studio, 2026-10-15 07:42-09:57),
   SPRT on the M4 from 10:01. **Volker 2026-10-15 16:20, the SPRT at LLR -2.27 after 8514 games: let it run, but
   3.2.0 is taken anyway - it is very close; steps 3-5 are tested against `nnue-s8-20m-v3.2.0-e800`**, the net of
   the same trainer.
   **Result: H0 accepted after 8933 games, bounds -2/+3** (M4, 10+0.05, 9 games; 2026-10-15 16:5x). 3.2.0 stays
   anyway (Volker, above).
3. New set, 20 M x 800, SPRT against 2 (old set, same trainer: `nnue-s8-20m-v3.2.0-e800`).
   **H1 accepted after 14856 games, bounds -2/+3** (Mac Studio, 12 games, 2.6+0.013, 2026-10-16 04:39-06:53):
   the new set (10 M HCE games from the new book, depth 9, play values as labels) beats the old sets 1-6.
4. Old + new set, 20 M x 800, SPRT against 2.
   **H1 accepted after 1728 games, bounds -2/+3** (Mac Studio, 12 games, 2.6+0.013, 2026-10-16 06:55-07:07).
   Reference field (Linux, 10000 games, 10+0.05, 2026-10-16): nnue-oldnew-20m-v3.2.0-e800 2694.3 +/-6, ahead of
   nnue-s8-40m 2674.5 +/-6 and A1 2671.0 +/-6; HCE 2417.7 +/-6. **Caveat found afterwards: the new set has no value
   on its ~18 book plies a game (the play never searched them), so a net from it never saw early positions** - fix
   under way (book evaluation in qet, values filled in by gamefile).
5. New set with WDL, three weightings of the result (looking for the best one), each SPRT against 2.
   The first try ran without results (gamefile read the result only from a [Result] tag, qet writes min=true pgns
   without one - fixed in chess-tools e703775); its nets are kept as nets-wdl95/90-noresult, their SPRTs measured
   nothing. **Again with results: WDL 0.95 (new set, book plies without value as in step 3) against step 3 - H1
   accepted after 1404 games, bounds -2/+3** (Mac Studio, 12 games, 2.6+0.013, 2026-10-16 17:26-17:36).
   **WDL 0.95 against step 4 (old + new set, no WDL): H1 accepted after 3752 games, bounds -2/+3** (2026-10-16
   17:50-18:21; 51.99 %, W 1069 D 1763 L 920) - the new set alone with WDL 0.95 is now the best net.
   Reference field (Linux, 10000 games, 10+0.05, 2026-10-16 18:50-21:30): nnue-wdl95-20m-v3.2.0-e800 2692.8 +/-6,
   step 4 (oldnew) 2684.0 +/-6, HCE 2407.4 +/-6.
   **WDL 0.90 against WDL 0.95 (both new set, with results): H1 accepted after 1983 games, bounds -2/+3** (Mac
   Studio, 12 games, 2.6+0.013, 2026-10-17 04:27-04:44; 53.73 %, W 578 D 975 L 430). 0.90 joins the reference
   field; 0.85 is in training. Reference field (Linux, 10000 games, 2026-10-17 04:46-06:57):
   nnue-wdl90-20m-v3.2.0-e800 2698.3 +/-6, wdl95 2683.0 +/-6, step 4 (oldnew) 2674.2 +/-6, HCE 2397.6 +/-6.
   **WDL 0.85 against WDL 0.90: H1 accepted after 4394 games, bounds -2/+3** (Mac Studio, 2026-10-17
   06:28-07:05; 51.63 %, W 1142 D 2253 L 999). 0.85 joins the reference field; 0.80 is in training.
   **WDL 0.80 against WDL 0.85: H1 accepted after 2883 games, bounds -2/+3** (Mac Studio, 2026-10-17
   08:28-08:52; 52.45 %, W 769 D 1486 L 628). 0.80 joins the reference field; 0.75 is in training.
   Reference field (Linux, 10000 games each, 2026-10-17 07:06-11:28): wdl80 2709.5 +/-7, wdl85 2690.2 +/-6,
   wdl90 2680.2 +/-6, wdl95 2664.9 +/-6, step 4 (oldnew) 2656.0 +/-6, HCE 2379.4 +/-6.
   **WDL 0.75 against WDL 0.80: H1 accepted after 11640 games, bounds -2/+3** (Mac Studio, 2026-10-17
   10:28-12:07; 50.66 %, W 2903 D 5987 L 2750) - the games to the decision rose from 2-4 k to 11.6 k. 0.70 was
   started and aborted after 4 min for 0.60 (a point beyond the expected optimum). Volker 2026-10-17: 0.75 and
   0.60 go into the reference field without a won head to head, to measure the curve.
   Closing run of the series (CLAUDE.md): **WDL 0.75 against WDL 0.95: H1 accepted after 1620 games, bounds
   -2/+3** (2026-10-17 12:08-12:21; 54.72 %, W 503 D 767 L 350).
   **WDL 0.60 against WDL 0.75: H0 accepted after 2979 games, bounds -2/+3** (2026-10-17 12:36-13:01; 47.67 %,
   W 671 D 1498 L 810). **The WDL weighting is settled: blend 0.75** for every further training with results. The
   optimum lies between 0.60 and 0.80, flat at 0.75-0.80; a point at 0.70 would not pay for itself. The reference
   field figures of 0.75 and 0.60 complete the curve - reference field, 10000 games each, 2026-10-17 16:3x: wdl80
   2697.9 +/-7, wdl75 2685.5 +/-6, wdl85 2678.6 +/-6, wdl90 2668.6 +/-6, wdl60 2665.5 +/-6, wdl95 2653.2 +/-6, oldnew
   2644.4 +/-6, HCE 2367.7 +/-6.
   **Volker 2026-10-17 17:0x: the standard is blend 0.80** (the reference field result is the basis); every new
   test 1:1 against the last 0.80 net (wdl80) until something better exists. c1 and c2 (0.75, started before)
   finish and are compared within the 0.75 line (c1 against wdl75, c2 against c1).
   **c1 (new set with results + book, 0.75) against wdl75: H1 accepted after 12112 games, bounds -2/+3** (Mac
   Studio, 2026-10-17 16:30-18:12; 50.62 %, W 3049 D 6163 L 2900) - the book helps under WDL too. c2 was stopped
   by mistake at epoch 357 (17:31) and resumed from its checkpoint (17:32).
   **c2 (c1 + sets 1-6 without results, 0.75) against c1: H1 accepted after 4541 games, bounds -2/+3** (Mac Studio,
   2026-10-17 18:47-19:24; 51.61 %, W 1241 D 2205 L 1095) - the old data helps under WDL too. Then c2 against wdl80
   (whether it is the new best net; not one factor): **c2 against wdl80: H1 accepted after 4828 games, bounds
   -2/+3** (2026-10-17 19:25-20:06; 51.50 %, W 1274 D 2425 L 1129). **c2 is the best net**; reference field
   (Linux, 10000 games, 20:07-22:20): nnue-c2-20m-v3.2.0-e800 2700.5 +/-6, wdl80 2691.2 +/-7, wdl75 2678.8 +/-6,
   HCE 2361.1 +/-6 - c2 +339 over HCE.
   **c1-b80 (new set with results + book, blend 0.80) against wdl80: undecided after 15000 games** (LLR -0.08, bounds
   -2/+3; 50.04 %, W 3676 D 7659 L 3665; Mac Studio 20:50-22:56). At 0.80 the book shows nothing measurable (at 0.75 it
   was H1); it stays the standard as the logical one (Volker's rule for an undecided book test).
   **c3 (c2 with the results of sets 1-3) against c2: H1 accepted after 2564 games, bounds -2/+3** (Mac Studio,
   2026-10-17 23:05-23:26; 52.75 %, W 658 D 1389 L 517) - the results of the equal-player old sets help. **c3 is the
   best net**; reference field (Linux, 10000 games, 23:28-01:40): nnue-c3-20m-v3.2.0-e800 2701.6 +/-7, c2 2694.1 +/-6,
   wdl80 2684.9 +/-7, HCE 2354.8 +/-6 - c3 +347 over HCE.
   **c1-b80-k4 (the book with WDL values, w = N/(N+4)) against c1-b80 (the plain book), blend 0.80: undecided after
   15000 games** (LLR -0.50, bounds -2/+3; 49.97 %, W 3565 D 7861 L 3574; Mac Studio 2026-10-18 01:06-03:12). Taken as
   agreed: the logical one, it did not lose - the book with WDL values (k = 4) is the standard.
   **c3-b80 (c3 at blend 0.80) against c3 (blend 0.75): H0 accepted after 6655 games, bounds -2/+3** (Mac Studio,
   2026-10-18 03:21-04:17; 48.99 %, W 1558 D 3405 L 1692) - on the combined data 0.80 is not the better blend.
   **Volker 2026-10-18: the final set is trained at blend 0.80** (the reference field had 0.80 ahead of 0.75 where the
   SPRTs preferred 0.75).
   **c4 (c3 with the results of sets 4-6 too, unequal players) against c3: H0 accepted after 1075 games, bounds
   -2/+3** (Mac Studio, 2026-10-18 05:36-05:45; 43.16 %, W 203 D 522 L 350) - the results of games between unequal
   players hurt, as Volker had assumed; sets 4-6 stay without results.
   **final1** (Mac Studio, `tmp/book10m-d9/train-final.sh final1`, 05:37-09:41, ~18 s an epoch) **against c3: H1 accepted
   after 4087 games, bounds -2/+3** (09:42-10:16; 51.77 %, W 1081 D 2070 L 936) - the best net; joins the reference
   field (Linux, from 10:1x).
   **Then (agreed with Volker 2026-10-18):** the final set - every factor's winner (new set with results, book once
   with WDL values k = 4, sets 1-3 with results, sets 4-6 by c4, blend 0.80, 40 M x 800, 256 x 32, 8 stacks) - SPRT
   against c3, then the reference field. **Once the final net is trained and shows in the Linux reference field that it
   is the best, move the evaluation of the 100 M book from the M4 to the Mac Studio** (resume there from the M4's
   state; the same HCE build, depth 9).
   **After final1 (Volker 2026-10-18): the WDL weight by bucket** - c1-b80-k4 again with `--blend-buckets
   0.69,0.72,0.75,0.78,0.81,0.84,0.87,0.90` (bucket 7, the opening, 0.90; bucket 0, the endgame, 0.69; trainer 3.3.0,
   0bf5a15; without the option 3.3.0 trains the nets of 3.2.0, CPU-proven) against c1-b80-k4 - the distribution of
   the WDL weight over the game the only difference. Idea: the result says more the nearer the end of the game.
   **Move of the 100 M book evaluation (Volker 2026-10-18):** only when the test now on the Mac Studio is through (the
   bucket training and its SPRT against c1-b80-k4) - then the evaluation goes to the Studio with 12 engines (qet 0620e6c
   built there, `~/dev/qapla/qapla-engine-tester/build/release`; `new-versions/Qapla-hce-53f972f-studio` gives the
   node counts and scores of the M4's `Qapla-set1-hce` at depth 9, 6 positions; the state is the out file itself). In training: c1-b80-k4 (the book with WDL values,
   k = 4) against c1-b80.
   **How the tests go on (Volker 2026-10-17):** every test against the net that differs in exactly the factor
   tested, whichever is the best net at the time - c1 at 0.80 against wdl80 (the book), the book with WDL values
   against c1 at 0.80 (WDL in the book), c3 against c2 (the results of the old sets). **Goal: a final set that uses
   the winning choice of every factor**, trained once all factors are decided.
   **Old and new data together with WDL** (Volker 2026-10-17, replaces 10. below), blend 0.75, the book once as
   standard, one factor per step: c1 = new set with results + book, against wdl75 (the book under WDL); c2 = c1 +
   sets 1-6 without results, against c1 (the old data); c3 = c1 + sets 1-3 with results and 4-6 without, against
   c2 (the results of the equal-player sets). Later: the mix of old and new, the WDL rate of the book.
   **WDL in the book values** (Volker 2026-10-17; the rule is in the nnue-training skill, "Value, win
   probability and WDL"): target = (1 - w) * sigmoid(score/400) + w * rate, w = N/(N+4) (20 % at one game, towards
   100 % at many), the same conversion as everywhere. Two steps, one factor each: (1) the book with a fixed
   w = 0.25 against c1 (WDL in the book at all); (2) w = N/(N+4) against (1) - probably not measurable, taken for
   being the logical one unless it loses. Needs the v2 book (M4, ~18:20) and `gamefile --book2gam wdl=`
   (chess-tools 37d86c0).
6. (Volker 2026-10-16) Curriculum: epochs 1-400 old + new set, 401-800 the new set only (the "fine tuning" on the
   set that did a little better), 20 M an epoch; SPRT against 4. In the gpu gap after the wdl90 training.
   Caveat: the new set still lacks values on its book plies, so the second half sees no early positions.
   **H0 accepted after 10490 games, bounds -2/+3** against 4 (Mac Studio, 12 games, 2.6+0.013, 2026-10-16
   13:15-14:42; 49.40 %, W 2406 D 5552 L 2532). 4 stays the best 8-bucket 20 M net.
7. (Volker 2026-10-16) Old + new set at 40 M x 800 - the one left with a chance to beat 4. (Written when the first
   WDL runs looked like no gain - they had trained without results; with results WDL is the largest step, see 5.)
8. (Volker 2026-10-16) One bucket (--stacks 1) with old + new, 20 M x 800, to see whether old + new profits from them. Against the best 8-bucket
   20 M x 800 net (curr if it beats 4, else 4).
   Run with the book as standard (2026-10-17): bookonce1 = old + new set + book, --stacks 1, against bookonce (8
   stacks, otherwise identical): **H0 accepted after 10777 games, bounds -2/+3** (Mac Studio, 14:27-15:58; 49.38 %,
   W 2659 D 5326 L 2792). The 8 stacks stay.
9. (Volker 2026-10-16) The book plies of the new set, evaluated once each (qet --bookeval, depth 9, HCE), two ways:
   (a) the values filled into all 10 M games (gamefile --pgn2gam book=) - early positions weighted as in the old sets;
   (b) the evaluated book as a set of its own (gamefile --book2gam, every book position once) beside the games
   without their book plies. Both old + new, 20 M x 800; SPRT against 4 and against each other.
   Volker 2026-10-16: if (a) against (b) comes out undecided, (b) - the book as a book, every position once - is the
   standard for all further trainings (the logical one; the old sets already overweight the start positions).
   **(a) filled against 4: undecided after 15000 games** (LLR -1.79, bounds -2/+3; 49.77 %, W 3694 D 7542 L 3764;
   Mac Studio 2026-10-16 19:45-21:50).
   **(b) bookonce against 4: H1 accepted after 12102 games, bounds -2/+3** (50.64 %, W 3118 D 6020 L 2964; Mac
   Studio 2026-10-17 02:22-04:04). (b) is the standard for the book plies from now on.
   **(a) filled against (b) bookonce: H0 accepted after 5588 games, bounds -2/+3** (48.73 %, W 1374 D 2698
   L 1516; Mac Studio 2026-10-17 04:05-05:10, paused 04:27-04:45 for the WDL 0.90 SPRT).
10. (Volker 2026-10-16) After the WDL weighting is settled: the old sets together with the new set with WDL at the
   best blend - (a) sets 1-6 without results (`oldwdl1`), SPRT against the best WDL net; (b) sets 1-3 with results
   (equal players: HCE-HCE, nnue1-nnue1, nnue2-nnue2), 4-6 without (`oldwdl2`), SPRT against (a). The trainer weights
   per position, so files with and without results train together without a change.
Then decide according to what was reached.

**What the new set is tested for (Volker 2026-10-11).** Always against the best set so far (sets 1-6, nowdl),
with the identical procedure of today's standard (256 x 32, 8 heads, sflike, 800 epochs, 20 M per epoch - or
40 M if the running 40 M training wins its head to head), head to head 3000 games; the reference field only for
a winner. The mixed 10 M set is no longer the comparison.
1. new set only against old set only, both without the result.
   1b. only if the new set is not worse: the new set with WDL, the weight of the result optimised. Start with the
   weighting that was successful then: `blend 0.95` (5 % result) - on set 1 it scored 40.65 % against HCE
   against 36.45 % for nowdl (1000 games each, +/-19 each, 2026-09-29), while blend 0.7-0.8 cost 79 Elo.
   In today's procedure that means `--blend-start 0.95 --blend-end 0.95` on the `.gam` with results.
2. new set + old set together (more training data).
3. first the old set, then in all further epochs only the new set - the coarse training may take the "odd"
   games too, the fine training only the new ones.
4. first the old set without the result, then only the new set with WDL - the fine training with WDL once a
   coarse net exists.
3 and 4 need a trainer option to change the data at an epoch (minor version); the epoch of the switch is open.
**To do after the label test (Volker 2026-10-10): remove `--skip-tactical`/`--no-skip-tactical` from the
trainer (always skip) and with it `--loader python` (it cannot skip) - trainer 3.1.0, default nets identical
(test_reproducible). Not before both label-test trainings are done, so that both run the same code.**
**A1 + psqt only if psqt pays (Volker 2026-10-09)** - the A1 command with `--psqt` the only difference,
~35 h on the Mac (`tmp/mac-a1-psqt.sh`, not started), then against A1 e800. The psqt test now running
on Linux is the same question on the 256 x 32 one-head net (s1-sflike).

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

### Architecture A4: further mixture-of-experts steps - only if A3 shows success (Volker, 2026-10-05)
Taken from large MoE language models (one with 512 experts per layer, better than its dense sibling at
less compute per token). Costs measured against the first layer of A1 (2 x 512 x 16 = 16,384 MACs):
- a router with access to the context: the choice computed from the accumulator (2 x 512 -> 8),
  +8,192 MACs, about +50 % on the first layer
- a choice of its own per layer: layer 2 picks its expert from the 16 outputs of layer 1 - almost free
- a fixed expert plus chosen ones (e.g. 9 x 16: one always, one of 8): the fixed one learns what holds
  everywhere, the chosen ones only the particular - doubles the first layer
- more experts (8 -> 16 -> ...): no cost per position, only data per expert

### Stockfish architecture on sets 1-6 with HCE labels, on to at least epoch 100 (Volker, 2026-10-04)
Goal: the same question as the run to epoch 10 - is it our data or our net and training? - answered at
a comparable saturation. The SF17 feature transformer is 12 times as wide as ours (3072 against 256), so
it needs about 12 times the positions before it can be compared with our net: our net was close to its
plateau at ~1 G positions, which makes ~12 G for the SF net, at least 90 more epochs of 100 M. At e10
(1.0 G positions) it scored 49 % against HCE and was still rising (e1: 32.6 %); that says nothing yet.
Measure every 10 epochs against HCE (2000 games, 10+0.05, like e1 and e10) and compare with our nets.

State 2026-10-05 19:25: 14 epochs done, paused for the early-position run. Resume with
`RESUME=<ckpt> ACCELS=mps sh tmp/sf-train-to100.sh` from
`tmp/sf-train-1/lightning_logs/version_4/checkpoints/epoch=13-step=103767.ckpt` (state after epoch 14;
`test/nnue/sf-train-1/sf-train-1-e10.ckpt` is the state after epoch 10, kept out of tmp/),
`--max-epochs 100` or more, otherwise the call of `tmp/sf-train-to10.sh`. ~50 min per epoch on the Mac
gpu, ~75 hours for 90 epochs - whenever the gpu is free, last on the list.

Against the hangs: `caffeinate -dimsu` running for the whole run (the night's two "hangs" were idle sleep
of the Mac), no pinned memory on MPS (fixed in nnue-pytorch qapla-sf17), stack dumps every minute, the
gpu watcher, memory log.
   **Pure lowsel, 2026-10-08 22:31:** set1-lowselpure (2.2.1, best e07, held back 0.002041) against set1-hce-retrain
   (2.2.0 = 2.2.1, e07) - same trainer, command, games; only the labels differ: lowsel 53.88 %, +/-10 each, 3000
   games, about +27 (difference error about +/-14). The wider search does make better labels; nolmr (lmr off as
   well) made worse ones (hce-retrain 55.08 % against it). Logs in test/nnue/results-lowselpure/.
   Set 1b played 2026-10-08 20:34 - 10-09 04:25: 1,000,333 games (`test/nnue/games-hce-t2-depth6.pgn`) - 313 more
   than leaves, from the pause for the lowsel head to head (qet replayed some leaves on resume; with 2 threads
   they are other games). Labelling on Linux since 04:27 (`chunks-set1b` -> `labelled-set1b`, HCE depth 8,
   1 thread, concurrency 30, chunk 101 has 333 games).
   **Open question (Volker 2026-10-08): the order of the data.** E.g. the "wild" games of the weak nets first as a
   grounding, the "good" HCE-HCE games after. Needs a trainer option that reads the files one after another
   instead of mixed (a minor version). Not started.
   Data saturation is unknown (Volker): the old measurements had too few games for small steps and came from the
   old training, which may not have used more data; Stockfish sees every position about once, we ~8 times.
