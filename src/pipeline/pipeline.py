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


# --------------------------------------------------------------------------- basics

def now():
    return datetime.now(timezone.utc).astimezone().strftime('%Y-%m-%d %H:%M:%S')


def load_config(path=CONFIG):
    with open(path, 'rb') as f:
        return tomllib.load(f)


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
# No [resign] block on purpose - qet ignores active=false, the block's presence switches the
# adjudication on, and a won game has to be played to its end. [draw] stays, an endless shuffle
# teaches a net nothing. rapid stays off: the draw adjudication reads the engine's info lines.
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
active=true
movenumber=60
movecount=20
score=20
test=false

[pgnoutput]
file={output}
append=true
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

    def write_ini(ini_path, first_opening, games, output, state_file):
        with open(ini_path, 'w') as f:
            f.write(PLAY_INI.format(
                id=step['id'], concurrency=concurrency, depth=step['depth'], book=step['book'],
                output=output, notation=step.get('notation', 'san'), games=games,
                repeat=per_opening, noswap='true' if selfplay else 'false',
                first_opening=first_opening, state_file=state_file))

    openings_total = step.get('openings', step['games'] // per_opening)
    slice_size = step.get('chunk_openings')

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
    copy['chunk_games'] = max(10, games // 2)
    copy['chunk_dir'] = f"test/log/smoke-chunks-{step['id']}"
    for key in ('output', 'input'):
        if key in copy:
            copy[key] = 'test/log/smoke-' + os.path.basename(copy[key])
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
        for key in ('output',):
            path = absolute(repo, tiny[key])
            if os.path.exists(path):
                os.remove(path)
        chunks = absolute(repo, tiny['chunk_dir'])
        if os.path.isdir(chunks):
            for name in os.listdir(chunks):
                os.remove(os.path.join(chunks, name))
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
    code = subprocess.call(['rsync', '-a', '--delete', '--info=stats1', source, remote])
    if code != 0:
        state.fail(step, t0, f'rsync exit {code}')
        return False
    state.finish(step, t0, {'to': step['to'], 'where': remote})
    return True


KINDS = {'play': step_play, 'label': step_label,
         'convert': step_convert, 'mirror': step_mirror}


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
