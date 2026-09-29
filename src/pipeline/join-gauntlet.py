"""Puts a freshly trained net into the gauntlet that is running on another machine.

    python3 src/pipeline/join-gauntlet.py --nets test/nnue/nets-set3 --name nnue-set3 \
        --log test/log/pipeline-set3-train.log

Five things happen, in this order, and each one is checked before the next:

1. the best epoch is read out of the training log - the net that played is then the net the run itself
   called best, not the last one or the one somebody picked
2. the net is copied to the tournament machine. Game files and nets are generated data: they do not
   travel through github, see delivery/deployment.md, so this is one of the few things that is copied
   directly
3. `nnueeval` on that machine says whether the net loads, whether the incremental accumulator matches
   a full refresh, and which vector path the build took. A net that does not load, or a build that
   fell back to the plain path, would play a tournament nobody can use - the run stops there
4. the tournament is stopped, the net gets an [engine] section in the tournament file, and the
   tournament is started again. That file is the whole configuration: qet reads the engines out of it
   whenever neither the call nor a settings file names one
5. afterwards it is checked that qet is running, that the field grew by exactly one, that no round was
   lost, and that the newcomer is actually playing

The tournament is down for about half a minute. Every game already played is in the tournament file
and the newcomer plays the rounds it is missing first, so nothing has to be repeated.
"""

import argparse
import os
import re
import subprocess
import sys

import pipeline as pl

SSH = ['ssh', '-n', '-o', 'ConnectTimeout=20']


def run(command, what, quiet=False):
    done = subprocess.run(command, capture_output=True, text=True, timeout=600)
    if done.returncode != 0:
        print(f'{what} failed: {(done.stderr or done.stdout).strip()[:400]}')
        sys.exit(1)
    if not quiet and done.stdout.strip():
        print('\n'.join('  ' + line for line in done.stdout.strip().splitlines()))
    return done.stdout


def best_net(nets_dir, log_path):
    """The net the training itself called best, or the newest one if it never said."""
    if os.path.exists(log_path):
        best = [line for line in open(log_path, errors='replace') if line.startswith('best epoch')]
        if best:
            name = best[-1].strip().split(' - ')[-1]
            path = os.path.join(nets_dir, name)
            if os.path.exists(path):
                return path, best[-1].strip()
    nets = sorted(f for f in os.listdir(nets_dir) if f.endswith('.nnue'))
    if not nets:
        print(f'no net in {nets_dir}')
        sys.exit(1)
    return os.path.join(nets_dir, nets[-1]), f'no best epoch in the log, taking {nets[-1]}'


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--nets', required=True, help='the directory the training wrote its nets to')
    parser.add_argument('--name', required=True, help='the name this engine plays under')
    parser.add_argument('--log', default='', help='the training log, for the best epoch')
    parser.add_argument('--host', default='qapla', help='which hosts entry runs the tournament')
    parser.add_argument('--state', default='test/log/strength-gauntlet.state')
    parser.add_argument('--concurrency', type=int, default=14)
    parser.add_argument('--config', default=pl.CONFIG)
    parser.add_argument('--local', default=pl.LOCAL)
    args = parser.parse_args()

    cfg = pl.load_config(args.config, args.local)
    host = cfg['hosts'][args.host]
    there, repo = host['ssh'], host['repo'].rstrip('/')
    nnue = f'{repo}/new-versions/Qapla-blendtest-nnue'
    net_there = f'{repo}/test/nnue/{args.name}-best.nnue'

    net, why = best_net(args.nets, args.log)
    print(f'== {why}')
    print(f'== {net}, {os.path.getsize(net):,} bytes -> {there}:{net_there}')
    run(['scp', '-q', net, f'{there}:{net_there}'], 'the copy')

    print('== does it load there, and on which path')
    out = run(SSH + [there, f'printf "nnueeval net {net_there}\\nquit\\n" | {nnue}'], 'nnueeval')
    line = next((l for l in out.splitlines() if 'reference' in l), '')
    if '(equal)' not in line:
        print(f'the accumulator does not match a full refresh: {line.strip()}')
        sys.exit(1)
    if 'plain' in line:
        print(f'this build has no vector path, the tournament would measure the build: {line.strip()}')
        sys.exit(1)

    print('== stop, add the engine, start again')
    before = run(SSH + [there, f'cd {repo} && grep -c "^.round.$" {args.state}; '
                                f'grep -c "^.engine.$" {args.state}'], 'counting', quiet=True)
    rounds, engines = (int(x) for x in before.split())
    print(f'  rounds {rounds}, engines {engines}')
    # pgrep -x matches the program name only. A pattern would find this very command line, which
    # names the tournament file - and killing that kills the session doing the work.
    run(SSH + [there, 'pgrep -x qet | xargs -r kill; sleep 8; true'], 'stopping')

    block = (f'[engine]\nid=tournament\nname={args.name}\ncmd={nnue}\ntrace=none\n'
             f'option.nnuefile={net_there}\n\n')
    adding = (f'cd {repo} && python3 -c "'
              f"import sys; p = sys.argv[1]; s = open(p).read(); "
              f"sys.exit('the field already holds {args.name}') if 'name={args.name}\\n' in s else None; "
              f"i = s.index('[tournament]'); open(p, 'w').write(s[:i] + sys.argv[2] + s[i:])"
              f'" {args.state} {block!r}')
    run(SSH + [there, adding], 'adding the engine')

    start = (f'cd {repo} && ( setsid nohup {host["qet"]} --concurrency={args.concurrency} '
             f'--logging path=test/log engine=false --tournament file={args.state} '
             f'>> test/log/gauntlet-run.log 2>&1 < /dev/null & ) ; sleep 75; '
             f'echo "qet $(pgrep -x qet)"; '
             f'echo "rounds $(grep -c \'^.round.$\' {args.state})"; '
             f'echo "engines $(grep -c \'^.engine.$\' {args.state})"; '
             f'tail -120 test/log/gauntlet-run.log | grep "^started" | sed "s/.*engines //" '
             f'| sort -u')
    out = run(SSH + [there, start], 'starting')
    ok = [l.strip() for l in out.splitlines()]
    running = any(l.startswith('qet ') and l.split()[1:] for l in ok)
    now_rounds = next((int(l.split()[1]) for l in ok if l.startswith('rounds')), -1)
    now_engines = next((int(l.split()[1]) for l in ok if l.startswith('engines')), -1)
    plays = any(args.name in l for l in ok if ' vs ' in l)
    for what, good in (('qet runs', running), (f'engines {engines} -> {now_engines}',
                                               now_engines == engines + 1),
                       (f'rounds {rounds} -> {now_rounds}', now_rounds >= rounds),
                       (f'{args.name} is playing', plays)):
        print(f'  {"ok  " if good else "FAIL"} {what}')
    if not (running and now_engines == engines + 1 and now_rounds >= rounds):
        sys.exit(1)
    if not plays:
        print('  it is in the field but has not been seen in a pairing yet - it may be waiting for '
              'a round of its own; look again in a few minutes')


if __name__ == '__main__':
    main()
