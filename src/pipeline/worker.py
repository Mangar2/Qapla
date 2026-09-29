"""One machine that takes work out of the table until there is none, then stops.

    python3 src/pipeline/worker.py                 work until the table is empty
    python3 src/pipeline/worker.py --once          one piece, for trying it out

Every worker is the same program and is bound to nothing: it asks the table for the most urgent open
piece, does it, hands the result to the object store and takes the next. That is what makes a machine
that can be taken away at any moment usable - the only thing lost is the piece in flight, and the
supervisor gives that back.

A piece of work is a chunk of a set, and there are three kinds. Playing a chunk produces a pgn of
games and then writes the job for labelling that same chunk. Labelling searches every position of it
and, when it was the last of its set, writes the job for converting the set. Converting joins the
labelled chunks into the packed game files the training reads.

When the table has nothing left the worker exits with 0, and whatever started it may switch the
machine off. That is the cost control: no work, no machine.
"""

import argparse
import json
import os
import socket
import subprocess
import sys
import time
from datetime import datetime, timezone

import pipeline as pl
import tasks as tk

LABEL_WEIGHT, CONVERT_WEIGHT = 0, 300000


def note(line):
    stamp = datetime.now(timezone.utc).strftime('%Y-%m-%d %H:%M:%S')
    print(f'{stamp} {line}', flush=True)


def identity():
    """The instance id, because that is what the supervisor can check for being alive."""
    try:
        token = subprocess.run(
            ['curl', '-s', '-X', 'PUT', 'http://169.254.169.254/latest/api/token',
             '-H', 'X-aws-ec2-metadata-token-ttl-seconds: 60'],
            capture_output=True, text=True, timeout=5).stdout.strip()
        if token:
            answer = subprocess.run(
                ['curl', '-s', '-H', f'X-aws-ec2-metadata-token: {token}',
                 'http://169.254.169.254/latest/meta-data/instance-id'],
                capture_output=True, text=True, timeout=5).stdout.strip()
            if answer.startswith('i-'):
                return answer
    except (subprocess.TimeoutExpired, OSError):
        pass
    return socket.gethostname()


# --------------------------------------------------------------------------------- the settings

PLAY_INI = """\
# Written by the worker for {set} chunk {chunk}. The openings are a slice of the book, which is what
# makes a chunk independent of every other one.
concurrency={concurrency}

[each]
tc=depth:{depth}
proto=uci

[logging]
path=test/log
engine=false

[openings]
file={book}
order=sequential
start={first_opening}

[draw]
movenumber=60
movecount=20
score=20
test=false

[pgnoutput]
file={output}
append=false
min=true
clock=false
eval=false
depth=false
pv=false
notation={notation}

[tournament]
type=round-robin
games={games}
rounds=1
repeat={repeat}
noswap={noswap}
ratinginterval=0
file={state_file}
"""

LABEL_INI = """\
# Written by the worker for {set} chunk {chunk}. Long notation and the full tag set are not free
# choices: src/trainer/convert.py reads this pgn, decodes long notation without a move generator,
# and takes the result of the game from the Result tag.
concurrency={concurrency}

[each]
tc=depth:{depth}
proto=uci

[logging]
path=test/log
engine=false

[analysis]
pgn={input}
direction=reverse

[pgnoutput]
file={output}
append=false
min=false
clock=false
eval=true
depth=false
pv=false
notation=lan
"""


class Worker:

    def __init__(self, cfg):
        self.cfg = cfg
        self.table = tk.Tasks(cfg)
        self.profile = cfg['worker']
        self.repo = self.profile['repo']
        self.bucket = cfg['aws']['bucket']
        self.owner = identity()
        self.log = os.path.join(self.repo, 'test/log/worker.log')
        os.makedirs(os.path.dirname(self.log), exist_ok=True)

    # ------------------------------------------------------------------ helpers

    def definition(self, name):
        found = next((s for s in self.cfg['sets'] if s['name'] == name), None)
        if found is None:
            raise SystemExit(f'the configuration knows no set {name}')
        return found

    def run(self, command, seconds=None):
        with open(self.log, 'a') as log:
            log.write(f'\n===== {datetime.now(timezone.utc)} {" ".join(command)}\n')
            log.flush()
            return subprocess.call(command, cwd=self.repo, stdout=log,
                                   stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL,
                                   timeout=seconds)

    def s3(self, *arguments):
        return subprocess.call(['aws', 's3', *arguments, '--only-show-errors'],
                               cwd=self.repo) == 0

    def engine_arguments(self, name):
        engine = self.profile['engines'][name]
        out = ['--engine', f'name={name}', f'cmd={os.path.join(self.repo, engine["binary"])}']
        for option, value in engine.get('options', {}).items():
            if option.lower().endswith('file'):
                value = os.path.join(self.repo, str(value))
            out.append(f'option.{option}={value}')
        return out

    def qet(self, ini_path, engines):
        command = [os.path.expanduser(self.profile['qet']), f'--settingsfile={ini_path}']
        for name in engines:
            command += self.engine_arguments(name)
        return command

    # ------------------------------------------------------------------ the three kinds

    def play(self, work):
        name, chunk = work['set'], work['chunk']
        d = self.definition(name)
        selfplay = d['white'] == d['black']
        per_opening = 1 if selfplay else 2
        first = (chunk - 1) * d['chunk_openings'] + 1
        openings = min(d['chunk_openings'], d['openings'] - first + 1)
        if openings <= 0:
            note(f'{name} chunk {chunk} is past the end of the book - nothing to play')
            return True
        output = os.path.join(self.repo, f'test/nnue/{name}-played/chunk-{chunk:04d}.pgn')
        os.makedirs(os.path.dirname(output), exist_ok=True)
        ini = os.path.join(self.repo, f'test/log/worker-play-{name}-{chunk:04d}.ini')
        with open(ini, 'w') as f:
            f.write(PLAY_INI.format(
                set=name, chunk=chunk, concurrency=self.profile['concurrency'],
                depth=d['play_depth'], book=d['book'], output=output,
                notation=d.get('notation', 'san'), games=openings * per_opening,
                repeat=per_opening, noswap='true' if selfplay else 'false',
                first_opening=first,
                state_file=f'test/log/worker-play-{name}-{chunk:04d}.state'))
        if os.path.exists(output):
            os.remove(output)
        engines = [d['white']] if selfplay else [d['white'], d['black']]
        command = self.qet(ini, engines)
        if selfplay:                      # the same engine twice, under two names
            second = self.engine_arguments(d['white'])
            second[1] = f'name={d["white"]}-b'
            command += second
        if self.run(command) != 0:
            note(f'{name} chunk {chunk}: the tester failed - see {self.log}')
            return False
        facts = pl.read_pgn(output)
        wanted = openings * per_opening
        note(f'{name} chunk {chunk} played: {facts["games"]} games, {facts["plies"]} plies, '
             f'outcomes {facts["outcomes"]}')
        if facts['games'] < wanted or facts['terminators'] < facts['games']:
            note(f'{name} chunk {chunk}: {facts["games"]} of {wanted} games, '
                 f'{facts["terminators"]} with a result - not handed over')
            return False
        if not self.s3('cp', output, f's3://{self.bucket}/{name}/played/'):
            note(f'{name} chunk {chunk}: could not hand it over')
            return False
        # The successor: labelling this very chunk, and it outranks anything not yet played.
        self.table.add_job(name, 'label', chunk, d['base'] + LABEL_WEIGHT)
        os.remove(output)
        return True

    def label(self, work):
        name, chunk = work['set'], work['chunk']
        d = self.definition(name)
        source = os.path.join(self.repo, f'test/nnue/{name}-played/chunk-{chunk:04d}.pgn')
        os.makedirs(os.path.dirname(source), exist_ok=True)
        if not os.path.exists(source):
            if not self.s3('cp', f's3://{self.bucket}/{name}/played/chunk-{chunk:04d}.pgn',
                           source):
                note(f'{name} chunk {chunk}: the played pgn is not in the store')
                return False
        output = os.path.join(self.repo, f'test/nnue/{name}-labelled/chunk-{chunk:04d}.pgn')
        os.makedirs(os.path.dirname(output), exist_ok=True)
        ini = os.path.join(self.repo, f'test/log/worker-label-{name}-{chunk:04d}.ini')
        with open(ini, 'w') as f:
            f.write(LABEL_INI.format(set=name, chunk=chunk,
                                     concurrency=self.profile['concurrency'],
                                     depth=d['label_depth'], input=source, output=output))
        if os.path.exists(output):
            os.remove(output)
        if self.run(self.qet(ini, [d['label_engine']])) != 0:
            note(f'{name} chunk {chunk}: the analysis failed - see {self.log}')
            return False
        expect = pl.read_pgn(source)['games']
        facts = pl.read_pgn(output)
        note(f'{name} chunk {chunk} labelled: {facts["games"]} games, {facts["plies"]} plies, '
             f'{facts["comments"]} values, {facts["results"]} result tags')
        if (facts['games'] < expect or facts['results'] < facts['games']
                or facts['comments'] < 0.95 * facts['plies'] or facts['san'] > facts['lan']):
            note(f'{name} chunk {chunk}: the labelled pgn did not check out - not handed over')
            return False
        if not self.s3('cp', output, f's3://{self.bucket}/{name}/labelled/'):
            note(f'{name} chunk {chunk}: could not hand it over')
            return False
        os.remove(source)
        os.remove(output)
        return True

    def convert(self, work):
        name = work['set']
        d = self.definition(name)
        directory = os.path.join(self.repo, f'test/nnue/{name}-labelled')
        os.makedirs(directory, exist_ok=True)
        note(f'{name}: fetching every labelled chunk')
        if not self.s3('sync', f's3://{self.bucket}/{name}/labelled/', directory):
            note(f'{name}: could not fetch the labelled chunks')
            return False
        import glob
        parts = sorted(glob.glob(os.path.join(directory, 'chunk-*.pgn')))
        joined = os.path.join(directory, f'{name}-joined.pgn')
        with open(joined, 'wb') as out:
            for part in parts:
                with open(part, 'rb') as inp:
                    while True:
                        block = inp.read(1 << 22)
                        if not block:
                            break
                        out.write(block)
        games = pl.count_lines(joined, '[White ')
        note(f'{name}: {len(parts)} chunks joined, {games} games')
        dataset = os.path.join(self.repo, 'test/nnue/dataset')
        os.makedirs(dataset, exist_ok=True)
        for variant in d.get('wdl', ['result', 'none']):
            target = os.path.join(dataset, d['output'] +
                                  ('' if variant == 'result' else '-nowdl') + '.gam')
            converter = os.path.join(self.repo, 'src/trainer/convert.py')
            if self.run([sys.executable, converter, joined, target, '--wdl', variant]) != 0:
                note(f'{name}: the converter failed for wdl={variant}')
                return False
            if not self.s3('cp', target, f's3://{self.bucket}/{name}/dataset/'):
                note(f'{name}: could not hand over {os.path.basename(target)}')
                return False
            note(f'{name}: {os.path.basename(target)} handed over')
        os.remove(joined)
        return True

    # ------------------------------------------------------------------ the loop

    def remaining_for(self, name, exclude):
        """Is anything of this set still open or in progress, other than my own claim?"""
        for item in self.table.open_work():
            if item['set'] != name or item['kind'] == 'convert':
                continue
            # A range that is used up still sits in the table until somebody tries to take from it,
            # and counting it as work meant the conversion of a finished set never became open.
            if item['form'] == 'range' and item['lo'] > item['hi']:
                continue
            return True
        for claim in self.table.claims():
            if claim['set'] == name and claim['kind'] != 'convert' and claim['id'] != exclude:
                return True
        return False

    def once(self):
        work = self.table.take('aws', self.owner)
        if work is None:
            return None
        note(f'took {work["set"]} {work["kind"]} chunk {work["chunk"]}')
        started = time.time()
        doer = {'play': self.play, 'label': self.label, 'convert': self.convert}[work['kind']]
        try:
            good = doer(work)
        except Exception as error:
            note(f'{work["set"]} {work["kind"]} {work["chunk"]} raised: {error}')
            good = False
        if not good:
            self.table.give_back({
                'id': f'claim#{self.owner}#{work["set"]}#{work["kind"]}#{work["chunk"]:04d}',
                **work, 'where': 'aws'})
            note('gave the piece back for another machine to try')
            return False
        claim_id = f'claim#{self.owner}#{work["set"]}#{work["kind"]}#{work["chunk"]:04d}'
        if not self.table.release_claim(self.owner, work):
            note('the claim could not be cleared - stopping, because a table that keeps stale '
                 'claims never lets a set reach its conversion')
            return False
        if work['kind'] == 'label' and not self.remaining_for(work['set'], claim_id):
            d = self.definition(work['set'])
            self.table.add_job(work['set'], 'convert', 0, d['base'] + CONVERT_WEIGHT)
            note(f'{work["set"]}: that was the last chunk - the conversion is now open')
        note(f'finished {work["set"]} {work["kind"]} chunk {work["chunk"]} '
             f'in {time.time() - started:.0f}s')
        return True

    def loop(self, only_once=False):
        note(f'worker {self.owner} starting, table: {self.table.summary()}')
        while True:
            outcome = self.once()
            if outcome is None:
                note('no work left - stopping')
                return 0
            if outcome is False:
                note('stopping after a failure, so a broken machine does not eat the queue')
                return 1
            if only_once:
                return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--once', action='store_true')
    parser.add_argument('--config', default=pl.CONFIG)
    parser.add_argument('--local', default=pl.LOCAL)
    arguments = parser.parse_args()
    cfg = pl.load_config(arguments.config, arguments.local)
    raise SystemExit(Worker(cfg).loop(arguments.once))


if __name__ == '__main__':
    main()
