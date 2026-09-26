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


class _Writer:
    """Collects positions and writes them to one cache."""

    def __init__(self, prefix, mode, sources):
        self.prefix = prefix
        self.sources = sources
        self.features = array('H')
        self.values = array('H')       # the code of a win probability, see format.py
        self.results = array('B')
        self.total = 0
        self.files = [open(prefix + name, mode)
                      for name in ('.features', '.values', '.results')]

    def add(self, squares, kings, whiteToMove, value, result):
        board = fmt.Board()
        board.squares, board.kings, board.white_to_move = squares, kings, whiteToMove
        own = fmt.WHITE if whiteToMove else fmt.BLACK
        other = fmt.BLACK if whiteToMove else fmt.WHITE
        for colour in (own, other):
            active = fmt.features(board, colour)
            # A position has at most 32 pieces and one of them is the own king, so the
            # slots are never all needed.
            self.features.extend(active)
            self.features.extend([PADDING] * (SLOTS - len(active)))
        self.values.append(value)
        self.results.append(result)
        self.total += 1

    def flush(self):
        self.features.tofile(self.files[0])
        self.values.tofile(self.files[1])
        self.results.tofile(self.files[2])
        del self.features[:], self.values[:], self.results[:]

    def close(self, counted):
        self.flush()
        for handle in self.files:
            handle.close()
        self.sources.append((counted, self.source))
        write_meta(self.prefix, self.sources)


def prepare(cache_prefix, game_paths, append=False, max_positions=None,
            report_every=500000, validation_every=0):
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
    validation_prefix = cache_prefix + '-val'
    _, validation_sources = read_meta(validation_prefix)
    if not append:
        validation_sources = []

    training = _Writer(cache_prefix, mode, sources)
    validation = _Writer(validation_prefix, mode, validation_sources) \
        if validation_every > 0 else None
    start = time.time()
    total = 0
    games = 0

    for game_path in game_paths:
        training.source = validation.source = game_path
        fromThisFile = [0, 0]
        for positions in fmt.read_positions_by_game(game_path):
            games += 1
            # Every n-th game, not every n-th position: the positions of one game are
            # almost the same position, so holding single ones back holds nothing back.
            target = validation if validation is not None and games % validation_every == 0 \
                else training
            for squares, kings, whiteToMove, value, result in positions:
                target.add(squares, kings, whiteToMove, value, result)
                fromThisFile[0 if target is training else 1] += 1
                total += 1
                if total % report_every == 0:
                    training.flush()
                    if validation is not None:
                        validation.flush()
                    print('%d positions, %.0f per second'
                          % (total, total / (time.time() - start)), flush=True)
            if max_positions is not None and total >= max_positions:
                break
        print('%s: %d positions for training, %d for validation'
              % (game_path, fromThisFile[0], fromThisFile[1]), flush=True)
        training.sources.append((fromThisFile[0], game_path))
        if validation is not None:
            validation.sources.append((fromThisFile[1], game_path))
        if max_positions is not None and total >= max_positions:
            break

    training.flush()
    write_meta(cache_prefix, training.sources)
    for handle in training.files:
        handle.close()
    if validation is not None:
        validation.flush()
        write_meta(validation_prefix, validation.sources)
        for handle in validation.files:
            handle.close()

    allTraining = sum(count for count, _ in training.sources)
    print('%d positions added in %.0f s; training holds %d, %.1f GB'
          % (total, time.time() - start, allTraining,
             allTraining * BYTES_PER_POSITION / 1e9), flush=True)
    if validation is not None:
        allValidation = sum(count for count, _ in validation.sources)
        print('validation holds %d positions in %s, every %dth game'
              % (allValidation, validation_prefix, validation_every), flush=True)
    return total


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('cache', help='prefix of the cache files to write')
    parser.add_argument('games', nargs='+', help='one or more game files to read')
    parser.add_argument('--append', action='store_true',
                        help='add to a cache that is already there')
    parser.add_argument('--max-positions', type=int, default=None)
    parser.add_argument('--validation-every', type=int, default=0,
                        help='put every n-th game into <cache>-val instead, 0 for none')
    arguments = parser.parse_args()
    prepare(arguments.cache, arguments.games, arguments.append, arguments.max_positions,
            validation_every=arguments.validation_every)
