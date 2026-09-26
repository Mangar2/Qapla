"""Trains the net on the cache prepare.py wrote.

    python3 train.py ../../test/nnue/cache --epochs 20 --out ../../test/nnue

Writes a checkpoint and a net file per epoch, so that a run can be stopped and the
best epoch picked afterwards. Whether an epoch is better is not decided here: an
evaluation that loses fewer games is the only thing that counts, and that is an
SPRT of the engine.

On this machine the device is the MPS backend of the M4. It is a good deal slower
than a graphics card but it is what there is, and for a net of this size it is
enough.
"""

import argparse
import time

import torch

import export
import netfile
from dataset import PositionCache
from model import HalfKaNet, loss_of


def pick_device(name):
    if name != 'auto':
        return torch.device(name)
    if torch.backends.mps.is_available():
        return torch.device('mps')
    if torch.cuda.is_available():
        return torch.device('cuda')
    return torch.device('cpu')


def train(arguments):
    device = pick_device(arguments.device)
    cache = PositionCache(arguments.cache)
    model = HalfKaNet().to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=arguments.learning_rate)
    batches = len(cache) // arguments.batch_size
    print('%d positions, %d batches of %d, device %s'
          % (len(cache), batches, arguments.batch_size, device))

    for epoch in range(1, arguments.epochs + 1):
        # The blend walks from the first value to the second over the run: follow
        # the search first, then let the results of the games correct it.
        blend = arguments.blend_start + (arguments.blend_end - arguments.blend_start) \
            * (epoch - 1) / max(1, arguments.epochs - 1)
        model.train()
        total, seen = 0.0, 0
        start = time.time()
        for own, opponent, value, result in cache.batches(arguments.batch_size, device):
            prediction = model(own, opponent)
            loss = loss_of(prediction, value, result, blend)
            optimizer.zero_grad(set_to_none=True)
            loss.backward()
            optimizer.step()
            model.clamp_weights()
            total += loss.detach().item()
            seen += 1
            if seen % arguments.report_every == 0:
                print('  epoch %d, batch %d/%d, loss %.6f, %.0f positions per second'
                      % (epoch, seen, batches, total / seen,
                         seen * arguments.batch_size / (time.time() - start)), flush=True)
        print('epoch %d done, blend %.2f, loss %.6f, %.0f s'
              % (epoch, blend, total / max(1, seen), time.time() - start), flush=True)
        torch.save(model.state_dict(), '%s/net-epoch%02d.pt' % (arguments.out, epoch))
        export.export(model, '%s/net-epoch%02d.nnue' % (arguments.out, epoch))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('cache', help='prefix of the files prepare.py wrote')
    parser.add_argument('--out', default='.', help='where the checkpoints and nets go')
    parser.add_argument('--epochs', type=int, default=20)
    parser.add_argument('--batch-size', type=int, default=16384)
    parser.add_argument('--learning-rate', type=float, default=1e-3)
    parser.add_argument('--blend-start', type=float, default=0.8,
                        help='weight of the search value at the first epoch')
    parser.add_argument('--blend-end', type=float, default=0.7,
                        help='and at the last one; what is left is the game result')
    parser.add_argument('--device', default='auto')
    parser.add_argument('--report-every', type=int, default=50)
    train(parser.parse_args())
