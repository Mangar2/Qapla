#!/bin/bash
# Per position node count and search time for one engine at a fixed depth.
# Output: id;nodes;time_ms;fen
ENGINE=$1; DEPTH=$2; EPD=${3:-test/epd/wmtest.epd}
tmp=$(mktemp -d)
while IFS= read -r line; do
  [ -z "$line" ] && continue
  fen=$(echo "$line" | sed 's/ bm .*//')
  id=$(echo "$line" | grep -o 'id "[^"]*"' | sed 's/id "//;s/"//')
  fifo=$tmp/in; out=$tmp/out
  rm -f "$fifo" "$out"; mkfifo "$fifo"; : > "$out"
  "$ENGINE" < "$fifo" > "$out" 2>/dev/null &
  pid=$!
  exec 3>"$fifo"
  printf 'uci\nisready\nposition fen %s\ngo depth %s\n' "$fen" "$DEPTH" >&3
  for i in $(seq 1 6000); do grep -q "^bestmove" "$out" && break; sleep 0.05; done
  printf 'quit\n' >&3
  exec 3>&-
  wait $pid 2>/dev/null
  last=$(grep "^info " "$out" | tail -1)
  nodes=$(echo "$last" | grep -o 'nodes [0-9]*' | awk '{print $2}')
  tm=$(echo "$last" | grep -o 'time [0-9]*' | awk '{print $2}')
  echo "$id;${nodes:-0};${tm:-0};$fen"
done < "$EPD"
rm -rf "$tmp"
