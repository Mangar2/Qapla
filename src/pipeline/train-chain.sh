#!/bin/sh
# Trains the sets one after the other on this machine and puts each finished net into the gauntlet on
# the tournament machine.
#
#     ( nohup sh src/pipeline/train-chain.sh >> test/log/train-chain.log 2>&1 & )
#
# One training at a time, because they share one gpu: two at once would only make both slower and
# their timings incomparable. A training that is already done is skipped by the pipeline, so the chain
# may be stopped and started again at any point.
#
# Set 4 is not in the object store yet while this is written. The chain waits for it, fetches it, puts
# it on the other machine as well and writes the manifest - and only then trains on it. Waiting is the
# right thing here rather than failing: the order of the trainings is what the series is about.
set -u
cd "$(dirname "$0")/../.."
JOIN="python3 src/pipeline/join-gauntlet.py"
BUCKET=$(python3 -c "import tomllib;print(tomllib.load(open('src/pipeline/local.toml','rb'))['aws']['bucket'])")
THERE=$(python3 -c "import tomllib;print(tomllib.load(open('src/pipeline/local.toml','rb'))['hosts']['qapla']['ssh'])")

say() { echo "=== $(date '+%Y-%m-%d %H:%M:%S') $*"; }

# id, nets directory, the name it plays under
train_and_join() {
    ID=$1; NETS=$2; NAME=$3
    # A step marked done in the configuration is not run - and then its net must not be put into the
    # tournament either. It was: set 4 was stopped by hand and marked done, the chain skipped the
    # training and handed its net to the gauntlet anyway, which is 5000 games of exactly the test
    # that had just been called off.
    if python3 -c "import tomllib,sys
steps = tomllib.load(open('src/pipeline/pipeline.toml','rb'))['steps']
sys.exit(0 if any(s['id'] == '$ID' and s.get('done') for s in steps) else 1)"; then
        say "$ID is marked done - neither trained nor put into the tournament"
        return 0
    fi
    say "$ID"
    python3 -u src/pipeline/pipeline.py run --only "$ID" || { say "$ID failed - the chain stops"; exit 1; }
    say "$ID trained, putting $NAME into the gauntlet"
    $JOIN --nets "$NETS" --name "$NAME" --log "test/log/pipeline-$ID.log" \
        || say "$NAME did not get into the gauntlet - the chain carries on with the next training"
}

# The training that is already running was started outside the chain; wait for it, then hand its net
# over like any other.
if pgrep -f "[t]rain.py" >/dev/null; then
    say "a training is already running, waiting for it"
    while pgrep -f "[t]rain.py" >/dev/null; do sleep 300; done
fi
if [ -d test/nnue/nets-set1-set2 ]; then
    say "putting nnue-set12 into the gauntlet"
    $JOIN --nets test/nnue/nets-set1-set2 --name nnue-set12 \
          --log test/log/pipeline-set12-train.log || say "nnue-set12 did not get in"
fi

train_and_join set3-train   test/nnue/nets-set3            nnue-set3
train_and_join set123-train test/nnue/nets-set1-set2-set3  nnue-set123

say "waiting for set 4 in the object store"
until [ "$(aws s3 ls "s3://$BUCKET/set4/dataset/" 2>/dev/null | grep -c '\.gam')" -ge 2 ]; do
    sleep 600
done
say "set 4 is there, fetching it and putting it on the other machine as well"
aws s3 cp "s3://$BUCKET/set4/dataset/" test/nnue/dataset/ --recursive \
    --exclude '*' --include '*.gam' --only-show-errors
rsync -a test/nnue/dataset/set4-nnue1-hce-depth6.gam \
         test/nnue/dataset/set4-nnue1-hce-depth6-nowdl.gam "$THERE:dev/qapla/test/nnue/dataset/"
python3 src/pipeline/pipeline.py manifest

train_and_join set4-train    test/nnue/nets-set4                    nnue-set4
train_and_join set14-train   test/nnue/nets-set1-set4               nnue-set14
train_and_join set1234-train test/nnue/nets-set1-set2-set3-set4     nnue-set1234
say "the chain is through"

# Set 5 arrived while the chain was running - the second mixed set, and the largest. Appended at the
# end rather than spliced in: the shell reads this file as it goes, so adding to the end is the one
# edit that cannot confuse a running chain.
train_and_join set5-train     test/nnue/nets-set5      nnue-set5
train_and_join set15-train    test/nnue/nets-set1-set5 nnue-set15
train_and_join set12345-train test/nnue/nets-all       nnue-all
say "the chain is through, all five sets"
