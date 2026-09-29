#!/bin/sh
# Starts the gauntlet of the nets against the hand written evaluation, or continues it.
#
#     sh test/tournament/run-gauntlet.sh            in the foreground
#     ( setsid nohup sh test/tournament/run-gauntlet.sh >> test/log/gauntlet-run.log 2>&1 & )
#
# Run it from the repository root; the paths in the settings file are relative to it.
#
# The whole field is passed on every start, and that is the reason this script exists: qet takes the
# engines out of the tournament file only when the call names none, so a call that named just the new
# net would play a tournament of one engine. A net joins by getting a line in the list below, the
# tournament is stopped, and this script is started again - the tournament file holds every game
# already played, and the newcomer plays the rounds it is missing first.
#
# A net whose file is not there is skipped with a note instead of being passed on. That keeps the
# script the same on every machine: the nets are generated data and do not travel through git, so a
# machine that has not been given one yet simply runs the field it has.
set -e
REPO=$(cd "$(dirname "$0")/../.." && pwd)
QET=${QET:-$HOME/bin/qet}
HCE="$REPO/new-versions/Qapla-blendtest-hce"
NNUE="$REPO/new-versions/Qapla-blendtest-nnue"

[ -x "$QET" ]  || { echo "no tester at $QET"; exit 1; }
[ -x "$HCE" ]  || { echo "no hce binary at $HCE - build it with NATIVE=1"; exit 1; }
[ -x "$NNUE" ] || { echo "no nnue binary at $NNUE - build it with NATIVE=1"; exit 1; }

# hce is the gauntlet engine, so it stands on one side of every game and all the other figures are
# read against the same opponent.
set -- --settingsfile=test/tournament/strength-gauntlet.ini \
       --engine "name=Qapla-HCE" "cmd=$HCE" gauntlet=true

while read -r NAME NET; do
    [ -z "$NAME" ] && continue
    if [ ! -f "$REPO/$NET" ]; then
        echo "skipping $NAME: $NET is not on this machine"
        continue
    fi
    set -- "$@" --engine "name=$NAME" "cmd=$NNUE" "option.NnueFile=$REPO/$NET"
done <<'FIELD'
nnue-set1-wdl test/nnue/nets-set1-wdl-epoch08.nnue
nnue-set1-nowdl test/nnue/nets-set1-nowdl-epoch08.nnue
nnue-set1-blend95 test/nnue/nets-set1-blend95-epoch08.nnue
nnue-set2 test/nnue/nets-set2-best.nnue
FIELD

cd "$REPO"
echo "=== $(date '+%Y-%m-%d %H:%M:%S') starting with $(( ($# - 4) / 4 )) nets besides hce ==="
exec "$QET" "$@"
