"""Compares two states of the trainer: do they make the same nets under one command?

    .venv/bin/python compare_trainers.py <commit A> <commit B> -- <train.py options>
    .venv/bin/python compare_trainers.py 52dc531 ca34193 -- --epochs 2 --workers 2

Both states come out of git (`git archive <commit> src/trainer`), never out of the working copy, into
tmp/compare-trainers/<commit>/. Each trains the fixture of test_reproducible.py - the first 2000 games
of set 1, a copy of its own so that no trainer touches another's index - on the CPU, with the same
command: the given options plus --device cpu --batch-size 1024. The md5 of every net, in epoch order,
decides: identical, or different from the first epoch on which they part.

On the CPU one trainer with one command gives the same bits every time (measured 2026-10-07), so a
difference is a difference of the trainers. An option one of the two states does not know is reported
and the comparison refused - it is never dropped silently. The result goes to stdout and is appended
to trainer-comparisons.md, with both commits and the whole command.
"""

import glob
import hashlib
import itertools
import os
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, '..', '..'))
WORK = os.path.join(REPO, 'tmp', 'compare-trainers')
SOURCE = os.path.join(REPO, 'test', 'nnue', 'dataset', 'set1-hce-depth6-nowdl.gam')
RECORD = os.path.join(HERE, 'trainer-comparisons.md')
GAMES = 2000
FIXED = ['--device', 'cpu', '--batch-size', '1024', '--report-every', '1000000']


def md5(path):
    with open(path, 'rb') as stream:
        return hashlib.md5(stream.read()).hexdigest()


def full(commit):
    return subprocess.run(['git', 'rev-parse', '--short', commit], cwd=REPO, check=True,
                          capture_output=True, text=True).stdout.strip()


def tree(commit):
    """src/trainer of the commit, unpacked once."""
    root = os.path.join(WORK, commit)
    trainer = os.path.join(root, 'src', 'trainer')
    if not os.path.exists(os.path.join(trainer, 'train.py')):
        os.makedirs(root, exist_ok=True)
        archive = subprocess.run(['git', 'archive', commit, 'src/trainer'], cwd=REPO, check=True,
                                 capture_output=True).stdout
        subprocess.run(['tar', '-x', '-C', root], input=archive, check=True)
    return trainer


def fixture():
    path = os.path.join(WORK, 'fixture.gam')
    if not os.path.exists(path):
        sys.path.insert(0, HERE)
        import format
        os.makedirs(WORK, exist_ok=True)
        format.write_games(path, list(itertools.islice(format.read_games(SOURCE), GAMES)))
    return path


def known_options(trainer):
    done = subprocess.run([sys.executable, 'train.py', '--help'], cwd=trainer, capture_output=True,
                          text=True)
    return set(re.findall(r'(--[a-z][a-z0-9-]*)', done.stdout))


def epoch_of(name):
    return int(re.search(r'epoch0*(\d+)\.nnue$', name).group(1))


def train(commit, options):
    trainer = tree(commit)
    tag = hashlib.md5(' '.join(options).encode()).hexdigest()[:10]
    out = os.path.join(WORK, commit, 'runs', tag)
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)
    data = os.path.join(out, 'fixture.gam')
    shutil.copy(fixture(), data)
    start = time.time()
    done = subprocess.run([sys.executable, '-u', 'train.py', data, '--out', out] + FIXED + options,
                          cwd=trainer, capture_output=True, text=True)
    with open(os.path.join(out, 'train.log'), 'w') as stream:
        stream.write(done.stdout + done.stderr)
    if done.returncode:
        raise SystemExit('%s failed:\n%s' % (commit, (done.stdout + done.stderr)[-2000:]))
    nets = sorted(glob.glob(os.path.join(out, '*.nnue')), key=epoch_of)
    return [md5(path) for path in nets], time.time() - start


def main():
    if '--' not in sys.argv or sys.argv.index('--') != 3:
        raise SystemExit(__doc__)
    a, b = full(sys.argv[1]), full(sys.argv[2])
    options = sys.argv[4:]
    asked = {o.split('=')[0] for o in options if o.startswith('--')}
    for commit in (a, b):
        missing = sorted(asked - known_options(tree(commit)))
        if missing:
            raise SystemExit('refused: %s does not know %s' % (commit, ', '.join(missing)))
    nets_a, time_a = train(a, options)
    nets_b, time_b = train(b, options)
    if nets_a == nets_b:
        verdict = 'identical (%d nets)' % len(nets_a)
    else:
        first = next((i for i, (x, y) in enumerate(zip(nets_a, nets_b)) if x != y), None)
        verdict = ('different from epoch %d on' % (first + 1)) if first is not None else \
            'different: %d nets against %d' % (len(nets_a), len(nets_b))
    line = '| %s | %s | `%s` | %s | %.0f s / %.0f s |' % (a, b, ' '.join(options), verdict, time_a, time_b)
    print(line)
    new = not os.path.exists(RECORD)
    with open(RECORD, 'a') as stream:
        if new:
            stream.write('# Trainer comparisons\n\nWritten by compare_trainers.py: two commits, one '
                         'command, the fixture of test_reproducible.py on the CPU.\n\n'
                         '| A | B | command | result | time A / B |\n|---|---|---|---|---|\n')
        stream.write(line + '\n')


if __name__ == '__main__':
    main()
