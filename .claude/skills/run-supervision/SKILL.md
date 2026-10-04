---
name: run-supervision
description: Keeping long runs (training, labelling, tournaments, SPRTs) going on all machines without idle time - status timer, watchers, start checks, error handling, follow-up work. Read and apply whenever any long run is going or being started, and after every interruption.
---

# Supervising long runs - no lost computing time

On 2026-10-04 about four hours of training time were lost: a hung gpu run was restarted several times
without the cause being found, each restart was checked only after 20 minutes, and the status reports
stopped silently. Every rule below exists to make that impossible.

## 1. After every interruption: the status timer first

After any interruption - Volker stops an action, a new message arrives in the middle of work, a context
compaction, a session restart - the first thing is to check that the status timer runs:

    pgrep -f "sleep 1799" >/dev/null && echo timer runs || echo NO TIMER

If it does not run, start it again at once, as a background Bash task:

    sleep 1799; echo status-due

1799 and nothing else: the odd number is how pgrep tells this timer from every other sleep. When it
fires, give the status report (section 8) and start it again in the same turn. The timer runs as long
as any run is going on any machine.

## 2. Every start is checked within seconds

A run counts as started only when it is seen computing, never because the command returned:

- gpu training: `ioreg -r -d 1 -c IOAccelerator | grep -o '"Device Utilization %"=[0-9]*'` above 90 %
  after 30-90 s
- cpu work: the process exists and its cpu time grows between two `ps` calls a few seconds apart
- qet: the run log shows `started round` lines, the first result within a minute
- remote machines: the same checks over ssh

If the check fails, look at the log right away - not after the next progress line.

## 2a. The Mac must not sleep

While anything runs on the Mac, a `caffeinate -dimsu` must be running:

    pgrep -x caffeinate >/dev/null || ( nohup caffeinate -dimsu > /dev/null 2>&1 < /dev/null & )

Checked at every start and in every status report. On 2026-10-04 the Mac went into idle sleep at 02:09
and at 05:26 - exactly when the gpu training "hung" - and slept until someone touched it; on the same
day a tournament ran at half speed for 40 minutes for the same reason. A run that stops without an
error message: look at `pmset -g log | grep -E " (Sleep|Wake|DarkWake) "` first.

## 3. Every long run gets a watcher that wakes me

A background Bash task with an until-loop that checks every 10 s and ends - which wakes me - as soon as
the run stalls or ends:

- gpu training: utilization below 50 % for 2 minutes
- any run: the process is gone
- remote runs: a check over ssh every few minutes (the label count, qet running)

A watcher is a script in `tmp/` run with `sh`: the Bash tool runs zsh, which does not split `$var` into words,
and a watcher parsing ssh output that way fired at once three times. Check after its start that it
reached its wait (`pgrep -f "sleep 120"`), not that it was launched. A watcher waits on a pid
(`kill -0 <pid>`), never on `pgrep -f <name>`: its own command line holds the name and matches forever - over ssh too, where the remote shell
running the command holds it (`pgrep -x <process>` is safe).

A watcher ends when the run ends; the follow-up is then started in that same turn (section 6). A
progress line written every 10 minutes is no watcher: a stall shows only after 20 minutes or more.

## 4. On an error: find the cause, fix it, then restart

Never restart unchanged. A run that failed once fails again for the same reason. Before a restart:

1. find the cause: stack dumps (python: `faulthandler.dump_traceback_later(60, repeat=True, file=...)`,
   see `tmp/sf-run-stacks.py`), the logs, `sample <pid>` on the Mac, the exit code
2. fix it, and reproduce in the shortest possible test - seconds to a few minutes, never a full epoch
3. restart with the fix and check per section 2

If the cause cannot be found at once, the restarted run gets the instruments that will show it next time
(stack dumps, a tighter watcher), and that is said in the report.

## 5. Every run has a goal, written down

Every run - started, paused or planned - stands in `plan/run-goals.md` with the goal it serves: the
question it answers, and what its result decides. Entered when the run starts, updated when it ends or
the goal changes. The goal is what decides the right follow-up: the next run is the one that brings
that goal - or the overall goal it belongs to - furthest, not simply the next item on some list.

## 6. No idle machine, the follow-up is decided before the end

- For every run, know before it ends what comes next on that machine. Ask Volker at the latest 30
  minutes before the expected end if it is not clear - not after the machine has stood still.
- **Waiting for Volker's answer must not leave a machine idle.** He is often away from the computer, and
  an answer may come hours later or not at all. If no answer has come when a machine runs out of work,
  start what follows the goal best by `plan/run-goals.md` - the proposal made in the question - and say
  so in the next report. Work that is thrown away later costs nothing compared with a machine standing
  still; when his answer comes, switch to it.
- No task may be tied to one machine without need. If a build depends on changes that must not reach
  the working branch, commit them to a temporary branch (`tmp-<task>`), push it, build from it on every
  machine, and delete the branch when the task is done.
- Write the follow-up as a chain that starts by itself (wait for the end, check the result, start the
  next step), and watch the chain like a run: a chain that stops at a check must wake me.
- No long chains written hours ahead (Volker, 2026-10-04): a chain covers only the next step, written
  when that step becomes relevant. Further steps stand in `plan/run-goals.md`, and a watcher on the
  current run wakes me so that I prepare the next step then.
- A chain or a guard that kills or stops something writes why into its log, and the watcher reports it.

## 7. Waiting is minimal

- Never wait a fixed long time for a result that can be checked earlier. Poll the condition in a
  background loop every few seconds and end the loop when it is met.
- A test is as short as the question allows: does it start, does it compute, does it hang - seconds.
- Background tasks of the harness are killed at their timeout; a run that must outlive that starts with
  `( nohup ... & )`, never as the background task itself.

## 8. The status report covers every machine

Every 30 minutes, a table with every machine that does anything - Mac, Linux (qapla), Windows (Ryzen9) -
and every machine that stands idle:

| machine | task | state |

State means progress and the expected end. An idle machine stands in the table as idle, with what it
should do next or the question to Volker. Every strength figure with its uncertainty.

Every figure in the report is checked live, not read from a log alone: the process runs (`pgrep -x`),
the progress since the last report matches the expected rate (games or steps per minute - a run at half
speed is a fault to find, as much as a stopped one), caffeinate runs on the Mac,
the log was written in the last minutes, and on the machine no leftovers sit beside it - after a test
run with qet, kill its engines as well (`pkill -x qet` leaves them running as orphans).
