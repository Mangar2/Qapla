"""
Statistics of the time actually spent per move, read from a qet PGN.

The SPRT ini writes the PGN with clock information only (`[pgnoutput] clock=true`), so every
move carries either the elapsed move time `{[%emt 0:00:00.115]}` or the clock left after it
`{[%clk 0:00:04.9]}`. The elapsed time is used directly; from a clock the time spent is the
drop against the same side's previous clock plus the increment.

The moves are grouped into the same blocks of five the tables in plan/version-log.md use, so
the numbers can be put next to the times the formula asks for.

    python test/tools/movetime-stats.py <file.pgn> [--engine <name>] [--blocks 40]

Without --engine every move of every player is counted.
"""

import sys
import re
import math
import statistics
from collections import defaultdict

CLK = re.compile(r"\[%(emt|clk)\s+([0-9:.]+)\]")
TAG = re.compile(r'^\[(\w+)\s+"(.*)"\]')


def to_ms(text):
    """0:00:04.9 or 4.9 or 1:02.5 to milliseconds"""
    parts = text.split(":")
    seconds = 0.0
    for part in parts:
        seconds = seconds * 60 + float(part)
    return int(round(seconds * 1000))


def increment_ms(timecontrol):
    """40/60+0.5 or 5+0.01 to the increment in milliseconds"""
    if not timecontrol or "+" not in timecontrol:
        return 0
    try:
        return int(round(float(timecontrol.split("+")[-1]) * 1000))
    except ValueError:
        return 0


def parse(path, engine=None):
    """Yields (moveNumber, milliseconds) for every move of the games in the file"""
    tags = {}
    body = []

    def flush():
        if not body:
            return
        text = " ".join(body)
        inc = increment_ms(tags.get("TimeControl"))
        white = tags.get("White", "")
        black = tags.get("Black", "")
        last = {0: None, 1: None}
        # A move is a token followed by an optional comment carrying the clock
        for index, match in enumerate(CLK.finditer(text)):
            side = index % 2
            name = white if side == 0 else black
            if engine and name != engine:
                last[side] = to_ms(match.group(2))
                continue
            value = to_ms(match.group(2))
            if match.group(1) == "emt":
                spent = value
            else:
                previous = last[side]
                last[side] = value
                if previous is None:
                    continue
                spent = previous - value + inc
            last[side] = value if match.group(1) == "clk" else last[side]
            if spent >= 0:
                yield index // 2 + 1, spent

    with open(path, encoding="utf-8", errors="replace") as handle:
        for line in handle:
            line = line.rstrip("\n")
            tag = TAG.match(line)
            if tag:
                if body:
                    yield from flush()
                    body = []
                    tags = {}
                tags[tag.group(1)] = tag.group(2)
            elif line.strip():
                body.append(line.strip())
        yield from flush()


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    path = sys.argv[1]
    engine = None
    blocks = 40
    if "--engine" in sys.argv:
        engine = sys.argv[sys.argv.index("--engine") + 1]
    if "--blocks" in sys.argv:
        blocks = int(sys.argv[sys.argv.index("--blocks") + 1])

    perBlock = defaultdict(list)
    total = 0
    for move, spent in parse(path, engine):
        block = (move - 1) // 5
        if block < blocks:
            perBlock[block].append(spent)
        total += 1

    if not total:
        print("no move times found - was the pgn written with clock=true?")
        return 1

    print(f"{total} move times" + (f" of {engine}" if engine else ""))
    print()
    print("| moves | count | mean | median | stddev | min | max |")
    print("|---:|---:|---:|---:|---:|---:|---:|")
    for block in sorted(perBlock):
        values = perBlock[block]
        mean = statistics.mean(values)
        median = statistics.median(values)
        deviation = statistics.pstdev(values) if len(values) > 1 else 0.0
        print(
            f"| {block * 5 + 1}-{block * 5 + 5} | {len(values)} | {mean:.0f} ms |"
            f" {median:.0f} ms | {deviation:.0f} ms | {min(values)} ms | {max(values)} ms |"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
