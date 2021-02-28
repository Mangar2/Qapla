# Qapla

Qapla chess engine

The current version 0.1.1 - a first pre-release.

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

A typical test is made with 20 seconds +1 second per move and 1200 games agains 12 engines of about equal playing strength. A minor version additionally gets test with the same 1200 games but a 5 minute clock setting.
The version 0.1 is tested against:

- Ruffian 1.05
- Abrok 5.0
- Nejmet 3.0.7
- Bumblebee 1.0
- Quark 2.35
- Spike 0.8
- Spike 0.9a
- EveAnn 1.71
- tjchess 1.3
- gaia 3.5
- Napoleon 1.8
- Qapla 0.0.14

A big thank you to the engine developers to have and keep their engines freely available. These engines are selected because they play with similar playing strength (with expection of Spike 0.9a and ruffian 1.05 which are significantly stronger) and because the run very stable and supports short time controls.

I use the "Spike GUI" that is not public available for the tests. It is able to play up to 10 games in parallel and has the ability to switch off all view windows to use very little CPU (about 1% of a single core when handling 10 games in parallel). Sadly, I do not provide the latest source code any more and I need tricks to install it on windows 10. It is developed with MFC ...

## What interfaces Qapla supports?

It supports UCI and Winboard, UCI is prefered. Winboard is currently mostly used for debugging on text mode. UCI supports more features than Winboard.

The following features are already supported in UCI

- Play games
- Analyze
- Pondering
- Time per move, time per game, moves to go, fixed search depth

Still missing

- Exact node count
- Multi-PV
- Exclude moves in search

The following "features" are only supported in winboard

- perft
- divide
- generate (to generate bitbases, but you cannot use them yet)
- whatif (switched off by compiler flag in release builds). Whatif is a debugging feature to debug the search. It supports to print the variables in dedicated nodes of the search tree

Perft and divide has multi cpu support and accepts the cores N command for perft. It has been tested with 32 threads in parallel on a 16 core hyperthreading machine

## Playing strength

Qapla never played in "official tournaments". I only have some raiting according my own test with short time control. I expect it to have abount 2400 "CCRL-ELo" or 2200 "CEGT-Elo". CCRL and CEGT are very popular chess engine raiting lists.

## What to expect from Qapla?

My current plan is

- Better pawn evaluation handling (I do not rate backward pawn and Qapla often gets a bad pawn structure)
- Better evaluation for passed pawns including the ability to push it
- Better king security - depending on the type of pieces attacking and defending the king
- Better end game especially pawn handling and king protecting pawns
- Try to implement NNUE based evaluation
- Improve implementation and integration of my own bitbases, maybe extend them to bitbase/tablebase

There are still many things to do in search - especially improve the search depth by late move reduction. I plan to do that only after having a suitable eval.

## More info on the chess engine

The chess engine is based on a bitboard move generator, which generates only legal moves.

### Table bases

Not yet, but a first version of own made (uncompressed) bitbases for KPK is already supported and included
It supports already generating bitbases, but they are not yet usable

### Search

Qapla has a classic recursive negamax search including the basic search features

- Move ordering (PV, good captures, killer1, killer2, non-captures, bad-captures)
- Null-window search on PV nodes after one best move has been found
- Iterative deepening with aspiration window
- Check extensions
- Tansposition table (fixed size: 32MB) - due to not yet implemented parameter to change it. Note changing the size of the TT has little effect on the playing strength
- Quiescense search with a check-evades for the first ply and a simple foreward pruning
- Conservative futility pruning

### Time management

Since 0.1.1, Qapla has an improved time management and will take lot more time in critical situations.

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
