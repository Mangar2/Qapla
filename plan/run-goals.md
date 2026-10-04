# Runs and their goals

Every run that is going, paused or planned, with the goal it serves. The goal decides the follow-up, see
`.claude/skills/run-supervision/SKILL.md`. Updated when a run starts, ends or changes its goal.

Overall goal: Qapla's own net, at the level of the Stockfish net in Qapla - about 500-600 Elo over the
hand written evaluation.

## Going

### Mac: Stockfish architecture (SF17) trained on sets 1-6, HCE labels, to epoch 10
Goal: find out whether it is our training data that must improve. If SF's architecture and procedure
on our data stay far below the Stockfish net, the data is the lever; if it comes close, the training is.
Next: export e10, play it against HCE in the tournament of e1 (2000 games, 10+0.05). After that the
Mac trains set 1 with Stockfish labels (below) - Volker, 2026-10-04.

### Linux and Windows: set 1 labelled with Qapla's search and Stockfish's evaluation (chunks 51-101 / 1-50)
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
- on the Mac, after the e10 tournament (Volker, 2026-10-04). The tournament is one round robin of
  set1-sf, set1-hce (e08) and HCE (the nnue binary without a net), 3000 games per pairing, concurrency 9

## Paused

### Linux: tournament HCE against the set1234 net, at round 44
Goal: a reference figure of our best single-head net against HCE.

### Mac: set 1 relabelled by a low-selectivity HCE (factor 3 nodes) - 14 of 101 chunks
Linux has labelled chunks 51-101 with it since 2026-10-04 09:14 (`Qapla-hce-lowsel-wide-linux`, node
counts identical to the Mac, concurrency 30); 15-50 remain for Windows or the Mac.

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
100`. The new run repeats that exactly, with the filtering loader (`src/trainer/native/batcher.cpp`).
The held back loss does not compare the two (different positions); only games.

Test: the 10000-games reference tournament on Linux (`test/tournament/strength-reference.ini`,
gauntlet `nnue-set1234`), in which s8-e14 already played: +17 Elo (+/-7) against set1234. The new
best epoch joins that field (see `src/pipeline/join-gauntlet.py`).

Machine: wherever a trainer is free first; the old run took 16 epochs of ~70 min, 18.5 hours on
the Mac (fewer positions per epoch with the filters).
