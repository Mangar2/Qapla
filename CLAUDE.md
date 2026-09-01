# Qapla2 — Project Instructions

## Building

The release build used throughout the test phase is, on **every** platform:

```
make Release -j
```

Not `make BUILD_TYPE=Release -j` — the `Release` target sets `BUILD_TYPE` itself. The
binary lands in `build/Release/Qapla.exe` (`build/Release/Qapla` on Linux/macOS) and that
is the path the EPD, SPRT and CLOP runs below refer to.

## Concurrency of test runs

**30 in total, never more — and that is a budget for the whole machine, not per run.** Two runs
side by side get `--concurrency=15` each, not 30 each.

`nproc` is not the number to size this by: it counts SMT threads, and the box here has 16 physical
cores behind its 32 threads. The ini files set 30 for the case that one run has the machine to
itself; override it on the command line as soon as something else is running at the same time.

The two free threads are not spare capacity, they are for qapla-engine-tester itself. It is lean,
but it drives every engine process, writes the pgn and keeps the state file — a run that claims
every thread starves its own driver.

## Mandatory: node-count comparison run for behaviour-neutral engine changes

Any change to the engine that is **not supposed to change how it plays** (refactorings,
cleanups, new code behind a zero weight, renamed/moved functions, performance rewrites,
…) MUST be verified with a comparison EPD run **before and after** the change:

```powershell
c:\development\bin\qet.exe --settingsfile=test/epd/epd-wmtest-depth.ini --engine name="Qapla current" cmd=c:/development/qapla2/build/Release/Qapla.exe
```

Run it from the repository root (the paths in the ini are relative to it). Rebuild
(`make Release -j`) between the two runs.

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
   `make BUILD_TYPE=Release clean` first (the `clean` target needs `BUILD_TYPE`
   spelled out), otherwise the old string stays in the binary.
   Verify with `printf 'uci\nquit\n' | ./build/Release/Qapla.exe | grep "^id name"`.
4. Run the EPD test once: the node count must differ, otherwise the change is not active.
5. Run (from the repo root, own state file per experiment):

```powershell
c:\development\bin\qet.exe --settingsfile=test/sprt/sprt-standard.ini --engine name=Qapla-baseline cmd=<baseline.exe> --engine name=Qapla-<change> cmd=c:/development/qapla2/build/Release/Qapla.exe gauntlet=true --sprt file=test/log/sprt-<change>.state
```

Command line parameters override the ini. Engine names must not contain spaces. Change
`test/sprt/sprt-standard.ini` only if qet reports an error in it.

**A time control other than the one in the ini goes in as `--each tc=20+0.1`, before the
`--engine` blocks.** `tc=` is also an option of `--engine`, so written after an engine block it
silently applies to that engine alone and the other one keeps the ini value. The run then compares
one engine at 20 seconds against one at 5 and reports a winrate near 90 % — the only warning is
the number itself. Check the move times in the pgn when a result looks too good.

The `--sprt file=` state file holds the tournament state. The run may be stopped at any
time and continued with the exact same call. If the LLR is still close to a bound at the
20000 games of the ini, continue with a raised limit, e.g. `--sprt maxgames=30000`, using
the same state file. With bounds only 5 Elo apart the LLR moves slowly — game counts in
the thousands say nothing, do not read a tendency into them.

qet signals the result via its exit code: 14 = H1 accepted, 15 = H0 accepted, 16 = undecided
within maxgames. A non-zero exit code here is a result, not an error.

### An SPRT has exactly one output

**H0, H1, or undecided. Nothing else.** Not an Elo figure, not "about −7", not a winrate read as a
strength. The run is a decision procedure against two bounds, and the decision is the whole of what
it proved.

The score at the stopping point is not an estimate of the effect. A run stops the moment the LLR
reaches a bound, and *that* is what stopped it — the score is biased downwards for H0 and upwards
for H1, precisely by the mechanism that ended the run. Reading a magnitude out of it invents a
number the run never measured, and two such numbers from two runs cannot be compared or added.

This binds everything written down, not just the summary line: **version log entries, code comments
and answers to me.** Write "H0 accepted after 6532 games, bounds −2/+3". Do not write "costs 7 Elo".
The game count is a fact about the run and may be reported; it says how quickly the bounds were
reached, not how large the effect is.

If the magnitude is the question, that is a different run: place the bounds where the question is
(H0 = 5, H1 = 12 asks whether something is worth about ten) and run that.

H1 accepted → the change stays. H0 accepted → move the commit to a branch `dead/<change>`
as documentation of what was already tried and revert it on the working branch.

### Closing run after a series of SPRTs

Once **more than two** SPRTs have been run to find the best version of one todo item, the winner
has to prove itself in a final run against the version the item started from.

Every single run accepts at alpha = 0.05, so over a series the chance that one of the survivors is
worth nothing is no longer small — and a run stops at the moment it reaches its bound, which
favours whatever was fluctuating upwards. The individual gains must not be added up either; the
closing run is the only honest number for the item as a whole.

If it fails, the series produced nothing, whatever the single runs said.

## The version log

`plan/version-log.md` gets an entry for **every** tagged version, written as soon as its result
is in — the kept ones and the reverted ones alike. A version whose entry is missing is a version
nobody can learn anything from later. Entries stay in ascending version order.

**The entry goes into a commit of its own.** Never into the commit that carries the change it
describes: when that change loses its SPRT and gets reverted, the revert takes the entry with it
and the record of the failure disappears — which is exactly the record worth keeping. After a
revert, check that the entry is still in the file.

## Tuning parameter values (CLOP)

Values are tuned with a CLOP run, then confirmed by an SPRT like any other change.

**Search and eval parameters are exposed differently.** Both flows build the normal `Release`
binary and differ only in what makes the options appear: a group flag for search values, a
per-file define for eval values. Read the section that matches what is being tuned and do not mix
the two — the wrong switch simply exposes nothing, or exposes hundreds of options that have
nothing to do with the run.

`ReleaseOpt` is used by neither flow. It only sets `PARAM_OPTIMIZE` globally, and it does not
compile: `PARAM_OPTIMIZE` turns `MaterialBalance::PAWN_VALUE_EG` into a runtime variable, which
`eval/tablebase-value.h` uses to initialise the `constexpr TB_CURSED_BONUS`.

### Search parameters only

**`ReleaseOpt` has nothing to do with search parameters.** It only sets `PARAM_OPTIMIZE`, which is
the eval switch. `tunable<>` does not read that define — the group flag alone decides whether the
UCI option exists — so a search parameter run uses the normal `Release` build.

1. Set the group flag of the parameters to be tuned to true — do not commit that — and build
   `make Release -j`. Check the options are there:
   `printf 'uci\nquit\n' | ./build/Release/Qapla.exe | grep "^option name"`.
2. Run (engine as parameter, the ini defines none — a command line `--engine` adds an engine
   instead of replacing one):

```powershell
c:\development\bin\qet.exe --settingsfile=test/clop/clop-standard.ini --engine name=Qapla cmd=c:/development/qapla2/build/Release/Qapla.exe --clop samples=<N> --clopvalue name=<UciOption> min=<min> max=<max> [--clopvalue ...]
```

3. Round the estimates, put them in as the new defaults, set the group flag back to false,
   then tag and run the SPRT against the previous version.

### Eval parameters only

Eval values are exposed by a per-file define instead of a group flag, and **for them
`ReleaseOpt` must not be built at all.** `ReleaseOpt` defines `PARAM_OPTIMIZE`, and every eval
file turns that into its own switch:

```cpp
#ifdef PARAM_OPTIMIZE
#define PARAM_OPTIMIZE_KING_ATTACK
#endif
```

So `ReleaseOpt` activates *all* of them at once — `PARAM_OPTIMIZE_BISHOP`, `_KNIGHT`, `_PAWN`,
`_QUEEN`, `_ROOK`, `_THREAT`, `_SPACE`, `_KING_ATTACK`, `_IMBALANCE`, `_MATERIALBALANCE`,
`_PST` — which floods the UCI option list with hundreds of entries that have nothing to do with
the run and costs speed in the parts that are not being tuned.

Instead add the define of the affected file only, above its `#ifdef PARAM_OPTIMIZE` block:

```cpp
#define PARAM_OPTIMIZE_KING_ATTACK
```

and build the normal `make Release -j`. Only that file's parameters become UCI
options, the rest of the eval keeps its compile time constants.

Steps 2 and 3 of the search flow apply unchanged. Remove the define again when the run is done —
it must never be committed.

### Applies to both

**Never tune more than 10 parameters in one run.** CLOP cannot separate more than that; split the
set into runs of related values instead.

**Values that can cancel each other out do not go into the same run.** Numerator and denominator
of the same division are the clearest case: countless pairs produce the same quotient, so the run
has no way to tell them apart and estimates nothing. Split them — tune one side, confirm it with an
SPRT, then tune the other side against the new baseline.

**A parameter whose range holds fewer than 5 values does not go into a run.** If the range can be
widened — by rescaling the term it sits in — widen it and tune it. If it cannot, because the value
is inherently discrete (a move number, a ply count, a small integer divisor), it is not a tunable
value at all: write it as a named compile time constant, not as a `param<>` call. A UCI option that
CLOP cannot move only clutters the option list.

Samples: ~2000 for a single parameter, ~5000 from five parameters upwards. Each sample costs
`gamespersample` games — 5000 samples took ~3.2 hours here.

**The current value must sit exactly in the middle between `min` and `max`.** Only one of the
two bounds is a free choice, the other one follows from it: with a current value of 17 and
`min=0`, `max` is necessarily 34. A range that is not centred on the current value biases the
run towards one direction and makes the result unusable.

CLOP estimates a value the more precisely the narrower the interval it has to explore. Pick the
width from the change to be expected: wide for a value that was never tuned, narrow when
re-optimizing a value that is already good.

The UCI option must accept that whole range (0 to 34 in the example above). If its own limits
are narrower, widen the limits of the UCI parameter and rebuild — otherwise CLOP proposes
values the engine never actually plays with.

## Shape of a tunable value

Applies to search and eval alike. Giving a name to a value that cannot move does not make it
tunable.

- Every case gets its own coefficients. Never derive one case from another — no scaling, no
  ratio, no shared factor. Dependent values cannot be moved apart by a tuning run.
- Every influence on the value enters as its own coefficient, not as a fixed multiplier, offset
  or exemption.
- Each coefficient must have room to move. A value confined to a few integers carries no signal.

When an existing expression has the wrong shape, reformulate it. Do not put a parameter into the
old form.

**A reformulation starts at the values it replaces.** The new coefficients get the defaults that
reproduce the old expression exactly, proven by an identical node count, and only then does a run
tune them. Choosing "reasonable looking" new values and tuning from there mixes two changes that
have to be measured separately — afterwards nobody can say whether a gain came from the new shape
or from the new values.

If two of the new coefficients coincide at that starting point and a third one therefore has no
effect, that is not a reason to move them apart by hand. Tune the two that do have an effect
first, then the third one against the values they reached.

## Tunable search parameters

Define them at the call site, using the value where it is needed instead of routing it through
a constant or a member. The definition is `tunable<OPTIMIZE, NAME, DEFAULT, MIN, MAX>()`, which
returns the parameter value, see `search/tunable.h`:

- `OPTIMIZE` false → `DEFAULT` as a compile time constant, the call is optimized away and no
  UCI option exists.
- `OPTIMIZE` true → the value is read from a variable that is registered as a UCI spin option
  named `NAME` with the limits `MIN` and `MAX` before main.

`OPTIMIZE` is a group flag; the group flags live in `search/search-config.h` and are false
by default. Set one to true only for a tuning run. Node count must be identical for both flag
states as long as all values are at their defaults.

`MIN` and `MAX` are the limits reported to the UCI interface and thus the range a CLOP run can
explore — see the range rule in the CLOP section above.

Do not add search parameters via the eval-style `UciParameterProvider` classes; those stay
for eval only.

## ToDo

If I ask you to work on ToDo
- They are comments in the code
- After doing them remove the ToDo comment and replace it by a result-measurement (usually an SPRT result)
- If successful, create a new commit with a new version tag
- Once the last ToDo is done, see, if there is still something to checkin, if so do so.
- Finally push all to github.

