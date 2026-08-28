# Syzygy probing — reference

Extracted from Ronald de Man's own probing tool, <https://github.com/syzygy1/probetool>
(`regular/tbprobe.h`, `regular/tbprobe.c`, `regular/probetool.c`, `README.md`). This is the
authoritative description of what the stored values mean and when they may be used. Where this
file and any other note in this repository disagree, this file wins.

## Values

WDL, from the side to move:

| value | meaning |
| --- | --- |
| -2 | loss |
| -1 | loss, but draw under the 50 move rule (blessed loss) |
| 0 | draw |
| 1 | win, but draw under the 50 move rule (cursed win) |
| 2 | win |

DTZ, from the side to move, distance to the next zeroing move (capture or pawn move):

| value | meaning |
| --- | --- |
| n < -100 | loss, but draw under the 50 move rule |
| -100 ≤ n < -1 | loss in n ply, assuming the halfmove counter is 0 |
| 0 | draw |
| 1 < n ≤ 100 | win in n ply, assuming the halfmove counter is 0 |
| 100 < n | win, but draw under the 50 move rule |

A mate returns -1, not 0.

## WDL — when the stored entry is usable

**The stored entry is a lower bound, never an upper one.** The generator may store a value that is
too low where a capture already reaches the true value (it costs nothing and compresses better),
but never one that is too high. Therefore:

```
true value = max(stored entry, best value over all captures)
```

The entry is read in *every* position, including positions that have captures — see `probe_ab`
(`tbprobe.c:1411`), which is the whole rule in five lines:

```c
static int probe_ab(TB_Position *pos, int alpha, int beta, int *result)
{
  int num = TB_generate_captures(pos);
  for (int m = 0; m < num; m++) {          // captures only, recursively
    ...
    alpha = max(alpha, -probe_ab(pos, -beta, -alpha, result));
    if (alpha >= beta) return alpha;
  }
  int v = probe_wdl_table(pos, result);
  return max(alpha, v);                    // stored entry combined, not discarded
}
```

`probe_wdl` (`tbprobe.c:1433`) states it directly: *"Since there is no winning capture, a
non-capture might be the best move. Therefore we need to probe the table."* and *"Now
max(v, bestCap) is the WDL value of the position without ep rights."*

Consequences:

- Resolution walks **captures only** — including capture promotions and underpromotions. Quiet
  moves are never played out, and quiet promotions do not make the entry unusable.
- A capture reaching 2 (win) ends the resolution; no table read is needed
  (`ZEROING_IS_BEST`).
- The recursion terminates because every step removes a piece.

**En passant is the one real exception.** The tables do not model ep rights, so the entry is the
value of the position *without* them. `probe_ab` is never called on a position with an ep capture
(assert, `tbprobe.c:1410`); `probe_wdl` tracks ep captures separately in `bestEp` and:

- if `bestEp` beats both the other captures and the entry, it is the value;
- if the position without ep rights is stalemate and an ep capture exists, `bestEp` is the value,
  overriding the entry (which is then 0).

## DTZ — when the stored entry is usable

`TB_probe_dtz` (`tbprobe.c:1669`) needs the WDL value first, and probes the DTZ table only after
three earlier exits:

1. WDL is a draw → dtz = 0, no probe.
2. The WDL resolution ended in `ZEROING_IS_BEST` (a winning capture, or an ep capture as the only
   best move) → dtz = ±1 / ±101 from the WDL value, no probe.
3. If winning, every **quiet pawn move** is tried first; one that preserves the WDL value gives
   dtz = ±1 / ±101, no probe.

Only then is the DTZ table read — and only then is it safe, because the value is now known to
belong to the position without ep rights.

**A DTZ table stores only one side to move per material.** If the entry belongs to the other side,
the probe reports `CHANGE_STM` and answers nothing. The value is then found by a one ply search
over the **quiet non-pawn moves** (pawn moves are already accounted for), taking the best reply:
minimum `v + 1` when winning, minimum `v - 1` when losing, starting from ±1 / ±101. A reply with
`v == 1` that is check with no legal moves is mate and fixes the value at 1.

**The value can be off by one**: a returned ±n may mean n+1 ply. Exception: tables holding
positions exactly on the 50 move edge are exact.

**Usability against the halfmove counter** (`cnt50`), the part an engine must honour:

- `dtz > 0` is certainly a win if `dtz + cnt50 ≤ 99`, and moves must be picked so that this stays
  true.
- If `dtz == 100` immediately after a capture or pawn move, it is also certainly a win; until the
  next zeroing move the inequality to preserve is `dtz + cnt50 ≤ 100`.
- If a move with `dtz + cnt50 ≤ 99` exists, never accept one with `dtz + cnt50 == 100`.

## Root ranking

`root_probe_dtz` (`probetool.c:216`) is called on the root position first; if DTZ files are
missing, `root_probe_wdl` (`probetool.c:282`) is the fallback. Both rank every root move, none is
removed.

Per root move: play it, then

- **zeroing move** (capture or pawn move) → dtz from the child's WDL value alone, i.e. ±1 / ±101;
- **other move** → the child's DTZ, corrected by one ply away from zero;
- a mating move (dtz 2 or 3, check, no legal replies) is forced to dtz = 1.

Rank, with `rep` = a position repeated since the last zeroing move:

```c
int r =  dtz > 0 ? (dtz + cnt50 <= 99 && !rep ? 1000 : 1000 - (dtz + cnt50))
       : dtz < 0 ? (-dtz * 2 + cnt50 < 100 ? -1000 : -1000 + (-dtz + cnt50))
       : 0;
```

WDL fallback ranks are `{ -1000, -899, 0, 899, 1000 }` for loss … win.

Equally ranked moves are equivalent in outcome; the move played must come from the highest rank.
Rank 900 means `dtz + cnt50 == 100` — winning only if the off-by-one does not bite. The scores are
meant to override the search score in UCI output.

## de Man's recommended engine use

Always playing the DTZ optimal move produces unnatural play. His preferred approach: use the
tables only to **preselect the set of equally ranked moves**, then run a normal search **without
tablebase probing** over just those moves. Fall back to strictly DTZ lowering moves only when no
progress is being made — in particular after a repetition, where DTZ optimal play must continue
until the next zeroing move, which also prevents further repetitions.

MultiPV follows from the ranks: search the highest ranked group first, then the next group.
