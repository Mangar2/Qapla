#!/usr/bin/env python3
"""
Compares the static evaluation of Qapla and Spike for one position of a game.

The position is taken from a pgn written by eval-weakness.py: by default the first hit named in the
game's EvalWeakness header, the position before Spike's move and the one after it. A move can also
be given directly ("26b" = black's 26th move), or a FEN instead of a game.

For each position it prints
  - Qapla's eval command (board with the value of every piece, the sum of the eval terms),
  - Spike's eval command (every eval term, white/black split into midgame/endgame).
Both engines print their values from white's point of view.

Spike prints its terms only in a build without _AUSLIEFERUNG, see Spike's Eval::printEvalTerms. Build it
with
    cmake -S ../spike -B build/spike-eval -G Ninja -DCMAKE_BUILD_TYPE=Release -DCOMMON_CXX_FLAGS=-DNDEBUG
    cmake --build build/spike-eval

Usage:
    compare-eval.py <pgn> <game>           game number in the pgn, 1 = first
    compare-eval.py <pgn> <game> --move 26b
    compare-eval.py --fen "<fen>"
"""

import argparse
import os
import re
import subprocess
import sys

import chess

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
QAPLA = os.path.join(ROOT, 'new-versions', 'Qapla-set1-hce')
SPIKE = os.path.join(ROOT, 'build', 'spike-eval', 'SpikeEngine')
TOKEN = re.compile(r'\{([^}]*)\}|([a-h][1-8][a-h][1-8][qrbnQRBN]?)|(1-0|0-1|1/2-1/2|\*)')
HIT = re.compile(r'\[EvalWeakness "move (\d+) \((white|black)\)')


def read_game(path, number):
    """Text of game number (1 = first) of the pgn."""
    count = 1
    lines = []
    in_body = False
    with open(path) as f:
        for line in f:
            if line.startswith('[') and in_body:
                if count == number:
                    return ''.join(lines)
                count += 1
                lines = []
                in_body = False
            elif line.strip() and not line.startswith('['):
                in_body = True
            lines.append(line)
    if count == number and in_body:
        return ''.join(lines)
    sys.exit('the pgn has only %d games' % count)


def parse_move(text):
    """"26b" -> (26, black)"""
    m = re.fullmatch(r'(\d+)([wb])', text)
    if not m:
        sys.exit('a move is given as <number>w or <number>b, e.g. 26b')
    return int(m.group(1)), chess.WHITE if m.group(2) == 'w' else chess.BLACK


def positions_from_game(text, move):
    """Returns [(title, fen), ...] for the position before the chosen move and after it."""
    headers = dict(re.findall(r'\[(\w+)\s+"([^"]*)"\]', text))
    if move is None:
        hit = HIT.search(text)
        if not hit:
            sys.exit('the game has no EvalWeakness header, give the move with --move')
        move = (int(hit.group(1)), chess.WHITE if hit.group(2) == 'white' else chess.BLACK)
    board = chess.Board(headers['FEN']) if 'FEN' in headers else chess.Board()
    body = text[text.rfind(']') + 1:]
    moves = []
    for comment, mv, res in TOKEN.findall(body):
        if mv:
            moves.append([mv.lower(), ''])
        elif comment and moves:
            moves[-1][1] = comment.strip()
    for mv, comment in moves:
        if (board.fullmove_number, board.turn) == move:
            san = board.san(chess.Move.from_uci(mv))
            before = board.fen()
            board.push(chess.Move.from_uci(mv))
            side = 'white' if move[1] == chess.WHITE else 'black'
            label = '%s %d%s' % (headers.get('White') if move[1] == chess.WHITE else headers.get('Black'),
                                 move[0], '.' if move[1] == chess.WHITE else '...')
            result = [('before %s %s {%s}' % (label, san, comment), before)]
            nxt = moves[moves.index([mv, comment]) + 1] if [mv, comment] != moves[-1] else None
            reply = ''
            if nxt:
                reply = ', reply %s {%s}' % (board.san(chess.Move.from_uci(nxt[0])), nxt[1])
            result.append(('after %s %s (%s to move%s)' % (label, san,
                           'black' if side == 'white' else 'white', reply), board.fen()))
            return result
        board.push(chess.Move.from_uci(mv))
    sys.exit('move %d%s is not in the game' % (move[0], 'w' if move[1] == chess.WHITE else 'b'))


def run(engine, commands):
    proc = subprocess.run([engine], input=commands, capture_output=True, text=True, timeout=60)
    return proc.stdout


def qapla_eval(engine, fen):
    out = run(engine, 'setboard %s\neval\nquit\n' % fen)
    # Drop the banner line
    return '\n'.join(out.splitlines()[1:])


def spike_eval(engine, fen):
    out = run(engine, 'xboard\nsetboard %s\neval\nquit\n' % fen)
    lines = out.splitlines()
    start = next((i for i, l in enumerate(lines) if l.startswith('Phase midgame')), None)
    if start is None:
        return out + '\n(no eval terms: Spike build with _AUSLIEFERUNG?)'
    return '\n'.join(lines[start:])


def main():
    parser = argparse.ArgumentParser(description='Static eval of Qapla and Spike side by side')
    parser.add_argument('pgn', nargs='?')
    parser.add_argument('game', nargs='?', type=int)
    parser.add_argument('--move', help='move of the game, e.g. 26b; default: the EvalWeakness hit')
    parser.add_argument('--fen', help='a position instead of a game')
    parser.add_argument('--qapla', default=QAPLA, help='Qapla binary, default: the one that played the games')
    parser.add_argument('--spike', default=SPIKE, help='Spike binary built without _AUSLIEFERUNG')
    args = parser.parse_args()

    if args.fen:
        positions = [('position', args.fen)]
    elif args.pgn and args.game:
        text = read_game(args.pgn, args.game)
        positions = positions_from_game(text, parse_move(args.move) if args.move else None)
    else:
        parser.error('give a pgn and a game number, or --fen')

    for title, fen in positions:
        print('=' * 100)
        print(title)
        print(fen)
        print('-' * 40 + ' Qapla (%s), white\'s point of view' % os.path.basename(args.qapla))
        print(qapla_eval(args.qapla, fen))
        print('-' * 40 + ' Spike (%s), white\'s point of view' % os.path.basename(args.spike))
        print(spike_eval(args.spike, fen))


if __name__ == '__main__':
    main()
