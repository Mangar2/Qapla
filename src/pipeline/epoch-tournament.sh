#!/bin/sh
# Plays every epoch of one training against the hand written evaluation, in one tournament.
#
#     ( nohup sh src/pipeline/epoch-tournament.sh set1234 test/nnue/nets-set1-set2-set3-set4 \
#         test/log/pipeline-set1234-train.log >> test/log/epochs-set1234.log 2>&1 & )
#
# Why: a training is stopped by the loss on games it does not train on, and how well that follows
# playing strength is unknown - two runs with the identical loss after two epochs ended 186 elo apart.
# One figure per epoch against one opponent, in one field, makes a curve out of it.
#
# The first epoch starts the tournament and writes the settings into its tournament file; every later
# one is added to that file by join-gauntlet.py. Each pairing is a round of its own, so an epoch plays
# its full number of games before the next one starts - see test/tournament/epochs.ini.
#
# It waits for the line the training writes after saving a net, not for the file: a file that exists
# may still be half written, and a net that is copied half is a net that plays like nothing at all.
#
# It ends when the training has stopped and no further epoch appears. It wants the same fourteen pairs
# as the big gauntlet, so do not start it while that one is playing.
#
# To stop it, stop what it started as well:
#     pkill -f "epoch-tournamen[t].sh"; pkill -f "join-gauntle[t].py"
# Killing the shell alone leaves a join it is in the middle of running on its own, and that join then
# starts this tournament - two seconds before the big one was started by hand, once, so that both ran
# on the same fourteen pairs.
set -u
cd "$(dirname "$0")/../.."
RUN=${1:?usage: epoch-tournament.sh <run name> <nets directory> <training log> [<last epoch>]}
NETS=${2:?the directory the training writes its nets to}
TRAINLOG=${3:?the training log, for the line that says a net was written}
LAST=${4:-20}

STATE=test/log/epochs-$RUN.state
CFG=src/pipeline/local.toml
THERE=$(python3 -c "import tomllib;print(tomllib.load(open('$CFG','rb'))['hosts']['qapla']['ssh'])")
REPO=$(python3 -c "import tomllib;print(tomllib.load(open('$CFG','rb'))['hosts']['qapla']['repo'].rstrip('/'))")
SSH="ssh -n -o ConnectTimeout=20"

say() { echo "=== $(date '+%Y-%m-%d %H:%M:%S') $*"; }

N=1
while [ "$N" -le "$LAST" ]; do
    NN=$(printf %02d "$N")
    NET=$NETS/net-epoch$NN.nnue
    say "waiting for epoch $NN"
    while ! grep -q "net-epoch$NN.nnue" "$TRAINLOG" 2>/dev/null; do
        if ! pgrep -f "[t]rain.py" >/dev/null; then
            say "no training is running any more and epoch $NN never came - that was the last one"
            exit 0
        fi
        sleep 120
    done
    [ -f "$NET" ] || { say "the log names epoch $NN but $NET is not there - stopping"; exit 1; }
    say "epoch $NN is written, handing it over"
    scp -q "$NET" "$THERE:$REPO/test/nnue/epoch-$RUN-e$NN.nnue"

    # Another tournament on that machine would share the fourteen pairs with this one, and a start
    # that fails leaves the watcher believing it succeeded - it did, because it asked whether any qet
    # was running and the big gauntlet was.
    RUNNING=$($SSH "$THERE" "pgrep -x qet >/dev/null && echo yes || echo no")
    if [ "$RUNNING" = yes ] && ! $SSH "$THERE" "pgrep -xa qet | grep -q epochs-$RUN"; then
        say "another tournament is running on $THERE - waiting for it to end"
        while $SSH "$THERE" "pgrep -xa qet | grep -vq epochs-$RUN" &&               $SSH "$THERE" "pgrep -x qet >/dev/null"; do sleep 120; done
    fi
    if $SSH "$THERE" "test -f $REPO/$STATE"; then
        python3 src/pipeline/join-gauntlet.py --state "$STATE" --nets "$NETS" --net "$NET" \
            --name "$RUN-e$NN" || say "epoch $NN did not get in - carrying on with the next"
    else
        say "first epoch - starting the tournament"
        $SSH "$THERE" "cd $REPO && ( setsid nohup ~/bin/qet \
            --settingsfile=test/tournament/epochs.ini \
            --tournament file=$STATE --pgnoutput file=test/log/epochs-$RUN.pgn \
            --engine name=Qapla-HCE cmd=$REPO/new-versions/Qapla-blendtest-hce gauntlet=true \
            --engine name=$RUN-e$NN cmd=$REPO/new-versions/Qapla-blendtest-nnue \
              option.NnueFile=$REPO/test/nnue/epoch-$RUN-e$NN.nnue \
            >> test/log/epochs-$RUN-run.log 2>&1 < /dev/null & ) ; sleep 60; \
            echo \"  qet \$(pgrep -xa qet || echo NONE)\"; tail -2 test/log/epochs-$RUN-run.log"
        # The tournament file is the proof: qet writes it within seconds of starting, and a qet that
        # refused its parameters writes nothing at all.
        if ! $SSH "$THERE" "test -f $REPO/$STATE"; then
            say "the tournament did not start - $STATE was never written, see epochs-$RUN-run.log"
            $SSH "$THERE" "tail -3 $REPO/test/log/epochs-$RUN-run.log"
            exit 1
        fi
        say "tournament started, $STATE is there"
    fi
    N=$((N + 1))
done
say "all $LAST epochs are in the tournament"
