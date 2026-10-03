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

**Confirmed** for a single set: set 2 (generation 1, a weak net, against itself) gave a net at least
73 Elo stronger than set 1 (HCE against itself) and at least 100 Elo stronger than set 3
(generation 2 against itself) - 5000 games each against HCE, all three labelled by HCE at depth 8,
trained without the game result, 124 to 139 million positions each.

**Not yet shown** either way whether the advantage remains once the net is fully trained: from three
to four sets on, no effect of the origin of a set could be measured (set123 62.2 %, set1234 63.2 %,
set12345 62.7 %, 5000 games each).
