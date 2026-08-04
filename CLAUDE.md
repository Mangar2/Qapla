# Qapla2 — Project Instructions

## Mandatory: node-count comparison run for behaviour-neutral engine changes

Any change to the engine that is **not supposed to change how it plays** (refactorings,
cleanups, new code behind a zero weight, renamed/moved functions, performance rewrites,
…) MUST be verified with a comparison EPD run **before and after** the change:

```powershell
c:\development\bin\qet.exe --settingsfile=test/epd/epd-wmtest-depth.ini --engine name="Qapla current" cmd=c:/development/qapla2/build/Release/Qapla.exe
```

Run it from the repository root (the paths in the ini are relative to it). Rebuild
(`make BUILD_TYPE=Release -j`) between the two runs.

**Do not set `rapid=true`** (neither in the ini nor on the command line). Rapid mode drops
all engine `info` lines to gain speed, and the node counts come from exactly those lines —
with rapid enabled the run reports no node counts and is useless for this purpose.

The report is written to
`test/epd/log/epd-report-<timestamp>.log`; the last two lines are the relevant ones:

```
Finished EPD test for engine: Qapla current, success rate: 22.00%, total nodes: 172461836
[Timer] Total runtime: elapsed = 0:08.224
```

Acceptance criteria:

- **total nodes must be identical** to the baseline run. Any difference means the search
  actually took a different path — the change is *not* behaviour-neutral and must be
  investigated, not waved through.
- **runtime must stay within 5%** of the baseline (`[Timer] Total runtime`). Identical node
  counts with a clearly slower run mean the change costs speed, which is a regression even
  though play is unchanged. Note that runtime is noisier than node counts — repeat the run
  before concluding that a difference is real.

Report both numbers (baseline vs. new) when presenting such a change.

When a change *is* meant to alter play, the node count is expected to differ — then the run
serves as proof that the new code is actually wired in and reached.

## Tunable search parameters

Define them at the call site, see `search/search-param.h`:

```cpp
node.setSE(param<SearchParameter::optimizeSE, "seMarginConst", 1, -100, 300>() + ...);
```

`param<OPTIMIZE, NAME, DEFAULT, MIN, MAX>()`: flag false → default as constexpr, no UCI
option; flag true → value read from a variable registered as UCI spin option before main.
Group flags like `optimizeSE` live in `search/searchparameter.h`, default false; set one to
true only for a tuning run. Node count must be identical for both flag states at defaults.

Do not add search parameters via the eval-style `UciParameterProvider` classes; those stay
for eval only.
