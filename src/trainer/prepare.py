"""Turns files of played games into the arrays the training reads.

    python3 prepare.py <cache prefix> <game file> [<game file> ...]
    python3 prepare.py <cache prefix> <game file> --append

The replay of a game is the expensive part of reading the training data - a packed move has
no departure square, so every position has to be walked to from the initial one. Doing that
once and caching the result is the difference between a few minutes and a few minutes per
epoch.

What is cached are the feature indices themselves, two perspectives of at most 32 each,
padded with the index FEATURE_COUNT which the model treats as nothing. That costs 128 bytes
per position and leaves the training with no work at all besides reading memory.

Several files go into one cache, and --append adds to a cache that is already there, so a
new set of games costs only its own time and not that of everything before it.

The value of a position is the code of a win probability, as the game file holds it, and the
result is one of four - win, draw, loss, or none for a game whose result says nothing about
its positions.

cache.meta names the format and lists every file that went in with the number of positions
it contributed. That is what lets --append refuse a file it already holds: the same games
twice is a silent error, and a cache nobody can read the history of is one nobody can trust.
"""

import argparse
import os
import time
from array import array

import format as fmt
import netfile

PADDING = netfile.FEATURE_COUNT
SLOTS = fmt.MAX_ACTIVE_FEATURES

# What the files hold. A cache written by another version is refused, see dataset.py.
CACHE_FORMAT = 'halfka-probability-v2'

BYTES_PER_POSITION = SLOTS * 2 * 2      # two perspectives of uint16 slots


def read_meta(prefix):
    """The format and the list of (positions, path) of a cache, or None if there is none."""
    path = prefix + '.meta'
    if not os.path.exists(path):
        return None, []
    with open(path) as stream:
        lines = [line.strip() for line in stream if line.strip()]
    if not lines:
        return None, []
    sources = []
    for line in lines[1:]:
        count, _, source = line.partition(' ')
        sources.append((int(count), source))
    return lines[0].split()[0], sources


def write_meta(prefix, sources):
    with open(prefix + '.meta', 'w') as stream:
        stream.write('%s %d\n' % (CACHE_FORMAT, SLOTS))
        for count, source in sources:
            stream.write('%d %s\n' % (count, source))


def prepare(cache_prefix, game_paths, append=False, max_positions=None,
            report_every=500000):
    existing_format, sources = read_meta(cache_prefix)
    if append:
        if existing_format is None:
            raise SystemExit('nothing to append to: %s.meta does not exist' % cache_prefix)
        if existing_format != CACHE_FORMAT:
            raise SystemExit('%s was written as %s, this code writes %s'
                             % (cache_prefix, existing_format, CACHE_FORMAT))
        known = sum(count for count, _ in sources)
        onDisk = os.path.getsize(cache_prefix + '.features')
        if onDisk != known * BYTES_PER_POSITION:
            raise SystemExit('%s holds %d positions by its own account and %d by its size - '
                             'an earlier run was interrupted, write it again'
                             % (cache_prefix, known, onDisk // BYTES_PER_POSITION))
        for path in game_paths:
            for _, source in sources:
                if os.path.abspath(source) == os.path.abspath(path):
                    raise SystemExit('%s is already in this cache; the same games twice '
                                     'would weigh them double' % path)
    else:
        if existing_format is not None:
            print('overwriting the cache that was there, %d positions from %d files'
                  % (sum(count for count, _ in sources), len(sources)))
        sources = []

    mode = 'ab' if append else 'wb'
    features = array('H')
    values = array('H')            # the code of a win probability, see format.py
    results = array('B')
    padding_row = [PADDING] * SLOTS
    start = time.time()
    total = 0

    with open(cache_prefix + '.features', mode) as feature_file, \
            open(cache_prefix + '.values', mode) as value_file, \
            open(cache_prefix + '.results', mode) as result_file:

        def flush():
            features.tofile(feature_file)
            values.tofile(value_file)
            results.tofile(result_file)
            del features[:], values[:], results[:]

        for game_path in game_paths:
            fromThisFile = 0
            for board, value, result in fmt.read_positions(
                    game_path, None if max_positions is None else max_positions - total):
                own_colour = fmt.WHITE if board.white_to_move else fmt.BLACK
                other_colour = fmt.BLACK if board.white_to_move else fmt.WHITE
                for colour in (own_colour, other_colour):
                    active = fmt.features(board, colour)
                    # A position has at most 32 pieces and one of them is the own king,
                    # so the slots are never all needed.
                    features.extend(active)
                    features.extend(padding_row[len(active):])
                values.append(value)
                results.append(result)
                fromThisFile += 1
                total += 1
                if total % report_every == 0:
                    flush()
                    print('%d positions, %.0f per second'
                          % (total, total / (time.time() - start)), flush=True)
                if max_positions is not None and total >= max_positions:
                    break
            flush()
            sources.append((fromThisFile, game_path))
            print('%s: %d positions' % (game_path, fromThisFile), flush=True)
            if max_positions is not None and total >= max_positions:
                break

    write_meta(cache_prefix, sources)
    allPositions = sum(count for count, _ in sources)
    print('%d positions added in %.0f s, the cache now holds %d from %d files, %.1f GB'
          % (total, time.time() - start, allPositions, len(sources),
             allPositions * BYTES_PER_POSITION / 1e9), flush=True)
    return total


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('cache', help='prefix of the cache files to write')
    parser.add_argument('games', nargs='+', help='one or more game files to read')
    parser.add_argument('--append', action='store_true',
                        help='add to a cache that is already there')
    parser.add_argument('--max-positions', type=int, default=None)
    arguments = parser.parse_args()
    prepare(arguments.cache, arguments.games, arguments.append, arguments.max_positions)
