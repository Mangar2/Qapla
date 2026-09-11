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

## Measured, 2026-09-11

| material | positions | our size | de Man |
|---|---|---|---|
| KRvK | 399112 | 208 | 208 |
| KQvK | 368452 | 336 | 272 |
| KPvK | 331352 | 9232 | 7824 |
| KRvKR | 21561456 | 15184 | 12944 |
| KQvKR | 19733336 | 40464 | 20496 |
| KQvKP | 16704944 | 72720 | 58064 |
| KBvKP | 18854368 | - | - |
| KPvKP | 14872176 | 321488 | 245328 |

Every one `identical`, and the generator's own values agree with the reference on all 92.8
million positions between them. `tbcheck` accepts all twenty files of a `KPKP` run.

The probe times in the table at the top were measured before the pawn tables existed; the policy
behind them - a cap on the symbols per block - is unchanged.

## The writer checks itself

Every `bitsyzygy` run reads each written position back out of the file and holds it against the
value that went in. The entry may be lower - that is what the format allows where a capture
reaches the value - but never higher, and never different where nothing was allowed to lower it.

That check is what found the symbol limit: 0xFFF is the leaf marker of the tree, so a vocabulary
of 4096 gives symbol 4095 a number that reads back as a terminal. Only tables large enough to
reach the cap were affected, which at the time meant the pawn tables alone.
