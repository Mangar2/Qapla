"""Brings a game file of the first format into the second.

    python3 upgrade.py <old file> <new file>

The first format had no header at all and stored the value of a position as eleven bits of
the engine's own unit, two's complement, so about twelve pawns either way, with -1024
meaning "no value". A game had to end when the value left that range, which is why those
files hold almost nothing of a decided endgame.

The second format stores a win probability instead, which has no range to leave, and puts
a magic and a version in front so that the two can never be mistaken for one another.

This exists because the games of the first format were played by the engine itself and
were never written as a pgn, so they cannot be converted again from an archive. Games that
do have one should go through convert.py instead - nothing here can recover what the old
range cut off.
"""

import sys

import format as fmt

OLD_NO_VALUE = -1024


def _unpack_old(record):
    move = record & 0x7FF
    result = (record >> 11) & 0x3
    value = record >> 13
    if value >= 1 << 10:                          # eleven bits, two's complement
        value -= 1 << 11
    return move, value, result


def upgrade(old_path, new_path):
    games = []
    moves = 0
    withValue = 0
    with open(old_path, 'rb') as stream:
        head = stream.read(len(fmt.GAME_FILE_MAGIC))
        if head == fmt.GAME_FILE_MAGIC:
            print('%s is already of the second format' % old_path)
            return None
        stream.seek(0)
        while True:
            count_byte = stream.read(1)
            if not count_byte:
                break
            count = count_byte[0]
            if count == 0:
                break
            data = stream.read(3 * count)
            if len(data) != 3 * count:
                break
            records = []
            for index in range(0, len(data), 3):
                move, value, result = _unpack_old(
                    data[index] | (data[index + 1] << 8) | (data[index + 2] << 16))
                if value == OLD_NO_VALUE:
                    code = fmt.NO_GAME_VALUE
                else:
                    code = fmt.code_of_value(value)
                    withValue += 1
                records.append((move, code, result))
                moves += 1
            games.append(records)
    fmt.write_games(new_path, games)
    print('games %d, moves %d, with a value %d, written to %s'
          % (len(games), moves, withValue, new_path))
    return len(games)


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(__doc__)
        raise SystemExit(1)
    raise SystemExit(0 if upgrade(sys.argv[1], sys.argv[2]) else 1)
