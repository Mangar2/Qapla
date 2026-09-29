"""Watches the runs and brings back a machine that has gone away.

    python3 src/pipeline/supervise.py                 look once
    python3 src/pipeline/supervise.py --every 1800    look every half hour

A spot instance is taken back with two minutes' notice and nothing restarts it. The work it had
finished is in s3 and a replacement skips it, so the whole recovery is: notice that a host with
unfinished steps has no instance, and run the resume script for it. That is what this does, and it
does it without anyone watching, which is the point - a night with a reclaimed instance otherwise
costs the whole night.

What it will not do is restart a host that is not ephemeral. A long-lived machine that stopped has
a reason, and starting it again over that reason would hide it.
"""

import argparse
import subprocess
import time
from datetime import datetime, timezone

import pipeline as pl
import status as st


def unfinished(cfg, host_key):
    """The steps of a host that are neither done nor marked finished in the configuration."""
    return [s for s in cfg['steps'] if s['host'] == host_key and not s.get('done')]


def watch_target(cfg, host_key):
    """What the instance has to deliver before it may switch itself off: the last step's objects."""
    for step in reversed(unfinished(cfg, host_key)):
        if 's3_results' in step:
            return step['s3_results'], 2 if step['kind'] == 'convert' else 0
    return None, 0


def revive(cfg, host_key, host, note):
    prefix, count = watch_target(cfg, host_key)
    command = ['sh', pl.os.path.join(pl.HERE, 'aws-resume.sh'),
               host['instance_type'], host_key]
    if prefix and count:
        command += [prefix, str(count)]
    note(f'{host_key}: no instance, bringing one up ({host["instance_type"]})')
    done = subprocess.run(command, capture_output=True, text=True, timeout=1800)
    for line in (done.stdout or '').splitlines()[-4:]:
        note(f'  {line}')
    if done.returncode != 0:
        note(f'{host_key}: the resume script failed, exit {done.returncode}')
        for line in (done.stderr or '').splitlines()[-3:]:
            note(f'  {line}')
        return False
    return True


def look(cfg, note):
    running = st.instances(cfg)
    for host_key, host in cfg['hosts'].items():
        steps = unfinished(cfg, host_key)
        if not steps or not st.is_ephemeral(host):
            continue
        if host_key in running:
            continue
        if 'instance_type' not in host:
            note(f'{host_key}: no instance and no instance_type in local.toml - cannot bring it up')
            continue
        revive(cfg, host_key, host, note)


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--every', type=int, default=0)
    parser.add_argument('--config', default=pl.CONFIG)
    parser.add_argument('--local', default=pl.LOCAL)
    arguments = parser.parse_args()
    cfg = pl.load_config(arguments.config, arguments.local)

    def note(line):
        stamp = datetime.now(timezone.utc).astimezone().strftime('%Y-%m-%d %H:%M:%S')
        print(f'{stamp} {line}', flush=True)

    while True:
        try:
            look(cfg, note)
        except Exception as error:                  # one failed look must not end the watch
            note(f'look failed: {error}')
        if not arguments.every:
            return
        time.sleep(arguments.every)


if __name__ == '__main__':
    main()
