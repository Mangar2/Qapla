# The Syzygy writer test suite

Four pawnless materials, chosen because between them they cover every shape the pawnless
index has: a unique piece and none, three pieces in the leading group and the king pair
alone, two sides stored and one.

Everything runs from the repository root. The reference set is a full Syzygy installation;
here it sits in `/home/mangar/dev/syzygy/tables`, and every command below takes its path as
an argument.

## 1. Generate the bitbases

```
cd test/bitbase
printf 'bitgenerate KRK cores 8\nquit\n'  | ../../build/Release/Qapla
printf 'bitgenerate KQK cores 8\nquit\n'  | ../../build/Release/Qapla
printf 'bitgenerate KRRK cores 8\nquit\n' | ../../build/Release/Qapla
printf 'bitgenerate KRKR cores 8\nquit\n' | ../../build/Release/Qapla
```

## 2. Write the Syzygy files

The generator writes them itself, for every material of the dependency tree:

```
cd test/bitbase
printf 'bitgenerate KPKP cores 8 syzygy ../syzygy\nquit\n' | ../../build/Release/Qapla
```

`KPKP` alone produces twenty tables that way. The order is what makes it work: whether an
entry may be stored below its true value is decided by probing the tables one capture down,
and in the recursion those exist by the time the parent is written.

Writing one table on its own, from an existing `.qwdl`:

```
printf 'bitsyzygy KRK qwdl test/bitbase/KRK.qwdl out test/syzygy\nquit\n' | ./build/Release/Qapla
```

Materials one capture down have to be written first here: a missing one costs size, silently.

Each run reports the slot coverage and verifies the check bytes it wrote.

## 3. Compare against the reference

```
printf 'bitsyzygycheck KRK test/syzygy <reference> qwdl test/bitbase/KRK.qwdl\nquit\n' | ./build/Release/Qapla
```

Walks **every** legal position of the material, resolves the captures on both sides - a stored
entry is a lower bound wherever a capture reaches the true value - and compares the sign, so
that a cursed win counts as a win. The generator does not know the fifty move rule yet.

`qwdl` is optional and only makes a difference when something differs: the generator's own value
is then printed next to the two answers, which says on which side of the bridge the fault sits.

**Expected: `identical`** - with one exception, see below.

With a `.qwdl` given, the check also compares the generator's own value against the reference
for every position and reports it in three parts. Below the true value is what the format
allows: the reader takes the better of the entry and the captures, so an entry may sit low
wherever a capture reaches the value. Above it is an error under any reading. The third part
counts positions whose index does not round-trip through the reverse index - the generator
marks those illegal and the compressor fills them with a neighbour, so there is nothing there
to compare.

### What the check reports

Three parts, and they mean different things. Below the true value is what the format allows:
the reader takes the better of the entry and the captures, so an entry may sit low wherever a
capture reaches the value. Above it is an error under any reading. The third part counts
positions whose index does not round-trip through the reverse index - the generator marks those
illegal and the compressor fills them with a neighbour, so there is nothing there to compare.

All three are zero for every material below.

## 4. Probe speed

```
printf 'bitsyzygyspeed KPKP test/syzygy-all <reference> positions 1000000\nquit\n' \
  | ./build/Release/Qapla
```

A million random legal positions of the material, probed from our file and from the reference
one, through the same reader. **Expected: `not slower`**, which the test reads as at most 1.05
times the reference time. Ten materials from KQvK to KPvKP measured between 0.65 and 0.99 of it.

The test exists because the size of a file and the time to probe it pull against each other, and
three of the four things below trade one for the other.

## Measured: every three and four piece table

All 35 materials of three and four pieces. The thirty of four pieces are enough to ask for,
the recursion writes the five of three on the way:

```
{ for m in KBBK KBNK KBPK KBKB KBKN KBKP KNNK KNPK KNKN KNKP KPPK KPKP KQBK KQNK KQPK \
           KQQK KQRK KQKB KQKN KQKP KQKQ KQKR KRBK KRNK KRPK KRRK KRKB KRKN KRKP KRKR; do
    echo "bitgenerate $m cores 8 syzygy test/syzygy-all"; done; echo quit; } \
  | ./build/Release/Qapla
```

**2026-09-12: all 35 `identical`, over 623652134 legal positions.** `tbcheck` accepts all 35
files.

**2026-09-11**, same result, and with the `.qwdl` of each material given as well: the generator's
own values agree with the reference everywhere - nothing below the true value, nothing above it,
nothing without a value of its own. `tbstat` reports the same legal counts and percentages for
ours as for his.

Together the 35 files are 1278896 bytes against de Man's 1262704, a factor of 1.01. They were
1758320 bytes before the four things below were found, a factor of 1.39.

## What the size was in

None of it was the compression itself. The format leaves parameters open, writes them into the
file and lets the reader follow - and all four were left at a fixed value here:

- **The group layout.** Which pieces form the leading group and where each group sits in the
  multiplication chain decide the order the values lie in, and the compression lives on that
  order. Between the best and the worst layout of KQvKN lie 11024 and 34000 bytes. The writer
  now ranks all layouts on a sample of the index and writes the best three out in full.
- **The span of the sparse index.** It was capped at 2^15, de Man goes to 2^20. On KBvKN that
  cost 700 of 3024 bytes - the compressed data was already smaller than his, the index ate it.
- **Terminals per block.** 32768 where the format allows 65536, so twice the blocks and twice
  the length table. The cap was there because the offset of a sparse index entry past the end of
  the table would not fit in its sixteen bits; the format solves that with blocks that do not
  exist, whose lengths are written and whose data is not. They are written now.
- **Block size against probe time.** A probe decodes its way from the start of a block, so the
  block size is what it costs. Chosen on size alone, 64 byte blocks win by a few percent - and
  KBvKP came out at 1.11 times de Man's probe time. The choice is now bounded by the symbols a
  block holds on average, which costs about three percent of the file and gives back a quarter
  of the time.

## The writer checks itself

Every `bitsyzygy` run reads each written position back out of the file and holds it against the
value that went in. The entry may be lower - that is what the format allows where a capture
reaches the value - but never higher, and never different where nothing was allowed to lower it.

Two things that check found, and two it could not:

- **The symbol limit.** 0xFFF is the leaf marker of the tree, so a vocabulary of 4096 gives
  symbol 4095 a number that reads back as a terminal. Only tables large enough to reach the cap
  were affected, which at the time meant the pawn tables alone.
- **Nothing about KPPvK** - and that is the instructive one. The self check walks the same index
  space the writer walks, so it cannot see a class the writer never visits. Qapla's index folds a
  pawn position and its file mirror into one class; the format mirrors by the file of the leading
  pawn, so a pawn set that is symmetric itself - a2 and h2 - leaves both images on the same file
  and in different slots. One of the two was never written. The comparison against the reference
  found it because it walks positions, not indices. A check that shares the assumptions of what
  it checks does not check those assumptions.
- **Nothing about the diagonal either**, and it is the same lesson a second time. Without pawns
  the format settles file and rank from its leading group but cannot settle the diagonal when
  that group stands on it - and the layout search made leading groups possible that do. Qapla's
  index folds the class, the format keeps two slots, one of them stayed empty and read back as
  its neighbour. 72 positions of KQvKB and 16 of KRvKB, again found by the comparison.

  The value is now written to the slot of the mirror image as well: the diagonal one without
  pawns, the file one with them. Which two those are is not a guess - the format normalises file
  and rank by itself, so only the diagonal can be left open, and with pawns only the file.
