---
name: qapla-parameter-tuning
description: Tune engine parameters with CLOP and verify them with SPRT through the Qapla Chess GUI's remote-control HTTP interface. Use when asked to optimize engine parameters, run a CLOP tuning session, or check whether a parameter change is an improvement.
---

# Tuning engine parameters through the Qapla Chess GUI

Everything runs inside the GUI so the user can watch the games on the boards. Do not drive
qapla-engine-tester directly for this.

## Reaching the GUI

Check `GET http://127.0.0.1:8137/health` first; it answers `{"ok":true}` and needs no token. The
port is configurable, 8137 is the default.

If nothing answers, start the GUI with `--remote-control --remote-control-port=8137` and wait for
health. Never start a second instance while one is running: both write the same profile and the
last one to exit wins.

You cannot close it. `close_application` is refused over the remote control; ask the user.

`GET /tools` lists the callable tools with their JSON schemas. `POST /tools/<name>` with the
arguments object answers `{"ok":bool,"content":string}`. `content` is prose written to be read,
not a status code; on `ok:false` it says what was applied and what was not, so read it before
retrying — a failed call may already have changed part of the state.

`GET /wait?type=clop|sprt|tournament|epd&timeout=<seconds>` blocks until that run stops and
answers `reason=finished|stopped|timeout|closed|not_running` with the full status. Use it instead
of polling `get_status`.

## Engines

`manage_engines` with `command` = `list|details|install|copy|delete|update|set_options`.

`install` takes `new_name` and `path` and starts the engine to detect its UCI options before
returning. The same executable may be installed several times under different names: option values
belong to the catalog entry, not to the file. That is how one build plays against itself under two
settings.

`details` lists the UCI options the program actually supports, with type, default and allowed
range. Read it before setting any option; the names differ from build to build.

An engine selected for a run is a *copy* of the catalog entry. `set_options` and `update` take
`target=catalog|sprt|tournament|epd`. Select the engines for a run first, then set options on that
run's copy — selecting takes a fresh copy from the catalog and discards earlier per-run values.

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
