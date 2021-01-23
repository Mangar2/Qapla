# Quabla Change log
Quabla chess engine

## 0.0.14
- Playing strength:
- Evaluation values for passed pawns changed - based on 900 very fast game per setting


## 0.0.12
- UCI
	- Now sending thinking updates every second
- Playing strength unchanged: 2200 ELO
- King security more balanced
- Refactoring ...

## 0.0.6 2021-01-18 minor change
- Playing strengh: 2200 Elo 
- Eval
	- Added King security. Counts non defended attacks / double attacks around the king (3x4 fields, two to the north). +65 Elo
	- Added an "Endgame" factor - reducing the king attack value based on the amount of pieces on the field

## 0.0.5 2021-01-17 minor change
- Playing strenght: 2100 Elo
- UCI support added (not feature complete, but working)
- Refactoring of several code parts
- Eval
	- 2 percent slower, due to export of calculation of attack bitboards from mobility code (currently unused)
	- Counting attacked opponent pieces not protected by pawns as mobility field in eval-mobility. + 20 Elo
- Winboard
	- Perft and divide are now doing the right things
- Perft/Devide
	- Now respects the cores N information of winboard and uses up to 32 parallel threads to calculate perft. Note: the chess engine itself does not yet support multi threading
- Clock
	- Reduce calculating time to 50%, if clock is below 10 seconds

## 0.0.3 minor change

- Playing strenght: 2000 Elo
- Implemented Nullmove, R=3 (2 near tree-leaf), not in endgame withouth range pieces, not going directly to quiescence search. (+ 70-80 ELO)
- Winboard
	- Pong now sends a pong number too
	- setboard/new now works in analyze mode
- Arena
	- Move format now fits for arena
- Bugfixes
	- Positions already visited in the game are now rated (near) draw in the search
	- Hanging fixed when calling new or setboard in analyze mode

## 0.0.2

- Winboard: 
	- Added "remove" command
- Bugfixes:
	- transposition table (TT) usage fixed, we have more hits now. ( + 5 ELO)
	- Nps value corrected (slightly)
	- Setboard with illegal fen no longer leads to a crash
- Refactoring

## 0.0.1 2021-01-09

Initial version. 

- Playing strength: about 1900 ELO (tested in a single 30s+1s blitz tournament against other weak rated engines) 
- Interfaces: Winboard
- Eval terms: Pawns (double, isolated, passed, connected-passed) and piece mobility 
- Endgame eval termns: KQKR, KQPsKRPs (King & Queen with pawns version King & Rook with pawns), KPsKPs, KBNK, KBBK, KBsPsK, KNPsK and a large range of piece combination where the engine will try for chace the opponent king to a border, a corner or a corner with a dedicated color. 
- Search terms: Alpha-Beta, Move ordering (PV, SEE, Killer, Capture, Re-Capture), Transposition Table, Quiescense with a simple foreward pruning, Chess evades for one ply in Quiescense.

This is a complete list, thus it does not have for example: Nullmove, Late-move-reduction or any term to look at King security.

### Tournament result, 100 games per Engine 

Set of 50 start positions (Noomen.pgn, Ply 10, 20 sec.+1), 
results: (6% means Quabla 0.0.1 got 6% of the points)

- Aristarch 6%
- Embracer1.12 78%
- Matheus2.3 38%
- Janwillem1.13 49%
- Qabla 018 34% (Same engine before refactoring)
- Nejmet 3.07 11%

