#!/bin/sh
# Plays the best epoch of a running training against the hand written evaluation, one at a time.
#
#     ( nohup sh src/pipeline/epoch-tournament.sh set12345 test/nnue/nets-set1-to-5 \
#         test/log/pipeline-set12345-train.log [binary] [host] >> test/log/epochs-set12345.log 2>&1 & )
#
# Why: the loss the training stops on is measured on games it does not train on, and how well it
# follows playing strength is what this measures - one figure per epoch, all against one opponent, so
# that the figures can be compared with each other.
#
# Which epoch: whenever the machine is free, the one the training calls best at that moment comes
# next - and only that one. The first version played every epoch in turn, and once the compiled
# loader made an epoch 44 minutes long the 47 minutes of 1500 games fell behind and the queue of
# waiting epochs grew without end. An epoch that was best for a while and then overtaken before the
# machine was free is simply never played; a later best epoch says more about the run anyway.
#
# Each epoch is a tournament of its own, epochs-<run>-eNN.state, against the same hce binary. With a
# single opponent the figure of an epoch is its score against hce and nothing else, so one shared
# field gave nothing a file per epoch does not - and the shared field needed join-gauntlet.py to stop
# qet, edit its file and start it again, which is a page of linux shell on the tournament machine.
# A file per epoch is started and, after an interruption, continued with the identical call.
#
# The machine is a host of src/pipeline/local.toml, the linux one by default. A host with
# os = "windows" is driven through the msys bash there (C:/msys64), and qet is started through WMI:
# a process an ssh session starts on windows is killed with the session.
#
# It waits while any other qet is playing on that machine - both want the same fourteen pairs.
# It ends when the training has stopped and its final best epoch has been played.
#
# To stop it, stop the shell; it starts nothing that runs on without it except qet itself:
#     pkill -f "epoch-tournamen[t].sh"
set -u
cd "$(dirname "$0")/../.."
RUN=${1:?usage: epoch-tournament.sh <run name> <nets directory> <training log> [binary] [host]}
NETS=${2:?the directory the training writes its nets to}
TRAINLOG=${3:?the training log, for the line that says which epoch is best}
# The nnue binary in new-versions/ on the tournament machine. A net with layer stacks needs one built
# after the stacks came in; the older one is kept for everything else.
BINARY=${4:-Qapla-blendtest-nnue}
HOST=${5:-qapla}

CFG=src/pipeline/local.toml
host() { python3 -c "import tomllib;print(tomllib.load(open('$CFG','rb'))['hosts']['$HOST'].get('$1','${2:-}'))"; }
THERE=$(host ssh)
REPO=$(host repo | sed 's#/$##')
QET=$(host qet '~/bin/qet')
OS=$(host os linux)
EXT=""
REMOTE_SHELL="sh -s"
if [ "$OS" = windows ]; then
    EXT=".exe"
    REMOTE_SHELL="C:/msys64/usr/bin/bash.exe -s"
fi

say() { echo "=== $(date '+%Y-%m-%d %H:%M:%S') $*"; }
# A command on the tournament machine, in its posix shell. The command travels on stdin, so nothing
# in it has to survive the quoting of a windows command line.
there() { printf 'export PATH=/usr/bin:/bin:$PATH\ncd %s || exit 9\n%s\n' "$REPO" "$1" \
    | ssh -o ConnectTimeout=20 "$THERE" "$REMOTE_SHELL"; }
qet_runs() {
    if [ "$OS" = windows ]; then there 'tasklist //FI "IMAGENAME eq qet.exe" //NH | grep -qi "^qet.exe"'
    else there "pgrep -x qet >/dev/null"; fi
}
# Starts the tournament of one epoch, or continues it: the call is the same either way.
start() {
    name=$1; net=$2
    state=test/log/epochs-$RUN-$name.state
    call="$QET --settingsfile=test/tournament/epochs.ini --tournament file=$state \
--pgnoutput file=test/log/epochs-$RUN-$name.pgn \
--engine name=Qapla-HCE cmd=$REPO/new-versions/Qapla-blendtest-hce$EXT gauntlet=true \
--engine name=$RUN-$name cmd=$REPO/new-versions/$BINARY$EXT option.NnueFile=$REPO/$net"
    if [ "$OS" = windows ]; then
        local_cmd=test/log/epochs-$RUN-$name-start.cmd
        printf '@echo off\r\ncd /d %s\r\n%s >> test\\log\\epochs-%s-%s-run.log 2>&1\r\n' \
            "$(echo "$REPO" | tr / '\\')" "$call" "$RUN" "$name" > "$local_cmd"
        scp -q "$local_cmd" "$THERE:$REPO/$local_cmd"
        win_cmd=$(echo "$REPO/$local_cmd" | tr / '\\')
        there "powershell -NoProfile -Command \"Invoke-CimMethod -ClassName Win32_Process -MethodName Create \
-Arguments @{CommandLine='cmd /c $win_cmd'; CurrentDirectory='$(echo "$REPO" | tr / '\\')'} | Out-Null\""
    else
        there "( setsid nohup $call >> test/log/epochs-$RUN-$name-run.log 2>&1 < /dev/null & )"
    fi
}
# Finished is when the last outcome table of the run log comes after the last game that was started;
# a state file without that was interrupted - a restart of the machine, a kill.
finished() {
    there "tail -2000 test/log/epochs-$RUN-$1-run.log 2>/dev/null | grep -E 'started round|Tournament outcome' \
        | tail -1 | grep -q 'Tournament outcome'"
}
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
    NN=e$(printf %02d "$best")
    NET=test/nnue/epoch-$RUN-$NN.nnue

    if qet_runs; then
        sleep 120; continue          # an epoch is still being played, or another tournament is
    fi
    if there "test -f test/log/epochs-$RUN-$NN.state"; then
        if ! finished "$NN"; then
            say "$NN was interrupted - continuing it"
            start "$NN" "$NET"; sleep 120; continue
        fi
        if [ "$training" = no ]; then
            say "the training has stopped and its best epoch $NN has been played - done"
            exit 0
        fi
        [ "$last" != "$NN" ] && say "$NN is played; waiting for a better epoch" && last=$NN
        sleep 120; continue
    fi

    LOCAL=$NETS/net-epoch$(printf %02d "$best").nnue
    [ -f "$LOCAL" ] || { say "the log calls epoch $best best but $LOCAL is not there - stopping"; exit 1; }
    say "the machine is free and $NN is the best epoch so far - playing it"
    scp -q "$LOCAL" "$THERE:$REPO/$NET"
    out=$(there "printf 'nnueeval net $NET\nquit\n' | new-versions/$BINARY$EXT 2>&1 | grep reference")
    echo "  $out"
    case "$out" in
        *equal*avx2*|*equal*neon*) ;;
        *) say "the net does not load cleanly or the build has no vector path - stopping"; exit 1 ;;
    esac
    start "$NN" "$NET"
    sleep 60
    qet_runs || { say "qet did not start - see test/log/epochs-$RUN-$NN-run.log there"; exit 1; }
    say "$NN started"
    sleep 120
done
