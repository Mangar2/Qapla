"""Trains the net on the cache prepare.py wrote.

    python3 train.py ../../test/nnue/cache --epochs 20 --out ../../test/nnue

Writes a checkpoint and a net file per epoch, so that a run can be stopped and the
best epoch picked afterwards.

With --validation it also measures the loss on games it does not train on, and stops
when that has not improved for --patience epochs. Without it there is no stopping
condition beyond the number of epochs somebody typed, and no way to tell a net that
has learned from one that has memorised - the training loss falls in both cases.

That loss is measured at one fixed blend, --blend-end, and not at the blend of the
epoch, which walks. Otherwise the epochs would not be comparable and the curve would
show the blend rather than the net.

It is still not the last word on which net plays better. Only an SPRT of the engine is.

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
from gamedata import GameFile
from model import HalfKaNet, loss_of


def pick_device(name):
    if name != 'auto':
        return torch.device(name)
    if torch.backends.mps.is_available():
        return torch.device('mps')
    if torch.cuda.is_available():
        return torch.device('cuda')
    return torch.device('cpu')


def measure(model, cache, batch_size, device, blend, max_batches=None):
    """The mean loss over a cache, without training on it."""
    model.eval()
    total, batches = 0.0, 0
    with torch.no_grad():
        for own, opponent, value, result, counts in cache.batches(
                batch_size, device, shuffle=False):
            total += float(loss_of(model(own, opponent), value, result, counts, blend))
            batches += 1
            if max_batches is not None and batches >= max_batches:
                break
    model.train()
    return total / max(1, batches)


def train(arguments):
    device = pick_device(arguments.device)
    if all(name.endswith('.gam') for name in arguments.cache):
        # Straight out of the packed game file: no prepared cache, the split over whole games and
        # over a seeded permutation, so two files of the same games get the same one.
        cache = GameFile(arguments.cache, arguments.batch_size, seed=arguments.seed,
                         validation_every=arguments.validation_every, part='training',
                         workers=arguments.workers)
        validation = GameFile(arguments.cache, arguments.batch_size, seed=arguments.seed,
                              validation_every=arguments.validation_every, part='validation',
                              workers=arguments.workers)
    else:
        if len(arguments.cache) != 1:
            raise SystemExit('several sources are only read as .gam game files')
        cache = PositionCache(arguments.cache[0])
        validation = PositionCache(arguments.validation) if arguments.validation else None
    model = HalfKaNet().to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=arguments.learning_rate)
    batches = len(cache) // arguments.batch_size
    print('%d positions, %d batches of %d, device %s'
          % (len(cache), batches, arguments.batch_size, device))
    if validation is not None:
        print('%d positions held back for validation, measured at blend %.2f'
              % (len(validation), arguments.blend_end))
    bestLoss, bestEpoch, since = float('inf'), 0, 0

    for epoch in range(1, arguments.epochs + 1):
        # The blend walks from the first value to the second over the run: follow
        # the search first, then let the results of the games correct it.
        blend = arguments.blend_start + (arguments.blend_end - arguments.blend_start) \
            * (epoch - 1) / max(1, arguments.epochs - 1)
        model.train()
        total, seen = 0.0, 0
        start = time.time()
        for own, opponent, value, result, counts in cache.batches(arguments.batch_size, device):
            prediction = model(own, opponent)
            loss = loss_of(prediction, value, result, counts, blend)
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
        trained = total / max(1, seen)
        torch.save(model.state_dict(), '%s/net-epoch%02d.pt' % (arguments.out, epoch))
        export.export(model, '%s/net-epoch%02d.nnue' % (arguments.out, epoch))
        if validation is None:
            print('epoch %d done, blend %.2f, loss %.6f, %.0f s'
                  % (epoch, blend, trained, time.time() - start), flush=True)
            continue

        held = measure(model, validation, arguments.batch_size, device, arguments.blend_end)
        improved = held < bestLoss
        if improved:
            bestLoss, bestEpoch, since = held, epoch, 0
        else:
            since += 1
        print('epoch %d done, blend %.2f, loss %.6f, held back %.6f%s, %.0f s'
              % (epoch, blend, trained, held, ' (best)' if improved else
                 ' (%d epochs without an improvement)' % since, time.time() - start),
              flush=True)
        if since >= arguments.patience:
            print('stopping: the loss on the games it does not train on has not improved '
                  'for %d epochs. Epoch %d was the best of them at %.6f.'
                  % (since, bestEpoch, bestLoss), flush=True)
            break
    if validation is not None and bestEpoch:
        print('best epoch %d, held back loss %.6f - net-epoch%02d.nnue'
              % (bestEpoch, bestLoss, bestEpoch), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('cache', nargs='+',
                        help='one or more .gam game files, read as one corpus in the order given, '
                             'or the prefix of a prepared cache')
    parser.add_argument('--seed', type=int, default=1,
                        help='decides the split and the order; the same seed gives the same split')
    parser.add_argument('--validation-every', type=int, default=100,
                        help='every nth game, over the permutation, is held back')
    parser.add_argument('--workers', type=int, default=2,
                        help='processes that read and build features, 0 for none')
    parser.add_argument('--out', default='.', help='where the checkpoints and nets go')
    parser.add_argument('--epochs', type=int, default=20)
    parser.add_argument('--batch-size', type=int, default=16384)
    parser.add_argument('--learning-rate', type=float, default=1e-3)
    parser.add_argument('--blend-start', type=float, default=0.8,
                        help='weight of the search value at the first epoch')
    parser.add_argument('--blend-end', type=float, default=0.7,
                        help='and at the last one; what is left is the game result')
    parser.add_argument('--validation', default=None,
                        help='prefix of a cache of games not trained on, as prepare.py '
                             '--validation-every writes it')
    parser.add_argument('--patience', type=int, default=3,
                        help='epochs without an improvement on it before stopping')
    parser.add_argument('--device', default='auto')
    parser.add_argument('--report-every', type=int, default=50)
    train(parser.parse_args())
