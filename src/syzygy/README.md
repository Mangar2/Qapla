# Syzygy format layer

Reads Syzygy endgame tablebase files. Nothing here knows about the engine: the caller fills a
`TbPosition` — a list of at most seven pieces with their squares — and gets the stored table entry
back. `tbprobe.h` is the complete interface.

## Origin

Ported from `src/syzygy/tbprobe.cpp` of **Stockfish 17**, which is itself an adaptation of Ronald
de Man's probing code. Stockfish is GPL-3.0; Qapla is AGPL-3.0 with GPL-3.0-or-later file headers,
and GPLv3 §13 explicitly permits combining GPLv3 code with an AGPLv3 work.

## What was taken

Only the format: table discovery and file mapping (`TBFile`), the table descriptors and their setup
(`TBTable`, `TBTables`, `setTable`, `setGroups`, `setSizes`, `setDtzMap`), the index computation
(`doProbeTable`) and the decompressor (`decompressPairs`), plus the constant maps built in
`initMaps`.

## What was left behind

Everything that makes or unmakes a move, because that is engine code:

- the probe that resolves captures to correct a win/draw/loss answer
- the distance probe's zeroing detection and its one-ply search when the entry answers for the
  other side to move
- the root move ranking

`probeWdlEntry` and `probeDtzEntry` therefore return the **stored entry, uncorrected**. A
win/draw/loss entry is only the true value of the position when the side to move has no capture
available — where one is, the generator stores a "don't care" value. A distance entry can come back
with `Status::OtherSideToMove`. Both corrections belong to the caller.

## Deliberate changes against upstream

Re-apply these when taking an upstream fix.

| change | why |
| --- | --- |
| position class replaced by `TbPosition` | keeps the engine out of this directory; twelve call sites |
| table discovery reads each directory once instead of trying to open every possible name | upstream opens one file per material combination, which is tens of thousands of misses per engine start. It cost about 50 ms every time `SyzygyPath` was set - invisible in any node count, but 20000 SPRT games pay it 20000 times |
| own material key, four bits per colour and piece type | the upstream key is a Zobrist key from the engine's position class |
| key mixed before use as a hash bucket | the new key layout is dense, the upstream one was not |
| plain `int` squares and local square helpers | avoids the engine's types |
| `std::popcount` / `std::countr_zero` | avoids the engine's bit helpers |
| corrupt or unmappable files are reported and treated as absent | upstream calls `exit()`, which an engine must not do |
| `File` / `Rank` / `Piece` enums replaced by `int` | same reason |
| internal piece encoding kept as upstream (type in bits 0-2, colour in bit 3) | the table files store it that way, so it is not free to choose |

## Test

`tbprobe-test.cpp` links against `tbprobe.cpp` alone — no board, no move generator, no search. If
it builds and runs, the layer really is free of the engine. Its `main` sits behind
`QAPLA_SYZYGY_TESTMAIN` so that the engine build, which globs every `.cpp`, does not end up with
two of them.

```
clang++ -std=c++20 -O2 -DQAPLA_SYZYGY_TESTMAIN \
    src/syzygy/tbprobe.cpp src/syzygy/tbprobe-test.cpp -o build/tbtest.exe
build/tbtest.exe C:/Chess/syzygy/tables
```

The two king-and-pawn cases are ground truth: Qapla's own compiled-in KPK bitbase answers draw for
the first and win for the second. Every case is additionally probed colour-mirrored and rank-flipped,
which has to return the identical value because the answer is relative to the side to move.

## Engine side

`tbposition-builder.h` fills `TbPosition` from a `MoveGenerator`. It is the only file in this
directory that includes an engine header, and the only one that may.
