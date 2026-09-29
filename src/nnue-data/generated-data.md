# The training data, and how each piece of it was made

One section per artefact, in the order the pieces are built. Every section names the command
that made it, where the file is, how long it took and what the run reported, so a piece can be
made again without guessing and a later run can be compared against an earlier one.

Everything lives under the repository, see CLAUDE.md. The data files are not versioned.

## The build these commands need

The generating commands only exist in a build that defines `QAPLA_GENERATE_NNUE_DATA`: without
it the search observer the collector hangs on is compiled away, see `search/search-config.h`.
The define is not a make dependency, so the build has to be cleaned first or the old binary
silently stays:

    make BUILD_TYPE=Release clean
    make Release -j EXTRA_DEFINES="-DQAPLA_GENERATE_NNUE_DATA"

Send `quit` with every command. The commands are one line on stdin, and until commit e2fa56f
the statistics loop turned at full cpu instead of ending when stdin closed.

## 1. The book of start positions

**File:** `test/nnue/start-positions-1m.bok`, 3,519,423 bytes
**Made on:** 27.09.2026, Mac mini M4
**Built from:** `test/nnue/start-positions-100k.bok` (100,020 leaves, 25.09.2026), extended by
900,000 leaves rather than started fresh, so the earlier book keeps its use and both are
comparable.

    printf 'nnuebook add test/nnue/start-positions-100k.bok out test/nnue/start-positions-1m.bok \
        leaves 900000 sd 8 collect 2 margin 100 seed 1\nquit\n' | ./build/Release/Qapla

`sd 8` searches every position to depth 8, `collect 2` takes every line of that search which
still has two plies below it, `margin 100` drops a line whose value is more than 100 from the
root value - a pawn is 80 to 95 in this unit. `maxply` stayed at its default of 120, `seed 1`
makes the random walk repeatable. How the three steps work is in `book-generator.h`.

**What the run reported:**

    Read 176326 moves, 100020 leaves from test/nnue/start-positions-100k.bok
    Searches: 4698, candidates: 1608058, rejected in check: 128295, mate: 561,
    off margin: 452944, null move: 0, too long: 0, lines: 1026258, new leaves: 900000
    Book: 1759667 moves, 1000020 leaves, 900000 of them new

So 1,000,020 leaves in 1,759,667 moves. Of 1,608,058 candidate lines, 452,944 were outside the
margin and 128,295 stood in check; 1,026,258 lines went in and produced the 900,000 new leaves.

**Time:** 25.1 s, single threaded - the generation runs on one core because the search observer
is one per process. That is about 36,000 new leaves per second, and the cost is linear in the
leaf count (measured: 2,000 leaves 0.08 s, 4,000 leaves 0.13 s, 8,000 leaves 0.25 s). A book is
cheap; nothing here needs to be planned around its runtime.

**On saturation:** the generator stops early when 100 searches in a row add nothing, and says so.
It did not say so, so the tree is not exhausted at these settings and a larger book is a matter
of asking for more leaves.

## 2. The book as an opening library

**File:** `test/nnue/start-positions-1m.pgn`, 148,456,406 bytes
**Made on:** 27.09.2026, from the book of section 1

    printf 'nnueopenings test/nnue/start-positions-1m.bok test/nnue/start-positions-1m.pgn\nquit\n' \
        | ./build/Release/Qapla

**Result:** 1,000,020 lines, one per leaf of the book, in 0.9 s.

Every line is one pgn game from the initial position with the moves in long algebraic notation
and `[Result "*"]`:

    [Event "Qapla position library"]
    [Result "*"]

    1. a2a3 g8f6 2. b1c3 d7d5 3. e2e3 d5d4 4. e3d4 c7c6 *

Line lengths: 14,990,554 plies over the 1,000,020 lines, 15.0 on average, shortest 4, longest 35.

**Lines and not positions on purpose.** An engine tester takes this as its opening library and
plays the line out, so the game it writes starts at move 1 and a later pass that evaluates that
pgn labels the opening moves as well. A library of plain fens would lose them - and those are
the plies the training data otherwise has none of, because every game starts at a leaf. The
notation is the one the exporter writes; the tester reads either.

## 3. The playing template: Qapla against itself at depth 6

**File:** `test/nnue/games-hce-depth6.pgn` (grows while the run goes)
**Started on:** 27.09.2026, Mac mini M4
**Engine:** `new-versions/Qapla-0.5.0-027-20-hce`, the hand crafted eval, built without
`QAPLA_GENERATE_NNUE_DATA` so the observer calls are not in it
**Settings:** `test/tournament/gen-hce-depth6.ini`

    ~/bin/qet --settingsfile=test/tournament/gen-hce-depth6.ini \
      --engine name=HCE-A cmd=<repo>/new-versions/Qapla-0.5.0-027-20-hce \
      --engine name=HCE-B cmd=<repo>/new-versions/Qapla-0.5.0-027-20-hce

One game per book leaf, 1,000,020 of them, openings read in order from the library of section 2.
`repeat=1` and `noswap=true`, because the search is deterministic at a fixed depth: a leaf and a
colour decide the game completely, and a colour swap would hand back the same game a second time.

The pgn holds the moves and nothing else - no evaluation, no clock, short notation. It is the
template a later pass analyses; the values come from that pass, not from the engine that played.
Short notation because it is shorter, and nothing here pays for it. The notation makes no
difference to the tester: it does not trust its input and runs every move through its move
generator either way, setting each piece of information and then asking whether exactly one
generated move matches. Notation matters one step further on, for the pgn the analysis *writes* -
`src/trainer/convert.py` reads that one and decodes long notation without a generator at all,
which is the one place where the choice buys something.

The run of 27.09.2026 was started with `notation=lan` before this was corrected, so its file
holds long notation. If that run is continued, it has to be continued with
`--pgnoutput notation=lan` on the command line, otherwise the two notations end up in one file.

**The run may be stopped and continued** with the identical call. `[tournament] file=` holds the
state and is written every 10 s, and the pgn is appended to rather than overwritten.

### Things worth knowing before starting one of these

**Stay a little under the core count: about 30 games on 32 cores, 90 on 96** - Volker's own
measurement, from before this work. The reason is not that the driver is short of a core. qet
parallelises cleanly, with three threads of its own per attached engine, and at depth 8 an engine
answers in well under a millisecond, hands back and waits. What costs the difference is the
context switching itself: thousands of switches a second, and the operating system needs time for
them that no process shows as its own.

So the number is not derived from anything, it is measured, and it does not follow the load
average. On the 32 core instance the load stood at 43.7 with `procs_blocked 0` and `iowait 0%`,
which looks like eleven processes waiting for a core - while 6 % of the cpu time was idle at the
same moment. Both together say only that the work arrives in bursts. Measure the throughput, do
not read the load.

**`rapid` has to stay off.** It would suppress the engine's `info` lines, and nothing in this
run reads a node count or a depth, so it looks like free speed. It is not: the draw adjudication
decides on a score, and the score comes from exactly those lines. With `rapid=true` the endless
games are no longer cut off.

**A block that is there is in force, a block that is not there is off.** That is how the tester
reads its settings, so a won game is played to its end by leaving the `[resign]` block out - not by
disabling it. There is an `active` flag, but it exists only so that a file written by the gui can be
read, where settings are stored and switched on and off; it is deliberately not in the parameter
documentation, and passing a setting that is then not in force makes no sense in a settings file
one writes oneself.

A first run of 7145 games was thrown away over this: the block was there with `active=false`,
which at the time was ignored, and 26 of 30 games in a check afterwards were cut short by
adjudication where without the block 27 of 30 ended in mate. The flag is honoured now, but the
rule above is the one to write settings by.

**Cost of playing games out:** 43 games/s with win adjudication, 31 games/s without it, at
concurrency 9. The second figure is the one that counts, and it puts the 1,000,020 games at
about 9 hours.

### What the finished run produced

**1,000,020 games in 9 h 59 min**, 1,042,477,487 bytes, about 140.0 plies per game, so roughly
140 million positions - and that is the number the labelling pass has to search.

(An earlier version of this file said 86.1 plies per game. That was a broken measurement: the awk
that counted them only looked at lines beginning with a move number, and a pgn wraps its movetext
over several lines. Counted over every movetext line, one chunk of 10,000 games holds 1,400,375
plies. Every figure in positions per second below was corrected by the same factor of 1.63.) The average over the whole run
was 27.8 games per second; a 65 second sample at the start said 31, so a short sample reads about
12 % high here.

How the games end:

| cause | games | share |
|---|---|---|
| mate | 768,962 | 76.9 % |
| threefold repetition | 185,558 | 18.6 % |
| insufficient material | 15,369 | 1.5 % |
| draw adjudication | 16,720 | 1.7 % |
| 50 move rule | 11,438 | 1.1 % |
| stalemate | 1,973 | 0.2 % |

Not one game decided by adjudication, which is what the missing `[resign]` block is for, and the
draw adjudication caught 16,720 endless ones, which is what the `[draw]` block is for. White won
392,361 and black 376,601 - the openings are the whole first move list of white, so a leaning
towards white is not expected here and there is none worth speaking of.

## 4. The labelling pass: every position searched to depth 8

**File:** `test/nnue/games-hce-depth6-labelled.pgn`, projected 2.3 GB (2,346 bytes per game)
**Started on:** 28.09.2026, Mac mini M4
**Engine:** `new-versions/Qapla-0.5.0-027-20-hce` at `tc=depth:8`
**Settings:** `test/tournament/label-hce-depth8.ini`, driver `test/tournament/label-chunks.sh`

    sh test/tournament/label-chunks.sh

The pass recomputes every position of every game of the template with the hand crafted eval at
depth 8 and writes the values into a new pgn. That pgn is what `src/trainer/convert.py` reads,
which fixes two of its settings: long notation, because the converter decodes those moves without
a move generator, and `min=false`, because it takes the game result from the `Result` tag.

Worth recording: the analysis reconstructs the result even from a template that carries no
`Result` tag, only the `0-1` at the end of the movetext. The minimal template loses nothing.

### It has to run in chunks

**A million games in one piece is killed.** qet holds a whole analysis in memory: 565 MB for
5,000 games, measured, and still climbing at that point - about a factor of 100 over the size of
the input pgn. Handed the full 1.04 GB template it died with SIGKILL.

So the template is split into 101 chunks of 10,000 games in `test/nnue/chunks/`, and the driver
runs them one after another into one output file. Memory stays around 4 GB per chunk and is given
back between them, because every chunk is its own process.

**That also makes the pass resumable**, which a single analysis is not: a finished chunk gets a
`.done` file beside it and is skipped when the driver is called again. Stop it whenever, call it
again with no arguments.

The chunks are uniform in length - within 3 % of each other across the file - so the rate does not
drift as the run goes on.

### Rate, and how not to measure it

**10.7 games per second, about 1,500 positions per second, at concurrency 9. The 1,000,020 games
take about 26 hours.** Labelling costs roughly three times as much per position as playing did, because
each position is searched on its own instead of profiting from the hash and history its
predecessor in a game left behind.

Two earlier figures for the same thing were both wrong, in opposite directions, and both came from
samples that were too small:

- 20 games at concurrency 2 gave 66 hours. Process starts dominate a sample that short.
- a 2 minute window on the running pass gave 13 hours. It caught a burst of games that were
  already in flight when the window opened.

The 4 minute window over 2,568 games is the one to trust. When a run is going to last a day, the
measurement of its rate deserves minutes, not seconds.

## What a machine needs before it can help

Any host may run a playing or a labelling step, and the steps are long, so a host that is almost
ready wastes a day before it says so. This is the list, and every item has a check that fails
loudly rather than a step that runs slowly.

**The repository, pulled.** Code reaches a host only through GitHub, never by copying - see
`delivery/deployment.md`. So: `git pull`, then `git log -1` and compare the hash with the one the
run is supposed to use. A stale checkout produces a stale binary and says nothing.

**The two engines, built there.** They differ by one define and are built one after the other,
each after a clean, because the define is not a make dependency:

    make BUILD_TYPE=Release clean && make Release -j                              # hce
    make BUILD_TYPE=Release clean && make Release -j EXTRA_DEFINES="-DQAPLA_USE_NNUE"

Keep them under `new-versions/` with names that say which is which, and check each one's version
string before using it: `printf 'uci\nquit\n' | <binary> | grep "^id name"`.

**`~/bin/qet`**, the engine tester, in that place on every host.

**The nets.** A net is data, not source, so it does not come through GitHub. The engine takes one
through the uci option `NnueFile` with an absolute path, which is how one binary plays with
several nets; `qapla.nnue` in the working directory is only the fallback. Both nets we have -
generation 1 (`test/nnue/nets/net-epoch20.nnue`) and generation 2
(`test/nnue/nets2/net-epoch05.nnue`) - have to be present before a set that uses them starts.

**The opening library** the set is played from, also data: `test/nnue/start-positions-1m.pgn`.
Every set has to be played from the *same* leaves, or the sets cannot be compared, so this file is
copied rather than regenerated - a regenerated book is a different book.

**Enough memory.** The labelling pass needs about 4 GB per chunk of 10,000 games. 31 GB is
comfortable; the chunk size is what to lower on a smaller machine.

**Physical cores, with one left free**, see the note above. `nproc` counts threads, not cores.

### The speed check, and why it is not optional

A vector path that is not compiled in costs a factor and is invisible otherwise - it happened here
twice. `nnueeval` names the path the search actually uses:

    printf 'nnueeval net <a net>\nquit\n' | <the nnue binary>
    ... nnue -10 reference -10 (equal) avx2, used by the search

It also compares the incremental accumulator against a full refresh, which is the `(equal)`.

Then the wmtest at depth 18, concurrency 4, for both binaries. Two things have to come out of it:

| host | path | hce | nnue | nnue costs |
|---|---|---|---|---|
| Mac mini M4 | neon | 17.4 M nps | 9.6 M nps | 1.81 x |
| Linux x86_64 | ssse3 | 8.85 M nps | 4.77 M nps | 1.86 x |
| Linux x86_64 | avx2 | 8.85 M nps | 5.82 M nps | 1.52 x |

**The node count is the cross-host check.** HCE gives exactly 248,740,566 nodes on both hosts, and
the two vector paths of the nnue build gave exactly 461,405,288 - the paths are bit-exact against
each other. A different number on a new host means a different build, not a faster machine.

**The ratio is the speed check.** The nnue may cost around 1.5 to 1.9 times the hce per node. Far
above that, its vector path is not active - ask `nnueeval` before blaming the machine.

`-march=x86-64-v2` is what the Makefile builds for on x86, and that excludes avx2. The release has
to run on old hardware, so this is right for the release; a helper binary that only ever runs on
one known machine may be built with `EXTRA_DEFINES="-DQAPLA_USE_NNUE -mavx2"` and is 22 % faster,
identical to the node.

## The pipeline that runs these steps

`src/pipeline/pipeline.py` with `src/pipeline/pipeline.toml`. Every value is in the toml - hosts,
engines, nets, depths, files, which step runs where - and the logic of the step kinds is in the
script, because chunking, resuming and the shape of a settings file are logic and not settings.

    python3 src/pipeline/pipeline.py status              what is done, what is next
    python3 src/pipeline/pipeline.py smoke --games 200   the same steps, small, checked
    python3 src/pipeline/pipeline.py run                 this host's pending steps
    python3 src/pipeline/pipeline.py launch              start another host's steps over ssh

Implemented kinds: `play` and `label`. A step of a kind that is not implemented stops the run with
a note instead of being skipped.

**Every step checks itself, twice.** Before it starts, each engine it needs has to answer `uci`
with a version, and an engine that carries a net has to load it - `nnueeval` reports the net, the
agreement between the incremental accumulator and a full refresh, and the vector path in use. A
step that runs for hours must not be the thing that discovers a missing net. Afterwards the pgn is
read back: games, plies, notation, a result per game, a value per ply for a labelled one, and the
spread of outcomes. A template whose games all end the same way fails the check.

**It reports while it runs.** A playing step writes a line every ten minutes with the games done,
the rate and the time left, counted from the tester's own log. Everything goes to
`test/log/pipeline.log` and the state to `test/log/pipeline-state.json`, both on the host that does
the work.

**Remote steps are autonomous.** `launch` starts the step over ssh inside `( setsid nohup ... & )`
and returns. The process lands in a session of its own and is reparented to init when the
connection closes, so the machine that started it may sleep or be switched off. Verified by
dropping the ssh session and watching the run continue.

**Work done before the pipeline existed is marked `external`** in the state file and never started
again - set 1 was played and labelled by hand, and a second start would write into the same files.

### Measured while starting set 2

The smoke test at 200 games said 5.3 games/s; the real run at ten minutes says 23.6. That is the
third time today a short sample was wrong by a factor, in both directions. A rate is worth
believing after minutes of a real run, not after a smoke test - the smoke test is there to prove
the settings, not the speed.

nnue-1 plays games of 129.8 plies, the hand crafted eval 140.0 - so set 2 holds about 130M
positions against set 1's 140M, slightly fewer rather than more. An earlier version of this file
had it the other way round, from the broken ply count described above.

## A spot instance as a third helper

Measured on 28.09.2026 with a c7g.8xlarge in us-east-1, labelling chunks 50 to 101 of set 1.

| | |
|---|---|
| instance | c7g.8xlarge, 32 graviton3 cores, 61 GB, arm64 |
| spot price | 0.411 $/h, bid capped at 0.60 |
| image | `<IMAGE>`, Ubuntu 24.04 arm64 with clang 18 |
| rate | about 3,880 positions/s at concurrency 31 - 361 s for a chunk of 10,000 games |
| memory | 1.1 GB peak for a chunk, so the chunk size is not what limits this host |
| cost | 52 chunks in 5.2 h, about 2.14 $, and it takes 13.5 h off the mac |

**The spot vCPU quota of this account in us-east-1 is 32**, so c7g.8xlarge is the largest that can
be had. That quota, not the price, is what decides the instance.

It counts the vCPUs of *all* running spot instances of the standard families in the region
together, not the size of one. Proven rather than assumed: while the 32 core instance was running,
a request for a single c7g.medium - one vCPU - was refused with `MaxSpotInstanceCountExceeded`.
Nothing is created by a refused request, so the check costs nothing. Two smaller instances are
therefore not a way around it; another region is, because quotas are regional, and so is a raised
quota. An increase to 128 vCPU was requested on 28.09.2026
(`f47792eb61d249ddb069c1ff33a9d614PhpB30a4`), which is what would matter for the remaining sets:
labelling all nine of them is about 25 days on one 32 core instance.

**A graviton3 core does about three quarters of an apple core here**: 125 positions per second
against 166, both averaged over the cores of the machine. The wmtest at concurrency 4 took 31.9 s
against 14.9 on the mac and 28.1 on the linux box.

**The node count is what proves the engine is the same one: 248,740,566 on all three hosts.** The
version string did *not* match - the fresh clone lacks the 0.5.0 tags, so `git describe` produced
`0.4.0-141-g5a744dd`. A version string is evidence about a build, the node count is evidence about
an engine, and here only the second one was available.

### What an instance needs that a long-lived host already has

Every one of these cost a failed attempt, and every one is silent until it bites:

**The gitignored directories.** A fresh clone has no `test/log`, `test/epd/log`, `test/nnue`. The
newer tester validates that a log path exists and refuses to start - which is the friendly version;
an older one would have written nowhere.

**The gitignored data.** `test/epd/wmtest.epd` is data (`*.epd` is ignored), so it does not come
with the clone. It goes through s3 like the nets and the book.

**The tester built from the branch that has the feature.** The image carried a checkout of the
tester's `0.5.0`, which builds fine and then says `"analysis" is not a valid parameter group`. The
reverse analysis lives on `0.7.0`. A tool that builds is not a tool that can do the job.

**The tester needs clang**, not gcc: its cmake passes `-stdlib=libc++`, and `c++` rejects it.

**A checkout that refuses to switch branches.** The image had a local change to the `Makefile`, so
`git checkout nnue` aborted - and the build that followed produced `Qapla 0.4.0` from the old
branch without a word. `git checkout -- . && git clean -fd`, then check the built binary's version
against the commit before using it. `delivery/deployment.md` says exactly this, and it was still
worth learning again.

### Handing work over as it finishes

A spot instance can be taken away with two minutes' notice, so the labelling step works chunk by
chunk and puts every finished chunk into s3 at once. **The existence of the result object is the
marker that a chunk is done**, so an instance that replaces a lost one skips what is already there
and loses only the chunk that was in flight. A handover that fails stops the run rather than
piling up results that exist in one place only.

Access is an IAM role on the instance, scoped to the one bucket, so nothing carries a key.

**A watchdog terminates the instance when the run is through**, but only if all 52 results really
are in s3; a failed run leaves the machine up to be looked at. For a one-time spot request the
shutdown behaviour is already `terminate` and cannot be set to anything else.

Chunks 50 to 101 carry a `.done` marker on the mac as well, so the two machines do not do the same
work twice. The two halves are concatenated afterwards - the mac holds 1 to 49 in one appended pgn,
the instance writes one file per chunk.

## Decisions about the packed game file

**Games longer than 255 plies stay cut off at 255.** The format has one length byte per game, and
about 2.4 % of the games of set 1 are longer than that, so they lose their tail from move 128 on.
That is deliberate and not a defect to be fixed: a long game holds many positions that barely
differ from one another, so it already contributes far more volume per unit of complexity than a
middlegame does. If anything, cutting *more* is the experiment worth running, not cutting less.
This would only change with layer stacks that have endgame buckets of their own, where those
positions would train their own parameters.

**Mate scores are kept, as a win probability.** `{M4}` and `{-M3}` become ±30000 in the engine's
unit and go through the sigmoid like any other value, so the sharpest positions in the file carry
a probability close to 1 or 0 rather than no value at all.

**The value of the last move of a drawn game is kept.** The tester writes the reason a game ended
behind the value - `{+0.01, Draw by threefold repetition}` - and the trailing comma used to make
the converter drop the value, which hit 23.5 % of the games of set 1 and exactly the positions
that show what a draw looks like. Fixed on 28.09.2026; on 20,000 games it was 4,445 moves without
a value before and none after.

**Conversion costs nothing worth planning around:** 20,000 games in 32 s on a busy machine, 13.8 s
of that cpu, so about 27 minutes for a million games next to other load and 12 on an idle machine.
Two variants of the same games cost twice that, which is still nothing against the 26 hours of
labelling.

## How many cores are worth buying: measured on 96

A c8g.24xlarge with 96 graviton4 cores played chunks of 20,000 games of set 4 - nnue-1 against the
hand crafted eval at depth 6, both colours per opening - one chunk per setup, so the time per chunk
is the answer.

| setup | games | time | games/s | of linear |
|---|---|---|---|---|
| 1 qet, concurrency 30 | 20,000 | 197 s | 101.5 | reference |
| 1 qet, concurrency 60 | 20,000 | 123 s | 162.6 | 80 % |
| 1 qet, concurrency 90 | 20,000 | 114 s | 175.4 | 58 % |
| 3 qet, concurrency 30 each | 20,000 | 99 s | 202.0 | 66 % |

**Three drivers beat one by 15 %, and that is the smaller half of the story.** One qet driving 90
games is a bottleneck, so splitting it up helps - but three of them still reach only two thirds of
what three times the cores should give. The machine itself does not scale here: at depth 6 an
engine answers in well under a millisecond and every answer is a context switch plus a round trip
over stdin and stdout, so with 90 engines the scheduling and the pipe traffic become the limit, not
the cores. More drivers do not change that, they only stop one of them being the narrowest point.

**So the conclusion is to stay with 32 core machines**, several of them rather than one large one.
The price says the same, though less reliably: the cheap quotes are in the availability zones that
have no capacity. c8g.24xlarge was quoted at 0.8641 $/h in us-east-1c and 0.9794 in us-east-1f,
and neither had any; it was delivered in us-east-1b at 1.3815 $/h, which is 14.39 $ per 1000
vCPU-hours against 12.78 for the c7g.8xlarge we were already running. The spread between the zones
was a factor of 2.8, and the default subnet lands in an expensive one - an availability zone has to
be chosen, not left to chance, and then checked for capacity.

At 30 cores the machine did 101.5 games/s. Whether a 32 core c7g reaches the same per core is not
measured, only assumed; what is measured is that going wider on one machine does not pay.

## For the next set: two things in the tester that change the shape of this

Both were reported out of this work and are on the tester's `0.7.0` branch. The runs of 28.09.2026
were started before them and were deliberately left alone; from the next set on they are the way to
do it.

**`--pgnoutput perround=true` writes one file per round** (`06cf4d3`): `file=games.pgn` becomes
`games-round-001.pgn`, `games-round-002.pgn` and so on, and a game lands in its round's file as
soon as it is finished. Since the opening index runs on across round boundaries - checked: round 1
takes openings 0 and 1, round 2 takes 2 and 3 - a *single* qet call can walk the whole library and
leave finished files behind it as it goes.

That replaces the chunked playing entirely. Instead of 101 calls with `start=` and `games=`, one
call with `games=200 rounds=10000` produces 10,000 files of 200 games, an uploader outside picks up
whatever appears, and the granularity at risk on a spot reclaim falls from six minutes to seconds.
The 101 process starts we pay today disappear with it.

**`--analysis` reads its pgn one game at a time** (`b226240`) instead of holding all of it. That was
the defect that killed the labelling of a 1.04 GB template with SIGKILL and forced the chunks in the
first place. So the reason for chunking the labelling is now only the spot instance: a chunk is a
unit that finishes and can be handed over. On a machine that cannot be taken away, a labelling pass
may be given the whole set in one go again.

## What set 1 answered: the result of the game hurts here

Two nets from the identical games, one trained with the result of every game and one without it,
both stopped by their own held-back loss at epoch 8, then a round robin of 1000 games per pairing at
10+0.05 on 29.09.2026:

| | Elo | +/- | score | draws |
|---|---|---|---|---|
| Qapla-HCE | 2714.2 | 15 | 72.60 % | 21.4 % |
| nnue, trained without the result | 2582.6 | 13 | 46.58 % | 27.2 % |
| nnue, trained with the result | 2503.2 | 14 | 30.83 % | 24.6 % |

**Without the result is 79 Elo better**, and head to head over 1000 games it is 463 wins to 232 with
305 draws, 61.6 %. The error bars are 13 and 14, so this is not a fluctuation.

### Where it does not come from

Volker found 79 Elo too large to be a property of the data and asked for the whole path to be
audited. It was, and nothing in it is wrong:

- **The result is on the right perspective.** Over 2.8 million positions the mean value is 0.5058
  and the mean result 0.5014, their correlation is 0.7386, and the result rises monotonically with
  the value: 0.004 where the search says 0.00-0.10, 0.492 at 0.45-0.55, 0.996 at 0.90-1.00. Split by
  ply the result is 0.500 everywhere. A turned sign would be plain in any of those numbers.
- **The loss masks correctly.** `weight = blend + (1-blend)*(1-counts)`, so at counts = 0 the weight
  is one and the result falls out entirely - the variant without it trains on the value alone.
- **Nothing is clipped by the quantization**, in either net, and no weight sits at the clamp that
  keeps them inside a signed byte: 2 of 16,384 above 90 % of it in one net, none in the other.

And one explanation that looked good and is wrong. I argued the noise of the result cannot average
out because 87.7 % of positions occur only once. Volker pointed out that the averaging unit is not
the position but the weight - a feature is piece by square by own king square, and every position
holding that feature acts on its weights. Measured over the set: the median used feature is touched
by 7,425 positions, the tenth percentile by 555, and features with fewer than a hundred hits carry
under one percent of all activations. A deviation of 0.1 over 7,425 samples is 0.0012. The noise
does average out, and it cannot account for 79 Elo.

### What it does come from, as far as it is measured

The nets predict the search value differently well. Mean square error against the value, on the same
held-back games and the same metric for both: **0.005802 with the result, 0.004463 without** - 30 %
worse. So the result term is not merely noisy, it is a different target, and in a net this small -
256 accumulator, 32 hidden - it competes for capacity with the one that matters. A larger net would
pay less for it.

That is a measurement, not yet an explanation. What would separate a question of dosage from one of
capacity is a run at `blend 0.95`, five percent result instead of twenty to thirty: if the damage
scales with the weight it is dosage, and if it stays it is capacity.
