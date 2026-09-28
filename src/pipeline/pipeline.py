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
import socket
import subprocess
import sys
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


def this_host(cfg):
    """The key of the host entry whose hostname matches this machine."""
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
    repo = repo_of(cfg, host_key)
    concurrency = step.get('concurrency', cfg['hosts'][host_key]['concurrency'])
    selfplay = step.get('selfplay', step['white'] == step['black'])
    ini_path = os.path.join(repo, 'test/log', f"pipeline-{step['id']}.ini")
    with open(ini_path, 'w') as f:
        f.write(PLAY_INI.format(
            id=step['id'], concurrency=concurrency, depth=step['depth'], book=step['book'],
            output=step['output'], notation=step.get('notation', 'san'), games=step['games'],
            repeat=1 if selfplay else 2, noswap='true' if selfplay else 'false',
            state_file=f"test/log/pipeline-{step['id']}.state"))

    log_path = os.path.join(repo, 'test/log', f"pipeline-{step['id']}.log")
    command = [os.path.expanduser(cfg['hosts'][host_key]['qet']),
               f'--settingsfile={ini_path}']
    command += engine_arguments(cfg, host_key, step['white'], repo)
    if step['black'] != step['white']:
        command += engine_arguments(cfg, host_key, step['black'], repo)
    else:
        # The same engine twice: qet needs two entries, and they must not share a name.
        second = engine_arguments(cfg, host_key, step['white'], repo)
        second[1] = f"name={step['white']}-b"
        command += second

    t0 = state.begin(step)
    code = run_tool(command, repo, log_path)
    games = count_lines(os.path.join(repo, step['output']), '[White ')
    if code != 0 or games < step['games']:
        state.fail(step, t0, f'qet exit {code}, {games} of {step["games"]} games - run again to continue')
        return False
    state.finish(step, t0, {'games': games, 'bytes': os.path.getsize(os.path.join(repo, step['output']))})
    return True


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
append=true
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


def step_label(cfg, host_key, step, state):
    repo = repo_of(cfg, host_key)
    concurrency = step.get('concurrency', cfg['hosts'][host_key]['concurrency'])
    source = absolute(repo, step['input'])
    if not os.path.exists(source):
        state.note(f"{step['id']} cannot start: {step['input']} is not there")
        return False

    # qet holds a whole analysis in memory - about 100 times the size of its input pgn - so a
    # template of a million games is killed. The chunks bound the memory and make the pass
    # resumable: a finished chunk keeps a .done marker and is skipped.
    directory = absolute(repo, step.get('chunk_dir', f"test/nnue/chunks-{step['id']}"))
    chunks = split_into_chunks(source, directory, step.get('chunk_games', 10000))
    log_path = os.path.join(repo, 'test/log', f"pipeline-{step['id']}.log")
    ini_path = os.path.join(repo, 'test/log', f"pipeline-{step['id']}.ini")

    t0 = state.begin(step)
    state.note(f"{step['id']} {len(chunks)} chunks, {sum(1 for c in chunks if os.path.exists(c + '.done'))} already done")
    for chunk in chunks:
        if os.path.exists(chunk + '.done'):
            continue
        with open(ini_path, 'w') as f:
            f.write(LABEL_INI.format(id=step['id'], concurrency=concurrency, depth=step['depth'],
                                     chunk=chunk, output=step['output']))
        command = [os.path.expanduser(cfg['hosts'][host_key]['qet']),
                   f'--settingsfile={ini_path}']
        command += engine_arguments(cfg, host_key, step['engine'], repo)
        code = run_tool(command, repo, log_path)
        if code != 0:
            state.fail(step, t0, f'qet exit {code} on {os.path.basename(chunk)} - run again to continue')
            return False
        open(chunk + '.done', 'w').close()
        done = sum(1 for c in chunks if os.path.exists(c + '.done'))
        state.note(f"{step['id']} {done}/{len(chunks)} chunks")

    games = count_lines(os.path.join(repo, step['output']), '[White ')
    state.finish(step, t0, {'games': games,
                            'bytes': os.path.getsize(os.path.join(repo, step['output']))})
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


KINDS = {'play': step_play, 'label': step_label}


# --------------------------------------------------------------------------- commands

def command_status(cfg, args):
    host_key = this_host(cfg)
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
    host_key = this_host(cfg)
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
    host_key = this_host(cfg)
    for step in cfg['steps']:
        if args.only and step['id'] != args.only:
            continue
        if step['host'] == host_key:
            continue
        host = cfg['hosts'][step['host']]
        only = f" --only {step['id']}" if args.only else ''
        remote = (f"cd {host['repo']} && setsid nohup python3 src/pipeline/pipeline.py run{only} "
                  f">> test/log/pipeline-nohup.log 2>&1 < /dev/null & echo started $!")
        print(f"launching on {step['host']}: {step['id'] if args.only else 'all its pending steps'}")
        subprocess.check_call(['ssh', host['ssh'], remote])
        return  # one launch per host is enough: the remote walks its own steps


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('command', choices=['status', 'run', 'launch'])
    parser.add_argument('--only', help='one step id')
    parser.add_argument('--config', default=CONFIG)
    args = parser.parse_args()
    cfg = load_config(args.config)
    {'status': command_status, 'run': command_run, 'launch': command_launch}[args.command](cfg, args)


if __name__ == '__main__':
    main()
