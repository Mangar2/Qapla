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

## Measured, 2026-09-11: every three and four piece table

All 35 materials of three and four pieces, generated with

```
cd test/bitbase-all
{ for m in KQK KRK KBK KNK KPK KQQK KQRK KQBK KQNK KQPK KRRK KRBK KRNK KRPK KBBK \
           KBNK KBPK KNNK KNPK KPPK KQKQ KQKR KQKB KQKN KQKP KRKR KRKB KRKN KRKP \
           KBKB KBKN KBKP KNKN KNKP KPKP; do
    echo "bitgenerate $m cores 8 syzygy ../syzygy-all"; done; echo quit; } \
  | ../../build/Release/Qapla
```

and checked against the reference one material per process, eight at a time.

**All 35 `identical`, over 623652134 legal positions**, and the generator's own values agree with
the reference everywhere - nothing below the true value, nothing above it, nothing without a
value of its own. `tbcheck` accepts all 35 files; `tbstat` reports the same legal counts and
percentages for ours as for his.

Together the 35 files are 1758320 bytes against de Man's 1262704, a factor of 1.39.

## The writer checks itself

Every `bitsyzygy` run reads each written position back out of the file and holds it against the
value that went in. The entry may be lower - that is what the format allows where a capture
reaches the value - but never higher, and never different where nothing was allowed to lower it.

Two things that check found, and one it could not:

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
