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

## Trying out a new version (SPRT)

Any change meant to make the engine stronger is decided by an SPRT run, never by the EPD
success rate.

1. Build HEAD *before* the change and copy the binary somewhere outside the repo — that is
   the baseline.
2. Apply the change, commit, tag it with the next version number (`0.4.0-027` → `0.4.0-028`,
   annotated). Every tested version gets its own tag.
3. Rebuild *after* tagging: the Makefile takes `QAPLA_VERSION` from `git describe --tags`,
   so the engine reports `Qapla 0.4.0-028` and the tag is visible in the test output. A
   build made before the tag shows `<tag>-<n>-g<hash>` instead — no code edit needed, only
   the right order. The version is a compile flag, not a make dependency: after tagging use
   `make BUILD_TYPE=Release clean` first, otherwise the old string stays in the binary.
   Verify with `printf 'uci\nquit\n' | ./build/Release/Qapla.exe | grep "^id name"`.
4. Run the EPD test once: the node count must differ, otherwise the change is not active.
5. Run (from the repo root, own state file per experiment):

```powershell
c:\development\bin\qet.exe --settingsfile=test/sprt/sprt-standard.ini --engine name=Qapla-baseline cmd=<baseline.exe> --engine name=Qapla-<change> cmd=c:/development/qapla2/build/Release/Qapla.exe gauntlet=true --sprt file=test/log/sprt-<change>.state
```

Command line parameters override the ini. Engine names must not contain spaces. Change
`test/sprt/sprt-standard.ini` only if qet reports an error in it.

The `--sprt file=` state file holds the tournament state. The run may be stopped at any
time and continued with the exact same call. If the LLR is still close to a bound at the
20000 games of the ini, continue with a raised limit, e.g. `--sprt maxgames=30000`, using
the same state file. With bounds only 5 Elo apart the LLR moves slowly — game counts in
the thousands say nothing, do not read a tendency into them.

qet signals the result via its exit code: 14 = H1 accepted, 15 = H0 accepted, 16 = undecided
within maxgames. A non-zero exit code here is a result, not an error.

H1 accepted → the change stays. H0 accepted → move the commit to a branch `dead/<change>`
as documentation of what was already tried and revert it on the working branch.

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
