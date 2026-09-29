#!/bin/bash
# Ends the instance once the pipeline is through - but only if the work really arrived in s3.
#
#   aws-watchdog.sh <s3 prefix> <how many objects have to be there>
#
# A failed run leaves the machine up on purpose, so it can still be looked at. For a one-time spot
# request an operating system shutdown terminates the instance, which is what we want: there is
# nothing on it worth keeping once its results are handed over.
PREFIX=$1
WANTED=$2
LOG=~/Qapla/test/log/watchdog.log
echo "$(date -u +%FT%TZ) watching for $WANTED objects under $PREFIX" >> "$LOG"
while pgrep -f "pipeline.py run" >/dev/null; do sleep 120; done
FOUND=$(aws s3 ls "$PREFIX/" 2>/dev/null | grep -c "")
echo "$(date -u +%FT%TZ) the run ended, $FOUND of $WANTED objects there" >> "$LOG"
if [ "$FOUND" -ge "$WANTED" ]; then
    echo "$(date -u +%FT%TZ) complete - shutting down" >> "$LOG"
    sudo shutdown -h now
else
    echo "$(date -u +%FT%TZ) incomplete - staying up to be looked at" >> "$LOG"
fi
