"""Turns a pgn of played games into the game file the training reads.

    python3 convert.py <pgn> <game file> [--wdl result|none] [--max-games N]

The pgn has to hold its moves in long algebraic notation - the tester writes them that
way with "notation=lan" - because then the converter needs no move generator and no
board: packing a move needs nothing but its two squares, only unpacking needs a position.
A pgn in the usual short notation would need a legal move generator to tell which of two
bishops is meant, which is a few hundred lines of exactly the kind of code that is wrong
without anyone noticing.

The evaluation of a move is read out of its comment. The tester writes the value of the
engine as pawns with two decimals, and the engine reports its own unit as if it were
centipawns, so a hundred times the number is the value in the unit of the engine and the
two decimals are one unit of resolution. From there it goes through the sigmoid into the
win probability the file stores, see format.code_of_value.

--wdl decides what becomes of the result of a game:
  result  the result of the game, as it stands. Right for games between engines of equal
          strength, where a result says something about the positions.
  none    the result says nothing. Right for a strong engine against a weak one, where
          it says something about the players instead, and where using it would teach
          the net that the positions the weaker one reaches are lost.

The perspective of the evaluations is checked rather than assumed. Seen from the side to
move, consecutive values of a game have opposite signs almost always; seen from white
they keep their sign. A file that does not clearly look like one of the two is refused
instead of converted with every sign turned around.
"""

import sys

import format as fmt

RESULT_OF_TAG = {'1-0': 'white', '0-1': 'black', '1/2-1/2': 'draw'}


class Counters:
    def __init__(self):
        self.games = 0
        self.written = 0
        self.moves = 0
        self.withValue = 0
        self.skippedFen = 0
        self.skippedNoValue = 0
        self.skippedUnpackable = 0
        self.truncated = 0
        self.badComments = 0
        self.sameSign = 0
        self.oppositeSign = 0


def _parse_value(comment, counters):
    """The value in the unit of the engine out of a pgn comment, or None."""
    token = comment.split()[0] if comment.split() else ''
    token = token.split('/')[0]                    # an optional /depth
    try:
        return int(round(float(token) * 100.0))
    except ValueError:
        # A mate announcement, however it is spelt: as far from equal as it gets.
        if 'M' in token.upper():
            return 30000 if not token.startswith('-') else -30000
        counters.badComments += 1
        return None


def _parse_movetext(text, counters):
    """The moves of a game as a list of (from, to, promotion, value or None)."""
    moves = []
    index = 0
    length = len(text)
    while index < length:
        character = text[index]
        if character.isspace():
            index += 1
            continue
        if character == '{':
            end = text.find('}', index)
            if end < 0:
                break
            if moves:
                moves[-1] = moves[-1][:3] + (_parse_value(text[index + 1:end], counters),)
            index = end + 1
            continue
        end = index
        while end < length and not text[end].isspace() and text[end] != '{':
            end += 1
        token = text[index:end]
        index = end
        if token[0].isdigit() and ('.' in token or token in RESULT_OF_TAG or token == '*'):
            continue                               # a move number or the result
        if token in RESULT_OF_TAG or token == '*':
            continue
        if len(token) < 4 or not ('a' <= token[0] <= 'h'):
            continue                               # not a move in long notation
        departure = (ord(token[0]) - ord('a')) + (ord(token[1]) - ord('1')) * 8
        destination = (ord(token[2]) - ord('a')) + (ord(token[3]) - ord('1')) * 8
        promotion = fmt.NO_PIECE
        if len(token) > 4:
            kind = {'q': fmt.QUEEN, 'r': fmt.ROOK, 'b': fmt.BISHOP, 'n': fmt.KNIGHT}.get(
                token[4].lower())
            if kind is None:
                continue
            promotion = kind + (fmt.WHITE if destination >= 56 else fmt.BLACK)
        moves.append((departure, destination, promotion, None))
    return moves


def _game_records(tags, moves, wdl, counters):
    """The records of one game, or None if it cannot be stored."""
    if 'FEN' in tags:
        # The file starts every game at the initial position; one that starts elsewhere
        # cannot be replayed and is not ours to guess about.
        counters.skippedFen += 1
        return None
    if len(moves) > 255:
        moves = moves[:255]
        counters.truncated += 1

    outcome = RESULT_OF_TAG.get(tags.get('Result', '*'))
    records = []
    previous = None
    for ply, (departure, destination, promotion, value) in enumerate(moves):
        packed = fmt.pack_move(departure, destination, promotion)
        if packed is None:
            counters.skippedUnpackable += 1
            return None
        if wdl == 'none' or outcome is None:
            result = fmt.RESULT_NONE
        elif outcome == 'draw':
            result = fmt.RESULT_DRAW
        else:
            whiteToMove = ply % 2 == 0
            sideToMoveWins = whiteToMove == (outcome == 'white')
            result = fmt.RESULT_WIN if sideToMoveWins else fmt.RESULT_LOSS
        if value is None:
            code = fmt.NO_GAME_VALUE
        else:
            code = fmt.code_of_value(value)
            counters.withValue += 1
            if previous is not None:
                if (value < 0) != (previous < 0):
                    counters.oppositeSign += 1
                else:
                    counters.sameSign += 1
            previous = value
        records.append((packed, code, result))
        counters.moves += 1
    if counters.withValue == 0 and not any(r[1] != fmt.NO_GAME_VALUE for r in records):
        counters.skippedNoValue += 1
        return None
    return records


def convert(pgn_path, game_path, wdl='result', max_games=None):
    counters = Counters()
    games = []

    def flush(tags, movetext):
        moves = _parse_movetext(movetext, counters)
        if not moves:
            return
        counters.games += 1
        records = _game_records(tags, moves, wdl, counters)
        if records:
            games.append(records)
            counters.written += 1

    tags, movetext, inMoves = {}, [], False
    with open(pgn_path, 'r', errors='replace') as stream:
        for line in stream:
            stripped = line.strip()
            if stripped.startswith('['):
                if inMoves:
                    flush(tags, ' '.join(movetext))
                    if max_games is not None and counters.games >= max_games:
                        tags, movetext, inMoves = {}, [], False
                        break
                    tags, movetext, inMoves = {}, [], False
                if ' "' in stripped:
                    name, _, rest = stripped[1:].partition(' "')
                    tags[name] = rest.rsplit('"', 1)[0]
                continue
            if stripped:
                inMoves = True
                movetext.append(stripped)
    if inMoves:
        flush(tags, ' '.join(movetext))

    decided = counters.sameSign + counters.oppositeSign
    fraction = counters.oppositeSign / decided if decided else 0.0
    print('evaluations from the side to move in %.1f%% of the pairs' % (fraction * 100.0))
    if decided > 100 and not (fraction > 0.8 or fraction < 0.2):
        print('Refused: the perspective of the evaluations is not clear. Neither the side '
              'to move nor white fits, so every sign could be wrong.')
        return None
    if decided > 100 and fraction < 0.2:
        print('Refused: the evaluations look like they are seen from white. This file '
              'stores them from the side to move; convert with the tester set to the '
              'engine\'s own point of view.')
        return None

    fmt.write_games(game_path, games)
    print('games %d, written %d, moves %d, with a value %d'
          % (counters.games, counters.written, counters.moves, counters.withValue))
    print('skipped: from a fen %d, without a value %d, unpackable move %d; truncated %d; '
          'unreadable comments %d' % (counters.skippedFen, counters.skippedNoValue,
                                      counters.skippedUnpackable, counters.truncated,
                                      counters.badComments))
    return counters


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(__doc__)
        raise SystemExit(1)
    arguments = sys.argv[3:]
    wdl = 'result'
    maximum = None
    while arguments:
        if arguments[0] == '--wdl':
            wdl = arguments[1]
            arguments = arguments[2:]
        elif arguments[0] == '--max-games':
            maximum = int(arguments[1])
            arguments = arguments[2:]
        else:
            print('unknown argument %s' % arguments[0])
            raise SystemExit(1)
    if wdl not in ('result', 'none'):
        print('--wdl takes result or none')
        raise SystemExit(1)
    raise SystemExit(0 if convert(sys.argv[1], sys.argv[2], wdl, maximum) else 1)
