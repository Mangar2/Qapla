# Training Qapla's NNUE

This is how Qapla's neural network evaluation came about: what we did, in which order, and what each
step was worth in playing strength. It describes principles and results, not every setting.

**One rule for the whole project: no outside knowledge.** Every training position was played by
Qapla, and every label comes from Qapla's own search. No evaluation of another engine went into the
net. The net is meant to grow out of Qapla's hand-crafted evaluation (HCE), pulling itself up by its
own bootstraps.

**How strength is measured.** Whether a step is better is decided by a sequential test (SPRT) of the new
net against the previous one, differing in that step alone. How much it is worth is measured in a
reference field: tournaments at 10+0.05 seconds per game from a common opening book, 10,000 games per net
against a fixed reference. Those figures are typically accurate to within about ±6 to ±7 Elo per net;
differences smaller than that are not claimed. Each heading gives what a step gained over the previous one
and, in brackets, where it stands against the HCE - all from one reference table (October 18, 2026).

## The net

A standard NNUE architecture, kept small:

- **Input:** HalfKA features - every piece on its square, seen from the king of each side,
  45,056 features per perspective.
- **Accumulator:** 256 neurons per perspective, updated incrementally as moves are made and unmade; the
  two perspectives are concatenated.
- **Head:** 512 → 32 → 32 → 1 with clipped ReLU, in eight copies ("layer stacks"); the number of pieces
  on the board chooses which one evaluates a position.
- **Quantized** to 16-bit accumulator weights and 8-bit dense weights for fast integer inference.

A wider net (512 accumulator neurons, 1024 → 16 → 32 → 1) was tried and played no better at the same
amount of training, so the cheaper one stayed.

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

Together **9 million games and about 1 billion labelled positions**.

### A larger book, one game per leaf

The second generation of data uses the same idea at a larger scale. The book was grown from one million
to **ten million leaves**, and from every leaf exactly one game was played, HCE against HCE, at **depth 9**.
The evaluations of the play itself are the labels - no separate labelling pass - and the results of the
games are kept. About 18 opening moves of each game come from the book and were never searched in the
games; so the book itself was evaluated once, every position at depth 9, and is trained as a set of its
own, **each book position exactly once**, however often it was played.

For each book position the book also counts the wins, draws and losses of the games through it. Where
many games went through a position, that score is a better estimate of its value than the search: its
value is mixed with the score, with a weight that grows with the number of games (20 % for one game, 50 %
for four, close to 100 % for many). An early lost rook, which these games lose practically always, thus
counts like a lost queen - the meaning sits in the position, not in the piece.

A book of 100 million leaves is being evaluated now; split into ten books that share the opening moves
alike, it is the start of ten further sets that can be played independently of each other.

## The main line

### Trained on one set, 139 million positions (−132 vs. HCE)

The first real net, trained on set 1 alone, was still clearly weaker than the hand-crafted evaluation
it learned from.

### More and more diverse data: +223 (+91 vs. HCE)

Trained on sets 1 to 4 the net passed the HCE. The games of the weak nets mattered: a set played by a
weak net against itself made the net learn faster than the HCE's own games.

### Eight heads and a newer trainer: +17 (+108 vs. HCE)

One head per range of piece counts, sharing the same accumulator, measured together with a change of
the trainer. Trained identically under the final procedure, one head and eight play equally strong.

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

### More training per epoch: +24 (+257 vs. HCE)

With the learning rate falling after every epoch, the size of an epoch decides how many positions the
net sees while it still learns fast. Epochs of 20 million positions instead of 10 gave a clear gain, and 40
million gave a little more again. (The wider net of the earlier version of this page owed its lead to the
doubled training, not to its size.)

### The new games: +20 (+277 vs. HCE)

The old six sets together with the ten million new games, at half the training per epoch of the step
before. The new set alone already beat the old six sets.

### The result of the game, a fifth of the target: +53 (+330 vs. HCE)

The largest step since leaving out captures. The label of a position becomes 80 % its search value and
20 % the result of its game - the result knows what a depth-9 search does not. The share was searched from
5 % to 40 %: each step up to 20-25 % won clearly, 40 % lost. In these games a small advantage is converted
very reliably - positions the search gives 65 % end up scoring 88 % - and the result carries exactly that.

An early test on the old set 1 had found the opposite (the result cost about 80 Elo); it used an older
trainer, a far shorter training and other games, and did not hold up here.

### Old sets, book and new games together: +9 (+339 vs. HCE)

The new games with their results, the book once per position, and the six old sets without results - each
of these confirmed separately to help.

### The results of the old games between equal players: +8 (+347 vs. HCE)

Sets 1 to 3, where both sides were the same player, with their results as well. The results of sets 4 to 6,
games between unequal players, made the net clearly weaker - there the result says more about the players
than about the position.

### Every winning choice, with 40 million positions an epoch: +17 (+364 vs. HCE)

One net from all the choices that won: the new games with their results, the book once per position with
the score of its games mixed in, sets 1 to 3 with results and 4 to 6 without, a fifth of the target from
the result, and epochs of 40 million positions. It is the net Qapla 2.0 carries inside the engine.

## Against other engines

In a tournament against other engines the version with the final net gained about **315 Elo over the HCE
version, not the 364** our reference field measures - about 15 % less. The figures on this page are
therefore not transferable one to one to play against other engines.

The tournament: a gauntlet against the 16 engines of the Qapla 0.5 test set (Amoeba 3.4, Avalanche 1.3.0,
Carp 2.0.0, Counter 3.8, Drofa 3.1.0, Lynx 1.7.0, Marvin 5.0.0, Minic 2.46, Nalwald 17.1, Princhess 0.17.0,
Simbelmyne 1.9.0, Tcheran 7.0, Vajolet2 2.8, Viridithas 5.1.0, Willow 3.0 and Qapla 0.4.0-091), 1,000 games
per opponent at 20+0.1 seconds, one thread each, from an 8-ply book:

| | score | performance |
|---|---|---|
| Qapla 0.5.0-001, the HCE release | 37.7 % (16,000 games) | −87 |
| Qapla 2.0.0-001, the final net | 77.4 % (16,000 games) | +213 |

Over all 16 opponents the net version is **about 318 Elo stronger**. Two possible causes for the smaller gap:

- **A gap of more than 300 Elo is hard to measure exactly.** At scores around 80 % a few games more or less
  move the figure a lot, and our reference field measures every net against one far weaker opponent.
- **We optimise against ourselves.** Every step was decided by games between Qapla versions; what wins
  there does not have to win as much against engines that play differently.

## Tried, and no better

Each against the best net of its day, differing in that point only:

- **A wider label search** (less pruning, about three times the nodes per label): no difference once the
  training was the final one.
- **A wider net** (512 instead of 256 accumulator neurons): no difference at the same training.
- **A piece-square part per bucket**, as Stockfish has it: no difference.
- **Fewer early positions**, as Stockfish skips them: no difference.
- **One head instead of eight**: no difference.
- **First all data, then the new games only** (a fine tuning on the better set): no gain.
- **The book values written into every game** instead of once per position: no gain.
