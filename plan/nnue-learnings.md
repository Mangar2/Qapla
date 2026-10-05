# NNUE for Qapla - what we have learned

Meta findings only: one topic or assumption each, and its state - confirmed, refuted or not yet
shown - with the measurement that shows it. Single results belong to the evidence, not to the list.
Entries are added only after they have been agreed.

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

**Confirmed**: the best net with eight heads (set123456, epoch 14) scored +17 Elo against the best
single-head net (set1234) over 10000 games (qet: +/-5). The two were trained on different corpora,
set123456 and set1234; without heads, corpora of four and five sets could not be told apart
(63.2 % and 62.7 % against HCE, 5000 games each).

**Probably needs much more training** - one series only: the eight-head run reached its best held
back loss at epoch 14 of set123456, about 14.6 billion positions; the single-head run at epoch 11 of
set1234, about 6.5 billion.

## 4. A better evaluation in the labels makes a better net

**Confirmed** for one set: the same positions of set 1, labelled once by Qapla's search with the hand
written evaluation and once by the same search at the same depth (8) with Stockfish's evaluation, both
trained alike (one head, nowdl, stopped by their own held back loss) - the net from the Stockfish labels
scores 68.90 % against the other over 3000 games (+/-11 each), about +138 Elo (head to head, 10+0.05, see the evidence below).

Evidence: tournament `tmp/set1-sf-vs-hce-labels.state`, 2026-10-04 - set1-sf 68.90 % against set1-hce
over 3000 games (+/-11 each), stopped after the head to head as planned.

## 5. Better labels pay more, the more complete the training

**Supported** by one pair of measurements: on 26 chunks of set 1 (260k games, a quarter of it, 5
epochs) the Stockfish labels won two SPRTs, the second at bounds 40/50 - +50 the likelier of those two
candidates; on the whole of set 1 trained to its best epoch the advantage is about +138 Elo +/-16.
The two runs differ in more than the amount of data (fixed 5 epochs against stopped at the best
epoch), and an SPRT gives no magnitude, so the size of the step is not measured.

The reading: gaps in the training stay gaps however good the labels are - the benefit of better labels
shows only where the net has learned enough. With more sets and a more complete training, Stockfish
labels can be expected to pay at least as much again.

## 6. A better search of our own makes a better net - without any outside help

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
