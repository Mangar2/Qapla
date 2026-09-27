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
the plies the training data otherwise has none of, because every game starts at a leaf. Long
algebraic notation and not short, because the converter that reads the labelled pgn back has no
move generator, see `src/trainer/convert.py`.

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

The pgn holds the moves and nothing else - no evaluation, no clock, long algebraic notation. It
is the template a later pass analyses; the values come from that pass, not from the engine that
played.

**The run may be stopped and continued** with the identical call. `[tournament] file=` holds the
state and is written every 10 s, and the pgn is appended to rather than overwritten.

### Things worth knowing before starting one of these

**Leave a core free.** The run uses concurrency 9 on a machine with 10 cores. qet is written
tightly, but at depth 6 a game is over in a moment and the turnover between engine processes is
so frequent that qet needs a core of its own to keep up: it drives every process, writes the pgn
and keeps the state file. Claiming every core starves the driver.

**Next time start it with `rapid=true`.** The `info` lines of the engine are not needed here -
nothing reads a node count or a depth out of this run, and the template carries no evaluation.
Switching them off saves the traffic and the parsing. One thing to check when doing it: the draw
adjudication decides on a score, and where it takes that score from with the info lines gone is
not something this run establishes - it ran with `rapid` off.

**qet ignores `active=false` in the `[resign]` block.** The block has to be *absent*, not
disabled. Measured with 30 games each: without the block 27 mates and 3 repetitions, with
`active=false` 26 games cut short by adjudication. The parser also demands `movecount` inside a
block it is being asked to switch off. So won games are played to their end only when no
`[resign]` block exists at all - and those late positions are the ones a net has the least of.
A first run of 7145 games was thrown away over this, see
`test/nnue/games-hce-depth6.discarded-resign-adjudication.pgn`.

**Cost of playing games out:** 43 games/s with win adjudication, 31 games/s without it, at
concurrency 9. The second figure is the one that counts, and it puts the 1,000,020 games at
about 9 hours.

**How the games end** (first 2043 of the run): 1517 by mate (74 %), 425 by threefold repetition,
37 by insufficient material, 34 by draw adjudication, 29 by the 50 move rule, 1 by stalemate. No
game decided by adjudication, which is what the missing `[resign]` block is for.
