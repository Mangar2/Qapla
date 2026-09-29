"""Runs the steps that turn book leaves into trained nets, one step at a time.

    python3 src/pipeline/pipeline.py status          what is done, what is next
    python3 src/pipeline/pipeline.py run             run this host's pending steps
    python3 src/pipeline/pipeline.py run --only ID   run one step
    python3 src/pipeline/pipeline.py launch          start other hosts' steps over ssh

Every value lives in pipeline.toml, the logic of the step kinds lives here. A configuration that
could express any sequence would be a shell script with an extra layer; one that names engines,
depths, files and hosts is what actually changes between runs.

Where the state is: on the host that does the work, under test/log. A step is resumable because
the tools are - qet continues a tournament from its state file, and the labelling pass skips the
chunks that carry a .done marker. So a step that was interrupted is simply started again, and the
state file here says which steps are behind us, not how far into one we got.

Autonomy: a remote step is started with setsid, detached from the ssh session, and writes its own
log on its own host. The machine that launched it may sleep or be switched off.
"""

import argparse
import json
import os
import re
import socket
import subprocess
import sys
import threading
import time
import tomllib
from datetime import datetime, timezone

HERE = os.path.dirname(os.path.abspath(__file__))
CONFIG = os.path.join(HERE, 'pipeline.toml')
LOCAL = os.path.join(HERE, 'local.toml')


# --------------------------------------------------------------------------- basics

def now():
    return datetime.now(timezone.utc).astimezone().strftime('%Y-%m-%d %H:%M:%S')


def load_config(path=CONFIG, local=LOCAL):
    """The steps out of the repository, the machines out of a file that is not in it.

    pipeline.toml names no host, no path, no bucket and no account - only what is to be done, with
    engines named the way local.toml defines them. That is what lets the steps be public while the
    installation stays private, and it is why an s3 prefix in a step is relative: the bucket comes
    from local.toml and is put in front of it here.
    """
    with open(path, 'rb') as f:
        cfg = tomllib.load(f)
    if not os.path.exists(local):
        raise SystemExit(f'{local} is missing - copy local.example.toml and fill it in')
    with open(local, 'rb') as f:
        cfg.update(tomllib.load(f))
    bucket = cfg.get('aws', {}).get('bucket')
    for step in cfg['steps']:
        for key, value in list(step.items()):
            if key.startswith('s3_') and isinstance(value, str) and not value.startswith('s3://'):
                if not bucket:
                    raise SystemExit(f'step {step["id"]} names {key}, but local.toml has no bucket')
                step[key] = f's3://{bucket}/{value.lstrip("/")}'
    return cfg


def this_host(cfg, override=None):
    """The key of the host entry for this machine.

    Matched on the hostname, unless the caller says which host this is - an ec2 instance gets a
    fresh hostname on every launch, so a spot host is named on the command line.
    """
    if override:
        if override not in cfg['hosts']:
            raise SystemExit(f'no hosts entry named {override}')
        return override
    me = socket.gethostname()
    for key, host in cfg['hosts'].items():
        if host['hostname'] == me or me.startswith(host['hostname']):
            return key
    raise SystemExit(f'this machine ({me}) is in no hosts entry of the configuration')


def repo_of(cfg, host_key):
    return cfg['hosts'][host_key]['repo']


def absolute(repo, path):
    return path if os.path.isabs(path) else os.path.join(repo, path)


class State:
    """What is done, on this host. One file, rewritten whole - it is tiny."""

    def __init__(self, repo):
        self.path = os.path.join(repo, 'test/log/pipeline-state.json')
        self.log = os.path.join(repo, 'test/log/pipeline.log')
        os.makedirs(os.path.dirname(self.path), exist_ok=True)
        self.data = {'steps': {}}
        if os.path.exists(self.path):
            with open(self.path) as f:
                self.data = json.load(f)

    def save(self):
        tmp = self.path + '.tmp'
        with open(tmp, 'w') as f:
            json.dump(self.data, f, indent=2)
        os.replace(tmp, self.path)

    def note(self, line):
        with open(self.log, 'a') as f:
            f.write(f'{now()} {line}\n')
        print(f'{now()} {line}', flush=True)

    def of(self, step_id):
        return self.data['steps'].get(step_id, {}).get('status', 'pending')

    def begin(self, step):
        entry = self.data['steps'].setdefault(step['id'], {})
        entry['status'] = 'running'
        entry['kind'] = step['kind']
        entry['host'] = step['host']
        entry.setdefault('first_started', now())
        entry['started'] = now()
        self.save()
        self.note(f"{step['id']} start kind={step['kind']}")
        return time.time()

    def finish(self, step, t0, result):
        entry = self.data['steps'][step['id']]
        entry['status'] = 'done'
        entry['ended'] = now()
        entry['seconds'] = round(time.time() - t0)
        entry['result'] = result
        self.save()
        self.note(f"{step['id']} done {entry['seconds']}s {result}")

    def fail(self, step, t0, message):
        entry = self.data['steps'][step['id']]
        entry['status'] = 'failed'
        entry['ended'] = now()
        entry['seconds'] = round(time.time() - t0)
        entry['error'] = message
        self.save()
        self.note(f"{step['id']} FAILED after {entry['seconds']}s: {message}")


def run_tool(command, repo, log_path):
    """Runs a tool, appending its output to a log. Returns its exit code."""
    with open(log_path, 'a') as log:
        log.write(f'\n===== {now()} {" ".join(command)}\n')
        log.flush()
        return subprocess.call(command, cwd=repo, stdout=log, stderr=subprocess.STDOUT,
                               stdin=subprocess.DEVNULL)


def engine_arguments(cfg, host_key, name, repo):
    """The --engine block of one engine as the configuration names it."""
    engines = cfg['hosts'][host_key]['engines']
    if name not in engines:
        raise SystemExit(f'host {host_key} has no engine "{name}"')
    engine = engines[name]
    args = ['--engine', f'name={name}', f'cmd={absolute(repo, engine["binary"])}']
    for option, value in engine.get('options', {}).items():
        # A net is named by an absolute path: the tester starts the engine in its own directory.
        if option.lower().endswith('file'):
            value = absolute(repo, str(value))
        args.append(f'option.{option}={value}')
    return args


# --------------------------------------------------------------------------- step: play

PLAY_INI = """\
# Written by src/pipeline/pipeline.py for step {id}. Do not edit - edit pipeline.toml.
#
# One game per book leaf when selfplay is set: the search is deterministic at a fixed depth, so a
# leaf and a colour decide the game completely and a colour swap would return the same game twice.
#
# No [resign] block on purpose: in the tester a block that is there is in force and a block that
# is not there is off, so a won game is played to its end by leaving it out. [draw] is there, an
# endless shuffle teaches a net nothing. rapid stays off: the draw adjudication reads the engine's
# info lines and would have nothing to decide on without them.
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
append=true
perround={perround}
min=true
clock=false
eval=false
depth=false
pv=false
notation={notation}

[tournament]
type=round-robin
games={games}
rounds={rounds}
repeat={repeat}
noswap={noswap}
ratinginterval=0
file={state_file}
"""


def step_play(cfg, host_key, step, state):
    """Plays engine against engine from the book and writes the template pgn.

    With chunk_openings the run is split: every chunk plays its own slice of the book into its own
    pgn, and with s3_results hands it over as soon as it is finished. That is what makes a spot
    instance usable for a run of many hours - the existence of the result object says a chunk is
    done, so a machine that replaces a reclaimed one loses only the chunk in flight. Without those
    two settings it is one qet call over the whole book, resumable through the tournament file.
    """
    repo = repo_of(cfg, host_key)
    concurrency = step.get('concurrency', cfg['hosts'][host_key]['concurrency'])
    selfplay = step.get('selfplay', step['white'] == step['black'])
    per_opening = 1 if selfplay else 2      # a colour swap gives two games from one opening
    log_path = os.path.join(repo, 'test/log', f"pipeline-{step['id']}.log")

    def qet_command(ini_path):
        command = [os.path.expanduser(cfg['hosts'][host_key]['qet']), f'--settingsfile={ini_path}']
        command += engine_arguments(cfg, host_key, step['white'], repo)
        if step['black'] != step['white']:
            command += engine_arguments(cfg, host_key, step['black'], repo)
        else:
            # The same engine twice: qet needs two entries and they must not share a name.
            second = engine_arguments(cfg, host_key, step['white'], repo)
            second[1] = f"name={step['white']}-b"
            command += second
        return command

    def write_ini(ini_path, first_opening, games, output, state_file, rounds=1):
        with open(ini_path, 'w') as f:
            f.write(PLAY_INI.format(
                id=step['id'], concurrency=concurrency, depth=step['depth'], book=step['book'],
                output=output, notation=step.get('notation', 'san'), games=games,
                repeat=per_opening, noswap='true' if selfplay else 'false',
                first_opening=first_opening, state_file=state_file, rounds=rounds,
                perround='true' if step.get('per_round') else 'false'))

    openings_total = step.get('openings', step['games'] // per_opening)
    slice_size = step.get('chunk_openings')

    if step.get('per_round'):
        # One call for the whole book, and the tester closes a file at every round boundary. The
        # opening index runs on across rounds - checked: round 1 takes openings 0 and 1, round 2
        # takes 2 and 3 - so rounds are a way of cutting the output, not of cutting the work. That
        # replaces the chunking: no process start per piece, and a finished file appears every
        # hundred games instead of every twenty thousand.
        rounds_wanted = openings_total // step['round_openings']
        directory = absolute(repo, step['output_dir'])
        os.makedirs(directory, exist_ok=True)
        ini_path = os.path.join(repo, 'test/log', f"pipeline-{step['id']}.ini")
        write_ini(ini_path, 1, step['round_openings'] * per_opening,
                  os.path.join(directory, step['id'] + '.pgn'),
                  f"test/log/pipeline-{step['id']}.state", rounds=rounds_wanted)
        expect = rounds_wanted * step['round_openings'] * per_opening
        t0 = state.begin(step)
        if not preflight(cfg, host_key, [step['white'], step['black']], state, step['id']):
            state.fail(step, t0, 'preflight failed - nothing was played')
            return False
        state.note(f"{step['id']} one call, {rounds_wanted} rounds of "
                   f"{step['round_openings'] * per_opening} games, {expect} games in all")
        with Progress(state, step['id'], log_path, 'finished ', expect):
            code = run_tool(qet_command(ini_path), repo, log_path)
        if code != 0:
            state.fail(step, t0, f'qet exit {code} - run again, the tournament file holds the state')
            return False
        import glob
        parts = sorted(glob.glob(os.path.join(directory, step['id'] + '*.pgn')))
        played = sum(count_lines(part, '[White ') for part in parts)
        state.note(f"{step['id']} {len(parts)} files, {played} games")
        if played < expect:
            state.fail(step, t0, f'{played} of {expect} games - run again to continue')
            return False
        state.finish(step, t0, {'files': len(parts), 'games': played,
                                'bytes': sum(os.path.getsize(part) for part in parts)})
        return True

    if not slice_size:
        ini_path = os.path.join(repo, 'test/log', f"pipeline-{step['id']}.ini")
        write_ini(ini_path, 1, step['games'], step['output'],
                  f"test/log/pipeline-{step['id']}.state")
        t0 = state.begin(step)
        if not preflight(cfg, host_key, [step['white'], step['black']], state, step['id']):
            state.fail(step, t0, 'preflight failed - nothing was played')
            return False
        with Progress(state, step['id'], log_path, 'finished ', step['games']):
            code = run_tool(qet_command(ini_path), repo, log_path)
        output = absolute(repo, step['output'])
        if code != 0:
            state.fail(step, t0, f'qet exit {code} - run again, the tournament file holds the state')
            return False
        if not verify_play(output, step, state, step['games']):
            state.fail(step, t0, 'the template did not verify')
            return False
        facts = read_pgn(output)
        state.finish(step, t0, {'games': facts['games'], 'plies': facts['plies'],
                                'bytes': os.path.getsize(output), 'outcomes': facts['outcomes']})
        return True

    # Chunked: every chunk is a slice of the book, written and handed over on its own.
    directory = absolute(repo, step.get('output_dir', f"test/nnue/{step['id']}"))
    os.makedirs(directory, exist_ok=True)
    count = (openings_total + slice_size - 1) // slice_size
    first, last = step.get('chunk_range', [1, count])
    handed_over = set(s3_names(step['s3_results'])) if step.get('s3_results') else set()

    t0 = state.begin(step)
    if not preflight(cfg, host_key, [step['white'], step['black']], state, step['id']):
        state.fail(step, t0, 'preflight failed - nothing was played')
        return False
    state.note(f"{step['id']} {count} chunks of {slice_size} openings, doing {first}..{last}, "
               f"{len(handed_over)} already handed over")

    worked = 0
    for number in range(first, min(last, count) + 1):
        name = f'chunk-{number:04d}.pgn'
        output = os.path.join(directory, name)
        if name in handed_over or os.path.exists(output + '.done'):
            continue
        first_opening = (number - 1) * slice_size + 1
        openings = min(slice_size, openings_total - first_opening + 1)
        ini_path = os.path.join(repo, 'test/log', f"pipeline-{step['id']}-{number:04d}.ini")
        write_ini(ini_path, first_opening, openings * per_opening, output,
                  f"test/log/pipeline-{step['id']}-{number:04d}.state")
        if os.path.exists(output):
            os.remove(output)          # append=true in the ini, so a restart must start clean
        code = run_tool(qet_command(ini_path), repo, log_path)
        if code != 0:
            state.fail(step, t0, f'qet exit {code} on {name} - run again to continue')
            return False
        if not verify_play(output, step, state, openings * per_opening):
            state.fail(step, t0, f'{name} did not verify')
            return False
        if step.get('s3_results') and not s3_put(output, step['s3_results'], state, step['id']):
            state.fail(step, t0, 'a finished chunk could not be handed over - stopping rather than '
                                 'playing on with games that exist here only')
            return False
        open(output + '.done', 'w').close()
        worked += 1
        spent = time.time() - t0
        left = (min(last, count) - number) * spent / worked
        state.note(f"{step['id']} chunk {number}/{min(last, count)}, {spent / 3600:.1f}h gone, "
                   f"{left / 3600:.1f}h left")

    state.finish(step, t0, {'chunks': worked, 'where': step.get('s3_results') or directory})
    return True


def s3_sync_down(prefix, name, target, state, step_id):
    """Fetches one object, if it is not already here. Returns False if it is not there either."""
    if os.path.exists(target):
        return True
    code = subprocess.call(['aws', 's3', 'cp', prefix.rstrip('/') + '/' + name, target,
                            '--only-show-errors'])
    if code != 0:
        state.note(f'{step_id} could not fetch {name} from {prefix}')
        return False
    return True


def s3_put(path, prefix, state, step_id):
    """Hands a finished file over. On a spot instance this is what makes the work survive."""
    code = subprocess.call(['aws', 's3', 'cp', path, prefix.rstrip('/') + '/' + os.path.basename(path),
                            '--only-show-errors'])
    if code != 0:
        state.note(f'{step_id} could not hand over {os.path.basename(path)} to {prefix}')
        return False
    return True


def s3_names(prefix):
    """The object names under a prefix, or an empty list if it cannot be listed."""
    done = subprocess.run(['aws', 's3', 'ls', prefix.rstrip('/') + '/'], capture_output=True, text=True)
    if done.returncode != 0:
        return []
    return [line.split()[-1] for line in done.stdout.splitlines() if line.strip()]


# --------------------------------------------------------------------------- step: label

LABEL_INI = """\
# Written by src/pipeline/pipeline.py for step {id}. Do not edit - edit pipeline.toml.
#
# Long notation and min=false are not free choices: src/trainer/convert.py reads this pgn, decodes
# long notation without a move generator, and takes the game result from the Result tag, which a
# minimal pgn does not write.
concurrency={concurrency}

[each]
tc=depth:{depth}
proto=uci

[logging]
path=test/log
engine=false

[analysis]
pgn={chunk}
direction=reverse

[pgnoutput]
file={output}
append={append}
min=false
clock=false
eval=true
depth=false
pv=false
notation=lan
"""


def split_into_chunks(source, directory, per_chunk):
    """Splits a pgn into chunks of whole games. Does nothing if the chunks are already there."""
    os.makedirs(directory, exist_ok=True)
    existing = sorted(f for f in os.listdir(directory) if f.startswith('chunk-') and f.endswith('.pgn'))
    if existing:
        return [os.path.join(directory, f) for f in existing]
    chunks, handle, games, number = [], None, 0, 0
    with open(source) as inp:
        for line in inp:
            if line.startswith('[White '):
                if games % per_chunk == 0:
                    if handle:
                        handle.close()
                    number += 1
                    path = os.path.join(directory, f'chunk-{number:04d}.pgn')
                    chunks.append(path)
                    handle = open(path, 'w')
                games += 1
            if handle:
                handle.write(line)
    if handle:
        handle.close()
    return chunks


def chunk_number(name):
    """The number in chunk-0042.pgn, or None."""
    match = re.search(r'chunk-(\d+)\.pgn$', name)
    return int(match.group(1)) if match else None


def step_label(cfg, host_key, step, state):
    """Searches every position of a template to a fixed depth and writes the values into a pgn.

    The work is done chunk by chunk for two reasons. qet holds a whole analysis in memory - about
    a hundred times the size of its input - so a template of a million games is killed outright.
    And a chunk is a unit that finishes: a host that is taken away mid-run loses the chunk it was
    working on and nothing else.

    A host may be given a range of chunks with chunk_range, so several machines share one set
    without touching each other's work. With s3_chunks and s3_results the chunks are fetched and
    the results handed over as each one finishes, which is what makes a spot instance usable: the
    existence of the result object is the marker that says a chunk is done, so a replacement
    instance picks up where the lost one stopped.
    """
    repo = repo_of(cfg, host_key)
    concurrency = step.get('concurrency', cfg['hosts'][host_key]['concurrency'])
    directory = absolute(repo, step.get('chunk_dir', f"test/nnue/chunks-{step['id']}"))
    per_chunk = bool(step.get('output_per_chunk') or step.get('s3_results'))
    first, last = step.get('chunk_range', [0, 10 ** 9])

    if step.get('s3_chunks'):
        os.makedirs(directory, exist_ok=True)
        names = sorted(n for n in s3_names(step['s3_chunks']) if chunk_number(n) is not None)
        if not names:
            state.note(f"{step['id']} cannot start: nothing under {step['s3_chunks']}")
            return False
        chunks = [os.path.join(directory, n) for n in names]
    elif step.get('input_glob'):
        # The playing step left one file per round. They are joined into one pgn rather than
        # analysed one by one: a call costs the start of every engine process, and ten thousand
        # calls would spend hours on nothing but starting up.
        import glob
        parts = sorted(glob.glob(absolute(repo, step['input_glob'])))
        if not parts:
            state.note(f"{step['id']} cannot start: nothing matches {step['input_glob']}")
            return False
        os.makedirs(directory, exist_ok=True)
        source = os.path.join(directory, step['id'] + '-joined.pgn')
        if not os.path.exists(source):
            state.note(f"{step['id']} joining {len(parts)} round files")
            with open(source, 'wb') as out:
                for part in parts:
                    with open(part, 'rb') as inp:
                        while True:
                            block = inp.read(1 << 22)
                            if not block:
                                break
                            out.write(block)
        chunks = [source]
    else:
        source = absolute(repo, step['input'])
        if not os.path.exists(source):
            state.note(f"{step['id']} cannot start: {step['input']} is not there")
            return False
        if step.get('chunk_games') == 0:
            # The analysis reads its pgn one game at a time since the tester's b226240, so a whole
            # set fits in one call. Chunks are only needed where a host may be taken away.
            os.makedirs(directory, exist_ok=True)
            chunks = [source]
        else:
            chunks = split_into_chunks(source, directory, step.get('chunk_games', 10000))

    mine = [c for c in chunks if first <= (chunk_number(c) or 0) <= last]
    output_dir = absolute(repo, step.get('output_dir', 'test/nnue/labelled'))
    if per_chunk:
        os.makedirs(output_dir, exist_ok=True)
    handed_over = set(s3_names(step['s3_results'])) if step.get('s3_results') else set()

    log_path = os.path.join(repo, 'test/log', f"pipeline-{step['id']}.log")
    ini_path = os.path.join(repo, 'test/log', f"pipeline-{step['id']}.ini")

    t0 = state.begin(step)
    if not preflight(cfg, host_key, [step['engine']], state, step['id']):
        state.fail(step, t0, 'preflight failed - nothing was labelled')
        return False

    def is_done(chunk):
        if os.path.exists(chunk + '.done'):
            return True
        return per_chunk and os.path.basename(chunk) in handed_over

    worked = 0
    state.note(f"{step['id']} chunks {first}..{last}: {len(mine)} of them, "
               f"{sum(1 for c in mine if is_done(c))} already done")
    for chunk in mine:
        if is_done(chunk):
            continue
        if step.get('s3_chunks') and not s3_sync_down(step['s3_chunks'], os.path.basename(chunk),
                                                      chunk, state, step['id']):
            state.fail(step, t0, f'chunk {os.path.basename(chunk)} could not be fetched')
            return False
        output = (os.path.join(output_dir, os.path.basename(chunk)) if per_chunk
                  else absolute(repo, step['output']))
        with open(ini_path, 'w') as f:
            f.write(LABEL_INI.format(id=step['id'], concurrency=concurrency, depth=step['depth'],
                                     chunk=chunk, output=output,
                                     append='false' if per_chunk else 'true'))
        command = [os.path.expanduser(cfg['hosts'][host_key]['qet']), f'--settingsfile={ini_path}']
        command += engine_arguments(cfg, host_key, step['engine'], repo)
        code = run_tool(command, repo, log_path)
        if code != 0:
            state.fail(step, t0, f'qet exit {code} on {os.path.basename(chunk)} - run again to continue')
            return False
        # Verified per chunk, not at the end: a wrong setting has to show on the first one.
        expect = read_pgn(chunk)['games']
        if per_chunk and not verify_label(output, step, state, expect):
            state.fail(step, t0, f'{os.path.basename(output)} did not verify')
            return False
        if step.get('s3_results') and not s3_put(output, step['s3_results'], state, step['id']):
            state.fail(step, t0, 'a finished chunk could not be handed over - stopping rather than '
                                 'working on with results that only exist here')
            return False
        open(chunk + '.done', 'w').close()
        worked += 1
        done = sum(1 for c in mine if is_done(c))
        spent = time.time() - t0
        # The rate counts only the chunks this run did. Chunks that were already done when it
        # started took no time here, and counting them makes the estimate too cheerful.
        left = (len(mine) - done) * spent / worked
        state.note(f"{step['id']} {done}/{len(mine)} chunks, {spent / 3600:.1f}h gone, "
                   f"{left / 3600:.1f}h left")

    if per_chunk:
        state.finish(step, t0, {'chunks': len(mine), 'where': step.get('s3_results') or output_dir})
    else:
        output = absolute(repo, step['output'])
        expect = read_pgn(source)['games']
        if not verify_label(output, step, state, expect):
            state.fail(step, t0, 'the labelled pgn did not verify')
            return False
        facts = read_pgn(output)
        state.finish(step, t0, {'games': facts['games'], 'plies': facts['plies'],
                                'values': facts['comments'], 'bytes': os.path.getsize(output)})
    return True


def count_lines(path, prefix):
    if not os.path.exists(path):
        return 0
    count = 0
    with open(path, errors='replace') as f:
        for line in f:
            if line.startswith(prefix):
                count += 1
    return count



class Progress:
    """Writes a line every so often while a long step runs.

    A playing step is silent for ten hours otherwise, and a run nobody can see the pace of is a
    run nobody can plan around. The count comes from the tester's own log, so this costs nothing
    and needs no cooperation from the tester.
    """

    def __init__(self, state, step_id, log_path, marker, total, every=600):
        self.state, self.step_id, self.log_path = state, step_id, log_path
        self.marker, self.total, self.every = marker, total, every
        self.stop = threading.Event()
        self.thread = threading.Thread(target=self._loop, daemon=True)
        self.started = time.time()

    def _count(self):
        return count_lines(self.log_path, self.marker)

    def _loop(self):
        first = self._count()
        while not self.stop.wait(self.every):
            done = self._count() - first
            spent = time.time() - self.started
            if done <= 0:
                self.state.note(f'{self.step_id} {round(spent)}s gone, nothing finished yet')
                continue
            rate = done / spent
            left = max(self.total - done, 0) / rate
            self.state.note(f'{self.step_id} {done}/{self.total} at {rate:.1f}/s, '
                            f'{spent / 3600:.1f}h gone, {left / 3600:.1f}h left')

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, *_):
        self.stop.set()


# --------------------------------------------------------------------------- preflight

LAN_MOVE = re.compile(r'^[a-h][1-8][a-h][1-8][qrbn]?$')
SAN_MOVE = re.compile(r'^(?:[KQRBN]?[a-h]?[1-8]?x?[a-h][1-8](?:=[QRBN])?|O-O(?:-O)?)[+#]?$')


def ask_engine(binary, lines, seconds=120):
    """Sends lines to an engine and returns what it said. Always ends the input."""
    try:
        done = subprocess.run([binary], input=''.join(l + '\n' for l in lines) + 'quit\n',
                              capture_output=True, text=True, timeout=seconds)
        return done.stdout
    except (subprocess.TimeoutExpired, OSError) as error:
        return f'<<{error}>>'


def preflight(cfg, host_key, names, state, step_id):
    """Checks every engine a step needs before the step starts.

    A step runs for hours; an engine that is not what it should be has to say so in the first
    seconds. Checked: the binary answers uci and names a version, and an engine that carries a
    net loads it, agrees with a full refresh and uses a vector path. A net that is not found
    leaves the engine playing something that is not the engine we meant.
    """
    repo = repo_of(cfg, host_key)
    ok = True
    for name in dict.fromkeys(names):
        engine = cfg['hosts'][host_key]['engines'][name]
        binary = absolute(repo, engine['binary'])
        if not os.path.exists(binary):
            state.note(f'{step_id} preflight {name}: no binary at {binary}')
            ok = False
            continue
        version = ''
        for line in ask_engine(binary, ['uci']).splitlines():
            if line.startswith('id name'):
                version = line[len('id name'):].strip()
        if not version:
            state.note(f'{step_id} preflight {name}: answers no "id name" - not a uci engine?')
            ok = False
            continue
        net = engine.get('options', {}).get('NnueFile')
        if net is None:
            state.note(f'{step_id} preflight {name}: {version}')
            continue
        net_path = absolute(repo, str(net))
        if not os.path.exists(net_path):
            state.note(f'{step_id} preflight {name}: no net at {net_path}')
            ok = False
            continue
        # nnueeval loads the net, compares the incremental accumulator against a full refresh
        # and names the vector path the search uses.
        report = ''
        for line in ask_engine(binary, [f'nnueeval net {net_path}']).splitlines():
            if 'used by the search' in line:
                report = line.strip()
        if not report:
            state.note(f'{step_id} preflight {name}: nnueeval said nothing about the net')
            ok = False
            continue
        path_name = report.split(',')[0].split()[-1]
        if '(equal)' not in report:
            state.note(f'{step_id} preflight {name}: incremental and refresh disagree - {report}')
            ok = False
        elif path_name == 'plain':
            state.note(f'{step_id} preflight {name}: no vector path, this costs a factor - {report}')
            ok = False
        else:
            state.note(f'{step_id} preflight {name}: {version}, net {os.path.basename(net_path)}, {path_name}')
    return ok


# --------------------------------------------------------------------------- verification

def read_pgn(path):
    """Counts what a pgn holds, without a move generator and without loading it whole."""
    games = plies = lan = san = comments = results = terminators = 0
    lengths = []
    causes = {}
    current = 0
    with open(path, errors='replace') as f:
        for line in f:
            if line.startswith('[White '):
                if games:
                    lengths.append(current)
                games += 1
                current = 0
                continue
            if line.startswith('[Result '):
                results += 1
                continue
            if line.startswith('['):
                continue
            comments += line.count('{')
            for token in line.split():
                if token in ('1-0', '0-1', '1/2-1/2', '*'):
                    terminators += 1
                    causes[token] = causes.get(token, 0) + 1
                elif LAN_MOVE.match(token):
                    lan += 1
                    plies += 1
                    current += 1
                elif SAN_MOVE.match(token):
                    san += 1
                    plies += 1
                    current += 1
    if games:
        lengths.append(current)
    return {'games': games, 'plies': plies, 'lan': lan, 'san': san, 'comments': comments,
            'results': results, 'terminators': terminators,
            'plies_per_game': round(plies / games, 1) if games else 0,
            'shortest': min(lengths) if lengths else 0, 'longest': max(lengths) if lengths else 0,
            'outcomes': causes}


def verify_play(path, step, state, expect_games):
    """The template has to hold the games, in the notation asked for, each with a result."""
    if not os.path.exists(path):
        state.note(f"{step['id']} verify: {path} is not there")
        return False
    facts = read_pgn(path)
    wanted = step.get('notation', 'san')
    state.note(f"{step['id']} verify: {facts['games']} games, {facts['plies_per_game']} plies each "
               f"(shortest {facts['shortest']}, longest {facts['longest']}), "
               f"lan {facts['lan']} san {facts['san']}, outcomes {facts['outcomes']}")
    problems = []
    if facts['games'] < expect_games:
        problems.append(f"{facts['games']} of {expect_games} games")
    if facts['terminators'] < facts['games']:
        problems.append(f"{facts['games'] - facts['terminators']} games without a result")
    if wanted == 'lan' and facts['san'] > facts['lan']:
        problems.append('asked for long notation, got short')
    if wanted == 'san' and facts['lan'] > facts['san']:
        problems.append('asked for short notation, got long')
    if facts['shortest'] == 0:
        problems.append('a game without a single move')
    # A template whose games all end the same way means the engine is not playing chess.
    if facts['games'] > 50 and len(facts['outcomes']) < 2:
        problems.append(f"every game ended {list(facts['outcomes'])} - is the engine sane?")
    for problem in problems:
        state.note(f"{step['id']} verify FAILED: {problem}")
    return not problems


def verify_label(path, step, state, expect_games):
    """The labelled pgn is what the converter reads: long notation, a Result tag, a value per ply."""
    if not os.path.exists(path):
        state.note(f"{step['id']} verify: {path} is not there")
        return False
    facts = read_pgn(path)
    state.note(f"{step['id']} verify: {facts['games']} games, {facts['plies']} plies, "
               f"{facts['comments']} values, {facts['results']} Result tags, "
               f"lan {facts['lan']} san {facts['san']}, outcomes {facts['outcomes']}")
    problems = []
    if facts['games'] < expect_games:
        problems.append(f"{facts['games']} of {expect_games} games")
    if facts['results'] < facts['games']:
        problems.append(f"{facts['games'] - facts['results']} games without a Result tag - "
                        'convert.py reads the tag, not the terminator')
    if facts['san'] > facts['lan']:
        problems.append('short notation - convert.py has no move generator and needs long')
    if facts['comments'] < 0.95 * facts['plies']:
        problems.append(f"only {facts['comments']} values for {facts['plies']} plies")
    for problem in problems:
        state.note(f"{step['id']} verify FAILED: {problem}")
    return not problems


# --------------------------------------------------------------------------- smoke test

def small(step, games):
    """The same step, shrunk, under its own id and its own files - a smoke test of the real thing."""
    copy = dict(step)
    copy['id'] = 'smoke-' + step['id']
    copy['games'] = games
    if step.get('per_round'):
        # Two rounds are enough to see that a file per round appears and that the opening index
        # carries on from one to the next.
        copy['round_openings'] = max(4, games // 4)
        copy['openings'] = copy['round_openings'] * 2
        copy['games'] = copy['openings']
    if step.get('chunk_games') != 0:
        copy['chunk_games'] = max(10, games // 2)
    copy['chunk_dir'] = f"test/log/smoke-chunks-{step['id']}"
    for key in ('output', 'input'):
        if key in copy:
            copy[key] = 'test/log/smoke-' + os.path.basename(copy[key])
    if 'output_dir' in copy:
        copy['output_dir'] = 'test/log/smoke-' + os.path.basename(copy['output_dir'].rstrip('/'))
    if 'input_glob' in copy:
        # The playing step wrote its files under the smoke id, so the pattern carries the prefix too.
        copy['input_glob'] = ('test/log/smoke-' + os.path.basename(os.path.dirname(copy['input_glob']))
                              + '/smoke-' + os.path.basename(copy['input_glob']))
    return copy


def command_smoke(cfg, args):
    """Runs this host's steps at a small size and verifies them. Minutes, not hours."""
    host_key = this_host(cfg, args.host)
    repo = repo_of(cfg, host_key)
    state = State(repo)
    games = args.games
    state.note(f'smoke test on {host_key}, {games} games per playing step')
    for step in cfg['steps']:
        if step['host'] != host_key:
            continue
        if args.only and step['id'] != args.only:
            continue
        if step['kind'] not in KINDS:
            state.note(f"smoke: kind {step['kind']} is not implemented yet - stopping here")
            return
        tiny = small(step, games)
        # A shrunk step is started from scratch every time, so old output must go.
        if 'output' in tiny:
            path = absolute(repo, tiny['output'])
            if os.path.exists(path):
                os.remove(path)
        for key in ('output_dir', 'chunk_dir'):
            if key not in tiny:
                continue
            directory = absolute(repo, tiny[key])
            if os.path.isdir(directory):
                for name in os.listdir(directory):
                    os.remove(os.path.join(directory, name))
        state.data['steps'].pop(tiny['id'], None)
        if not KINDS[step['kind']](cfg, host_key, tiny, state):
            state.note('smoke test FAILED - the real run is not started')
            raise SystemExit(1)
    state.note('smoke test passed')


# --------------------------------------------------------------------------- step: convert

def step_convert(cfg, host_key, step, state):
    """Turns the labelled pgn of a set into the packed game file the training reads.

    The parts of a set are joined here: a set labelled by two machines arrives as one appended pgn
    from the one and a file per chunk from the other, and what the training wants is one file. The
    pgn parts are concatenated - a pgn is a sequence of games, so joining them is joining bytes -
    and the converter runs over the result once per wdl variant.

    Two variants on purpose. Whether the result of a game helps a net or misleads it is not
    something we know, so the same games are written once with the result and once without, and the
    two nets trained from them are compared. Both come from the identical pgn, so nothing but the
    wdl field differs.
    """
    import glob
    repo = repo_of(cfg, host_key)
    dataset = absolute(repo, step.get('dataset_dir', 'test/nnue/dataset'))
    os.makedirs(dataset, exist_ok=True)

    parts = []
    for pattern in step['inputs']:
        found = sorted(glob.glob(absolute(repo, pattern)))
        if not found:
            state.note(f"{step['id']} cannot start: nothing matches {pattern}")
            return False
        parts += found
    state.note(f"{step['id']} {len(parts)} pgn parts, "
               f"{sum(os.path.getsize(p) for p in parts) / 1e9:.2f} GB")

    t0 = state.begin(step)
    joined = os.path.join(dataset, step['output'] + '-joined.pgn')
    with open(joined, 'wb') as out:
        for part in parts:
            with open(part, 'rb') as inp:
                while True:
                    block = inp.read(1 << 22)
                    if not block:
                        break
                    out.write(block)
    games = count_lines(joined, '[White ')
    state.note(f"{step['id']} joined into one pgn: {games} games, "
               f"{os.path.getsize(joined) / 1e9:.2f} GB")
    if step.get('expect_games') and games != step['expect_games']:
        state.fail(step, t0, f"{games} games, {step['expect_games']} expected - a part is missing")
        os.remove(joined)
        return False

    results = {}
    for variant in step.get('wdl', ['result', 'none']):
        name = step['output'] + ('' if variant == 'result' else '-nowdl') + '.gam'
        target = os.path.join(dataset, name)
        command = ['python3', 'convert.py', joined, target, '--wdl', variant]
        code = run_tool(command, os.path.join(repo, 'src/trainer'),
                        os.path.join(repo, 'test/log', f"pipeline-{step['id']}.log"))
        if code != 0 or not os.path.exists(target):
            state.fail(step, t0, f'convert.py exit {code} for wdl={variant}')
            return False
        facts = gam_facts(target)
        results[name] = facts
        state.note(f"{step['id']} {name}: {facts['games']} games, {facts['positions']} positions, "
                   f"{facts['bytes'] / 1e6:.0f} MB, wdl {facts['wdl']}")

    # The variants must be the same games. Anything else means the converter is not deterministic
    # and the comparison of the two nets would not be a comparison of the wdl.
    counts = {(f['games'], f['positions']) for f in results.values()}
    if len(counts) > 1:
        state.fail(step, t0, f'the variants differ in their games or positions: {counts}')
        return False

    os.remove(joined)
    state.note(f"{step['id']} the joined pgn is removed again, the game files hold everything")
    # On a machine that may be taken away the game files have to leave it.
    if step.get('s3_results'):
        for name in results:
            if not s3_put(os.path.join(dataset, name), step['s3_results'], state, step['id']):
                state.fail(step, t0, f'{name} could not be handed over')
                return False
    write_manifest(cfg, host_key, dataset, state)
    state.finish(step, t0, {name: {'games': f['games'], 'positions': f['positions']}
                            for name, f in results.items()})
    return True


def gam_facts(path):
    """Games, positions and the spread of the four wdl states in a packed game file."""
    games = positions = 0
    wdl = {0: 0, 1: 0, 2: 0, 3: 0}
    with open(path, 'rb') as f:
        f.read(12)
        while True:
            head = f.read(1)
            if not head:
                break
            length = head[0]
            data = f.read(3 * length)
            games += 1
            positions += length
            for i in range(0, 3 * length, 3):
                record = data[i] | (data[i + 1] << 8) | (data[i + 2] << 16)
                wdl[(record >> 11) & 3] += 1
    names = {0: 'loss', 1: 'draw', 2: 'win', 3: 'none'}
    return {'games': games, 'positions': positions, 'bytes': os.path.getsize(path),
            'wdl': {names[k]: v for k, v in wdl.items() if v}}



# --------------------------------------------------------------------------- step: train

def step_train(cfg, host_key, step, state):
    """Trains a net from a packed game file and hands the result over.

    The trainer needs torch, which no other step does, so it is installed here rather than in the
    bootstrap every machine runs. On a machine without a gpu this is cpu work: the rate is measured
    in the log of the run itself, and it decides whether a machine of this kind is worth using for
    training at all.
    """
    repo = repo_of(cfg, host_key)
    trainer = os.path.join(repo, 'src/trainer')
    log_path = os.path.join(repo, 'test/log', f"pipeline-{step['id']}.log")
    out_dir = absolute(repo, step['out_dir'])
    os.makedirs(out_dir, exist_ok=True)

    games = absolute(repo, step['games'])
    t0 = state.begin(step)
    if not os.path.exists(games):
        if not step.get('s3_input'):
            state.fail(step, t0, f'{step["games"]} is not there and no s3_input says where to get it')
            return False
        os.makedirs(os.path.dirname(games), exist_ok=True)
        state.note(f"{step['id']} fetching the game file")
        if subprocess.call(['aws', 's3', 'cp', step['s3_input'], games, '--only-show-errors']) != 0:
            state.fail(step, t0, f'could not fetch {step["s3_input"]}')
            return False

    # torch is a dependency of this step alone. A venv keeps it out of the system python.
    venv = os.path.join(trainer, '.venv')
    python = os.path.join(venv, 'bin', 'python')
    if not os.path.exists(python):
        state.note(f"{step['id']} making a venv and installing torch - this takes a few minutes")
        if subprocess.call([sys.executable, '-m', 'venv', venv]) != 0:
            state.fail(step, t0, 'could not make the venv')
            return False
        if subprocess.call([python, '-m', 'pip', '-q', 'install', 'torch', 'numpy']) != 0:
            state.fail(step, t0, 'could not install torch')
            return False
    version = subprocess.run([python, '-c', 'import torch;print(torch.__version__)'],
                             capture_output=True, text=True)
    state.note(f"{step['id']} torch {version.stdout.strip() or 'missing'}")

    command = [python, 'train.py', os.path.relpath(games, trainer),
               '--out', os.path.relpath(out_dir, trainer),
               '--blend-start', str(step['blend_start']), '--blend-end', str(step['blend_end']),
               '--epochs', str(step.get('epochs', 20)), '--patience', str(step.get('patience', 3)),
               '--workers', str(step.get('workers', 6)), '--seed', str(step.get('seed', 1)),
               '--validation-every', str(step.get('validation_every', 100))]
    with Progress(state, step['id'], log_path, 'epoch', step.get('epochs', 20), every=900):
        code = run_tool(command, trainer, log_path)
    if code != 0:
        state.fail(step, t0, f'train.py exit {code} - see the log')
        return False

    best = ''
    for line in open(log_path, errors='replace'):
        if line.startswith('best epoch'):
            best = line.strip()
    nets = sorted(f for f in os.listdir(out_dir) if f.endswith('.nnue'))
    if step.get('s3_results'):
        for name in nets:
            if not s3_put(os.path.join(out_dir, name), step['s3_results'], state, step['id']):
                state.fail(step, t0, f'{name} could not be handed over')
                return False
        s3_put(log_path, step['s3_results'], state, step['id'])
    state.finish(step, t0, {'nets': len(nets), 'best': best})
    return True


# --------------------------------------------------------------------------- the manifest

def write_manifest(cfg, host_key, dataset, state):
    """Writes DATASET.md beside the files: what each one is and how it came about.

    Generated rather than written by hand, from the state file and the files themselves, so it
    cannot drift away from what is actually in the directory.
    """
    repo = repo_of(cfg, host_key)
    lines = ['# The training data in this directory', '',
             'Generated by `src/pipeline/pipeline.py`; do not edit by hand, it is rewritten.',
             f'Written on {now()} on host `{host_key}`.', '',
             'This directory holds only files that are still used. Everything about how they came',
             'to be is in `src/nnue-data/generated-data.md`; this is the index.', '']
    for name in sorted(os.listdir(dataset)):
        path = os.path.join(dataset, name)
        if name == 'DATASET.md' or not os.path.isfile(path):
            continue
        size = os.path.getsize(path)
        lines.append(f'## {name}')
        lines.append('')
        lines.append(f'{size:,} bytes')
        lines.append('')
        if name.endswith('.gam'):
            facts = gam_facts(path)
            lines.append(f"{facts['games']:,} games, {facts['positions']:,} positions, "
                         f"{size / max(facts['positions'], 1):.1f} bytes per position.")
            lines.append('')
            lines.append('Result of every position: ' +
                         ', '.join(f'{k} {v:,}' for k, v in facts['wdl'].items()) + '.')
            lines.append('')
            lines.append('The template pgn of these games is not kept, because it is derivable '
                         'from this file:')
            lines.append('')
            lines.append(f'    python3 src/trainer/export-pgn.py <this file> <a pgn>')
            lines.append('')
            lines.append('Every ply is in here, including the moves of the book line, and the '
                         'result of the game is in every record - so a pass that labels the games '
                         'again with a better evaluator has everything it needs. What the game '
                         'file does not hold is the notation, and that is all the exporter '
                         'rebuilds.')
            if 'none' in facts['wdl'] and len(facts['wdl']) == 1:
                lines.append('')
                lines.append('Every position carries "none", so this file trains on the values '
                             'alone - the variant that ignores the result of the game.')
            lines.append('')
        for step in cfg['steps']:
            if step.get('output') and name.startswith(step['output']):
                lines.append(f"Made by step `{step['id']}` ({step['kind']}) on `{step['host']}`: "
                             f"{step.get('doc', '')}")
                entry = state.data['steps'].get(step['id'], {})
                if entry.get('seconds'):
                    lines.append('')
                    lines.append(f"That step took {entry['seconds'] // 3600}h "
                                 f"{entry['seconds'] % 3600 // 60}m.")
                lines.append('')
                break
    path = os.path.join(dataset, 'DATASET.md')
    with open(path, 'w') as f:
        f.write('\n'.join(lines) + '\n')
    state.note(f'manifest written: {path}')


# --------------------------------------------------------------------------- step: mirror

def step_mirror(cfg, host_key, step, state):
    """Copies the dataset directory to another host, so no file exists in one place only.

    Data, not code: source and binaries reach a host through github, see delivery/deployment.md,
    but a game file is neither and cannot get there that way. rsync deletes on the far side, so
    the two directories really are the same and no old file survives there unnoticed.
    """
    repo = repo_of(cfg, host_key)
    target = cfg['hosts'][step['to']]
    source = absolute(repo, step.get('dataset_dir', 'test/nnue/dataset')).rstrip('/') + '/'
    remote = f"{target['ssh']}:{target['repo'].rstrip('/')}/" \
             f"{step.get('dataset_dir', 'test/nnue/dataset').rstrip('/')}/"
    t0 = state.begin(step)
    files = [f for f in sorted(os.listdir(source)) if os.path.isfile(os.path.join(source, f))]
    total = sum(os.path.getsize(os.path.join(source, f)) for f in files)
    state.note(f"{step['id']} {len(files)} files, {total / 1e9:.2f} GB to {remote}")
    # Only options that the rsync of macOS 2.6.9 knows as well - --info=stats1 is not one of them.
    code = subprocess.call(['rsync', '-a', '--delete', source, remote])
    if code != 0:
        state.fail(step, t0, f'rsync exit {code}')
        return False
    state.finish(step, t0, {'to': step['to'], 'files': len(files), 'bytes': total})
    return True


KINDS = {'play': step_play, 'label': step_label, 'convert': step_convert,
         'mirror': step_mirror, 'train': step_train}


# --------------------------------------------------------------------------- commands

def command_status(cfg, args):
    host_key = this_host(cfg, args.host)
    state = State(repo_of(cfg, host_key))
    print(f'this host: {host_key}\n')
    print(f'{"step":28} {"kind":6} {"host":6} {"status":8} {"time":>9}  result')
    for step in cfg['steps']:
        entry = state.data['steps'].get(step['id'], {})
        status = entry.get('status', 'pending') if step['host'] == host_key else 'elsewhere'
        seconds = entry.get('seconds')
        spent = f'{seconds // 3600}h{seconds % 3600 // 60:02d}m' if seconds else ''
        result = entry.get('result') or entry.get('error') or ''
        print(f'{step["id"]:28} {step["kind"]:6} {step["host"]:6} {status:8} {spent:>9}  {result}')


def command_run(cfg, args):
    host_key = this_host(cfg, args.host)
    state = State(repo_of(cfg, host_key))
    for step in cfg['steps']:
        if args.only and step['id'] != args.only:
            continue
        if step['host'] != host_key:
            continue
        # "external" is work that was done outside the pipeline, before it existed, or is being
        # done by hand right now. Never start it again - it would write into the same files.
        if state.of(step['id']) in ('done', 'external'):
            continue
        # done = true in the configuration says a step is finished for good. The state file lives
        # on the host that works, and a spot instance that replaces a reclaimed one starts with an
        # empty one - so what is finished has to be recorded where every host reads it.
        if step.get('done'):
            continue
        if step['kind'] not in KINDS:
            state.note(f"{step['id']} kind {step['kind']} is not implemented yet - stopping here")
            return
        if not KINDS[step['kind']](cfg, host_key, step, state):
            return


def command_launch(cfg, args):
    """Starts the pending steps of the other hosts, detached, and returns at once."""
    host_key = this_host(cfg, args.host)
    for step in cfg['steps']:
        if args.only and step['id'] != args.only:
            continue
        if step['host'] == host_key:
            continue
        host = cfg['hosts'][step['host']]
        only = f" --only {step['id']}" if args.only else ''
        only += f" --host {step['host']}" if 'hostname' not in host else ''
        # The subshell around the background job is what lets ssh close: started inside "( ... & )"
        # the process is disowned at once, and setsid puts it in a session of its own, so it
        # survives the connection, the terminal and this machine going to sleep.
        remote = (f"cd {host['repo']} && "
                  f"( setsid nohup python3 src/pipeline/pipeline.py run{only} "
                  f">> test/log/pipeline-nohup.log 2>&1 < /dev/null & ) ; "
                  f"sleep 2 ; pgrep -f 'pipeline.py run' | head -1")
        print(f"launching on {step['host']}: {step['id'] if args.only else 'all its pending steps'}")
        pid = subprocess.run(['ssh', '-n', host['ssh'], remote], capture_output=True, text=True,
                             timeout=60).stdout.strip()
        print(f"  running there as pid {pid or '(not found - look at test/log/pipeline-nohup.log)'}")
        return  # one launch per host is enough: the remote walks its own steps


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('command', choices=['status', 'run', 'launch', 'smoke'])
    parser.add_argument('--only', help='one step id')
    parser.add_argument('--games', type=int, default=200, help='games per playing step in a smoke test')
    parser.add_argument('--host', help='which hosts entry this machine is, when the hostname does not say')
    parser.add_argument('--config', default=CONFIG)
    args = parser.parse_args()
    cfg = load_config(args.config)
    {'status': command_status, 'run': command_run, 'launch': command_launch,
     'smoke': command_smoke}[args.command](cfg, args)


if __name__ == '__main__':
    main()
