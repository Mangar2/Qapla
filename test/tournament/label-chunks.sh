#!/bin/sh
# Runs the labelling pass over the chunks of a playing template, one after another.
#
# qet holds a whole analysis in memory - 565 MB for 5000 games - so a template of a million games
# cannot be handed to it in one piece; it is killed. The chunks are the way around it, and they
# make the pass resumable, which a single analysis is not: every finished chunk gets a .done file
# beside it and is skipped on the next call.
#
#   sh test/tournament/label-chunks.sh <chunk dir> <labelled pgn> <engine>
#
# Stop it at any time and call it again with the same arguments.
set -e
DIR=${1:-test/nnue/chunks}
OUT=${2:-test/nnue/games-hce-depth6-labelled.pgn}
ENGINE=${3:-<REPO>/new-versions/Qapla-0.5.0-027-20-hce}
INI=test/tournament/label-hce-depth8.ini
LOG=test/log/label-chunks.log

for chunk in "$DIR"/chunk-*.pgn; do
    [ -f "$chunk.done" ] && continue
    echo "$(date '+%H:%M:%S') $chunk" >> "$LOG"
    # Only the input file is overridden. The output file, its notation and which comments it
    # carries stay in the ini: a command line group may fall back to defaults for the keys it
    # does not name, and the defaults here are wrong - san instead of lan, clock and depth on.
    ~/bin/qet --settingsfile="$INI" \
        --analysis pgn="$chunk" \
        --engine name=HCE cmd="$ENGINE" >> "$LOG" 2>&1
    touch "$chunk.done"
done
echo "$(date '+%H:%M:%S') all chunks labelled" >> "$LOG"
