# Runs and their goals

Every run that is going, paused or planned, with the goal it serves. The goal decides the follow-up, see
`.claude/skills/run-supervision/SKILL.md`. Updated when a run starts, ends or changes its goal.

Overall goal: Qapla's own net, at the level of the Stockfish net in Qapla - about 500-600 Elo over the
hand written evaluation.

## Going

### Mac: Stockfish architecture (SF17) trained on sets 1-6, HCE labels, to epoch 10
Goal: find out whether it is our training data that must improve. If SF's architecture and procedure
on our data stay far below the Stockfish net, the data is the lever; if it comes close, the training is.
Next: export e10, play it against HCE in the tournament of e1 (2000 games, 10+0.05).

### Linux and Windows: set 1 labelled with Qapla's search and Stockfish's evaluation (chunks 51-101 / 1-50)
Goal: the Stockfish-eval test for set 1 - does a better label of the same positions make the net
clearly stronger? (Order of the levers, 2026-10-01: capacity, better labels, new games.) The 26-chunk
pre-test accepted H1 at bounds 40/50. Afterwards, idea 3 (low-selectivity HCE labels, paused below).
Next: join both halves, convert to .gam, train set 1 exactly like the old set-1 net and play it
against the old set-1 net (nowdl, HCE labels).

## Paused

### Linux: tournament HCE against the set1234 net, at round 44
Goal: a reference figure of our best single-head net against HCE.

### Mac: set 1 relabelled by a low-selectivity HCE (factor 3 nodes), 14 of 101 chunks
Goal: do labels from a deeper, less pruned HCE search alone make the net stronger?
