# Qapla

Qapla chess engine

The current version 0.2 - the second pre-release version

## What is a chess engine?

A chess engine is a console application playing chess. It has no graphical user interface. Use a popular chess UI and include Qapla in it.

## Hardware requirement

Qapla is currently only available for 64 bit windows. Qapla uses hardware support for some strongly used routines in modern computer chess (popCount, bitScanForeward). They are supported by intel processors since more than 10 years. If you have older processors, use the OLD_HW version. It is about 20% less fast than the version using hardware and it has (whyever) double size.

## Version numbering

Qapla's version numbers have three digits:

- 3rd digit: I create a "patch" version number for every build I test for improvements. Thus I might create 5 new versions in a day and run a test with five different versions afterwards.
- 2cnd digit: Once I have enough features, elo's together, I will built a new minor version, currently I plan to have at least 50 elo difference between minor versions
- 1st digit: I will create the 1.0.X only if it is full featured (mutil-cpu, multi-pv, tablebases, ...)

## How do I test

A typical test is made with 60 seconds +0.01 second per move and 1400 games agains 14 engines of about equal playing strength. Current test set:

- Ruffian 1.05
- Glaurung 1.21
- Fruit 2.3.1
- Critter 0.52
- WildCat 8
- Aristarch 4.50
- Spike 0.9a
- Spike 1.2 Turin
- Midnight 5
- Amoeba 2.1
- Marvin 3.0
- Laser 1.0
- viridithas 2.7.0
- Leorik 2.3

A big thank you to the engine developers to have and keep their engines and also older versions freely available. These engines are selected because they play in average with similar playing strength and because the run very stable and supports short time controls. The current version is plays slightly above the average of these engines.

I use the "Spike GUI" that is not public available for the tests. It is able to play up to 10 games in parallel and has the ability to switch off all view windows to use very little CPU (about 1% of a single core when handling 10 games in parallel). Sadly, I do not provide the latest source code any more and I need tricks to install it on windows 10. It is developed with MFC ...

## What interfaces Qapla supports?

It supports UCI and Winboard, UCI is prefered. Winboard is currently mostly used for debugging on text mode. UCI supports more features than Winboard.

The following features are already supported in UCI

- Play games
- Analyze
- Pondering
- MultiPV (needs some improvements here :-) )
- Time per move, time per game, moves to go, fixed search depth

### Not yet supported in UCI

- Exact node count
- Exclude moves in search

### The following "features" are only supported in winboard

- perft
- divide
- generate (to generate bitbases, but you cannot use them yet)
- whatif (switched off by compiler flag in release builds). Whatif is a debugging feature to debug the search. It supports to print the variables in dedicated nodes of the search tree

Perft and divide has multi cpu support and accepts the cores N command for perft. It has been tested with 32 threads in parallel on a 16 core hyperthreading machine

### Pondering in Winboard

Pondering is only rudimentary supported in Winboard. Instead of continuing on ponder-hit, Qapla will stop the search and start a new search profiting from the transposition table. This is not the best way to do it, but it is the easiest way to implement it. I will improve it later.

## Playing strength

Qapla never played in "official tournaments". I only have some raiting according my own test with short time control. I expect it to have abount 2700 "CCRL-ELo" or 2500 "CEGT-Elo". CCRL and CEGT are very popular chess engine raiting lists.

## Improvements on Qapla 0.2 (to 0.1.1)

- Minor changes in eval, mostly rooks and queen on #7 rank
- Implemented late move reduction
- Implemented kind of butterfly history boards (but with piece/target square instead of source/target square)
- Singular extension (removed check extensions)
- IID
- Verify on Null moves
- More consistend aspiration window
- Better primary variant line due to always extend the last ply on PV node search, if a transposition table entry is found
- Refactoring many code lines left due to the first non-recursive approach
- Bug fixing

## What to expect from Qapla?

My current plan is

- Refactoring eval to calculate an index per piece and then retrieve the pice value from a table. This shall improve the ability to automatically improve eval weights
- Further improve search and implement more of the "classic" functionality that is still missing (futility pruning, razoring, ...)
- The new features (eval-refinement history, continuation history, ...) will be implemented later
- Try to generate nnue training data with my engine and implement a nnue network and train it myself.

## More info on the chess engine

The chess engine is based on a bitboard move generator, which generates only legal moves.

### Bitbases

I have build bitbase support, but it is not yet included

### Table bases

Not yet

### Multi-Threading

Not yet

### Search

Qapla has a classic recursive negamax search including the basic search features

- Move ordering (PV, good captures, killer1, killer2, non-captures, bad-captures)
- Null-window search on PV nodes after one best move has been found with verification search
- Iterative deepening with aspiration window
- Singular extension
- Tansposition table (fixed size: 32MB) - due to not yet implemented parameter to change it. Note changing the size of the TT has little effect on the playing strength
- Quiescense search with a check-evades for all plies
- Late move reduction with piece/position history
- Late move pruning
- Reverse futility pruning (thus futility pruning after making the move)

### Time management

Using longer time for critical situation

### Eval

Qapla already has several eval terms

- Piece Mobility, counting the amount of fields a piece may move to - (+ pass through - e.g. a rook passes through another rook or queen of the same color)
- Double Pawn penalty
- Isolated Pawn penalty
- Passe pawn bonus + connecte passed pawn bonus + protected passed pawn bonus (a non passed pawn protects a passed pawn)
- King security
- Threat count evaluation
- Piece square tables
- Knight outpost
- Rook trapped by king penalty
- Rook on half and full open files
- Rooks on 7th rank

### Endgame eval terms

As it does not support tablebases Qapla implements algorithms to play "good enough" in several clear endgame positions. This is the current list
To read the list one example: KQR*B*N*P*KB means a King with one Queen + zero or more Rooks + zero or more Bishops + zero or more Knights + zero or more pawns against King and Bishop. The idea is to have an algorithm for a King and Queen winning against King and Bishop and then use the same algorithm, if the Queen side has more material.

- KQKR
- KQR*B*N*P*KB (Queen vs. Bishop)
- KQR*B*N*P*KN (Queen vs. Knight)
- KQ+R*B*N*K (Queen to mate a King)
- KQP+KRP+ (Queen with pawns agains Rook with pawns)
- KR+B*N*K (Rook to mate a King)
- KBBK (Two bishops to mate a King or draw value, if the bishops are of the same color)
- KB+P+K
- KBNK
- KNP+K
- KP+KP+

Combination getting a winnign value bonus to encurage the engine to exchange material to get to those situations

- KQ+R*B*N*P+K
- KR+B*N*P+K
- KNNPK

Combinations getting a draw value

- KBK
- KN+K
- KK

and more already ...
