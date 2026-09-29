"""Puts open work into the task table.

    seed-tasks.py set3 --play 50-101 --label 1-49     ranges of chunks
    seed-tasks.py set3 --play 77                      one chunk again
    seed-tasks.py --summary                           what is left

Ranges are given rather than worked out from the object store, because the table is the master and
the store says nothing about state. A set that was half played by an older run is handed over by
naming the ranges once; a chunk that has to be redone is one job.

What is not seeded: labelling a chunk appears when its playing is finished, and the conversion of a
set appears when the last of its labels is - both written by the worker that finished the piece
before. So the table only ever holds what is open, and a finished piece is simply not in it.
"""

import argparse

import pipeline as pl
import tasks as tk

# The weight of a kind inside a set's rank: labelling before playing, because a set is only useful
# once it is labelled and labelling is the slower of the two.
WEIGHT = {'label': 0, 'play': 100000, 'convert': 300000}


def span(text):
    """"50-101" or "77"."""
    if '-' in text:
        first, last = text.split('-', 1)
        return int(first), int(last)
    return int(text), int(text)


def seed(cfg, name, kind, text):
    definition = next((s for s in cfg['sets'] if s['name'] == name), None)
    if definition is None:
        raise SystemExit(f'no set named {name} in the configuration')
    table = tk.Tasks(cfg)
    first, last = span(text)
    rank = definition['base'] + WEIGHT[kind]
    if first == last:
        table.add_job(name, kind, first, rank)
        print(f'{name}: one {kind} job for chunk {first}')
    else:
        table.add_range(name, kind, first, last, rank)
        print(f'{name}: {kind} chunks {first} to {last} as one range, '
              f'{last - first + 1} pieces')


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('set', nargs='?')
    parser.add_argument('--play', help='a range like 50-101, or one chunk')
    parser.add_argument('--label', help='a range like 1-49, or one chunk')
    parser.add_argument('--convert', action='store_true', help='the conversion of the set')
    parser.add_argument('--summary', action='store_true')
    parser.add_argument('--config', default=pl.CONFIG)
    parser.add_argument('--local', default=pl.LOCAL)
    arguments = parser.parse_args()
    cfg = pl.load_config(arguments.config, arguments.local)
    table = tk.Tasks(cfg)
    if arguments.set:
        for kind, text in (('play', arguments.play), ('label', arguments.label)):
            if text:
                seed(cfg, arguments.set, kind, text)
        if arguments.convert:
            seed(cfg, arguments.set, 'convert', '0')
    print('table:', table.summary())


if __name__ == '__main__':
    main()
