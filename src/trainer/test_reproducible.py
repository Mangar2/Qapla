"""Proves that the trainer produces the same nets as the version recorded in reproducible.json.

    .venv/bin/python test_reproducible.py            # check against the record, exit 1 on a difference
    .venv/bin/python test_reproducible.py --record   # write the record anew (only for a minor or major)

The trainer is versioned like the engine is checked by its node count (CHANGELOG.md): a patch must
produce bit-identical nets under every setting, a minor changes the net under at least one. This test
is the proof. It trains a small fixture - the first 2000 games of set 1 - on the CPU, once per entry of
RUNS, and compares the md5 of every net written with the record.

On the CPU two runs with the same seed give the same bits, the workers included. On the GPU (mps) they
do not, not even without workers (measured 2026-10-07), so this test cannot run there, and a GPU
training carries a spread of its own that only games measure.

RUNS is the smallest set that still reaches every option that changes a net: each procedure (one pass
an epoch with Adam, fixed epochs with RangerLite or Adam), one and eight heads, both loaders, the
filters, the sizes, the worker count (it changes the order), non-default values of every number, and a
resumed run. A new option that changes nets gets a place in one of them, or a run of its own.
"""

import argparse
import glob
import hashlib
import itertools
import json
import os
import re
import shutil
import subprocess
import sys

import format

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.join(HERE, '../../test/nnue/dataset/set1-hce-depth6-nowdl.gam')
WORK = os.path.join(HERE, '../../tmp/reproducible')
RECORD = os.path.join(HERE, 'reproducible.json')
GAMES = 2000

COMMON = ['--device', 'cpu', '--batch-size', '1024', '--report-every', '1000000']
RUNS = {
    'pass-adam-1head': ['--epoch-size', '0', '--epochs', '2', '--no-skip-tactical', '--workers', '2'],
    'pass-adam-8heads-neighbours': ['--epoch-size', '0', '--epochs', '2', '--stacks', '8', '--neighbours',
                                    '--neighbour-weight', '0.5', '--skip-early', '--seed', '7',
                                    '--validation-every', '50', '--blend-start', '0.9', '--blend-end',
                                    '0.5', '--learning-rate', '5e-4', '--patience', '1', '--workers', '0'],
    'pass-python-loader': ['--epoch-size', '0', '--epochs', '1', '--no-skip-tactical', '--loader', 'python',
                           '--workers', '0'],
    'fixed-ranger-1head': ['--epoch-size', '100000', '--epochs', '2', '--save-every', '1',
                           '--validate-every', '1', '--workers', '2'],
    'fixed-adam-8heads-512x16': ['--epoch-size', '100000', '--epochs', '2', '--save-every', '1',
                                 '--stacks', '8', '--accumulator', '512', '--l1', '16', '--skip-early',
                                 '--optimizer', 'adam', '--lr-gamma', '0.9', '--learning-rate', '2e-3',
                                 '--no-skip-tactical', '--workers', '1'],
}
# A resumed run: one epoch, then --resume to the second. Its second net is recorded.
RESUMED = ('fixed-ranger-resumed', ['--epoch-size', '100000', '--save-every', '1', '--workers', '2'])


def md5(path):
    with open(path, 'rb') as stream:
        return hashlib.md5(stream.read()).hexdigest()


def fixture():
    path = os.path.join(WORK, 'fixture.gam')
    if not os.path.exists(path):
        os.makedirs(WORK, exist_ok=True)
        format.write_games(path, list(itertools.islice(format.read_games(SOURCE), GAMES)))
    return path


def epoch_of(name):
    return int(re.search(r'epoch0*(\d+)\.nnue$', name).group(1))


def nets(out):
    found = sorted(glob.glob(os.path.join(out, '*.nnue')), key=epoch_of)
    return [md5(path) for path in found]


def train(name, options, data):
    out = os.path.join(WORK, name)
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)
    command = [sys.executable, '-u', 'train.py', data, '--out', out] + COMMON + options
    done = subprocess.run(command, cwd=HERE, capture_output=True, text=True)
    if done.returncode:
        raise SystemExit('%s failed:\n%s' % (name, done.stdout[-2000:] + done.stderr[-2000:]))
    return out


def run_all():
    data = fixture()
    result = {'fixture': md5(data), 'runs': {}}
    for name, options in RUNS.items():
        result['runs'][name] = nets(train(name, options, data))
        print('%-30s %s' % (name, ' '.join(m[:8] for m in result['runs'][name])), flush=True)
    name, options = RESUMED
    out = train(name, options + ['--epochs', '1'], data)
    command = [sys.executable, '-u', 'train.py', data, '--out', out] + COMMON + options \
        + ['--epochs', '2', '--resume']
    done = subprocess.run(command, cwd=HERE, capture_output=True, text=True)
    if done.returncode:
        raise SystemExit('%s failed:\n%s' % (name, done.stdout[-2000:] + done.stderr[-2000:]))
    result['runs'][name] = nets(out)
    print('%-30s %s' % (name, ' '.join(m[:8] for m in result['runs'][name])), flush=True)
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--record', action='store_true',
                        help='write the nets of this trainer as the new record (minor or major only)')
    arguments = parser.parse_args()
    import version
    result = run_all()
    result['version'] = version.VERSION
    if arguments.record:
        with open(RECORD, 'w') as stream:
            json.dump(result, stream, indent=2)
            stream.write('\n')
        print('recorded for %s' % version.VERSION)
        return
    with open(RECORD) as stream:
        record = json.load(stream)
    if result['fixture'] != record['fixture']:
        raise SystemExit('the fixture differs from the recorded one - the test proves nothing')
    differ = [name for name in record['runs'] if record['runs'][name] != result['runs'].get(name)]
    missing = [name for name in result['runs'] if name not in record['runs']]
    if differ or missing:
        print('DIFFERENT from %s: %s%s' % (record['version'], ', '.join(differ),
                                         ('; not in the record: ' + ', '.join(missing)) if missing else ''))
        sys.exit(1)
    print('identical to %s in all %d runs' % (record['version'], len(record['runs'])))


if __name__ == '__main__':
    main()
