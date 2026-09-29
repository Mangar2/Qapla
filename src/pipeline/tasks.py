"""The task table: the open work, and who is currently on it. Nothing else.

The table is the master. What lies in the object store is data and says nothing about state - a file
may be deleted there without changing what is to be done, and a chunk may be asked for again by
writing one job into the table. That is the whole point of keeping the state here and not in the
shape of a bucket.

Only two kinds of thing are in it:

  open work     either a single job - one set, one kind, one chunk - or a range, "chunks 51 to 101
                of this kind", out of which a worker takes one at a time. A range is one item for
                a hundred pieces of work, which is why the table stays small.
  a claim       written when a worker takes work, deleted when it hands the result over. It carries
                the owner and the time, and that is what lets the supervisor give back the work of
                a machine that has gone away.

Work that is finished is in neither: it is simply no longer there. So the table shrinks as the run
proceeds, and "what is left to do" is a query rather than a comparison.

Taking a chunk out of a range is one conditional update that increments the range's lower end and
returns what it was before, so two workers can never get the same number. Taking a single job is a
conditional delete: whoever deletes it has it.

The access goes through the aws command line, not a library - every machine here has it, and a
claim happens once per piece of work of several minutes.
"""

import json
import subprocess
from datetime import datetime, timezone

OPEN, CLAIMED = 'open', 'claimed'


def now():
    return datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')


class Tasks:

    def __init__(self, cfg):
        self.region = cfg['aws']['region']
        self.table = cfg['aws']['table']

    def _run(self, *arguments, quiet=False):
        command = ['aws', 'dynamodb', *arguments, '--region', self.region,
                   '--table-name', self.table, '--output', 'json']
        done = subprocess.run(command, capture_output=True, text=True, timeout=120)
        if done.returncode != 0:
            if not quiet:
                print(f'dynamodb: {done.stderr.strip()[:200]}', flush=True)
            return None
        return json.loads(done.stdout) if done.stdout.strip() else {}

    # ------------------------------------------------------------------ putting work in

    def add_job(self, set_name, kind, chunk, rank_base, where='aws'):
        """One piece of work. This is how a chunk is asked for again, and how a successor appears."""
        item = {'id': {'S': f'job#{set_name}#{kind}#{chunk:04d}'}, 'status': {'S': OPEN},
                'form': {'S': 'single'}, 'set': {'S': set_name}, 'kind': {'S': kind},
                'chunk': {'N': str(chunk)}, 'rank_base': {'N': str(rank_base)},
                'rank': {'N': str(rank_base + chunk)}, 'where': {'S': where},
                'created': {'S': now()}}
        return self._run('put-item', '--item', json.dumps(item)) is not None

    # A range sorts behind every single job of the same kind, and that is deliberate: a job is
    # either work that was given back by a machine that vanished, or a chunk somebody asked for
    # again. Both are more urgent than starting something new, and a chunk number is never as
    # large as this penalty.
    RANGE_PENALTY = 500

    def add_range(self, set_name, kind, first, last, rank_base, where='aws'):
        """A hundred pieces of work as one item. Workers take chunks off its lower end."""
        item = {'id': {'S': f'range#{set_name}#{kind}'}, 'status': {'S': OPEN},
                'form': {'S': 'range'}, 'set': {'S': set_name}, 'kind': {'S': kind},
                'lo': {'N': str(first)}, 'hi': {'N': str(last)},
                'rank_base': {'N': str(rank_base)},
                'rank': {'N': str(rank_base + self.RANGE_PENALTY)},
                'where': {'S': where}, 'created': {'S': now()}}
        return self._run('put-item', '--item', json.dumps(item)) is not None

    # ------------------------------------------------------------------ taking work out

    def _take_from_range(self, item):
        """Increments the lower end and returns what it was - so no two workers get the same.

        The attributes are called lo and hi because "first" and several other obvious names are
        reserved words in dynamodb, which the service reports only as a validation error.
        """
        answer = self._run(
            'update-item', '--key', json.dumps({'id': {'S': item['id']}}),
            '--update-expression', 'ADD lo :one',
            '--condition-expression', 'lo <= hi',
            '--expression-attribute-values', json.dumps({':one': {'N': '1'}}),
            '--return-values', 'UPDATED_OLD', quiet=True)
        if answer is None:
            return None                      # exhausted, or somebody emptied it first
        return int(answer['Attributes']['lo']['N'])

    def _take_single(self, item):
        """Whoever deletes the job has it."""
        return self._run('delete-item', '--key', json.dumps({'id': {'S': item['id']}}),
                         '--condition-expression', 'attribute_exists(id)',
                         quiet=True) is not None

    def take(self, where, owner):
        """The most urgent open piece of work, claimed, or None if there is none."""
        for item in self.open_work():
            if item.get('where') != where:
                continue
            if item['form'] == 'range':
                chunk = self._take_from_range(item)
                if chunk is None:
                    # Only a range that is really used up is taken out. A failure for any other
                    # reason must not delete work - which it did once, and cost the two ranges.
                    if item['lo'] > item['hi']:
                        self.retire_range(item['id'])
                    continue
            else:
                if not self._take_single(item):
                    continue
                chunk = item['chunk']
            # The rank a piece carries is its own, not its range's - so work that comes back
            # after a machine vanished is handed out before the range moves on.
            work = {'set': item['set'], 'kind': item['kind'], 'chunk': chunk,
                    'rank_base': item['rank_base']}
            self._claim(owner, work)
            return work
        return None

    def retire_range(self, item_id):
        """An exhausted range is taken out, so nobody looks at it again."""
        self._run('delete-item', '--key', json.dumps({'id': {'S': item_id}}), quiet=True)

    # ------------------------------------------------------------------ claims

    def _claim(self, owner, work):
        item = {'id': {'S': f'claim#{owner}#{work["set"]}#{work["kind"]}#{work["chunk"]:04d}'},
                'status': {'S': CLAIMED}, 'owner': {'S': owner}, 'set': {'S': work['set']},
                'kind': {'S': work['kind']}, 'chunk': {'N': str(work['chunk'])},
                'rank_base': {'N': str(work['rank_base'])},
                'rank': {'N': str(work['rank_base'] + work['chunk'])},
                'where': {'S': 'aws'}, 'claimed_at': {'S': now()}}
        self._run('put-item', '--item', json.dumps(item))

    def release_claim(self, owner, work):
        """The work is done: the claim goes, and nothing takes its place."""
        self._run('delete-item', '--key', json.dumps(
            {'id': {'S': f'claim#{owner}#{work["set"]}#{work["kind"]}#{work["chunk"]:04d}'}}))

    def give_back(self, claim):
        """A claim of a machine that is gone becomes an open job again."""
        self.add_job(claim['set'], claim['kind'], claim['chunk'], claim['rank_base'],
                     claim.get('where', 'aws'))
        self._run('delete-item', '--key', json.dumps({'id': {'S': claim['id']}}))

    # ------------------------------------------------------------------ reading

    def _by_status(self, status, limit=400):
        answer = self._run('query', '--index-name', 'by-status',
                           '--key-condition-expression', '#s = :s',
                           '--expression-attribute-names', json.dumps({'#s': 'status'}),
                           '--expression-attribute-values', json.dumps({':s': {'S': status}}),
                           '--max-items', str(limit))
        return [plain(item) for item in (answer or {}).get('Items', [])]

    def open_work(self):
        """Open items, most urgent first - the index is sorted by rank."""
        return self._by_status(OPEN)

    def claims(self):
        return self._by_status(CLAIMED)

    def summary(self):
        """What is left, in pieces: a range counts as many as it still holds."""
        left, singles, ranges = 0, 0, 0
        for item in self.open_work():
            if item['form'] == 'range':
                ranges += 1
                left += max(0, item['hi'] - item['lo'] + 1)
            else:
                singles += 1
                left += 1
        return {'pieces left': left, 'ranges': ranges, 'single jobs': singles,
                'in progress': len(self.claims())}


def plain(item):
    out = {}
    for key, value in item.items():
        kind, raw = next(iter(value.items()))
        out[key] = int(raw) if kind == 'N' else raw
    return out
