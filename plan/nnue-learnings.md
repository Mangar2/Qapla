# NNUE for Qapla - what we have learned

Meta findings only: one topic or assumption each, and its state - confirmed, refuted or not yet
shown - with the measurement that shows it. Single results belong to the evidence, not to the list.
Entries are added only after they have been agreed.

**Revised 2026-10-17 (Volker):** entries 3-6 corrected - they compared nets from a 0.x trainer with nets from
a 1.x trainer, two factors at once (`src/trainer/CHANGELOG.md`, "Which nets came from which version"), and
entry 6 rested on a set with only 14 % of the labels it was named for. Entries 9-13 are the steps that made
the best net stronger since, the last section the ones that came out level with the best net of their day.
SPRTs are quoted as their decision only (H0/H1, games, bounds); figures with an error bar come from
tournaments or the reference field (10000 games against nnue-set1234, 10+0.05, Linux), whose absolute Elo
shifts by a few points whenever a net joins - only differences inside one table mean something.

## 1. Replacing the hand written evaluation by a strong net is worth an order of magnitude of 600 Elo

**Confirmed** - as an order of magnitude; there is no error estimate for the figure.

Evidence: a short tournament on 2026-09-27 (Mac, 60+0.5), Qapla with the hand written evaluation
against the same Qapla with Stockfish's evaluation, nothing else different. After 260 games the
Stockfish evaluation had scored about 95 %, without one loss.

The search was not adapted to the Stockfish evaluation in any way, so the figure is not the best
the net could do. At the time the Stockfish value even reached the search unconverted, in
Stockfish's internal unit, about four times Qapla's centipawn scale.

## 2. Games between players with a weak evaluation make the net learn faster

**Confirmed** for a single set: set 2 (the first net, nnue1, a weak one, against itself) gave a net at least
73 Elo stronger than set 1 (HCE against itself) and at least 100 Elo stronger than set 3
(the second net, nnue2, against itself) - 5000 games each against HCE, all three labelled by HCE at depth 8,
trained without the game result, 124 to 139 million positions each.

**Not yet shown** either way whether the advantage remains once the net is fully trained: from three
to four sets on, no effect of the origin of a set could be measured (set123 62.2 %, set1234 63.2 %,
set12345 62.7 %, 5000 games each).

## 3. Eight layer stacks instead of one head make the net stronger - a detail, not a step

**Corrected 2026-10-17: not shown.** Measured with one factor (2026-10-08): s1-sflike e800 (one head) against
s8-sflike e800 (eight heads), the same trainer and command, only `--stacks` different - one head 50.92 %, +/-9
each, 3000 games, no measurable difference; in the reference field later s1-sflike 2604.5 +/-6, s8-sflike
2600.6 +/-5. On today's data (old + new set + book, 20 M x 800, trainer 3.2.0) one head against eight:
**H0 accepted after 10777 games**, bounds -2/+3 (2026-10-17) - one head not better; eight stay the standard,
as they cost nothing measurable either. The original entry, kept as it was:

**Confirmed** (two factors - see the revision note): the best net with eight heads (set123456, epoch 14) scored +17 Elo against the best
single-head net (set1234) over 10000 games (qet: +/-5). The two were trained on different corpora,
set123456 and set1234; without heads, corpora of four and five sets could not be told apart
(63.2 % and 62.7 % against HCE, 5000 games each).

**Probably needs much more training** - one series only: the eight-head run reached its best held
back loss at epoch 14 of set123456, about 14.6 billion positions; the single-head run at epoch 11 of
set1234, about 6.5 billion.

## 4. A better evaluation in the labels makes a better net

**Corrected 2026-10-17: plausible, but not measured with one factor** - set1-sf came from trainer 1.1.1, its
opponent e08 from 0.6.1, which cannot be reproduced. Not re-measured: no external evaluation goes into our
net (run-goals, principle), so Stockfish labels are no longer pursued. The original entry:

**Confirmed** for one set: the same positions of set 1, labelled once by Qapla's search with the hand
written evaluation and once by the same search at the same depth (8) with Stockfish's evaluation, both
trained alike (one head, nowdl, stopped by their own held back loss) - the net from the Stockfish labels
scores 68.90 % against the other over 3000 games (+/-11 each), about +138 Elo (head to head, 10+0.05, see the evidence below).

Evidence: tournament `tmp/set1-sf-vs-hce-labels.state`, 2026-10-04 - set1-sf 68.90 % against set1-hce
over 3000 games (+/-11 each), stopped after the head to head as planned.

## 5. Better labels pay more, the more complete the training

**Corrected 2026-10-17: not shown** - it rests on entry 4 and on two runs that differ in more than the amount
of data. The original entry:

**Supported** by one pair of measurements: on 26 chunks of set 1 (260k games, a quarter of it, 5
epochs) the Stockfish labels won two SPRTs, the second at bounds 40/50 - +50 the likelier of those two
candidates; on the whole of set 1 trained to its best epoch the advantage is about +138 Elo +/-16.
The two runs differ in more than the amount of data (fixed 5 epochs against stopped at the best
epoch), and an SPRT gives no magnitude, so the size of the step is not measured.

The reading: gaps in the training stay gaps however good the labels are - the benefit of better labels
shows only where the net has learned enough. With more sets and a more complete training, Stockfish
labels can be expected to pay at least as much again.

## 6. A better search of our own makes a better net - without any outside help

**Corrected 2026-10-17: refuted for today's training.** The +29 below was no measurement of the wider search:
only chunks 1-14 of set1-lowsel carried lowsel labels (15-101 were labelled by a label-chunks.sh that dropped
the options), and the two nets came from different trainers (1.1.1 against 0.6.1). Both nets retrained with
their original trainers: HCE 57.12 % against lowsel (+/-10 each, 3000 games). With labels as the only factor:
set 1 pure lowsel against set 1 HCE 50.78 % (+/-9 each, 3000 games, A1 method with skip, 2026-10-10); lowsel
sets 1+3 against HCE sets 1+3, 256 x 32, 8 heads, 20 M an epoch, 100 epochs: 50.00 % (+/-9 each, 3000 games).
Under today's training the wider label search brings nothing measurable; labelling with it was ended. nolmr
(about 6x the labelling time) was not better than lowsel either (47.98 % against it, 2600 games, +/-11 each),
and with the same trainer HCE labels beat nolmr (55.08 %). Evidence: `test/nnue/forensics-set1-lowsel/`,
`test/nnue/results-lowsel-safeguard/`. The original entry:

**Confirmed** for one set: the same positions of set 1, labelled at the same depth (8) with the same
hand written evaluation, once by the default search and once by a less selective one (futility
pruning off, lmrDivisor 476 - about 2.9x the nodes, +112 Elo +/-14 over the default at depth 8). Both
trained alike (one head, nowdl, stopped by their own held back loss): the net from the wider search
scores 54.17 % against the other over 3000 games (+/-10 each), about +29 Elo (+/-14).

So the quality of the net can be raised by the quality of our own search alone - the lever we keep
while we use no external evaluation. By entry 5 the advantage can be expected to grow as the training
gets more complete; for the search that is not measured yet.

Evidence: tournament `tmp/set1-lowsel-vs-hce.state` on Linux, 2026-10-04 - set1-lowsel against the old
set-1 net (e08, default-search labels), 10+0.05.

## 7. Leaving out captures and checks is a must

**Confirmed**: the best run so far (sets 1-6, HCE labels, 8 heads) trained again identically, only
leaving out every position whose move captures or that is in check (24 % of the positions; Stockfish's
trainer skips the same) - in the reference field against nnue-set1234 the filtered net stands at 2688
Elo (+/-5, 10000 games), the unfiltered one at 2613 (+/-5, 10000 games): about +75 Elo (+/-7).

So the filter is the default of the trainer; it is switched off only where an old result without it
has to be reproduced for a comparison.

Evidence: `test/log/strength-reference.state` on Linux, 2026-10-05, nnue-set123456-s8-skip (best epoch
9) against nnue-set123456-s8 (best epoch 14), both against nnue-set1234 at 10+0.05.

## 8. Stockfish's training procedure beats ours - it is the new standard

**Confirmed**: the same net (8 heads), the same data (sets 1-6, HCE labels) and the same filter
(learning 7), trained once our way (an epoch is one pass, Adam lr 1e-3, stopped by the held back loss:
best epoch 9 of 11) and once Stockfish's way (epochs of 10 M positions from an endless stream - scaled
from Stockfish's 100 M to a net about a tenth of its size -, RangerLite, lr 8.75e-4 falling by 0.992 an
epoch, no stopping rule). Head to head, 2000 games each at 10+0.05: Stockfish's way at epoch 300 scored
56.35 % (+/-11), about +44 Elo; at epoch 420 57.57 % (+/-11), about +53 Elo.

Epoch 300 is 3 G positions, about four passes over the filtered data, against nine passes for our way.
So the trainer uses Stockfish's procedure by default.

Evidence: `tmp/sflike-e300-vs-s8-skip.state` and `tmp/sflike-e420-vs-s8-skip.state` on Linux,
2026-10-05.

## 9. The net needs training in proportion to its size - 20 M, then 40 M positions an epoch

**Confirmed.** With the learning rate falling per epoch, the epoch size decides how many positions the
high-rate phase sees. A1 (512 x 16) with 20 M an epoch against the 256 x 32 net of 10 M at epoch 100: 57.57 %
(+/-11, 2000 games), where the 10 M run of A1 had stood at 50.85 %. The control, 256 x 32 with 20 M an epoch
against A1 with 20 M: 50.70 % (+/-9 each, 3000 games) - A1's lead came from the doubled training, not from the
width. 40 M against 20 M (256 x 32, 8 heads, 800 epochs): 51.45 % over 5000 games (2605 +/-7 against 2595 +/-7),
**SPRT H1 accepted after 7650 games, bounds 0/5** (2026-10-12). Reference field 2026-10-12: s8-40m 2686.3 +/-6,
A1 2682.8 +/-6, s8-sflike (10 M) 2662.4.

Since 2026-10-15 the test programme trains with 20 M (2.2 h a run on the Mac Studio) so that one factor can be
tested a run; 40 M is a lever kept for the final net.

## 10. More distinct games pay clearly

**Confirmed** for one set: set 4 whole (2 M games) against every second game of it (1 M), the same steps
otherwise: 59.87 % (+/-10 each, 3000 games, 2026-10-09), about +70. The data is not saturated. Evidence:
`test/nnue/results-data/`.

## 11. One game per leaf of a large book, played at depth 9 with the play values as labels, beats the old sets

**Confirmed.** The new set: 10 M games HCE against HCE, one per leaf of a 10 M book, depth 9, the evaluations
of the play itself as labels (no separate labelling). New set against sets 1-6 (both without results, 20 M x
800, trainer 3.2.0): **H1 accepted after 14856 games**; old + new together against the old sets: **H1 after
1728 games** (bounds -2/+3, Mac Studio, 2.6+0.013). Reference field 2026-10-16: old + new (oldnew) 2694.3
+/-6 against s8-40m 2674.5 +/-6 - at half the training per epoch.

Caveat found afterwards: the ~18 book plies of each game had no value, so these nets never saw an early
position. Fixed by entry 12.

## 12. The book positions need values - each one once

**Confirmed.** The book of the new set evaluated once per position (qet --bookeval, HCE depth 9, 7.48 M
positions). Trained as a set of its own, every book position once, beside the games (bookonce) against
oldnew without book values: **H1 accepted after 12102 games**. The same values filled into all 10 M games
(every opening position as often as it was played): undecided against oldnew after 15000 games, and against
bookonce **H0 accepted after 5588 games** (bounds -2/+3). The book as a book, every position once, is the
standard.

## 13. The game result helps a lot - at blend 0.75, a quarter result

**Confirmed** for the new set, against the early finding on set 1 (blend 0.8-0.7 cost 79 Elo, 2026-09-29: a
0.x trainer, 8 epochs, other games). The new set with results against the same set without: WDL 0.95 **H1
after 1404 games**; WDL 0.95 against oldnew (twice the data, no results): **H1 after 3752 games**. Then each
weight against the previous: 0.90 > 0.95 H1 after 1983, 0.85 > 0.90 H1 after 4394, 0.80 > 0.85 H1 after 2883,
0.75 > 0.80 H1 after 11640 (the decisions getting slower: the optimum flattens), 0.60 against 0.75 **H0 after
2979**. Closing run 0.75 against 0.95: **H1 after 1620 games** (all bounds -2/+3, Mac Studio, 2.6+0.013,
2026-10-16/17). Reference field 2026-10-17 (one table): wdl80 2709.5 +/-7, wdl85 2690.2 +/-6, wdl90 2680.2
+/-6, wdl95 2664.9 +/-6, oldnew (no results) 2656.0 +/-6; completed at 16:3x (the scale had shifted by ~12):
wdl80 2697.9 +/-7, wdl75 2685.5 +/-6, wdl85 2678.6 +/-6, wdl90 2668.6 +/-6, wdl60 2665.5 +/-6, wdl95 2653.2 +/-6,
oldnew 2644.4 +/-6. Between 0.75 and 0.80 SPRT (0.75 better) and reference field (0.80 ahead,
about 12 +/-11) disagree; the two measure against different opponents at different time controls - flat
there either way.

Why it pays here, as far as measured: these games are much sharper than the conversion sigmoid(value/400)
assumes - positions at 65 % by the formula score 88 %, at 75 % they score 97 % (2.25 M positions, 2026-10-17).
The result carries what the depth-9 HCE value does not know.

A trap on the way: the first WDL runs (2026-10-16) trained without any result - gamefile read it only from a
[Result] tag, qet writes min=true pgns without one (fixed in chess-tools e703775). Their nets are kept as
`-noresult`; their SPRTs measured nothing. A conversion is checked for its results before it is trained.

## Tried, and level with the best net of its day

Each against the best net of the same training at the time, one factor different:
- **Fewer early positions** (Stockfish's soft early skipping): 51.73 % at e100, 50.72 % at e200 (+/-11, 2000
  games each); SPRT undecided after 7875 games (50.00 %), stopped (2026-10-05).
- **Wider accumulator** A1, 512 x 16 against 256 x 32, the same training (20 M): 50.70 % for 256 x 32, +/-9 each,
  3000 games (2026-10-10). The cheaper 256 x 32 stays.
- **PSQT** (a learned piece-square part per bucket): 49.12 %, +/-9 each, 3000 games (2026-10-09).
- **One head instead of eight** under today's training: 50.92 % for one head; on today's data H0 after 10777
  games (entry 3).
- **Wider label search** (lowsel, nolmr) under today's training: 50.78 % and 50.00 % (entry 6).
- **Trainer 3.2.0** (own Metal kernels, 1.9x faster) against 3.0.0, same data and command: **H0 accepted after
  8933 games**, bounds -2/+3 - kept for its speed (Volker 2026-10-15).
- **Curriculum**: epochs 1-400 old + new set, 401-800 the new set only, against oldnew: **H0 accepted after
  10490 games**, bounds -2/+3 (2026-10-16).
- **Book values filled into every game** against no book values: undecided after 15000 games (entry 12).
