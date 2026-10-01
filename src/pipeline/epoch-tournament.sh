#!/bin/sh
# Plays the best epoch of a running training against the hand written evaluation, one at a time.
#
#     ( nohup sh src/pipeline/epoch-tournament.sh set12345 test/nnue/nets-set1-to-5 \
#         test/log/pipeline-set12345-train.log >> test/log/epochs-set12345.log 2>&1 & )
#
# Why: the loss the training stops on is measured on games it does not train on, and how well it
# follows playing strength is what this measures - one figure per epoch, all against one opponent in
# one field, so that the figures can be compared with each other.
#
# Which epoch: whenever the tournament has finished an epoch, the one the training calls best at that
# moment comes next - and only that one. The first version played every epoch in turn, and once the
# compiled loader made an epoch 44 minutes long the 47 minutes of 1500 games fell behind and the queue
# of waiting epochs grew without end. An epoch that was best for a while and then overtaken before the
# tournament was free is simply never played; a later best epoch says more about the run anyway.
#
# The tournament holds one untested engine at a time, so qet ends by itself when that epoch has its
# games, and an idle machine is the signal for the next one. A new tournament file is started by the
# first epoch and every later one is added to it by join-gauntlet.py.
#
# It waits while any other tournament is playing on that machine - both want the same fourteen pairs.
# It ends when the training has stopped and its final best epoch has been played.
#
# To stop it, stop what it started as well:
#     pkill -f "epoch-tournamen[t].sh"; pkill -f "join-gauntle[t].py"
# Killing the shell alone leaves a join it is in the middle of running on its own, and that join then
# starts the tournament by itself.
set -u
cd "$(dirname "$0")/../.."
RUN=${1:?usage: epoch-tournament.sh <run name> <nets directory> <training log>}
NETS=${2:?the directory the training writes its nets to}
TRAINLOG=${3:?the training log, for the line that says which epoch is best}

STATE=test/log/epochs-$RUN.state
CFG=src/pipeline/local.toml
THERE=$(python3 -c "import tomllib;print(tomllib.load(open('$CFG','rb'))['hosts']['qapla']['ssh'])")
REPO=$(python3 -c "import tomllib;print(tomllib.load(open('$CFG','rb'))['hosts']['qapla']['repo'].rstrip('/'))")
SSH="ssh -n -o ConnectTimeout=20"

say() { echo "=== $(date '+%Y-%m-%d %H:%M:%S') $*"; }
last=""

while :; do
    training=no
    pgrep -f "[t]rain.py" >/dev/null && training=yes
    # The epoch the training calls best right now. The line is written after the net was saved and
    # measured, so the file is complete when it appears.
    best=$(grep -E "^epoch [0-9]+ done.*\(best\)" "$TRAINLOG" 2>/dev/null | tail -1 | awk '{print $2}')
    if [ -z "$best" ]; then
        [ "$training" = no ] && { say "no epoch was ever written and no training runs - nothing to do"; exit 0; }
        sleep 120; continue
    fi
    NN=$(printf %02d "$best")
    NAME=$RUN-e$NN

    if $SSH "$THERE" "pgrep -x qet >/dev/null"; then
        sleep 120; continue          # an epoch is still being played, or another tournament is
    fi
    # Idle can mean finished or interrupted. qet ends a finished tournament with its outcome table;
    # anything else - a restart of the machine, a kill - leaves a pairing half played, and being in
    # the tournament file is then not the same as having been played.
    if $SSH "$THERE" "test -f $REPO/$STATE" && \
       ! $SSH "$THERE" "tail -40 $REPO/test/log/epochs-$RUN-run.log | grep -q 'Tournament outcome'"; then
        say "the tournament was interrupted - continuing it before choosing the next epoch"
        $SSH "$THERE" "cd $REPO && ( setsid nohup ~/bin/qet --concurrency=14 --logging path=test/log \
            engine=false --tournament file=$STATE >> test/log/epochs-$RUN-run.log 2>&1 < /dev/null & )"
        sleep 120; continue
    fi
    if $SSH "$THERE" "grep -qx 'name=$NAME' $REPO/$STATE 2>/dev/null"; then
        if [ "$training" = no ]; then
            say "the training has stopped and its best epoch $NN has been played - done"
            exit 0
        fi
        [ "$last" != "$NAME" ] && say "$NAME is played; waiting for a better epoch" && last=$NAME
        sleep 120; continue
    fi

    NET=$NETS/net-epoch$NN.nnue
    [ -f "$NET" ] || { say "the log calls epoch $NN best but $NET is not there - stopping"; exit 1; }
    say "the tournament is free and epoch $NN is the best so far - playing it"
    scp -q "$NET" "$THERE:$REPO/test/nnue/epoch-$RUN-e$NN.nnue"
    if $SSH "$THERE" "test -f $REPO/$STATE"; then
        python3 src/pipeline/join-gauntlet.py --state "$STATE" --nets "$NETS" --net "$NET" \
            --name "$NAME" || say "$NAME did not get in"
    else
        say "first epoch - starting the tournament"
        $SSH "$THERE" "cd $REPO && ( setsid nohup ~/bin/qet \
            --settingsfile=test/tournament/epochs.ini \
            --tournament file=$STATE --pgnoutput file=test/log/epochs-$RUN.pgn \
            --engine name=Qapla-HCE cmd=$REPO/new-versions/Qapla-blendtest-hce gauntlet=true \
            --engine name=$NAME cmd=$REPO/new-versions/Qapla-blendtest-nnue \
              option.NnueFile=$REPO/test/nnue/epoch-$RUN-e$NN.nnue \
            >> test/log/epochs-$RUN-run.log 2>&1 < /dev/null & ) ; sleep 60"
        if ! $SSH "$THERE" "test -f $REPO/$STATE"; then
            say "the tournament did not start - $STATE was never written, see epochs-$RUN-run.log"
            exit 1
        fi
        say "tournament started"
    fi
    sleep 120
done
