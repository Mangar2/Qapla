"""Reads a packed game file directly and hands out batches, without a prepared cache.

The cache prepare.py writes is not needed. Measured: replaying a game costs almost nothing
(2.0 M positions a second) and building the features of a position costs 218,000 a second on one
core, while the training step consumes about 330,000 - so two or three worker processes feed it
without ever being the limit. The cache bought a factor of 1.5 on one core and nothing at all with
workers, for 128 bytes a position on disk against the 3 bytes a game file needs.

What the cache did buy is random access, and that is what this replaces. A position is not
addressable on its own - a packed move has no departure square, so a game can only be walked from
its start - so the unit of access is a game. An index of one offset per game makes any game
reachable, the games are visited in a random order, and a shuffle buffer breaks up the positions
inside a game, which would otherwise arrive as a hundred nearly identical rows in a row.

The split into training and validation is over whole games and over a seeded permutation, so two
game files with the same number of games get the identical split - which is what lets two nets
trained from the same games with and without the result of the game be compared.

Several files are read as one corpus. Each keeps its own index, and the game ids run through them in
the order they were named, so the permutation reaches across all of them and a batch holds games from
every file. That is why the order of the files is part of the experiment: naming them the other way
round holds different games back. Reading them in place rather than concatenating them keeps each set
a file of its own, which is what a set trained on alone needs.
"""

import os
import numpy as np
import torch

import format as fmt
import netfile

PADDING = netfile.FEATURE_COUNT
SLOTS = fmt.MAX_ACTIVE_FEATURES
INDEX_VERSION = 1
BUFFER_POSITIONS = 1 << 17          # the shuffle buffer of one worker


def build_index(path, quiet=False):
    """One entry per game: where it starts, how many plies, how many of them carry a value.

    One pass over the file, and the result is kept beside it - 12 bytes a game, against the
    2.4 GB the file itself has for a set of this size.
    """
    cache = path + '.idx.npz'
    if os.path.exists(cache) and os.path.getmtime(cache) >= os.path.getmtime(path):
        held = np.load(cache)
        if int(held['version'][0]) == INDEX_VERSION:
            return held['offsets'], held['lengths'], held['usable']
    offsets, lengths, usable = [], [], []
    with open(path, 'rb') as f:
        header = f.read(12)
        if len(header) != 12:
            raise ValueError(f'{path} is not a game file')
        offset = 12
        while True:
            head = f.read(1)
            if not head:
                break
            plies = head[0]
            data = f.read(3 * plies)
            if len(data) != 3 * plies:
                raise ValueError(f'{path} ends inside a game')
            with_value = 0
            for i in range(0, 3 * plies, 3):
                record = data[i] | (data[i + 1] << 8) | (data[i + 2] << 16)
                if (record >> 13) & 0x7FF != fmt.NO_GAME_VALUE:
                    with_value += 1
            offsets.append(offset)
            lengths.append(plies)
            usable.append(with_value)
            offset += 1 + 3 * plies
    offsets = np.array(offsets, dtype=np.uint64)
    lengths = np.array(lengths, dtype=np.uint8)
    usable = np.array(usable, dtype=np.uint16)
    np.savez(cache, offsets=offsets, lengths=lengths, usable=usable,
             version=np.array([INDEX_VERSION]))
    if not quiet:
        print(f'{path}: {len(offsets):,} games, {int(lengths.sum()):,} positions, '
              f'{int(usable.sum()):,} of them with a value')
    return offsets, lengths, usable


def load_indexes(paths):
    """The indexes of several files as one, plus which file each game came from."""
    offsets, lengths, usable, source = [], [], [], []
    for number, path in enumerate(paths):
        one, two, three = build_index(path)
        offsets.append(one)
        lengths.append(two)
        usable.append(three)
        source.append(np.full(len(one), number, dtype=np.uint8))
    return (np.concatenate(offsets), np.concatenate(lengths),
            np.concatenate(usable), np.concatenate(source))


def split_games(count, seed, every):
    """Validation games and training games, over a seeded permutation of whole games.

    Whole games because the positions of one game are nearly the same position: holding back
    single positions holds nothing back, the net has seen the rest of the game. Seeded because
    two files of the same games have to get the same split.
    """
    order = np.random.default_rng(seed).permutation(count)
    validation = np.sort(order[::every])
    training = np.sort(np.setdiff1d(order, validation, assume_unique=True))
    return training, validation


class _Games(torch.utils.data.IterableDataset):
    """Walks its games in a random order and yields whole batches of positions."""

    def __init__(self, paths, game_ids, batch_size, seed, buffer=BUFFER_POSITIONS):
        self.paths, self.batch_size, self.seed, self.buffer = paths, batch_size, seed, buffer
        self.game_ids = np.asarray(game_ids)
        self.offsets, self.lengths, _, self.source = load_indexes(paths)
        self.epoch = 0

    def _positions_of(self, handles, game_id):
        """The positions of one game as (own features, opponent features, value code, result)."""
        offset = int(self.offsets[game_id])
        plies = int(self.lengths[game_id])
        handle = handles[int(self.source[game_id])]
        handle.seek(offset + 1)
        data = handle.read(3 * plies)
        board = fmt.Board()
        out = []
        for i in range(0, 3 * plies, 3):
            move, value, result = fmt._unpack_record(
                data[i] | (data[i + 1] << 8) | (data[i + 2] << 16))
            if value != fmt.NO_GAME_VALUE:
                own = fmt.WHITE if board.white_to_move else fmt.BLACK
                other = fmt.BLACK if board.white_to_move else fmt.WHITE
                first, second = fmt.features(board, own), fmt.features(board, other)
                # Padded here: a list concat costs nothing, while writing 16,384 rows into a
                # numpy array one slice at a time costs more than building the features did.
                out.append((first + [PADDING] * (SLOTS - len(first)),
                            second + [PADDING] * (SLOTS - len(second)),
                            value, result))
            departure, destination, promotion = fmt.unpack_move(move, board.squares)
            board.apply(departure, destination, promotion)
        return out

    def __iter__(self):
        info = torch.utils.data.get_worker_info()
        ids = self.game_ids if info is None else self.game_ids[info.id::info.num_workers]
        seed = self.seed + self.epoch * 1000003 + (info.id if info else 0)
        rng = np.random.default_rng(seed)
        ids = rng.permutation(ids)
        span = float(fmt.MAX_VALUE_CODE - fmt.MIN_VALUE_CODE)
        pool = []
        # Every file stays open for the whole epoch: the games arrive in a random order and would
        # otherwise be reopening a file at every second game.
        handles = [open(path, 'rb') for path in self.paths]
        try:
            for game_id in ids:
                pool += self._positions_of(handles, int(game_id))
                if len(pool) < self.buffer:
                    continue
                rng.shuffle(pool)
                keep = len(pool) % self.batch_size
                for start in range(0, len(pool) - keep, self.batch_size):
                    yield _as_arrays(pool[start:start + self.batch_size], span)
                pool = pool[len(pool) - keep:] if keep else []
        finally:
            for handle in handles:
                handle.close()
        rng.shuffle(pool)
        for start in range(0, len(pool) - self.batch_size + 1, self.batch_size):
            yield _as_arrays(pool[start:start + self.batch_size], span)


def _as_arrays(rows, span):
    """One batch as the five arrays the model wants.

    uint16 for the features, not int64: an index reaches 45,056 and the batch travels from a worker
    process to the main one, where eight bytes a slot would be four times the traffic for nothing.
    The cast to the long the embedding wants happens once the batch is on the device.
    """
    own = np.array([row[0] for row in rows], dtype=np.uint16)
    other = np.array([row[1] for row in rows], dtype=np.uint16)
    codes = np.fromiter((row[2] for row in rows), dtype=np.float32, count=len(rows))
    stored = np.fromiter((row[3] for row in rows), dtype=np.uint8, count=len(rows))
    values = (codes - fmt.MIN_VALUE_CODE) / span
    counts = (stored != fmt.RESULT_NONE).astype(np.float32)
    results = np.minimum(stored, 2).astype(np.float32) / 2.0
    return own, other, values, results, counts


class GameFile:
    """A game file as a source of batches, with the same shape PositionCache had."""

    def __init__(self, paths, batch_size, seed=1, validation_every=100, part='training',
                 workers=2, buffer=BUFFER_POSITIONS):
        paths = [paths] if isinstance(paths, str) else list(paths)
        offsets, lengths, usable, _ = load_indexes(paths)
        training, validation = split_games(len(offsets), seed, validation_every)
        ids = training if part == 'training' else validation
        self.paths, self.batch_size, self.workers = paths, batch_size, workers
        self.game_count = len(ids)
        self.position_count = int(usable[ids].sum())
        self.dataset = _Games(paths, ids, batch_size, seed, buffer)
        print(f'{part}: {self.game_count:,} games, {self.position_count:,} positions'
              f'{" over " + str(len(paths)) + " files" if len(paths) > 1 else ""}')

    def __len__(self):
        return self.position_count

    def batches(self, batch_size, device, shuffle=True, generator=None):
        # batch_size and shuffle are fixed when the file is opened; the arguments stay for the
        # sake of the interface PositionCache defined.
        self.dataset.epoch += 1
        loader = torch.utils.data.DataLoader(self.dataset, batch_size=None,
                                             num_workers=self.workers,
                                             persistent_workers=False)
        for own, other, values, results, counts in loader:
            yield (own.to(device).long(), other.to(device).long(), values.to(device),
                   results.to(device), counts.to(device))
