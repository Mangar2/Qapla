"""Reads the cache prepare.py wrote and hands out batches.

The cache is three files of fixed width records, so a batch is a gather of rows
out of memory mapped arrays and needs no worker processes. Shuffling is a
permutation of the row numbers, which is what the fixed width buys: the positions
of one game are next to each other in the file and highly correlated, and training
on them in that order is training on almost the same position a hundred times.
"""

import numpy as np
import torch

import netfile
from prepare import SLOTS


class PositionCache:

    def __init__(self, prefix):
        features = np.memmap(prefix + '.features', dtype=np.uint16, mode='r')
        self.features = features.reshape(-1, 2, SLOTS)
        self.values = np.memmap(prefix + '.values', dtype=np.int16, mode='r')
        self.results = np.memmap(prefix + '.results', dtype=np.uint8, mode='r')
        if len(self.features) != len(self.values) or len(self.values) != len(self.results):
            raise ValueError('the three cache files do not hold the same number of positions')

    def __len__(self):
        return len(self.values)

    def batches(self, batch_size, device, shuffle=True, generator=None):
        """Yields (own features, opponent features, value, result) as tensors."""
        order = np.random.default_rng(generator).permutation(len(self)) if shuffle \
            else np.arange(len(self))
        for start in range(0, len(order) - batch_size + 1, batch_size):
            rows = np.sort(order[start:start + batch_size])
            features = np.asarray(self.features[rows], dtype=np.int64)
            values = np.asarray(self.values[rows], dtype=np.float32)
            # The result is stored as 0 loss, 1 draw, 2 win, seen from the side to
            # move; the loss wants it as a probability.
            results = np.asarray(self.results[rows], dtype=np.float32) / 2.0
            yield (torch.from_numpy(features[:, 0]).to(device),
                   torch.from_numpy(features[:, 1]).to(device),
                   torch.from_numpy(values).to(device),
                   torch.from_numpy(results).to(device))
