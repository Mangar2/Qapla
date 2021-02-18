# Qapla
Qapla chess engine

The current version 0.0.52 - near to a first pre-release. I expect to release the version 0.1 soon. I do not expect any change in playing strength to 0.1, only "features" like pondering.

## What is a chess engine?

A chess engine is a console application playing chess. It has no graphical user interface. Use a popular chess UI and include Qapla in it.

## What interfaces Qapla supports?

It supports Winboard and UCI, UCI is prefered.

## More info on the chess engine

The chess engine is based on a bitboard move generator, which generates only legal moves. 

### Table bases

Not yet, but a first version of own made (uncompressed) bitbases for KPK 
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