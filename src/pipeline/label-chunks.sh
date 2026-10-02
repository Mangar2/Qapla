#!/bin/sh
# Labels the chunks of a set one after the other, with any engine, on the machine it runs on.
#
#     sh src/pipeline/label-chunks.sh <chunk dir> <output dir> <qet> <engine binary> <engine dir> \
#         <depth> <concurrency> [first chunk] [last chunk]
#
# The range splits a set between machines: each labels the chunks of its own range, numbered as in
# chunk-0051.pgn, and leaves the others alone. Without it, every chunk of the directory.
#
# What src/pipeline/pipeline.py does in its label step, without the configuration around it: the
# pipeline knows an engine by a name in local.toml and starts it in its own directory, and an engine
# with the stockfish evaluation reads its nets from nnue/ under the directory it runs in - so it needs
# a working directory of its own, which qet's dir= gives it. Written for the windows machine, where it
# runs in the msys bash, but nothing in it is windows.
#
# Every chunk is one qet call with the ini of the label step. A chunk whose output holds as many games
# as its input gets a .done marker, and a chunk with a marker is not touched again: stopping and
# starting the script loses at most the chunk that was running.
set -u
CHUNKS=${1:?chunk directory}
OUT=${2:?output directory}
QET=${3:?qet}
ENGINE=${4:?engine binary, absolute}
ENGINE_DIR=${5:?working directory of the engine, absolute}
DEPTH=${6:?depth}
CONCURRENCY=${7:?concurrency}
FIRST=${8:-1}
LAST=${9:-999999}
mkdir -p "$OUT"
say() { echo "=== $(date '+%Y-%m-%d %H:%M:%S') $*"; }

number_of() { basename "$1" .pgn | sed 's/^chunk-0*//'; }
mine() { n=$(number_of "$1"); [ "${n:-0}" -ge "$FIRST" ] && [ "${n:-0}" -le "$LAST" ]; }
total=0; before=0
for chunk in "$CHUNKS"/chunk-*.pgn; do
    mine "$chunk" || continue
    total=$((total + 1)); [ -f "$chunk.done" ] && before=$((before + 1))
done
say "$total chunks in $FIRST..$LAST, $before done before"
for chunk in "$CHUNKS"/chunk-*.pgn; do
    mine "$chunk" || continue
    [ -f "$chunk.done" ] && continue
    name=$(basename "$chunk")
    output=$OUT/$name
    ini=$OUT/$name.ini
    # The ini of the label step in src/pipeline/pipeline.py. Long notation and min=false are what
    # src/trainer/convert.py needs: it decodes long notation without a move generator and takes the
    # result from the Result tag, which a minimal pgn does not write.
    cat > "$ini" <<EOF
concurrency=$CONCURRENCY

[each]
tc=depth:$DEPTH
proto=uci

[logging]
path=test/log
engine=false

[analysis]
pgn=$chunk
direction=reverse

[pgnoutput]
file=$output
append=false
min=false
clock=false
eval=true
depth=false
pv=false
notation=lan
EOF
    start=$(date +%s)
    "$QET" --settingsfile="$ini" --engine name=label cmd="$ENGINE" dir="$ENGINE_DIR"
    code=$?
    want=$(grep -c '^\[White ' "$chunk")
    got=$(grep -c '^\[White ' "$output" 2>/dev/null || echo 0)
    if [ "$code" -ne 0 ] || [ "$want" != "$got" ]; then
        say "$name: qet exit $code, $got of $want games - stopping, run again to continue"
        exit 1
    fi
    touch "$chunk.done"
    done=0
    for c in "$CHUNKS"/chunk-*.pgn; do mine "$c" && [ -f "$c.done" ] && done=$((done + 1)); done
    say "$name: $got games in $(( $(date +%s) - start )) s, $done of $total done"
done
say "all $total chunks done"
