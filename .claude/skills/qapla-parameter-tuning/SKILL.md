---
name: qapla-parameter-tuning
description: Run tournaments, SPRT tests, CLOP tuning and EPD analysis through the Qapla Chess GUI's remote-control HTTP interface, so the games are visible on its boards. Use when asked to play engines against each other, optimize engine parameters, run a CLOP tuning session, or check whether a change is an improvement.
---

# Driving the Qapla Chess GUI

Everything runs inside the GUI so the user can watch the games on the boards. Do not drive
qapla-engine-tester directly for this.

## Reaching the GUI

Check `GET http://127.0.0.1:8137/health` first; it answers `{"ok":true}` and needs no token. The
port is configurable, 8137 is the default.

If nothing answers, start the GUI with `--remote-control --remote-control-port=8137` and wait for
health. It lives next to `qet` — `~/bin` on Linux and macOS, `c:\development\bin` on Windows, see
the tool table in `CLAUDE.md` — but the file has two names in the field: `qcg` / `qcg.exe`, and
`qapla-chess-gui` / `qapla-chess-gui.exe`. Take whichever exists rather than the one the table
happens to name; the same is true of `qet` and `qapla-engine-tester`.

Start it detached and poll health, it does not return: `nohup <gui> --remote-control
--remote-control-port=8137 > test/log/qcg.log 2>&1 &` on Linux and macOS, `start "" <gui>
--remote-control --remote-control-port=8137` on Windows. That log is worth keeping: it repeats the
whole configuration of a run, every engine with the options it was really given, which is how to
check that an option arrived rather than assuming it.

Never start a second instance while one is running: both write the same profile and the last one to
exit wins.

You cannot close it. `close_application` is refused over the remote control; ask the user.

`GET /tools` lists the callable tools with their JSON schemas, in the shape a language model API
uses: a JSON array of `{"type":"function","function":{"name","description","parameters"}}`, so the
name is one level down and not at the top of each entry. The descriptions there are the truth about
the arguments and are worth reading before a first call of a tool; this file names the fields that
matter, not all of them.

`POST /tools/<name>` with the arguments object answers `{"ok":bool,"content":string}`. `content` is prose written to be read,
not a status code; on `ok:false` it says what was applied and what was not, so read it before
retrying — a failed call may already have changed part of the state.

`GET /wait?type=clop|sprt|tournament|epd&timeout=<seconds>` blocks until that run stops and
answers `reason=finished|stopped|timeout|closed|not_running` with the full status. Use it instead
of polling `get_status`.

## Engines

`manage_engines` with `command` = `list|details|install|copy|delete|update|set_options`.

`install` takes `new_name` and `path` and starts the engine to detect its UCI options before
returning. `path` has to be absolute. The same executable may be installed several times under
different names: option values belong to the catalog entry, not to the file. That is how one build
plays against itself under two settings.

An engine runs with the GUI's working directory, not the one it was built in. Every path an engine
takes as an option — a net, a book, a tablebase directory — therefore has to be absolute, or the
engine looks for it somewhere nobody meant.

`details` lists the UCI options the program actually supports, with type, default and allowed
range. Read it before setting any option; the names differ from build to build.

An engine selected for a run is a *copy* of the catalog entry. `set_options` and `update` take
`target=catalog|sprt|tournament|epd`. Select the engines for a run first, then set options on that
run's copy — selecting takes a fresh copy from the catalog and discards earlier per-run values.

## Running a tournament

`configure_tournament`, then `start` with `type=tournament`.

Needed before it can start, here or in an earlier session: `engines` and `openings_file`. The rest
has useful previous values, and a call changes only the fields it passes.

* `engines` is an array of catalog names and sets up a round robin of all of them.
* `games` is **games per pairing per round**, not the total of the tournament. Two engines are one
  pairing, so a thousand games between two of them is `games=1000, rounds=1`. With three engines the
  same number plays three thousand.
* `time_control` is `"<base>+<increment>"` in seconds, increment optional. `base` may be `M:S` or
  `H:M:S`.
* `concurrency` is the only field that may be changed while a run is going, and it takes effect at
  once. Everything else is rejected until the run is stopped.
* `pgn_file` and `event` name the output and the event; `openings_file` takes an EPD or a PGN.

Adjudication is **off by default**, which for long time controls means every drawn game is played to
the bitter end. `draw_mode` and `resign_mode` take `off`, `test` (decide and log, do not end games)
or `active`. The defaults behind them are sensible: draw after 20 consecutive moves within 20
centipawns from full move 80, resign after 5 consecutive moves at 500 centipawns or worse.

`resign_two_sided` decides whether both engines must agree the position is lost. Set it when the two
engines do not share an evaluation - a hand written one against a net, say. Otherwise the weaker
judge alone can end games, and its mistakes become results.

`get_status` with `type=tournament` reports the configuration and the standings; `/wait?type=
tournament` blocks until it ends. Estimate the length before starting one: games times the seconds a
game takes, divided by the concurrency. A thousand games of 60+1 on ten in parallel is some six
hours.

## Running CLOP

`configure_clop`, then `start` with `type=clop`. It is reachable only over the remote control.

Required: `engine`, `opponents`, `parameters`, `time_control` (e.g. `"20+0.1"`), `openings_file`.
`engine` and `opponents` must be sent in the same call because they carry the two roles.
`parameters` is an object of UCI option name to search range, e.g.
`{"timeShareHalfTime": "5000 35000"}`.

Optional: `samples`, `games_per_sample`, `warmup_samples`, `active_pairs`, `concurrency`, `seed`,
`h`, `prior_variance`. `concurrency` is the only setting that also takes effect on a run already
going.

`get_status` with `type=clop` reports sample progress, phase and the current estimate. The run
writes nothing to any engine; applying the result is a separate `manage_engines` call.

The CLOP configuration is not persisted. A GUI restart loses it.

## Choosing parameters and ranges

Choose the range as tight as you dare. Over a parameter's full declared range CLOP spends its
samples separating values that barely differ; over too narrow a range it cannot follow a value
that may still move far.

Never tune parameters that cancel each other out: numerator and denominator of the same term, or
all piece values at once. CLOP cannot distinguish a change from a change that undoes it. This is
decided when choosing which parameters to expose in the engine source, not when configuring the
run.

## Verifying with SPRT

CLOP's optimum is a fit through noisy samples and frequently is not an improvement. SPRT decides.
Rejection is a normal outcome, not a sign that the tuning run went wrong.

Give the SPRT a different `seed` than the CLOP run used. The same seed draws the same openings out
of the book, which would test the values on the positions they were found on. The book itself need
not change: at these time controls the games never repeat exactly anyway.

Set it up by copying the baseline catalog entry under a new name, setting the tuned values on the
copy, then `configure_sprt` with `champion` = baseline and `challenger` = copy, and `start` with
`type=sprt`.

## Whole flow

Expose the chosen parameters as UCI options in the engine source and build. Install that build
twice under distinct names. Run CLOP. Copy the baseline entry, set the estimated values on the
copy. Run SPRT of the copy against the baseline with a different seed. Keep the new values only if
SPRT accepts them.
