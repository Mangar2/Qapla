"""Turns a file of played games into the arrays the training reads.

The replay of a game is the expensive part of reading the training data - a packed
move has no departure square, so every position has to be walked to from the
initial one. Doing that once and caching the result is the difference between a
few minutes and a few minutes per epoch.

What is cached are the feature indices themselves, two perspectives of at most 32
each, padded with the index FEATURE_COUNT which the model treats as nothing. That
costs 128 bytes per position and leaves the training with no work at all besides
reading memory.

    python3 prepare.py ../../test/nnue/games-100k.gam ../../test/nnue/cache

writes cache.features (uint16), cache.values (int16) and cache.results (uint8).

The cache belongs to the feature set it was written with. Change the features and
it has to be written again - there is no version in it, because the only honest
answer to a stale cache is to rebuild it.
"""

import sys
import time
from array import array

import format as fmt
import netfile

PADDING = netfile.FEATURE_COUNT
SLOTS = fmt.MAX_ACTIVE_FEATURES


def prepare(game_path, cache_prefix, max_positions=None, report_every=500000):
    features = array('H')
    values = array('h')
    results = array('B')
    padding_row = [PADDING] * SLOTS
    start = time.time()
    count = 0

    with open(cache_prefix + '.features', 'wb') as feature_file, \
            open(cache_prefix + '.values', 'wb') as value_file, \
            open(cache_prefix + '.results', 'wb') as result_file:
        for board, value, result in fmt.read_positions(game_path, max_positions):
            own_colour = fmt.WHITE if board.white_to_move else fmt.BLACK
            other_colour = fmt.BLACK if board.white_to_move else fmt.WHITE
            for colour in (own_colour, other_colour):
                active = fmt.features(board, colour)
                # A position has at most 32 pieces and one of them is the own
                # king, so the slots are never all needed.
                features.extend(active)
                features.extend(padding_row[len(active):])
            values.append(value)
            results.append(result)
            count += 1
            if count % report_every == 0:
                features.tofile(feature_file)
                values.tofile(value_file)
                results.tofile(result_file)
                del features[:], values[:], results[:]
                elapsed = time.time() - start
                print('%d positions, %.0f per second' % (count, count / elapsed), flush=True)
        features.tofile(feature_file)
        values.tofile(value_file)
        results.tofile(result_file)

    print('%d positions in %.0f s, %s.features is %.1f GB'
          % (count, time.time() - start, cache_prefix,
             count * SLOTS * 2 * 2 / 1e9), flush=True)
    return count


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print('usage: prepare.py <games file> <cache prefix> [max positions]')
        raise SystemExit(1)
    limit = int(sys.argv[3]) if len(sys.argv) > 3 else None
    prepare(sys.argv[1], sys.argv[2], limit)
