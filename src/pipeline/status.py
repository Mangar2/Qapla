"""Collects where every run stands, on every machine, and says what is no longer running.

    python3 src/pipeline/status.py                 once
    python3 src/pipeline/status.py --every 600     again every ten minutes

Three things are asked, because three things go wrong. A step can stall - then its log stops moving.
A machine can go away - a spot instance is taken back with two minutes' notice, and nothing restarts
it by itself. And work can be done without its result arriving anywhere durable - so the count of
objects handed over is asked separately from the machine that was supposed to hand them over.

Nothing in this file names a host, a path, an account or a bucket: all of that is in local.toml,
which is not in the repository. An ephemeral machine is not in there either, since it gets a new
address every time - it is found by its tag, which the resume script sets from the host key.
"""

import argparse
import json
import os
import subprocess
import sys
import time
from datetime import datetime, timezone

import pipeline as pl


def run(command, seconds=60):
    """The output of a command, or an empty string. A machine that does not answer is a result."""
    try:
        done = subprocess.run(command, capture_output=True, text=True, timeout=seconds)
        return done.stdout.strip() if done.returncode == 0 else ''
    except (subprocess.TimeoutExpired, OSError):
        return ''


def is_ephemeral(host):
    """A machine that comes and goes has neither a fixed name nor a fixed ssh target."""
    return 'hostname' not in host and 'ssh' not in host


def instances(cfg):
    """The running instances, by the host key their tag names."""
    aws = cfg.get('aws', {})
    if not aws:
        return {}
    text = run(['aws', 'ec2', 'describe-instances', '--region', aws['region'],
                '--filters', 'Name=instance-state-name,Values=running,pending',
                '--query', 'Reservations[].Instances[].{Id:InstanceId,Type:InstanceType,'
                           'Ip:PublicIpAddress,Up:LaunchTime,'
                           'Name:Tags[?Key==`Name`]|[0].Value}',
                '--output', 'json'])
    found = {}
    for entry in json.loads(text) if text else []:
        name = entry.get('Name') or ''
        if name.startswith('qapla-'):
            found[name[len('qapla-'):]] = entry
    return found


def remote(cfg, host_key, host, running, command):
    """Runs a command on a host, wherever that host happens to be."""
    aws = cfg.get('aws', {})
    # Every host runs it in its own working copy, so the command is always prefixed with the move
    # into it - a bare command on a fresh ssh connection starts in the home directory.
    inside = f'cd {host["repo"]} && {command}'
    if 'ssh' in host:
        return run(['ssh', '-n', '-o', 'ConnectTimeout=10', host['ssh'], inside])
    if is_ephemeral(host):
        entry = running.get(host_key)
        if not entry or not entry.get('Ip'):
            return ''
        pem = os.path.expanduser(aws.get('pem', ''))
        return run(['ssh', '-n', '-o', 'StrictHostKeyChecking=no', '-o', 'ConnectTimeout=10',
                    '-i', pem, 'ubuntu@' + entry['Ip'], inside])
    return run(['sh', '-c', inside])


def age_of(cfg, host_key, host, running):
    """The last line of a host's log and how long ago it was written.

    The age is worked out on the host itself. A log carries the host's own local time, and the
    machines here do not share one - the instances run in utc and the control machine does not -
    so comparing a remote timestamp with a local clock is off by the difference between the two,
    which once looked exactly like a two hour stall that was not there.
    """
    answer = remote(cfg, host_key, host, running,
                    "tail -1 test/log/pipeline.log 2>/dev/null; date '+%Y-%m-%d %H:%M:%S'")
    if not answer:
        return '', None
    parts = answer.splitlines()
    if len(parts) < 2:
        return '', None
    line, now = parts[-2], parts[-1]
    try:
        stamp = ' '.join(line.split()[:2])
        seconds = (datetime.strptime(now, '%Y-%m-%d %H:%M:%S')
                   - datetime.strptime(stamp, '%Y-%m-%d %H:%M:%S')).total_seconds()
    except ValueError:
        return line, None
    return line, seconds


def since(seconds):
    if seconds is None:
        return '?'
    if seconds < 90:
        return f'{seconds:.0f}s ago'
    if seconds < 5400:
        return f'{seconds / 60:.0f}min ago'
    return f'{seconds / 3600:.1f}h ago'


def handed_over(cfg, prefix):
    """How many objects are under an s3 prefix."""
    text = run(['aws', 's3', 'ls', prefix.rstrip('/') + '/'])
    rows = [line.split() for line in text.splitlines() if line.strip()] if text else []
    newest = None
    for row in rows:
        try:
            when = datetime.strptime(row[0] + ' ' + row[1], '%Y-%m-%d %H:%M:%S')
        except (ValueError, IndexError):
            continue
        # These come from the aws client and are in this machine's own time, so the age may be
        # worked out here - unlike a line out of a log on another machine.
        seconds = (datetime.now() - when).total_seconds()
        newest = seconds if newest is None else min(newest, seconds)
    return len(rows), newest


def report(cfg):
    now = datetime.now(timezone.utc).astimezone().strftime('%Y-%m-%d %H:%M:%S')
    print(f'===== {now} =====')
    running = instances(cfg)

    print('\nmachines')
    for key, host in cfg['hosts'].items():
        steps = [s for s in cfg['steps'] if s['host'] == key and not s.get('done')]
        if not steps:
            continue
        if is_ephemeral(host):
            entry = running.get(key)
            if not entry:
                print(f'  {key:6} NO INSTANCE - {len(steps)} steps assigned, nothing is running them')
                continue
            up = entry.get('Up', '')[:16].replace('T', ' ')
            where = f"{entry['Id']} {entry['Type']} since {up}"
        else:
            where = host.get('ssh', 'local')
        line, seconds = age_of(cfg, key, host, running)
        stale = '   <-- QUIET' if seconds is not None and seconds > 3600 else ''
        print(f'  {key:6} {where}')
        print(f'         {line if line else "(no answer)"}')
        if line:
            print(f'         written {since(seconds)}{stale}')

    print('\nwork handed over')
    for step in cfg['steps']:
        if step.get('done') or 's3_results' not in step:
            continue
        count, newest = handed_over(cfg, step['s3_results'])
        last = f', newest {since(newest)}' if newest is not None else ''
        print(f'  {step["id"]:22} {count:>5} objects under .../{step["s3_results"].split("/", 3)[-1]}{last}')

    print('\nrunning instances')
    if not running:
        print('  none')
    for key, entry in sorted(running.items()):
        print(f'  {key:6} {entry["Id"]} {entry["Type"]:14} {entry.get("Ip") or "-"}')
    print(flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--every', type=int, default=0,
                        help='seconds between reports; without it the report is made once')
    parser.add_argument('--config', default=pl.CONFIG)
    parser.add_argument('--local', default=pl.LOCAL)
    arguments = parser.parse_args()
    cfg = pl.load_config(arguments.config, arguments.local)
    while True:
        try:
            report(cfg)
        except Exception as error:                       # a report must not end the watch
            print(f'report failed: {error}', flush=True)
        if not arguments.every:
            return
        time.sleep(arguments.every)


if __name__ == '__main__':
    main()
