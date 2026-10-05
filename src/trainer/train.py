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
import os
import time

import torch

import export
import netfile
from dataset import PositionCache
from gamedata import GameFile
from model import HalfKaNet, loss_of, neighbour_loss_of


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


def take_up(model, optimizer, out, device):
    """Continues from the newest checkpoint in out: which epoch comes next and where the rule stood.

    A training cannot be split into pieces, so on a machine that may be taken away it has to be able
    to carry on. Without this a reclaimed spot instance started again at epoch one - seven times in
    three and a half hours once, each attempt overwriting the nets of the one before.

    An old checkpoint holds the weights alone. It is accepted, and the run goes on from the epoch its
    name gives, but Adam starts with empty moments and the stopping rule starts over: what it does not
    know it cannot restore, and saying so is better than pretending otherwise.
    """
    kept = sorted(f for f in os.listdir(out) if f.startswith('net-epoch') and f.endswith('.pt'))
    if not kept:
        print('--resume: nothing to take up in %s, starting at epoch 1' % out, flush=True)
        return 1, float('inf'), 0, 0
    path = os.path.join(out, kept[-1])
    held = torch.load(path, map_location=device, weights_only=False)
    if not isinstance(held, dict) or 'model' not in held:
        epoch = int(kept[-1][len('net-epoch'):-len('.pt')])
        model.load_state_dict(held)
        print('--resume: %s holds the weights only - going on at epoch %d, with the optimizer and '
              'the stopping rule starting over' % (kept[-1], epoch + 1), flush=True)
        return epoch + 1, float('inf'), 0, 0
    model.load_state_dict(held['model'])
    optimizer.load_state_dict(held['optimizer'])
    print('--resume: %s, going on at epoch %d, best so far epoch %d at %.6f, %d epochs without an '
          'improvement' % (kept[-1], held['epoch'] + 1, held['bestEpoch'], held['bestLoss'],
                           held['since']), flush=True)
    return held['epoch'] + 1, held['bestLoss'], held['bestEpoch'], held['since']


def make_optimizer(model, arguments):
    """Adam as before, or RangerLite with the settings nnue-pytorch gives it by default."""
    if arguments.optimizer == 'adam':
        return torch.optim.Adam(model.parameters(), lr=arguments.learning_rate)
    from ranger_lite import RangerLite
    # As RangerLiteWrapper(legacy_mode=False) configures it: lookahead every 5 steps blended at
    # 0.5, positive-negative momentum 1.0, no norm loss, no weight decay, betas and eps default.
    return RangerLite(model.parameters(), lr=arguments.learning_rate, weight_decay=0.0,
                      use_legacy_scoping_bug=False, normloss_active=False, pnm_activate=True,
                      pnm_momentum=1.0, lookahead_blending_alpha=0.5, lookahead_mergetime=5)


def train_in_fixed_epochs(arguments, cache, validation, device):
    """Stockfish's way: an epoch is a fixed number of positions, not a pass over the data.

    The positions come from one endless stream: when a pass over the games is used up the next one
    starts, with games in a new order, as nnue-pytorch's loader cycles through its files. The
    learning rate falls by --lr-gamma after every epoch (StepLR, step 1). There is no stopping rule:
    the run goes to --epochs, and the nets in between are measured in games. The held back loss is
    only watched, every --validate-every epochs.

    A net goes out every --save-every epochs, the state to continue from after every epoch
    (last.pt) and kept every 100. --resume continues from last.pt; the stream then starts a fresh
    pass, which is the one thing a resumed run does differently from an uninterrupted one.
    """
    torch.manual_seed(arguments.seed)
    model = HalfKaNet(stacks=arguments.stacks).to(device)
    optimizer = make_optimizer(model, arguments)
    scheduler = torch.optim.lr_scheduler.StepLR(optimizer, step_size=1, gamma=arguments.lr_gamma)
    ranger = arguments.optimizer == 'ranger'
    steps = arguments.epoch_size // arguments.batch_size
    print('epochs of %d positions (%d batches of %d), up to %d epochs, %s at lr %g falling by %g '
          'an epoch, device %s' % (steps * arguments.batch_size, steps, arguments.batch_size,
                                   arguments.epochs, arguments.optimizer, arguments.learning_rate,
                                   arguments.lr_gamma, device), flush=True)
    first = 1
    last = os.path.join(arguments.out, 'last.pt')
    if arguments.resume and os.path.exists(last):
        held = torch.load(last, map_location=device, weights_only=False)
        model.load_state_dict(held['model'])
        optimizer.load_state_dict(held['optimizer'])
        scheduler.load_state_dict(held['scheduler'])
        first = held['epoch'] + 1
        print('--resume: going on at epoch %d, lr %g' % (first, scheduler.get_last_lr()[0]),
              flush=True)

    def stream():
        while True:
            yield from cache.batches(arguments.batch_size, device)

    def for_inference(on):
        # RangerLite keeps its lookahead (slow) weights apart; measuring and writing a net use
        # them, as nnue-pytorch does when it validates and saves.
        if ranger:
            optimizer.eval() if on else optimizer.train()

    batches = stream()
    for epoch in range(first, arguments.epochs + 1):
        blend = arguments.blend_start + (arguments.blend_end - arguments.blend_start) \
            * (epoch - 1) / max(1, arguments.epochs - 1)
        model.train()
        for_inference(False)
        total, start = 0.0, time.time()
        for seen in range(1, steps + 1):
            own, opponent, value, result, counts = next(batches)
            loss = loss_of(model(own, opponent), value, result, counts, blend)
            optimizer.zero_grad(set_to_none=True)
            loss.backward()
            optimizer.step()
            model.clamp_weights()
            total += loss.detach().item()
            if seen % arguments.report_every == 0:
                print('  epoch %d, batch %d/%d, loss %.6f, %.0f positions per second'
                      % (epoch, seen, steps, total / seen,
                         seen * arguments.batch_size / (time.time() - start)), flush=True)
        lr = scheduler.get_last_lr()[0]
        scheduler.step()
        for_inference(True)
        line = 'epoch %d done, lr %.3g, blend %.2f, loss %.6f' % (epoch, lr, blend, total / steps)
        if validation is not None and epoch % arguments.validate_every == 0:
            line += ', held back %.6f' % measure(model, validation, arguments.batch_size, device,
                                                 arguments.blend_end)
        if epoch % arguments.save_every == 0 or epoch == arguments.epochs:
            export.export(model, '%s/net-epoch%03d.nnue' % (arguments.out, epoch))
        state = {'epoch': epoch, 'model': model.state_dict(), 'optimizer': optimizer.state_dict(),
                 'scheduler': scheduler.state_dict()}
        torch.save(state, last)
        if epoch % 100 == 0:
            torch.save(state, '%s/state-epoch%03d.pt' % (arguments.out, epoch))
        print('%s, %.0f s' % (line, time.time() - start), flush=True)


def train(arguments):
    netfile.configure(accumulator=arguments.accumulator, l1=arguments.l1)
    print('net: accumulator %d, first layer %d, %d stack(s)'
          % (netfile.ACCUMULATOR_SIZE, netfile.L1_SIZE, arguments.stacks), flush=True)
    device = pick_device(arguments.device)
    if all(name.endswith('.gam') for name in arguments.cache):
        # Straight out of the packed game file: no prepared cache, the split over whole games and
        # over a seeded permutation, so two files of the same games get the same one.
        cache = GameFile(arguments.cache, arguments.batch_size, seed=arguments.seed,
                         validation_every=arguments.validation_every, part='training',
                         workers=arguments.workers, loader=arguments.loader,
                         skip_tactical=arguments.skip_tactical, skip_early=arguments.skip_early)
        validation = GameFile(arguments.cache, arguments.batch_size, seed=arguments.seed,
                              validation_every=arguments.validation_every, part='validation',
                              workers=arguments.workers, loader=arguments.loader,
                              skip_tactical=arguments.skip_tactical, skip_early=arguments.skip_early)
    else:
        if len(arguments.cache) != 1:
            raise SystemExit('several sources are only read as .gam game files')
        cache = PositionCache(arguments.cache[0])
        validation = PositionCache(arguments.validation) if arguments.validation else None
    if arguments.epoch_size > 0:
        if arguments.neighbours:
            raise SystemExit('--epoch-size does not take --neighbours')
        return train_in_fixed_epochs(arguments, cache, validation, device)
    if arguments.optimizer != 'adam':
        raise SystemExit('--optimizer ranger only with --epoch-size')
    # The initial weights come from the seed too. Without it two runs on the same data started from
    # different nets, and their losses could not be compared step by step - which is how the
    # compiled loader was checked against the python one.
    torch.manual_seed(arguments.seed)
    model = HalfKaNet(stacks=arguments.stacks).to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=arguments.learning_rate)
    batches = len(cache) // arguments.batch_size
    print('%d positions, %d batches of %d, device %s'
          % (len(cache), batches, arguments.batch_size, device))
    if validation is not None:
        print('%d positions held back for validation, measured at blend %.2f'
              % (len(validation), arguments.blend_end))
    bestLoss, bestEpoch, since = float('inf'), 0, 0
    first = 1
    if arguments.resume:
        first, bestLoss, bestEpoch, since = take_up(model, optimizer, arguments.out, device)

    for epoch in range(first, arguments.epochs + 1):
        # The blend walks from the first value to the second over the run: follow
        # the search first, then let the results of the games correct it.
        blend = arguments.blend_start + (arguments.blend_end - arguments.blend_start) \
            * (epoch - 1) / max(1, arguments.epochs - 1)
        model.train()
        total, seen = 0.0, 0
        start = time.time()
        for own, opponent, value, result, counts in cache.batches(arguments.batch_size, device):
            if arguments.neighbours:
                outputs, stack = model.all_heads(own, opponent)
                loss = neighbour_loss_of(outputs, stack, value, result, counts, blend,
                                         arguments.neighbour_weight)
            else:
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
        # The checkpoint carries the optimizer and the stopping rule as well as the weights, so that
        # --resume continues the run rather than starting a similar one. Adam's moments are part of
        # the state: without them the first epoch after a resume takes a different step than it would
        # have, and the run is no longer the run it claims to continue.
        torch.save({'epoch': epoch, 'model': model.state_dict(),
                    'optimizer': optimizer.state_dict(), 'bestLoss': bestLoss,
                    'bestEpoch': bestEpoch, 'since': since},
                   '%s/net-epoch%02d.pt' % (arguments.out, epoch))
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
        torch.save({'epoch': epoch, 'model': model.state_dict(),
                    'optimizer': optimizer.state_dict(), 'bestLoss': bestLoss,
                    'bestEpoch': bestEpoch, 'since': since},
                   '%s/net-epoch%02d.pt' % (arguments.out, epoch))
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
    parser.add_argument('--epochs', type=int, default=None,
                        help='800 with --epoch-size (the default), 20 with --epoch-size 0')
    parser.add_argument('--batch-size', type=int, default=16384)
    parser.add_argument('--learning-rate', type=float, default=None,
                        help='8.75e-4 with ranger, 1e-3 with adam')
    parser.add_argument('--blend-start', type=float, default=0.8,
                        help='weight of the search value at the first epoch')
    parser.add_argument('--blend-end', type=float, default=0.7,
                        help='and at the last one; what is left is the game result')
    parser.add_argument('--validation', default=None,
                        help='prefix of a cache of games not trained on, as prepare.py '
                             '--validation-every writes it')
    parser.add_argument('--patience', type=int, default=3,
                        help='epochs without an improvement on it before stopping')
    parser.add_argument('--stacks', type=int, default=1,
                        help='how many layer stacks: 1 is the net as it was, 8 one head per phase '
                             'of the game, chosen by the number of pieces as in the engine')
    parser.add_argument('--accumulator', type=int, default=256,
                        help='width of the accumulator per perspective; the engine has to be built '
                             'with the same size')
    parser.add_argument('--l1', type=int, default=32,
                        help='width of the first dense layer of each head; the engine has to be '
                             'built with the same size')
    parser.add_argument('--neighbours', action='store_true',
                        help='with stacks: every position trains its own head and the two next to '
                             'it; the held back loss is still measured with the own head alone, as '
                             'the engine evaluates')
    parser.add_argument('--neighbour-weight', type=float, default=1.0,
                        help='what a neighbouring head counts against the own one')
    parser.add_argument('--loader', choices=['native', 'python'], default='native',
                        help='native: the compiled loader, verified bit for bit against the python '
                             'one and about twenty times faster per core; python: the old one')
    parser.add_argument('--skip-tactical', action=argparse.BooleanOptionalAction, default=True,
                        help='leave out every position whose move captures or that is in check, in '
                             'training and validation alike - the native loader only. On by default '
                             '(learning 7: about +75 Elo); --no-skip-tactical only to reproduce an '
                             'old result without it')
    parser.add_argument('--skip-early', action=argparse.BooleanOptionalAction, default=False,
                        help='keep early positions only with a probability rising over the ply, by '
                             'nnue-pytorch\'s default curve (0.1 at ply 0 to 1.0 from ply 20) - '
                             'in training and validation alike; a test, off by default')
    parser.add_argument('--epoch-size', type=int, default=10_000_000,
                        help='positions per epoch: an epoch is a fixed slice of an endless stream, as '
                             'in nnue-pytorch, with no stopping rule - the standard (learning 8); 0 for '
                             'the old way, one pass an epoch and a stop on the held back loss')
    parser.add_argument('--lr-gamma', type=float, default=0.992,
                        help='with --epoch-size: the learning rate is multiplied by it after every '
                             'epoch (nnue-pytorch: 0.992)')
    parser.add_argument('--optimizer', choices=['adam', 'ranger'], default=None,
                        help='ranger: RangerLite as nnue-pytorch uses it - the default with --epoch-size; '
                             'adam: the default with --epoch-size 0, and the only choice there')
    parser.add_argument('--validate-every', type=int, default=10,
                        help='with --epoch-size: epochs between two measurements of the held back loss')
    parser.add_argument('--save-every', type=int, default=100,
                        help='with --epoch-size: epochs between two nets written (nnue-pytorch: 20; ours 100, Volker 2026-10-05)')
    parser.add_argument('--resume', action='store_true',
                        help='carry on from the newest checkpoint in --out instead of starting over')
    parser.add_argument('--device', default='auto')
    parser.add_argument('--report-every', type=int, default=50)
    arguments = parser.parse_args()
    fixed = arguments.epoch_size > 0
    if arguments.epochs is None:
        arguments.epochs = 800 if fixed else 20
    if arguments.optimizer is None:
        arguments.optimizer = 'ranger' if fixed else 'adam'
    if arguments.learning_rate is None:
        arguments.learning_rate = 8.75e-4 if arguments.optimizer == 'ranger' else 1e-3
    train(arguments)
