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

**On x86 every binary of ours is built with `NATIVE=1`.** The Makefile default is
`-march=x86-64-v2` so that the release starts on old hardware, and that excludes avx2 - the nnue
then runs its ssse3 path and is 22 % slower, node for node the same. `NATIVE=1` switches the
optimized builds to `-march=native`, which is what the ARM branch of the Makefile has always done;
that is why the Mac was never affected and why this was easy to overlook on Linux for a while.

    make Release -j NATIVE=1 EXTRA_DEFINES="-DQAPLA_USE_NNUE"

Ask `nnueeval` which path a binary took before trusting a measurement or a tournament made with it.

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


## Where training is limited, and what machine it wants

Measured on 29.09.2026, on the same net and the same data:

| | positions/s |
|---|---|
| training on the mac, over its gpu | 220,000 |
| the gpu alone, fed a fixed batch with no data cost | 206,000 |
| the gpu alone at a batch of 65,536 | 235,000 |
| the data loader alone, six worker processes | 428,000 |
| training on a 32 core graviton3 instance, cpu only | 106,000 |

**Corrected on 01.10.2026: this conclusion was wrong.** The compiled loader (below) took the same
training on the same mac from 189,000 to 326,000 positions a second. What limited it was the way the
python loader handed its batches over: six worker processes pickling arrays into the main process,
whose time the training loop needed. The loader alone measured 428,000 because nobody was training
at the same moment. The paragraph that follows is kept as it was written.

**The training is limited by the gpu, not by the data path.** The loader has almost twice the
headroom it needs, and feeding the gpu a fixed batch with no reading and no feature building at all
changes nothing. Raising the batch from 16,384 to 65,536 reaches the ceiling of 235,000 and gains
13 % - but a four times larger batch is a different optimisation, not free speed: it needs the
learning rate adjusted, and it makes a run incomparable with one trained at the old size.

It is not the per step overhead either: from a batch of 4,096 to 16,384 the rate rises by half, and
from 65,536 upwards it does not rise at all. So a gpu with more memory bandwidth would help roughly
in proportion, and what it would help with is most likely the backward pass of the feature
transformer, which scatters gradients into a table of 11.5 million parameters.

**The cpu instance wants nine cores, not thirty-two.** Six loader processes and one that computes:
load 9 of 32, and three quarters of the machine idle. c7g.4xlarge at 0.21 $/h is the size for it
against 0.41 for the 32 core one - the first run of it was started on 32 by carrying the type over
from the playing hosts, where 30 concurrent games really do need them. A playing or labelling host
wants the cores; a training host does not.

### A gpu instance, measured on 01.10.2026

`g6.2xlarge`: one NVIDIA L4, 8 vCPU (4 cores with two threads each), spot at about 0.91 $/h. The
deep learning image (PyTorch 2.7, CUDA 12.8) keeps torch in `/opt/pytorch/bin/python3`, not in the
system python; `train-instance.sh` looks for it. On the six-set corpus, 1.03 billion positions an
epoch:

| | positions/s |
|---|---|
| the mac, four sets, while the tournament machine was being driven from it | 166,000 - 171,000 |
| the L4 instance, eight loader processes | 209,000 |

The L4 was **54 % busy** at that rate, so here the loader is the limit and not the gpu - the
opposite of the mac, whose stronger cores feed six processes at 428,000. How the loader scales on
that instance, one file, three minutes each:

| loader processes | positions/s | |
|---|---|---|
| 2 | 72,500 | |
| 4 | 116,900 | × 1.61 |
| 8 | 196,600 | × 1.68 |

It scales well with cores, so a 32 vCPU instance would feed about 500,000. But the L4 itself caps out
near 390,000 - 54 % busy at 209,000 - which is 2.3 times the mac. **The bar for moving training off
the mac was three times its speed**, so an L4 does not clear it whatever the core count, and
`g6.8xlarge` carries the same L4. Only a 32 core machine with a faster gpu could: `g5.8xlarge` with an
A10G or `g6e.8xlarge` with an L40S. Neither was tried: the spot quota for G instances was 8 vCPU, a
second request is refused while the first is open, and at the time of measuring no zone had A10G
capacity at all.

The training stays on the mac. Two things are in place should that change: `train.py --resume`
continues from the newest checkpoint with the optimizer and the stopping rule restored, so a reclaim
costs one epoch instead of the run, and `aws-resume.sh` brings up a gpu machine with the host key
`train`, the image in `QAPLA_IMAGE`, and the sets named in `QAPLA_FILES`.

## The compiled loader, 01.10.2026

`src/trainer/native/batcher.cpp`, built on first use into `build/trainer/batcher` by `gamedata.py`.
It reads the game files through `mmap`, so every stream of a training shares one copy in the page
cache, replays the games, builds the HalfKA features and writes whole batches to a pipe. `train.py
--loader native` is the default; `--loader python` keeps the old one for comparisons.

What stays in python is everything that makes two runs comparable: the index, the split into training
and validation, and the order of the games, drawn with numpy from the same seed as before. Only the
shuffle inside the buffer is the loader's own. Verified:

| check | result |
|---|---|
| features, position by position, against `_positions_of` - 3,612 games of all six sets including the first and the last of each file | 433,030 positions, 0 differences |
| the same after the change to `mmap`, sets 1, 5 and 6 | 146,584 positions, 0 differences |
| the index against the stored python index, sets 1 to 4 | identical, 0.3 s per file instead of about 25 minutes |
| the first buffer of a stream, same seed, against the python loader | 131,069 of 131,072 positions identical; the other three are the remainder each side keeps |
| loss after 2,000 batches, same seed, same number of streams | 0.004485 against 0.004531 |
| one stream alone, set 5 | 3.9 million positions a second on one core |
| a training on the mac, five sets | 326,600 positions a second against 189,500, at a quarter of one core |

`train.py` seeds torch with `--seed` now as well. Before, the initial weights were not seeded, and two
runs on the same data could not be compared step by step: the first comparison of the two loaders
showed an 18 % higher loss for the new one that turned out to be the other starting net.

## The book of 10 M start positions (2026-10-10)

**Built from:** `test/nnue/start-positions-1m.bok`, extended by 9,000,000 leaves, as the 1 M book was from the
100 k one (Volker 2026-10-10: a new HCE-HCE set of 10 M games, one per leaf):

    printf 'nnuebook add test/nnue/start-positions-1m.bok out test/nnue/start-positions-10m.bok \
        leaves 9000000 sd 8 collect 2 margin 100 seed 2\nquit\n' | new-versions/Qapla-bookgen-mac stat

`Qapla-bookgen-mac` is c03335a built with `EXTRA_DEFINES=-DQAPLA_GENERATE_NNUE_DATA` (the search observer).
Seed 2, not 1, so that the walk is not the one that made the 1 M book. 10 min 57 s on the Mac:

    Searches: 46328, candidates: 16920129, rejected in check: 1511730, mate: 8522, off margin: 5077172,
    lines: 10322705, new leaves: 9000000
    Book: 17484675 moves, 10000020 leaves, 9000000 of them new

As the opening library: `nnueopenings test/nnue/start-positions-10m.bok test/nnue/start-positions-10m.pgn`,
10,000,020 lines in 24 s, 1.68 GB. Some old leaves are inner nodes now - their lines run on to new leaves.
Both files on the T7 (symlinked in test/nnue/), the pgn also on Linux.

### Played at depth 9, the play values as labels (2026-10-11)

Volker 2026-10-11: one game per leaf, HCE against HCE (`new-versions/Qapla-hce-linux`, 1 thread), **depth 9**, and
the values of the play itself are the labels - qet writes them into the pgn (`eval=true`, `depth=true`, e.g.
`{+4.66/9}`), no separate labelling pass. Depth 10 was judged too risky as labels. The results are kept, for WDL.

- The 1.68 GB library made qet exit after ~5 min without a word, so it is split into 11 parts of 1 M leaves
  (`test/nnue/book10m-parts/openings-NN.pgn` on Linux, the last one 20 leaves; md5-identical to the Mac split)
  and played one part after the other: `tmp/book10m-d9/run-linux.sh`, ini `play-template.ini`, out
  `test/nnue/games-book10m-d9/part-NN.pgn`.
- Checked on 20 games: the play values are from the side to move, as the labels of `label-chunks.sh`; a
  separate depth-9 labelling of the same games gives close values (mean |p diff| 0.010, sign agreement 97 %;
  not identical, the play keeps its hash between moves). `gamefile --pgn2gam` reads them (`/9`, mates); the
  book plies carry no value.
- Cost on 100 games (Mac): depth 10 ~8.5x depth 6, depth 9 ~0.65x depth 10. Linux, concurrency 30: 12 games/s,
  ~9.5 days for the 10 M. Started 2026-10-11 04:27.

## Set 1b: set 1's book again, HCE-HCE with 2 threads (2026-10-11)

1,000,333 games (the pause and resume of the play replayed a few leaves), depth 6, 2 threads per engine so that
every game is a new one; labelled with `Qapla-hce-linux` at depth 8, 1 thread, as set 1. Converted on the Mac:
`test/nnue/dataset/set1b-hce-t2-depth6.gam` and `-nowdl.gam`, 139,007,738 plies with a value, 21,958 games
truncated at 255 plies.

### The 10 M book with W/D/L counts (2026-10-17)

Volker 2026-10-17: the book of the new set trained with the results of its 10 M games -
`test/nnue/start-positions-10m-wdl.bok` (on the T7, 245 MB), payload `qapla-wdl-count-v1` (three uint32 per node:
wins, draws, losses of the side that plays the node's move). Built on the M4 with chess-tools 26534de:
`gamefile --bookwdl book=<9-bit book of start-positions-10m.pgn> out=... in=games-book10m-d9-clean/part-01..10.pgn`
(138 s). 10,000,020 games read and counted, none without a result, none outside the book; 1-0 3,790,628,
0-1 3,667,526, 1/2 2,541,866; 179,477,454 counts in book nodes. Read back with `--book2pgn values=true`.

The same book as rate and significance (Volker 2026-10-17): `test/nnue/start-positions-10m-wdlrate.bok` (on the T7,
70 MB), payload `qapla-wdl-rate-v1` - byte 1 the score (W + D/2)/N of the side that plays the move on 0..255
(0 and 255 only at exactly 0 % / 100 %), byte 2 floor(log2 N) + 1. Built with chess-tools c7cdf9e,
`gamefile --wdl2rate in=start-positions-10m-wdl.bok out=...` (reproduced byte for byte). Of the 604,279 nodes with
16 games or more, 12,433 were always lost and 13,873 always won; of the 16,880,396 nodes with 1-15 games,
6,446,085 and 4,835,940 (13,829,821 nodes carry a single game).

Created from the evaluated book as template (Volker 2026-10-17, chess-tools 03f8e4c):
`gamefile --bookwdl book=start-positions-10m-eval.bok out=start-positions-10m-eval-wdl.bok
rate=start-positions-10m-eval-wdlrate.bok in=games-book10m-d9-clean/part-01..10.pgn` (138 s, M4; both on the T7,
symlinked in test/nnue). `-eval-wdlrate` (`qapla-score-wdl-rate-v1`, 4 bytes: int16 score of the position the move
is played from, rate, significance) is the book to use; `-eval-wdl` (`qapla-score-wdl-count-v1`, score + three
uint32) is the source for training it further. Every node carries a score (7,484,656 evaluated positions);
counts and statistics as above. Reproduced byte for byte here.

### The book evaluated again, version 2 - a node is the position after its move (2026-10-17)

The first evaluation gave every node the score of the position before its move, while the W/D/L counts belong to
the position after it - the combined book was shifted by one ply. Re-evaluated on the M4 (qet 0620e6c,
`--bookeval` version 2, engine `new-versions/Qapla-set1-hce`, depth 9, 9 engines, 10:12-18:35): the 7,484,656
values of v1 taken over exactly, the 10,000,020 leaves searched. Node counts and scores of the M4 and Linux HCE
builds agree (5 positions at depth 9; 358,841 values of a sample of the book).
- `test/nnue/start-positions-10m-eval-v2.bok` (`qapla-search-score-v2`, start position in header record 7)
- `test/nnue/start-positions-10m-eval-v2-wdl.bok` (score + W/D/L counts, the source for training further) and
  `-eval-v2-wdlrate.bok` (score + rate + significance), from `gamefile --bookwdl book=<eval-v2> ...` over the 10
  clean parts (chess-tools 89e3f57) - counts identical to the v1 count book.
- `book10m-d9-bookonce.gam` rebuilt from the v2 book (`--book2gam compact=true`): byte for byte the old file.
- **WDL in the book values** (chess-tools 37d86c0, rule in the nnue-training skill): `book10m-d9-bookonce-wdl-w020.gam`
  (`wdl=w:0.20`, fixed 20 %) and `book10m-d9-bookonce-wdl-k4.gam` (`wdl=k:4`, w = N/(N+4)), both 4,330,676 lines,
  7,484,656 values, no results; 7,472,276 values mixed, 12,380 mate scores unchanged; mean |target - p| 0.051 and
  0.082. On the T7 (dataset) and on the Mac Studio, md5 identical.

### The book of 100 M start positions (2026-10-17, building)

Volker 2026-10-17: more training data will be needed - a book ten times the 10 M book, built and evaluated on the
M4, then split into 10 books (`gamefile --booksplit ... parts=10`, chess-tools 4add7dc), each the start of an
independent set of 10 M games. Built from the 10 M book like that one from the 1 M book, seed 3:

    printf 'nnuebook add test/nnue/start-positions-10m.bok out tmp/book100m/start-positions-100m.bok \
        leaves 90000000 sd 8 collect 2 margin 100 seed 3\nquit\n' | new-versions/Qapla-bookgen-mac stat

19:10-19:51 on the M4: `Searches: 465791, candidates: 176229252, rejected in check: 17402319, mate: 125114, off
margin: 55300469, lines: 103401350, new leaves: 90000000` - **Book: 174,845,166 moves, 100,000,020 leaves**,
`test/nnue/start-positions-100m.bok` (T7, 350 MB). As the opening library: `nnueopenings ...
start-positions-100m.pgn`, 100,000,020 lines in 2 min, 18.9 GB (T7). The 9-bit book for the evaluation again from
the pgn (`gamefile --pgn2book`, 17 min; nodes as the generator's book, only the header differs), then
`qet --bookeval` version 2 on the M4 from 20:11 (9 engines, `Qapla-set1-hce`, depth 9; 174,845,167 positions,
~460 a second at the start - 4 to 7 days with the M4's throttling; out `tmp/book100m/start-positions-100m-eval-v2.bok`,
log `tmp/book100m/bookeval.log`, resumable with the identical call).
Moved to the Mac Studio on 2026-10-18 12:08 (12 engines, `Qapla-hce-53f972f-studio`, the same node counts as the M4's
and Linux's HCE builds) and finished there on 2026-10-20 18:32: **174,845,167 positions evaluated**,
`test/nnue/start-positions-100m-eval-v2.bok` (T7, 699 MB, md5 fe730ecc657ed16a8dafa802c8b8a254).

**Split into 10 books** (`gamefile --booksplit ... parts=10`, chess-tools 4add7dc, 17 s): `test/nnue/book100m-parts/
start-positions-100m-eval-v2-01..10.bok` (T7, 127 MB each, also on the Mac Studio in `tmp/book100m/parts`, md5
identical) - 10,000,002 lines each, 100,000,020 together; leaf i of the book goes with its line into part
(i mod 10) + 1, so every part has all opening moves alike. Each part carries the scores of the book.

### Set from book part 01 - playing (2026-10-20)

Volker 2026-10-20: part 01 played on the Mac Studio like the 10 M set - one game per leaf, HCE against HCE
(`Qapla-hce-53f972f-studio`, 1 thread), depth 9, the play values as labels and the results kept for WDL; qet reads
the 9-bit book directly as its opening file (qapla-engine-tester 0620e6c, `--openings file=<part>.bok`), 12 games at
a time. `tmp/book100m-d9/play-part.sh 01`, ini from `tmp/book100m-d9/play-template.ini`, out
`test/nnue/games-book100m-d9/part-01.pgn`, state `tmp/book100m-d9/part-01.state` (resumable). Started 18:45; qet
needs ~100 s to read the 10 M lines before the first game. Checked on the first games: the book line is played
without values, then every move carries its depth-9 value (`{+0.12/9}`), the result stands as the final token.
**The M4 plays the last quarter of part 01** (Volker 2026-10-20): openings 7,500,002-10,000,002 (1-based). qet holds a
whole 10 M part book in memory and was killed for it on the M4 (17 GB; exit 137 after ~75 s), so these 2,500,001
lines are written as three opening pgns (`gamefile --book2pgn`, then cut: `tmp/book100m-d9/openings-01b-1..3.pgn`,
833,334 / 833,334 / 833,333 lines, the first being line 7,500,002) and played one after the other by
`tmp/book100m-d9/play-01b.sh` (9 games, `Qapla-set1-hce`, depth 9) into `test/nnue/games-book100m-d9/part-01b.pgn`.
The Studio run is stopped by `tmp/book100m-d9/stop-01-at.sh` once it starts opening 7,500,001 (0-based); the games
cut off then are found by their book line when the two pgns are joined (duplicates removed the same way) and
played afterwards.
Both runs were stopped on 2026-10-20 22:55 (Volker): the Studio at 215,896 games, the M4 at 20,126 of
`openings-01b-1.pgn` (openings started up to 7,520,136). **qet fix 3bc23c1** (chess-tools-cf): a book as opening
file is kept as its tree, a line built only when its game starts - a 10 M part book now takes ~300 MB instead of
over 6 GB (M4: peak 305 MB, 18 games from start=7500002 in 3 s, exactly book lines 7,500,002-7,500,019). The M4
goes on from the book since 2026-10-21 01:15: `tmp/book100m-d9/play-01b-book.ini`, start=7520137, 2,479,866 games,
appended to `part-01b.pgn` (state `part-01b-book.state`); the 9 games cut off at the stop are found at the join.
The Studio resumes its state once the fix is on GitHub.
The Studio resumed on 2026-10-21 03:23 with qet 3bc23c1 (pushed), 261 MB, from its state.

### The part books pruned of the leaves the 10 M games played (2026-10-21)

The 100 M book was grown out of the 10 M book instead of being built anew, and its new leaves lie on the old games:
**9,809,232 of the 10,000,020 games of the 10 M set run exactly through a leaf of the 100 M book** (9.8 % of its
leaves; no game through two) - start positions whose games have been played. Counted by following every game
through the book (`tmp/q1prep/leafhit.cpp`, checked against Python on a sample). Volker 2026-10-21: remove them
from the 10 part books, every leaf with its line back to the branching - **`gamefile --bookprune`** (chess-tools
e65b23f; checked on 20,000 games: exactly the 1,960 leaves they reach gone, the other lines in their order with
identical values, none of the games reaching a leaf of the result). `tmp/q1prep/prune-parts.sh` on the M4, all 10 M
games against each part, ~5 min a part: `test/nnue/book100m-parts-pruned/start-positions-100m-eval-v2-NN-pruned.bok`
(T7). Part 01: 981,155 of 10,000,002 leaves removed, 9,018,847 kept, 28,871,127 of 31,720,966 nodes.

**Part 01 goes on from its pruned book** (Volker 2026-10-21): both runs stopped at 15:43, the next opening mapped to
the pruned book by walking both books' leaves side by side (`tmp/q1prep/map-index.py`; the first new games checked
to start from exactly the expected pruned leaves), a margin of 300 (Studio) and 200 (M4) openings before the last
one started so that no game cut off is lost:
- Studio: `tmp/book100m-d9/play-01c.sh`, ini `play-01c.ini`, pruned leaves 755,159-6,759,852 (original 839,141 to
  7,500,001, 0-based 839,140 on), 6,004,694 games into `part-01c.pgn`; it ends exactly before the M4's range, no stop
  watcher needed. `part-01.pgn` holds 839,538 games (original openings up to 839,440 started).
- M4: ini `play-01d.ini`, pruned leaves 7,094,209-9,018,847 (original 0-based 7,868,920 on), 1,924,639 games into
  `part-01d.pgn`. `part-01b.pgn` holds 369,102 games (original openings up to 7,869,120 started).
At the conversion the four pgns are joined by book line: games of removed leaves and duplicates out, missing leaves
played afterwards.

### Quarter 1 of part 01 and the 100 M book as training data (2026-10-22)

Quarter 1 = the pruned leaves 0-2,252,715 (original 0-2,499,999; first moves a3, b3, c3, d3, e3 and part of f3 - the
book is ordered by the first move, so a quarter holds some opening families only). Collected on the Mac Studio at
21:41 by `tmp/book100m-d9/q1/collect-quarter.py` from `part-01.pgn` and `part-01c.pgn`: every leaf once, **2,252,716
games, none missing**; left out 84,051 games of leaves removed by the pruning and 426 duplicates of the restarts.
`gamefile --pgn2gam wdl=result` (chess-tools e65b23f, built on the Studio): **`test/nnue/dataset/book100m-d9-p01q1-wdl.gam`**
(Studio), 316,491,310 plies, 268,401,050 with a value, 65,014 games truncated at 255 plies; 1-0 835,299, 0-1 893,422,
1/2 523,995 (37.1 / 39.7 / 23.3 %; part 01 of the 10 M set 35.7 / 38.4 / 25.8 %).

**The book trained is the whole 100 M book** (Volker 2026-10-22), no longer the 10 M book: it holds every position of
the 10 M book too and all of it is evaluated. `gamefile --bookwdl` on `start-positions-100m-eval-v2.bok` with the 10 M
games and quarter 1 as game files (the same call on the 10 M book reproduces `start-positions-10m-eval-v2-wdl.bok`
byte for byte): 12,252,736 games counted; then `--book2gam compact=true wdl=k:4`:
**`test/nnue/dataset/book100m-d9-bookonce-wdl-k4-q1.gam`** (Studio, 2.8 GB) - 43,538,915 lines, **74,845,147 positions
once each** (final1's book file had 7,484,656), 13,138,217 of them mixed with the results of their games (mean
|target - p| 0.078), 61,537,134 without a game keep their depth-9 value, 169,796 mate scores unchanged.

**Quarter 2 of part 01** (2026-10-24 18:27, Studio): pruned leaves 2,252,716-4,507,086 from `part-01c.pgn`, **2,254,371
games, none missing, no duplicate**; `test/nnue/dataset/book100m-d9-p01q2-wdl.gam`, 314,206,493 plies, 267,241,497 with a
value, 61,116 truncated; 1-0 866,775, 0-1 869,904, 1/2 517,692 (38.4 / 38.6 / 23.0 %). Trained together with quarter 4
(Volker 2026-10-24).

**Quarter 4 of part 01** (2026-10-25, M4): pruned leaves 6,759,852-9,018,846 from `part-01b.pgn` (old book, 34,570 games
of removed leaves left out) and `part-01d.pgn` (185 duplicates of the restart); the 9 leaves cut off at the stop of the
pgn-based run played afterwards (`tmp/q1prep/play-missing.ini`, same engine and settings): **2,258,995 games, none
missing**. `book100m-d9-p01q4-wdl.gam` (Studio dataset, md5 2cb38a40...), 312,094,805 plies, 263,893,948 with a value,
60,654 truncated; 1-0 827,656, 0-1 925,144, 1/2 506,195 (36.6 / 41.0 / 22.4 %).
**Book file for step 2:** `--bookwdl` on the 100 M book with the 10 M games and quarters 1, 2, 4 - 16,766,102 games -
then `--book2gam compact=true wdl=k:4`: `book100m-d9-bookonce-wdl-k4-q124.gam`, 74,845,147 positions, 22,220,900 of them
mixed with their results. The M4 plays quarter 1 of part 02 since 2026-10-25 02:56 (`part-02a.pgn`).

**Quarter 3 of part 01 - part 01 complete** (2026-10-26, Studio): `part-01c` ended 11:36 with all 6,004,694 games (exit
0); quarter 3 = pruned leaves 4,507,087-6,759,851, **2,252,765 games, none missing**; `book100m-d9-p01q3-wdl.gam`,
302,987,063 plies, 254,013,769 with a value, 51,812 truncated; 1-0 1,062,395, 0-1 755,330, 1/2 435,040 (47.2 / 33.5 /
19.3 % - other first moves). The four quarters hold 9,018,847 games, every leaf of the pruned part book once. The Studio
plays quarter 2 of part 02 since 11:37 (`part-02b.pgn`, `play-02b.ini`). Book file for step 3: `--bookwdl` with the
10 M games and all of part 01 (19,018,867 games), `book100m-d9-bookonce-wdl-k4-q1234.gam`, 26,763,365 of 74,845,147
positions mixed with their results.
