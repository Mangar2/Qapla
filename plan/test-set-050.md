# New test set for Qapla 0.4.0-090 and later

## Why

In `qapla_040test090.qtour` Qapla 0.4.0-090 scores **63.7 %** overall (9552/15000).
Only Drofa 3.1.0 still reaches 45 % against it; the other 14 opponents are between
26 % and 44 %. A pool that far below the engine under test measures almost nothing:
most games are won by a margin the next change cannot widen.

## Selection method

1. Score of every opponent against Qapla 0.4.0-090, taken from the round data of
   `qapla_040test090.qtour` (1000 games per pairing).
2. CCRL 40/40 rating of every opponent (`rating_list_all.html`, 1 CPU entries).
   Qapla's own rating follows from opponent rating + Elo difference of the measured
   score, once per opponent:

   | opponent | CCRL 40/40 | Qapla score | implied Qapla |
   |---|---|---|---|
   | Amoeba 2.7 | 2922 | 62.3 % | 3009 |
   | Leorik 2.5 | 2922 | 63.1 % | 3015 |
   | Counter 3.5 | 2931 | 62.5 % | 3020 |
   | Viridithas 3.0.0 | 2908 | 65.6 % | 3020 |
   | Reckless 0.4.0 | 2926 | 63.3 % | 3021 |
   | Simbelmyne 1.7.0 | 2913 | 65.2 % | 3022 |
   | Vajolet2 2.3 | 2981 | 58.7 % | 3042 |
   | Drofa 3.1.0 | 3025 | 53.6 % | 3050 |
   | Spike 1.4.1 | 2943 (Blitz) | 65.0 % | 3051 |
   | Lynx 1.6.0 | 2956 | 63.5 % | 3052 |
   | Willow 2.9 | 2982 | 60.4 % | 3055 |
   | Marvin 3.4.0 | 2962 | 63.3 % | 3057 |
   | Winter 0.5 | 2913 | 70.0 % | 3060 |
   | Polaris 1.7.0 | 2955 | 64.8 % | 3061 |
   | Qapla 0.3.2 | 2887 | 73.8 % | 3067 |

   Mean **3040**, spread 3009–3067. Take Qapla 0.4.0-090 as CCRL 40/40 ≈ **3040 ± 30**.
3. Target band: opponents from 45 % (≈ 3005) to ~70 % (≈ 3190), evenly spread, so a
   gain of 10–20 Elo shows up somewhere in the middle of the list instead of at a
   saturated edge.
4. Prefer newer versions of engines already used: they are known to run here, are
   available for Linux, and their UCI behaviour is already tested.
5. One version per engine family — two versions of the same engine make
   correlated errors and do not count as two independent measurements.

## Proposal — 15 opponents

Expected score = expected score of the opponent against Qapla 0.4.0-090 at 3040.
One version per engine family: two versions of the same engine make correlated
errors and do not count as two independent measurements.

| # | engine | CCRL 40/40 | expected | Linux binary |
|---|---|---|---|---|
| 1 | Drofa 3.1.0 | 3025 | 48 % | have it (carry-over) |
| 2 | Amoeba 3.4 | 3042 | 50 % | `amoeba-3.4-linux.tar.xz` |
| 3 | Counter 3.8 | 3051 | 52 % | `counter-3.8-linux-amd64` |
| 4 | Simbelmyne 1.9.0 | 3060 | 53 % | `simbelmyne-v1.9.0-x86_64-v3` |
| 5 | Avalanche 1.3.0 | 3078 | 55 % | `Avalanche-x86_64-linux-1.3.0` |
| 6 | Minic 2.46 | 3086 | 57 % | `minic_2.46_linux_x64_skylake` |
| 7 | Lynx 1.7.0 | 3099 | 58 % | `Lynx-1.7.0-linux-x64.zip` |
| 8 | Viridithas 5.1.0 | 3116 | 61 % | build (Rust) |
| 9 | Princhess 0.17.0 | 3123 | 62 % | `princhess` |
| 10 | Carp 2.0.0 | 3131 | 63 % | `carp_v2.0-x86_64-unknown-linux-gnu` |
| 11 | Vajolet2 2.8 | 3143 | 64 % | `Vajolet2_2.8_bmi` |
| 12 | Marvin 5.0.0 | 3155 | 66 % | zip -> `linux/marvin_64bit_popcnt` |
| 13 | Tcheran 7.0 | 3171 | 68 % | `tcheran-v7.0-linux-x86_64-v3` |
| 14 | Nalwald 17.1 | 3191 | 70 % | `Nalwald-17.1-modern-linux.1-modern` |
| 15 | Willow 3.0 | 3210 | 73 % | `willow3_0-linux` |

Ratings step from 3025 to 3210 in roughly even intervals, so a gain of 10-20 Elo
shows up in the middle of the list instead of at a saturated edge.

Nine of the fifteen are newer versions of engines from the old set: Drofa, Amoeba,
Counter, Simbelmyne, Lynx, Viridithas, Vajolet2, Marvin, Willow. The other six are
new families; they were picked because the one-version-per-family rule leaves no
suitable version of a used engine in that rating slot.

Expected overall score of Qapla 0.4.0-090 against this set: **~40 %**.

## Notes

- 14 of 15 have a ready Linux binary in their GitHub release; only Viridithas 5.1.0
  has to be built (`cargo build --release`, Linux binaries exist only from v9 on).
  Ready-binary alternative for that slot: Igel 2.4.0 (3107) or Molybdenum 2.1 (3173),
  both slightly off the target rating.
- Willow 3.0 at 73 % is above the 70 % target. Willow has no version between
  2.9 (2982) and 3.0 (3210), the same gap exists for Leorik (2.5 = 2922 -> 3.0 = 3226)
  and Reckless (0.4.0 = 2926 -> 0.5.0 = 3211). Willow 3.0 is the top end of the list;
  drop it if 73 % turns out too one-sided.
- Drofa 3.1.0 is the only carry-over. It is not needed as a scale anchor - Qapla
  0.4.0-090 itself is in both lists and ties the two scales together - it is in
  because it is the one opponent of the old set that still reaches 45 %.
- Expected scores carry the +-30 Elo of the Qapla calibration, i.e. roughly
  +-4 percentage points. After the first full run, replace whatever fell below
  40 % or above 75 %.
- Keep the settings of the old run: 20.0+0.02, Hash 32, 1 thread, 1000 games per
  pairing (15000 games total).

## Dropped from the old set

Six of the fifteen opponents of `qapla_040test090.qtour` have no successor in the new
list, in no version. Score = their score against Qapla 0.4.0-090 in the old run.

| engine | CCRL 40/40 | score | why not |
|---|---|---|---|
| Qapla 0.3.2 | 2887 | 26 % | the predecessor, 150 Elo below. Qapla 0.4.0-090 is itself the reference now |
| Winter 0.5 | 2913 | 30 % | 0.7 (3035), 0.8 (3118), 0.9 (3170) and 1.0 (3202) all fit the band, but none of them has a Linux binary in its release - Linux assets start at 4.0 (3438). Every slot they would take is already filled by a ready binary |
| Leorik 2.5 | 2922 | 37 % | 2.5 is 120 Elo too weak, and the next version 3.0 jumps to 3226 (~76 %), past the top of the band. Nothing in between |
| Reckless 0.4.0 | 2926 | 37 % | same gap: 0.4.0 = 2926, next is 0.5.0 = 3211 (~73 %), which is Willow 3.0's slot - and Reckless 0.5.0 ships Windows binaries only |
| Spike 1.4.1 | 2943 (Blitz) | 35 % | closed source, 1.4.1 is the last release (2011). No newer version exists |
| Polaris 1.7.0 | 2955 | 35 % | 1.8.1 (3023) would fit, but that slot is the carry-over Drofa 3.1.0 (3025) and Polaris ships no Linux binary. After 1.8.1 the engine was renamed Stormphrax and starts at 3317 (~88 %) |
