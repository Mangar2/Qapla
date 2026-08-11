"""
Statistics of the time actually spent per move, read from a qet PGN.

The SPRT ini writes the PGN with clock information only (`[pgnoutput] clock=true`), so every
move carries its elapsed time as a comment, `{0.24s}`, possibly followed by the reason the
game ended, `{0.07s, Draw by threefold repetition}`.

The games start from a book position, so the PGN move numbers do not start at one. The blocks
count from the first move a player actually searched, which is what the tables in
plan/version-log.md list.

    python test/tools/movetime-stats.py <file.pgn> [--engine <name>] [--blocks 40]

Without --engine every move of every player is counted.
"""

import sys
import re
import statistics
from collections import defaultdict

TIME = re.compile(r"\{\s*(\d+(?:\.\d+)?)s")
TAG = re.compile(r'^\[(\w+)\s+"(.*)"\]')


def games(path):
    """Yields (tags, movetext) per game"""
    tags, body, inHeader = {}, [], False
    with open(path, encoding="utf-8", errors="replace") as handle:
        for line in handle:
            tag = TAG.match(line.strip())
            if tag:
                if body:
                    yield tags, " ".join(body)
                    tags, body = {}, []
                tags[tag.group(1)] = tag.group(2)
                inHeader = True
            elif line.strip():
                inHeader = False
                body.append(line.strip())
    if body:
        yield tags, " ".join(body)


def moveTimes(path, engine=None):
    """Yields (moveNumber, milliseconds), the move number counted from the first searched move"""
    for tags, text in games(path):
        white, black = tags.get("White", ""), tags.get("Black", "")
        for index, match in enumerate(TIME.finditer(text)):
            name = white if index % 2 == 0 else black
            if engine and name != engine:
                continue
            yield index // 2 + 1, int(round(float(match.group(1)) * 1000))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    path = sys.argv[1]
    engine = sys.argv[sys.argv.index("--engine") + 1] if "--engine" in sys.argv else None
    blocks = int(sys.argv[sys.argv.index("--blocks") + 1]) if "--blocks" in sys.argv else 40

    perBlock = defaultdict(list)
    total = 0
    for move, spent in moveTimes(path, engine):
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
        deviation = statistics.pstdev(values) if len(values) > 1 else 0.0
        print(
            f"| {block * 5 + 1}-{block * 5 + 5} | {len(values)} |"
            f" {statistics.mean(values):.0f} ms | {statistics.median(values):.0f} ms |"
            f" {deviation:.0f} ms | {min(values)} ms | {max(values)} ms |"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
