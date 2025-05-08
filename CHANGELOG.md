# Qapla Change log
Qapla chess engine

## 0.3.0 2025-05-06

- Playing strength: + 100 ELO to 0.2.57 (about 2850 - 2900 ELO on CCRL)
- Github repository renamed to "Qapla"
- Features
    - UCI supports node count on "go" command. It will be "nearly" reached, not exactly. It is additional to other limits so you might limit depth, time and nodes.
	- UCI now supports searchmoves to search only a given set of moves
	- Trial feature:
		- Support of Qapla Bitbases (trial version). Should you use it? No!! But you may, if you want. It is tested for 5 picees but 6 pieces should work too. It has two design flaws that needs to be changed: Only WD instead of WDL (Win Draw Loss). So you need as example KRPKR and KRKRP bitbase to find out, if side to move is winning, play draw or loose. Additionally 50 moves draw is currently ignored.
	- Developer feature:
		- Support of Qapla Bitbases (trial version). Should you use it? No!! But you may, if you want. It is tested for 5 picees but 6 pieces should work too. It has two design flaws that needs to be changed: Only WD instead of WDL (Win Draw Loss). So you need as example KRPKR and KRKRP bitbase to find out, if side to move is winning, play draw or loose. Additionally 50 moves draw is currently ignored.- Some statistic features I use to improve the engine (you will see lots of code for it, but it is just for me :-). 
		- Ability to generate games in a dense file format for future NNUE training. 
		- Ability to evaluate fen positions from a file - I use this to check, if a new build did not change the behaviour of the engine.
		- Ability to run statistics agains itself to optimize eval parameters
- Fixes
	- Memory usage now really shrinks after setting smaller tt size
	- Fixed that last move in pv was sometimes not a legal move due to uninitialized variable
	- Fixed that fail low situation now always leads to longer time usage
	- Pawn hash and transposition tables are now only cleared on new game
	- Code improvements - many warnings fixed, many to go
- Eval
	- Many changes in eval weights
	- Added term for pinned pieces
	- Added term for overloaded attacks
	- Added differentiation of pawn evaluation in mid- adn endgame
	- Added term to attract King to pawns in endgame
	- Added few more endgame evaluations
	- Added linked bitbase for KPK
	- Added eval reduction to handle 50 move rule after 20 "silent" halfmoves
- Search
	- Added 50 move rule cutoff 
	- Changed foreward futility pruning return value to be more near to current eval
- Outlook
    - Working on playing strength with "classic" (< 2010) features until gaining Spike 1.4 strength
	- Then adding new search ideas from the past 15 years (like move continuation history) that are not in spike
	- Then generating test games and implement NNUE

## 0.2.57 2025-02-09

- Playing strength: About 2700 ELO
- Eval
	- Minor changes in eval, mostly rooks and queen on #7 rank
- Search 
	- Exclude moves in search
	- Exact node count
	- Implemented late move reduction
	- Implemented kind of butterfly history boards (but with piece/target square instead of source/target square)
	- Singular extension
	- IID
	- Verify on Null moves
	- More consistend aspiration window
	- Better primary variant line due to always extend the last ply on PV node search, if a transposition table entry is found
	- Refactoring many code lines left due to the first non-recursive approach
	- Bug fixing- Implemented late move reduction
	- Implemented kind of butterfly history boards (but with piece/target square instead of source/target square)
	- Singular extension + check extension
	- IID
	- Verify on Null moves
	- More consistend aspiration window
	- Better primary variant line due to always extend the last ply on PV node search, if a transposition table entry is found
	- Refactoring many code lines left due to the first non-recursive approach
- UCI
	- MultiPV
	- Pondering bug fixed
- XBoard
	- Simple pondering for XBoard (using tt to remember pondering result)
- Other 
	- Thread caching for computation thread (no longer creating a new thread whenever a new move is played)
	- Nore consistently preventing loss on time for very short time controls
- Supported OS
	- Windows version MSVC
	- Linux version (clang++ compiled) availabe. The linux version is about 10% faster than the windows version

## 0.1.1 2021-02-27

- Playing strength: about 2400 ELO (no significant improve compared to 0.0.52)
- Implemented a compiler option to support older hardware (without pop-count)
- UCI
	- Implementing pondering
	- Implemented "Lower bound"/"Upper bound" information to info strings
- Search
	- Improved time management (use more time on drop/fail low situation)
	- Implemented a pawn hash (no speed improvement measured, still kept it for later use with more pawn eval terms)

## 0.0.52 2021-02-18

- Playing strenght: about 2400 ELO
- Bypassed Spike 0.8 playing strength
- Bugfixing
	- Fixed bugs where the transition table kept draw values for replaying moves from earlier games or after undo moves
	- Bugfix in uci: BINC (increase for black) is no longer ignored
	- Bugfix Crash, if search depths > 128 plys
- Search
	- Using hash entries even in PV, if the value > winning bonus. This fixed problems with finding mates
	- Adding root moves with a stable bubblesort moving all historic PV moves to the front. Prepared for multi-pv search and for excluded moves, still this is not yet implemented.
	- Rearranged some code parts. 	
- Eval
	- Added some drawish endgame evaluation terms like KRBKR + 5 ELO (maybe)
	- Every term will have separate values for mid- and endgame. Added a class to handle this.
	- Added rook evaluation terms: trapped rook, rook on open and half open file: + 10 ELO
	- Changed pawn value to less especially in midgame: + 10 ELO
	- Added rook evaluation term for protecting passed pawns from behind + 15 ELO 
	- Added bonus for double bishop on different colors, changed mobility values for bishops with low mobility
	- Added evaluation terms for knights like outpost
	- Added bitbases - but yet only activated for KPK
	- Added dedicated evaluation for KPsK
	- Added threat count evaluation (hanging pieces, pieces attacked by pawns) + 10 ELO
	- Added piece square tables + 30 ELO
	- Added futility pruning (conservative) + 25 ELO

## 0.0.14 2021-01-23

- Playing strength: about 2250 ELO
	- Improved parameters for King security (+10 elo)
	- Improved parameters for passed pawns  (+30 elo) ; Identified new parameters for passed pawns by playing very fast games (5s + 0.1ms)

## 0.0.12 2021-01-21

- UCI
	- Now sending thinking updates every second
- Playing strength unchanged: 2200 ELO
- King security more balanced
- Refactoring ...

## 0.0.6 2021-01-18

- Playing strengh: 2200 Elo 
- Eval
	- Added King security. Counts non defended attacks / double attacks around the king (3x4 fields, two to the north). +65 Elo
	- Added an "Endgame" factor - reducing the king attack value based on the amount of pieces on the field

## 0.0.5 2021-01-17

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

## 0.0.3 2021-01-12

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

## 0.0.2 2021-01-10

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

