# Training Qapla's NNUE

This is how Qapla's neural network evaluation came about: what we did, in which order, and what each
step was worth in playing strength. It describes principles and results, not every setting.

**One rule for the whole project: no outside knowledge.** Every training position was played by
Qapla, and every label comes from Qapla's own search. No evaluation of another engine went into the
net. The net is meant to grow out of Qapla's hand-crafted evaluation (HCE), pulling itself up by its
own bootstraps.

**How strength is measured.** Every figure is the result of a tournament at 10+0.05 seconds per game
from a common opening book, mostly 10,000 games per engine against a fixed reference. The figures are
typically accurate to within about ±5 to ±15 Elo; differences smaller than that are not claimed.
Each heading gives what a step gained over the previous one and, in brackets, where it stands against
the HCE.

## The net

A standard NNUE architecture, kept small:

- **Input:** HalfKA features - every piece on its square, seen from the king of each side,
  45,056 features per perspective.
- **Accumulator:** 256 neurons per perspective, updated incrementally as moves are made and unmade;
  the two perspectives are concatenated.
- **Head:** 512 → 32 → 32 → 1 with clipped ReLU, in eight copies ("layer stacks"); the number of
  pieces on the board chooses which one evaluates a position.
- **Quantized** to 16-bit accumulator weights and 8-bit dense weights for fast integer inference.

## The training data

### A book of positions from the search tree

We did not start from games of strong players. We started from a book of **one million start
positions** built by Qapla itself: from the initial position, all moves, then lines taken half at
random out of Qapla's own search tree, kept only while their value stays close to the best line. The
idea: a net is asked to evaluate the positions a search visits, not only the positions that end up
being played, so the training positions should come from the same place.

### Six sets of games, labelled by Qapla's search

From every book position a game was played at depth 6. Every position of every game was then searched
to depth 8 by the HCE, and that search value became the label. The labelling ran in
[qapla-engine-tester](https://github.com/Mangar2/qapla-engine-tester), which walks each game backwards and
which spread the work over a Linux machine, a Windows machine and, for a while, cloud instances.

To make the games diverse we used three different players: the HCE and two deliberately weak early
nets, trained on far too little data. Games between them - all against all, six sets - wander into
positions a strong engine would never reach, while the labels still come from a sound search:

| set | players | games |
|---|---|---|
| 1 | HCE - HCE | 1 M |
| 2 | early net 1 - early net 1 | 1 M |
| 3 | early net 2 - early net 2 | 1 M |
| 4 | early net 1 - HCE | 2 M |
| 5 | early net 2 - HCE | 2 M |
| 6 | early net 1 - early net 2 | 2 M |

Together **9 million games and about 1 billion labelled positions**. The result of a game is not used
in training.

## The main line

### Trained on one set, 139 million positions (−132 vs. HCE)

The first real net, trained on set 1 alone, was still clearly weaker than the hand-crafted evaluation
it learned from.

### More and more diverse data: +223 (+91 vs. HCE)

Trained on sets 1 to 4 the net passed the HCE. The games of the weak nets mattered: a set played by a
weak net against itself made the net learn faster than the HCE's own games. From three or four sets
on, adding data no longer helped this net.

### Eight heads instead of one: +17 (+108 vs. HCE)

One head per range of piece counts, sharing the same accumulator. A small step, and one that needs
more training than a single head before it pays.

### Leaving out captures and checks: +75 (+184 vs. HCE)

Positions from which a capture is played, or in which the side to move is in check, are left out.
Leaving these positions out (about a quarter of all) changed nothing else and was one of the
largest single gains. It is now always on.

### Fixed-size epochs and a decaying learning rate: +49 (+233 vs. HCE)

Until here an epoch was one pass over the data, with a fixed learning rate, stopped when the loss on
held-back games stopped improving. We switched to a different procedure: epochs of a fixed size
(10 million positions, scaled to the size of our net) drawn from an endless stream of the data, the
RangerLite optimizer, and a learning rate that falls a little after every epoch - 800 epochs in all,
with no stopping rule.
