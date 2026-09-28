#!/bin/bash
# Brings a fresh machine to the state a helper needs, and refuses to finish if anything is off.
#
#   ssh <host> 'bash -s' < src/pipeline/bootstrap-instance.sh
#
# Everything here was a failed attempt first, on 28.09.2026, on an image that already carried both
# repositories: the checkout would not switch branches over a local change, the build then produced
# the old branch's version without a word, the tester built fine and lacked the feature, its cmake
# needs clang, and the gitignored directories and data files are simply not there after a clone.
set -e
BUCKET=s3://<BUCKET>
QAPLA=~/Qapla
TESTER=~/qapla-engine-tester

echo "=== $(date -u +%FT%TZ) $(nproc) cores, $(free -g | awk '/Mem:/{print $2}') GB ==="

# The image may carry an old checkout with local changes. Code comes from github, nothing else.
for d in "$QAPLA" "$TESTER"; do
    [ -d "$d" ] || git clone -q "https://github.com/Mangar2/$(basename $d).git" "$d"
done

cd "$QAPLA"
git checkout -q -- . && git clean -qfd
git fetch -q --all --tags && git checkout -q nnue && git pull -q
WANT=$(git rev-parse --short HEAD)
echo "qapla at $(git log -1 --oneline)"

for variant in hce nnue; do
    [ $variant = nnue ] && DEFINES="-DQAPLA_USE_NNUE" || DEFINES=""
    make BUILD_TYPE=Release clean >/dev/null 2>&1
    make Release -j ${DEFINES:+EXTRA_DEFINES="$DEFINES"} >/dev/null 2>/tmp/build-$variant.err \
        || { echo "FAILED to build $variant"; tail -5 /tmp/build-$variant.err; exit 1; }
    mkdir -p new-versions && cp build/Release/Qapla new-versions/Qapla-$variant
    GOT=$(printf 'uci\nquit\n' | ./new-versions/Qapla-$variant | grep '^id name')
    echo "$variant: $GOT"
    # The version string carries the commit. A binary from the wrong branch says so here or never.
    echo "$GOT" | grep -q "$WANT" || { echo "FAILED: $variant does not carry commit $WANT"; exit 1; }
done

# The tester: the branch matters, not just that it builds. Analysis lives on 0.7.0, and its cmake
# passes -stdlib=libc++, which gcc rejects.
cd "$TESTER"
git checkout -q -- . 2>/dev/null || true
git fetch -q --all && git checkout -q 0.7.0 && git pull -q
echo "tester at $(git log -1 --oneline)"
rm -rf build-rel
cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ >/dev/null 2>&1
cmake --build build-rel -j >/tmp/tester.err 2>&1 || { echo "FAILED to build the tester"; tail -5 /tmp/tester.err; exit 1; }
mkdir -p ~/bin && ln -sf "$TESTER/build-rel/qapla-engine-tester" ~/bin/qet
~/bin/qet --help 2>&1 | grep -q -- "--analysis" || { echo "FAILED: this tester has no --analysis"; exit 1; }

# Gitignored, so absent after a clone - and the tester refuses to start without its log path.
cd "$QAPLA"
mkdir -p test/log test/epd/log test/clop/log test/nnue/nets

# Data is not source and does not come through github.
aws s3 cp $BUCKET/data/wmtest.epd test/epd/wmtest.epd --only-show-errors
aws s3 cp $BUCKET/data/start-positions-1m.pgn test/nnue/start-positions-1m.pgn --only-show-errors
aws s3 cp $BUCKET/data/net-epoch20.nnue test/nnue/nets/net-epoch20.nnue --only-show-errors

# The net has to load, the incremental accumulator has to agree with a full refresh, and there has
# to be a vector path. Anything else costs a factor and is invisible later.
REPORT=$(printf 'nnueeval net %s/test/nnue/nets/net-epoch20.nnue\nquit\n' "$QAPLA" \
         | ./new-versions/Qapla-nnue | grep 'used by the search')
echo "nnue: $REPORT"
echo "$REPORT" | grep -q '(equal)' || { echo "FAILED: incremental and refresh disagree"; exit 1; }
echo "$REPORT" | grep -qv 'plain,' || { echo "FAILED: no vector path"; exit 1; }

echo "=== $(date -u +%FT%TZ) ready ==="
