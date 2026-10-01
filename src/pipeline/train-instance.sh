#!/bin/bash
# Brings a gpu machine to the state a training needs, and starts one.
#
#   ssh <host> 'bash -s' -- <bucket> <run> <nets dir> <game files...> < src/pipeline/train-instance.sh
#
# Short on purpose: a training machine needs the trainer and torch, not an engine and not the tester,
# and the image already carries torch with cuda - which is why a Deep Learning AMI is used rather than
# plain ubuntu with an hour of driver installation.
#
# It starts the training with --resume. That is the whole reason a training may run on the spot market
# at all: the nets and the checkpoint of every epoch go to the object store as they are written, and a
# machine that replaces a reclaimed one takes up the newest checkpoint instead of starting over.
set -e
BUCKET=s3://${1:?the bucket the game files are in}
RUN=${2:?a name for this training, for the paths in the object store}
shift 2
FILES="$@"
[ -n "$FILES" ] || { echo "no game files named"; exit 1; }

QAPLA=~/Qapla
NETS=$QAPLA/test/nnue/nets-$RUN
DATA=$QAPLA/test/nnue/dataset

echo "=== $(date -u +%FT%TZ) $(nproc) cores, $(free -g | awk '/Mem:/{print $2}') GB ==="
nvidia-smi --query-gpu=name,memory.total --format=csv,noheader || { echo "no gpu here"; exit 1; }

[ -d "$QAPLA" ] || git clone -q https://github.com/Mangar2/Qapla.git "$QAPLA"
cd "$QAPLA"
git fetch -q origin nnue && git reset -q --hard origin/nnue
echo "qapla at $(git log -1 --oneline)"

mkdir -p "$DATA" "$NETS" "$QAPLA/test/log"
for f in $FILES; do
    [ -f "$DATA/$f" ] || { echo "fetching $f"; aws s3 cp "$BUCKET/training/$f" "$DATA/$f" --only-show-errors; }
done
# Whatever a previous instance of this run already produced, so --resume has something to take up.
aws s3 cp "$BUCKET/$RUN/nets/" "$NETS/" --recursive --only-show-errors 2>/dev/null || true
ls "$NETS" | tail -3

python3 -c "import torch;print('torch', torch.__version__, 'cuda', torch.cuda.is_available(), torch.cuda.get_device_name(0) if torch.cuda.is_available() else '')"
python3 -c "import numpy" 2>/dev/null || pip -q install numpy

# Every epoch's net and checkpoint leave the machine as they are written: this instance may be taken
# away at any moment, and what is only here is lost when it is.
cat > "$QAPLA/carry-over.sh" <<CARRY
while :; do
    aws s3 sync "$NETS/" "$BUCKET/$RUN/nets/" --only-show-errors
    aws s3 cp "$QAPLA/test/log/train-$RUN.log" "$BUCKET/$RUN/train.log" --only-show-errors 2>/dev/null || true
    sleep 300
done
CARRY
chmod +x "$QAPLA/carry-over.sh"
( setsid nohup bash "$QAPLA/carry-over.sh" >/dev/null 2>&1 & )

cd "$QAPLA/src/trainer"
RELATIVE=""
for f in $FILES; do RELATIVE="$RELATIVE ../../test/nnue/dataset/$f"; done
( setsid nohup python3 -u train.py $RELATIVE --out "../../test/nnue/nets-$RUN" \
    --blend-start 0.8 --blend-end 0.7 --epochs 20 --patience 2 --workers "$(nproc)" --seed 1 \
    --validation-every 100 --resume \
    >> "$QAPLA/test/log/train-$RUN.log" 2>&1 < /dev/null & )
sleep 90
tail -6 "$QAPLA/test/log/train-$RUN.log"
