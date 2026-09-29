"""Keeps the workers going, gives back what a vanished machine was holding, and adopts old runs.

    python3 src/pipeline/supervise.py                 look once
    python3 src/pipeline/supervise.py --every 1800    look every half hour

Three jobs, and each of them exists because something went wrong without it:

**Give back what is lost.** A claim in the table names the machine that holds it. A claim whose
machine is no longer running is work nobody is doing, and it becomes an open job again - ranked
before anything not yet started, since it is a piece somebody was already waiting for.

**Keep the workers going.** While the table holds open work, there should be as many workers as the
profile asks for. A worker that finished because the table was empty is not replaced; one that
vanished is.

**Adopt what the old, machine-bound runs were doing.** Before the queue, a machine was tied to a set
and a step, and losing it meant losing the rest of that set's run. Such a machine is not brought
back any more: its set is put into the queue instead, one job per chunk that is missing, and from
then on every worker can carry it. Which chunks are missing is read from the object store - that is
a legitimate way to *create* work, even though the store never says what state the work is in.

It will not restart a machine that is not ephemeral. A long-lived one that stopped has a reason, and
starting it again over that reason would hide it.
"""

import argparse
import subprocess
import time
from datetime import datetime, timezone

import pipeline as pl
import status as st
import tasks as tk


def note(line):
    stamp = datetime.now(timezone.utc).astimezone().strftime('%Y-%m-%d %H:%M:%S')
    print(f'{stamp} {line}', flush=True)


def objects(prefix):
    done = subprocess.run(['aws', 's3', 'ls', prefix.rstrip('/') + '/'],
                          capture_output=True, text=True, timeout=180)
    if done.returncode != 0:
        return set()
    return {line.split()[-1] for line in done.stdout.splitlines() if line.strip()}


# ------------------------------------------------------------------------- giving work back

def give_back_lost(cfg, table, alive):
    """Every claim whose machine is gone becomes an open job again."""
    for claim in table.claims():
        if claim.get('owner') in alive:
            continue
        table.give_back(claim)
        note(f'gave back {claim["set"]} {claim["kind"]} chunk {claim["chunk"]} - '
             f'{claim.get("owner", "?")} is gone')


# ------------------------------------------------------------------------- adopting an old run

def adopt(cfg, table, set_name):
    """Puts every chunk of a set that is not finished into the queue, one job each.

    Single jobs rather than a range, because the missing chunks need not be a stretch: a machine
    that died in the middle leaves holes, and a job per chunk is exact where range arithmetic would
    have to guess.
    """
    definition = next((s for s in cfg['sets'] if s['name'] == set_name), None)
    if definition is None:
        note(f'cannot adopt {set_name}: the configuration does not describe it')
        return
    bucket = cfg['aws']['bucket']
    chunks = -(-definition['openings'] // definition['chunk_openings'])
    played = objects(f's3://{bucket}/{set_name}/played')
    labelled = objects(f's3://{bucket}/{set_name}/labelled')
    have = {item['id'] for item in table.open_work()} | {c['id'] for c in table.claims()}
    play_jobs = label_jobs = 0
    for chunk in range(1, chunks + 1):
        piece = f'chunk-{chunk:04d}.pgn'
        if piece in labelled:
            continue
        kind = 'label' if piece in played else 'play'
        if f'job#{set_name}#{kind}#{chunk:04d}' in have:
            continue
        weight = 0 if kind == 'label' else 100000
        table.add_job(set_name, kind, chunk, definition['base'] + weight)
        if kind == 'play':
            play_jobs += 1
        else:
            label_jobs += 1
    note(f'adopted {set_name}: {play_jobs} chunks to play, {label_jobs} to label, '
         f'{len(labelled)} of {chunks} already labelled')


def adopt_orphans(cfg, table, running):
    """An old machine-bound run whose machine is gone is handed to the queue instead of restarted."""
    for host_key, host in cfg['hosts'].items():
        if not st.is_ephemeral(host) or host_key in running:
            continue
        steps = [s for s in cfg['steps'] if s['host'] == host_key and not s.get('done')]
        if not steps:
            continue
        for set_name in sorted({s['id'].split('-')[0] for s in steps}):
            note(f'{host_key} is gone and held the old run of {set_name} - handing it to the queue')
            adopt(cfg, table, set_name)
        # Once adopted, the host must not be considered again.
        for step in steps:
            step['done'] = True


# ------------------------------------------------------------------------- keeping workers

def start_worker(cfg):
    profile = cfg['worker']
    command = ['sh', pl.os.path.join(pl.HERE, 'aws-resume.sh'),
               profile['instance_type'], 'worker']
    note(f'starting a worker ({profile["instance_type"]})')
    done = subprocess.run(command, capture_output=True, text=True, timeout=2400)
    for line in (done.stdout or '').splitlines()[-4:]:
        note(f'  {line}')
    if done.returncode != 0:
        for line in (done.stderr or '').splitlines()[-3:]:
            note(f'  {line}')
        note(f'the resume script failed, exit {done.returncode}')
    return done.returncode == 0


def count_workers(cfg):
    """How many workers are running. They all carry the same tag, so they have to be counted."""
    aws = cfg['aws']
    text = subprocess.run(
        ['aws', 'ec2', 'describe-instances', '--region', aws['region'],
         '--filters', 'Name=tag:Name,Values=qapla-worker',
         'Name=instance-state-name,Values=running,pending',
         '--query', 'length(Reservations[].Instances[])', '--output', 'text'],
        capture_output=True, text=True, timeout=120)
    try:
        return int(text.stdout.strip())
    except ValueError:
        return 0


def keep_workers(cfg, table):
    """As many workers as the profile asks for, while there is open work."""
    left = table.summary()['pieces left']
    have = count_workers(cfg)
    wanted = cfg['worker'].get('wanted', 1)
    if not left:
        if have:
            note(f'nothing left to do; the {have} running workers will stop by themselves')
        return
    if have >= wanted:
        return
    note(f'{left} pieces left, {have} of {wanted} workers running')
    for _ in range(wanted - have):
        if not start_worker(cfg):
            return          # the quota is full or no zone has capacity - try again next time


def look(cfg):
    table = tk.Tasks(cfg)
    running = st.instances(cfg)
    alive = {entry['Id'] for entry in running.values()}
    give_back_lost(cfg, table, alive)
    adopt_orphans(cfg, table, running)
    keep_workers(cfg, table)
    note(f'table: {table.summary()}')


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--every', type=int, default=0)
    parser.add_argument('--config', default=pl.CONFIG)
    parser.add_argument('--local', default=pl.LOCAL)
    arguments = parser.parse_args()
    cfg = pl.load_config(arguments.config, arguments.local)
    while True:
        try:
            look(cfg)
        except Exception as error:
            note(f'look failed: {error}')
        if not arguments.every:
            return
        time.sleep(arguments.every)


if __name__ == '__main__':
    main()
