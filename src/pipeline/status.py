"""What every machine is doing and what is left in the queue.

    python3 src/pipeline/status.py                 once
    python3 src/pipeline/status.py --every 180     again every three minutes

Three questions, because three things go wrong: has something stopped moving, is a machine missing,
and is there work nobody is on. Every age is worked out on the machine that wrote the line - the
instances run in utc and the control machine does not, and comparing across that difference once
looked exactly like a two hour stall that was not there.

Nothing here names a host, a path, an account or a bucket: that is all in local.toml, which is not in
the repository. A worker is not in there either, since it gets a new address every time - it is found
by its tag.
"""

import argparse
import json
import os
import socket
import subprocess
import time
from datetime import datetime, timezone

import pipeline as pl
import tasks as tk


def run(command, seconds=60):
    try:
        done = subprocess.run(command, capture_output=True, text=True, timeout=seconds)
        return done.stdout.strip() if done.returncode == 0 else ''
    except (subprocess.TimeoutExpired, OSError):
        return ''


def is_ephemeral(host):
    return 'hostname' not in host and 'ssh' not in host


def instances(cfg):
    """Every running instance, grouped by the name its tag carries. Workers share one name."""
    aws = cfg.get('aws', {})
    if not aws:
        return {}
    text = run(['aws', 'ec2', 'describe-instances', '--region', aws['region'],
                '--filters', 'Name=instance-state-name,Values=running,pending',
                '--query', 'Reservations[].Instances[].{Id:InstanceId,Type:InstanceType,'
                           'Ip:PublicIpAddress,Up:LaunchTime,Cores:CpuOptions.CoreCount,'
                           'Name:Tags[?Key==`Name`]|[0].Value}', '--output', 'json'])
    found = {}
    for entry in json.loads(text) if text else []:
        name = (entry.get('Name') or '')
        if name.startswith('qapla-'):
            found.setdefault(name[len('qapla-'):], []).append(entry)
    return found


def since(seconds):
    if seconds is None:
        return '?'
    if seconds < 90:
        return f'{seconds:.0f}s ago'
    if seconds < 5400:
        return f'{seconds / 60:.0f}min ago'
    return f'{seconds / 3600:.1f}h ago'


def ask(cfg, host, ip, command):
    """Runs a command in a machine's working copy, wherever that machine is."""
    inside = f'cd {host["repo"]} && {command}'
    if 'ssh' in host:
        return run(['ssh', '-n', '-o', 'ConnectTimeout=10', host['ssh'], inside])
    if ip:
        pem = os.path.expanduser(cfg['aws'].get('pem', ''))
        return run(['ssh', '-n', '-o', 'StrictHostKeyChecking=no', '-o', 'ConnectTimeout=10',
                    '-i', pem, 'ubuntu@' + ip, inside])
    return run(['sh', '-c', inside])


def last_line(cfg, host, ip, log):
    """The last line of a log and how old it is, worked out where it was written."""
    answer = ask(cfg, host, ip, f"tail -1 {log} 2>/dev/null; date '+%Y-%m-%d %H:%M:%S'")
    parts = answer.splitlines() if answer else []
    if len(parts) < 2:
        return '', None
    line, clock = parts[-2], parts[-1]
    try:
        stamp = ' '.join(line.split()[:2])
        age = (datetime.strptime(clock, '%Y-%m-%d %H:%M:%S')
               - datetime.strptime(stamp, '%Y-%m-%d %H:%M:%S')).total_seconds()
    except ValueError:
        return line, None
    return line, age


def report(cfg):
    stamp = datetime.now(timezone.utc).astimezone().strftime('%Y-%m-%d %H:%M:%S')
    print(f'===== {stamp} =====')
    running = instances(cfg)
    table = tk.Tasks(cfg)
    left = table.summary()

    print('\nthe queue')
    print(f'  {left["pieces left"]} pieces left, {left["in progress"]} in progress, '
          f'{left["ranges"]} ranges and {left["single jobs"]} single jobs')
    per_set = {}
    for item in table.open_work():
        pieces = max(0, item['hi'] - item['lo'] + 1) if item['form'] == 'range' else 1
        per_set.setdefault(item['set'], {}).setdefault(item['kind'], 0)
        per_set[item['set']][item['kind']] += pieces
    for name in sorted(per_set):
        kinds = ', '.join(f'{k} {v}' for k, v in sorted(per_set[name].items()))
        print(f'    {name}: {kinds}')
    for claim in sorted(table.claims(), key=lambda c: (c['set'], c['chunk'])):
        print(f'    in progress: {claim["set"]} {claim["kind"]} chunk {claim["chunk"]} '
              f'on {claim.get("owner", "?")}')

    print('\nworkers')
    workers = running.get('worker', [])
    wanted = cfg['worker'].get('wanted', 1)
    if not workers:
        print(f'  none of {wanted} - nobody is taking work out of the queue')
    for entry in workers:
        line, age = last_line(cfg, cfg['worker'], entry.get('Ip'), 'test/log/worker-run.log')
        stale = '   <-- QUIET' if age is not None and age > 1800 else ''
        print(f'  {entry["Id"]} {entry["Type"]} {entry.get("Ip") or "-"}')
        print(f'    {line or "(no answer)"}')
        if line:
            print(f'    written {since(age)}{stale}')
    if workers and len(workers) < wanted:
        print(f'  {len(workers)} of {wanted} - the supervisor should be bringing up more')

    print('\nmachines with work of their own')
    for key, host in cfg['hosts'].items():
        steps = [s for s in cfg['steps'] if s['host'] == key and not s.get('done')]
        if not steps:
            continue
        entries = running.get(key, [])
        if is_ephemeral(host) and not entries:
            print(f'  {key:6} NO INSTANCE - {len(steps)} steps assigned to it')
            continue
        ip = entries[0].get('Ip') if entries else None
        where = f'{entries[0]["Id"]} {entries[0]["Type"]}' if entries else host.get('ssh', 'local')
        line, age = last_line(cfg, host, ip, 'test/log/pipeline.log')
        stale = '   <-- QUIET' if age is not None and age > 3600 else ''
        print(f'  {key:6} {where}')
        print(f'    {line or "(no answer)"}')
        if line:
            print(f'    written {since(age)}{stale}')

    cores = sum(e.get('Cores', 0) for group in running.values() for e in group)
    print(f'\ncores in use: {cores}')
    print(flush=True)


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
            report(cfg)
        except Exception as error:
            print(f'report failed: {error}', flush=True)
        if not arguments.every:
            return
        time.sleep(arguments.every)


if __name__ == '__main__':
    main()
