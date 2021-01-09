# Quabla Change log
Quabla chess engine

## 0.0.2

- Winboard: 
	- Added "remove" command
- Bugfixes:

## 0.0.1 2021-01-09

Initial version. 

- Playing strength: about 1900 ELO (tested in a single 30s+1s blitz tournament against other weak rated engines) 
- Interfaces: Winboard
- Eval terms: Pawns (double, isolated, passed, connected-passed) and piece mobility 
- Endgame eval termns: KQKR, KQPsKRPs (King & Queen with pawns version King & Rook with pawns), KPsKPs, KBNK, KBBK, KBsPsK, KNPsK and a large range of piece combination where the engine will try for chace the opponent king to a border, a corner or a corner with a dedicated color. 
- Search terms: Alpha-Beta, Move ordering (PV, SEE, Killer, Capture, Re-Capture), Transposition Table, Quiescense with a simple foreward pruning, Chess evades for one ply in Quiescense.

This is a complete list, thus it does not have for example: Nullmove, Late-move-reduction or any term to look at King security.