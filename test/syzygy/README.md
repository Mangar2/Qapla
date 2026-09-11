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

```
printf 'bitsyzygy KRK qwdl test/bitbase/KRK.qwdl out test/syzygy\nquit\n' | ./build/Release/Qapla
```

and the same for `KQK`, `KRRK`, `KRKR`. Materials one capture down have to be written first:
whether an entry may be stored below its true value is decided by probing them, and a missing
one costs size, silently.

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

**Expected: `identical` for all four.**

## 4. Measure the probe speed

```
printf 'bitsyzygyspeed KRK test/syzygy <reference> positions 1000000\nquit\n' | ./build/Release/Qapla
```

A million random legal positions from a fixed seed, the same ones in the same order for both
sets, three runs each of which the fastest counts. What is measured is the stored entry alone -
the capture resolution above it is engine work and would add the same constant to both sides.

The reference is measured in the same run rather than written down, because only the ratio is a
property of the files; the absolute numbers below belong to one machine and one day.

**Expected: `not slower`.** The test fails above 1.05 times the reference.

The entry sums printed per side are not a cross check and do not have to match: a stored entry
is a lower bound, so the two sets legitimately differ wherever a capture already reaches the
value. Step 3 is what compares the answers.

## Measured, 2026-09-11

| material | our size | de Man | probe, ours | probe, reference | ratio |
|---|---|---|---|---|---|
| KRvK | 208 | 208 | 128.0 ns | 160.3 ns | 0.80 |
| KQvK | 336 | 272 | 176.0 ns | 201.2 ns | 0.87 |
| KRRvK | 3152 | 1936 | 213.1 ns | 231.1 ns | 0.92 |
| KRvKR | 15184 | 12944 | 450.2 ns | 453.7 ns | 0.99 |

The probe cost is dominated by the walk from the start of a block to the wanted entry, so it
follows the symbols per block - which is capped at 128 for exactly this reason. Buying a few
bytes with a larger block was measured at 1.25 and 1.53 times the reference and is not worth
it.

The remaining size difference is the grammar. At equal sequence length - 4344 symbols against
de Man's 4437 for `KRRvK` - his code costs 2.67 bits per symbol and ours 4.48, because his
tail sits on 74 symbols and ours on 306. Neither a wider search over the vocabulary, nor
dissolving the rare rules, nor weighting the rule choice by the terminals it covers changed
that; see `plan/syzygy-writer.md`.
