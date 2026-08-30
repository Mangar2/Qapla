# Quiescence ToDos — implementation and test protocol

Branch `release0.5`, starting point `70b0565` (`0.5.0-001-4-g70b0565`).

All six ToDo comments in [quiescencese.cpp](../search/quiescencese.cpp) are worked here, one
version tag and one SPRT per item, each against the version the item started from.

## Common test setup

- Build: `make Release -j`, binary `build/Release/Qapla`
- Machine: 32 cores, Linux
- EPD: `qet --settingsfile=test/epd/epd-wmtest-depth.ini` — node count proves the change is
  reached, nothing else (see memory note: it is no effort measure)
- SPRT: `test/sprt/sprt-standard.ini` — 5+0.01, concurrency 30, `book8ply.raw`, bounds
  H0 = -2, H1 = +3, alpha = beta = 0.05, maxgames 20000, unless an item asks for its own setup
- qet exit codes: 14 = H1, 15 = H0, 16 = undecided

## Baseline

| | value |
|---|---|
| binary | `/home/mangar/chess/qs-todo-versions/Qapla-base-0.5.0-001` |
| EPD nodes | 56158265 |
| EPD success | 22.00 % |
| EPD runtime | 0:03.154 |

## The six items

| # | place | what it asks |
|---|---|---|
| 1 | tt cutoff | guard the cutoff with `abs(ttValue) < MIN_MATE_VALUE` |
| 2 | after stand-pat | node level delta pruning, `standPat + queen + margin < alpha` |
| 3 | `computePruneForewardValue` | exempt only queen promotions from pruning, not all |
| 4 | `computePruneForewardValue` | drop the `doFutilityOnCapture` guard |
| 5 | move loop | beta side probe on the SEE value, margin tuned by CLOP |
| — | node entry | drop the two mate distance cutoffs |

Results follow below, one section per item, in the order they were run.
