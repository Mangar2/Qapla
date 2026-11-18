#!/usr/bin/env python3
"""
Evaluation weakness filter for played games of Spike against Qapla.

Every move carries the evaluation of the engine that played it, from its own point of view
({+0.65/9}, {-M2/9}). For a move n of Spike and Qapla's reply n+1, both seen from Spike:

    eval(n) > eval(n+1) + p     Spike sees itself more than p better than Qapla does one ply later

sets the threshold sw = eval(n+1) + p. Move n is a hit if

  - Spike's evaluation of none of its later moves in that game falls below sw, and
  - Spike saw the result before and Qapla was clearly wrong (everything from Spike's point of view):
      win:  Spike wins,          eval(n) > p,  eval(n+1) <= scp
      draw: the game is drawn,   eval(n) > -p, eval(n+1) < dcp

Every game with a hit is copied unchanged to the output pgn, with one header per kind (win, draw) that
names its first hit:

    [EvalWeakness "move 26 (black) Re8: spike +2.00, qapla -0.47, sw +1.47, result 0-1"]

Usage:
    eval-weakness.py --p 100 [--scp 200] [--dcp -200] [--max-games N] --out result.pgn <pgn> [<pgn> ...]

A pgn given as host:path is read over ssh (ssh host cat path), so files on another machine need
no copy.
"""

import argparse
import multiprocessing
import os
import re
import subprocess
import sys
import time

import chess

MATE = 100000
TOKEN = re.compile(r'\{([^}]*)\}|([a-h][1-8][a-h][1-8][qrbnQRBN]?)|(1-0|0-1|1/2-1/2|\*)')
EVAL = re.compile(r'\s*([+-]?)(?:M(\d+)|(\d+(?:\.\d+)?))/')
HEADER = re.compile(r'\[(\w+)\s+"([^"]*)"\]')
RESULT = {'1-0': 1, '0-1': -1, '1/2-1/2': 0}
BATCH = 2000

# Set in every worker by init_worker
P = 0
SCP = 0
DCP = 0
SPIKE = ''


def parse_eval(comment):
    """Centipawns from the point of view of the side that moved, None without an evaluation."""
    m = EVAL.match(comment)
    if not m:
        return None
    sign = -1 if m.group(1) == '-' else 1
    if m.group(2) is not None:
        return sign * (MATE - int(m.group(2)))
    return sign * round(float(m.group(3)) * 100)


def fmt(cp):
    if abs(cp) >= MATE - 1000:
        return ('-' if cp < 0 else '') + 'M' + str(MATE - abs(cp))
    return '%+.2f' % (cp / 100)


def init_worker(p, scp, dcp, spike):
    global P, SCP, DCP, SPIKE
    P = p
    SCP = scp
    DCP = dcp
    SPIKE = spike


def analyse_game(text):
    """Returns (stats, game text with the hit headers or None)."""
    stats = {'games': 1, 'skipped': 0, 'spike moves': 0, 'triggered': 0, 'held': 0,
             'win positions': 0, 'draw positions': 0, 'win games': 0, 'draw games': 0}
    headers = dict(HEADER.findall(text))
    body = text[text.rfind(']') + 1:] if headers else text
    moves = []
    evals = []
    result = None
    for comment, move, res in TOKEN.findall(body):
        if move:
            moves.append(move.lower())
            evals.append(None)
        elif res:
            result = res
        elif moves and evals[-1] is None:
            evals[-1] = parse_eval(comment)
    if result not in RESULT or SPIKE not in (headers.get('White'), headers.get('Black')):
        stats['skipped'] = 1
        return stats, None
    fen = headers.get('FEN')
    board = chess.Board(fen) if fen else chess.Board()
    spike_white = headers.get('White') == SPIKE
    # Plies of Spike: same parity as the first ply if Spike is the side to move at the start
    first = 0 if board.turn == (chess.WHITE if spike_white else chess.BLACK) else 1
    spike_result = RESULT[result] if spike_white else -RESULT[result]

    # lowest[i]: lowest evaluation of Spike over its plies from i on
    lowest = [float('inf')] * (len(moves) + 2)
    for i in range(len(moves) - 1, -1, -1):
        lowest[i] = lowest[i + 1]
        if (i - first) % 2 == 0 and evals[i] is not None:
            lowest[i] = min(lowest[i], evals[i])

    found = {}
    taken = set()
    for i in range(first, len(moves) - 1, 2):
        if evals[i] is None:
            continue
        stats['spike moves'] += 1
        if evals[i + 1] is None:
            continue
        qapla = -evals[i + 1]
        if evals[i] <= qapla + P:
            continue
        stats['triggered'] += 1
        sw = qapla + P
        if lowest[i + 2] < sw:
            continue
        stats['held'] += 1
        if spike_result == 1 and evals[i] > P and qapla <= SCP:
            kind = 'win'
        elif spike_result == 0 and evals[i] > -P and qapla < DCP:
            kind = 'draw'
        else:
            continue
        stats[kind + ' positions'] += 1
        if kind not in taken:
            taken.add(kind)
            found[i] = sw
            stats[kind + ' games'] += 1
    if not found:
        return stats, None

    tags = []
    for i, move in enumerate(moves):
        if i in found:
            tags.append('[EvalWeakness "move %d (%s) %s: spike %s, qapla %s, sw %s, result %s"]\n' % (
                board.fullmove_number, 'white' if board.turn == chess.WHITE else 'black',
                board.san(chess.Move.from_uci(move)), fmt(evals[i]), fmt(evals[i + 1]), fmt(found[i]), result))
            if len(tags) == len(found):
                break
        board.push(chess.Move.from_uci(move))
    # The tags go behind the last header line
    end = text.rfind(']') + 1
    end = text.find('\n', end) + 1 if text.find('\n', end) >= 0 else end
    return stats, text[:end] + ''.join(tags) + text[end:]


def analyse_batch(batch):
    total = {}
    out = []
    for text in batch:
        stats, game = analyse_game(text)
        for k, v in stats.items():
            total[k] = total.get(k, 0) + v
        if game is not None:
            out.append(game)
    return total, out


def open_source(source):
    if ':' in source and not os.path.exists(source):
        host, path = source.split(':', 1)
        proc = subprocess.Popen(['ssh', host, 'cat', path], stdout=subprocess.PIPE)
        return proc, proc.stdout
    return None, open(source, 'rb')


def read_batches(stream, size):
    """Yields lists of game texts; games start at their first header."""
    games = []
    lines = []
    in_body = False
    for raw in stream:
        line = raw.decode('utf-8', 'replace')
        if line.startswith('[') and in_body:
            games.append(''.join(lines))
            lines = []
            in_body = False
            if len(games) == size:
                yield games
                games = []
        elif line.strip() and not line.startswith('['):
            in_body = True
        lines.append(line)
    if lines and in_body:
        games.append(''.join(lines))
    if games:
        yield games


def main():
    parser = argparse.ArgumentParser(description='Evaluation weakness filter for Spike against Qapla games')
    parser.add_argument('--p', type=int, required=True, help='margin in centipawns')
    parser.add_argument('--out', required=True, help='pgn file to write')
    parser.add_argument('--scp', type=int, default=200,
                        help='Spike wins: Qapla\'s evaluation from Spike\'s point of view must be at most this')
    parser.add_argument('--dcp', type=int, default=-200,
                        help='draw: Qapla\'s evaluation from Spike\'s point of view must be below this')
    parser.add_argument('--max-games', type=int, default=0, help='stop after this many games written, 0: no limit')
    parser.add_argument('--spike', default='spike', help='player name of Spike in the pgn')
    parser.add_argument('--workers', type=int, default=max(1, os.cpu_count() - 1))
    parser.add_argument('pgn', nargs='+')
    args = parser.parse_args()

    # A small limit is reached within the first games, large batches would read far beyond it
    size = BATCH if args.max_games == 0 else max(1, min(BATCH, args.max_games * 10))
    total = {}
    written = 0
    start = time.time()
    with open(args.out, 'w') as out, multiprocessing.Pool(args.workers, init_worker, (args.p, args.scp, args.dcp, args.spike)) as pool:
        for source in args.pgn:
            name = os.path.basename(source.split(':', 1)[-1])
            proc, stream = open_source(source)
            games = 0
            for stats, hits in pool.imap(analyse_batch, read_batches(stream, size)):
                for k, v in stats.items():
                    total[k] = total.get(k, 0) + v
                games += stats.get('games', 0)
                for game in hits:
                    if args.max_games and written >= args.max_games:
                        break
                    out.write(game.rstrip('\n') + '\n\n')
                    written += 1
                if args.max_games and written >= args.max_games:
                    break
                if games % 200000 < size:
                    print('%s: %d games, %d written, %.0f s' % (name, games, written, time.time() - start),
                          file=sys.stderr, flush=True)
            limit_reached = args.max_games and written >= args.max_games
            if proc and limit_reached:
                proc.kill()
            stream.close()
            if proc and not limit_reached and proc.wait() != 0:
                print('ssh for %s ended with exit code %d' % (source, proc.returncode), file=sys.stderr)
                sys.exit(1)
            print('%s done: %d games' % (name, games), file=sys.stderr, flush=True)
            if args.max_games and written >= args.max_games:
                pool.terminate()
                break
    print('p = %d, scp = %d, dcp = %d' % (args.p, args.scp, args.dcp))
    for k in ('games', 'skipped', 'spike moves', 'triggered', 'held', 'win positions', 'draw positions',
              'win games', 'draw games'):
        print('%-15s %d' % (k, total.get(k, 0)))
    print('%-15s %d' % ('written', written))
    print('%-15s %.0f s' % ('time', time.time() - start))


if __name__ == '__main__':
    main()
