"""Writes the games of a packed game file back out as a pgn.

    python3 export-pgn.py <game file> <pgn> [--max-games N]

Why this exists: the labelling pass takes a pgn, so re-labelling a set with a better evaluator
needs one - and a game file holds everything a pgn of played games holds. Every ply is in it,
including the moves of the book line, and the result of the game is in every record. What it does
not hold is the notation, and that is all this rebuilds.

The moves come out in long algebraic notation, which is what src/trainer/convert.py reads back
without a move generator. Games longer than 255 plies were already cut off when the file was
written - a length byte per game - so what comes out here is as long as what went in.

Checked on set 1, 200 games: 27,918 plies, not one move different from the game file, and the
result tags come out as they went in. That is why the template pgn of a set is not kept - it is
derivable, and a set costs about twelve minutes to write back out.
"""

import argparse
import format as fmt

RESULT_TAG = {fmt.RESULT_WIN: ('1-0', '0-1'), fmt.RESULT_LOSS: ('0-1', '1-0')}


def result_of(game):
    """The result of the game as a pgn tag, seen from white.

    The result in a record is seen from the side to move, and the side to move of the first
    position is white, so the first record decides. A game whose records say nothing gets a star.
    """
    if not game:
        return '*'
    stored = game[0][2]
    if stored == fmt.RESULT_DRAW:
        return '1/2-1/2'
    if stored == fmt.RESULT_NONE:
        return '*'
    return RESULT_TAG[stored][0]


def square_name(square):
    return 'abcdefgh'[square % 8] + '12345678'[square // 8]


def write(game_path, pgn_path, max_games=None):
    written = 0
    with open(pgn_path, 'w') as out:
        for game in fmt.read_games(game_path):
            board = fmt.Board()
            moves = []
            for packed, _value, _result in game:
                departure, destination, promotion = fmt.unpack_move(packed, board.squares)
                text = square_name(departure) + square_name(destination)
                if promotion != fmt.NO_PIECE:
                    text += 'qrbn'[(promotion >> 1) - 2] if promotion >> 1 >= 2 else 'q'
                moves.append(text)
                board.apply(departure, destination, promotion)
            out.write('[Event "Qapla rebuilt from a game file"]\n')
            out.write('[White "A"]\n[Black "B"]\n')
            out.write('[Result "%s"]\n[SetUp "0"]\n\n' % result_of(game))
            line = []
            for ply, move in enumerate(moves):
                if ply % 2 == 0:
                    line.append('%d.' % (ply // 2 + 1))
                line.append(move)
                if len(line) >= 12:
                    out.write(' '.join(line) + '\n')
                    line = []
            line.append(result_of(game))
            out.write(' '.join(line) + '\n\n')
            written += 1
            if max_games and written >= max_games:
                break
    print('%d games written to %s' % (written, pgn_path))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('games')
    parser.add_argument('pgn')
    parser.add_argument('--max-games', type=int, default=None)
    arguments = parser.parse_args()
    write(arguments.games, arguments.pgn, arguments.max_games)
