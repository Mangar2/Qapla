# Ideas for 0.5.0

Collected ideas for the next release, one line each. Unordered and unfiltered: an entry
here is a candidate, not a decision. Each one that gets built follows the normal route -
tag, EPD run, SPRT - and gets its own entry in `version-log.md`.

1. Move ordering: countermove heuristic.
2. Move ordering: follow-up move heuristic.
3. Pruning before the move loop, on both bounds: score < alpha and score > beta.
4. Investigate the improving flag - is it handled correctly everywhere it is used?
5. Investigate move ordering and forward pruning in quiescence search.
6. Pawn storm.
7. Systematic fill-ins for the PST so they can be CLOP optimized.
8. Multi-threat bonus: reward positions with several distinct threats at once.
9. Optimize the pawn flank evaluation in the endgame.
10. Multi-CPU version, simple variant: threads sync through the transposition table only.
11. Lazy eval.
12. Lazy move generation: try the PV move first, without generating moves.
13. Investigate every enable/disable switch for search algorithms (e.g. null move) - is there anything that still needs an SPRT?
14. Probing with a shallow search.
15. Use qsearch instead of eval for pruning decisions.
16. History in the reductions and in the pruning, not only in the move ordering - the ButterflyBoard is read by the move provider and by `rootmoves` alone, `computeLMR` never sees it.
17. Capture pruning: drop losing captures at low remaining depth by their SEE value - SEE currently orders moves and prunes in quiescence, the main search does not use it.
18. Capture history for the move ordering: captures are sorted by SEE and MVV/LVA only, there is no learned component for them - the ButterflyBoard holds quiet moves.
19. Correction history: correct the static eval from the difference between eval and search result, indexed by pawn key, material or move continuation. `eval-correction.h` is a static table by material signature and is commented out, it is not this.
20. Reduce the root search depth on a re-search with a changed aspiration window, above all after a fail high (Stockfish does this) - the retry currently only widens the window by `awRetryFactor` and searches the same `searchDepth` again.
21. Pawn structure history for the move ordering: a history table indexed by the pawn hash key together with the move, so ordering knowledge carries over between positions that share a pawn structure. Belongs with 16 and 18.

## From the old Spike engine (`~/dev/spike`)

Features Spike has and Qapla does not. Two of them stay out on purpose: king attraction,
tried in Qapla and worth nothing, and Spike's static mate-in-one test before the null move -
a lot of code for little gain, it only survived in Spike because it was already written.

22. Null move mate threat: extend when the null move search returns a mate value against us (Spike: `mNMExt`).
23. Gate forward pruning on the king attack value: no futility while the own king is under attack above a threshold (Spike: `MaxFutilityKingThread`).
24. Trapped bishop - Qapla scores a trapped rook only.
25. Blocked d2/e2 pawn: penalty when the d or e pawn is blocked by an own piece (Spike: `cValBlockedPawn_D_E`).
26. Minor defended by a pawn as a general term - Qapla scores outposts, and only for knights.
27. Queen on the 7th rank - Qapla scores the rook on the 7th, not the queen (Spike: `cValMajors7thRank` covers both).
