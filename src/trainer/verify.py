"""Checks that this code and the engine mean the same thing by a net.

    python3 verify.py <net file> [<games file>]

Prints the value of the start position and, if a game file is given, of its first
positions, computed with the integer arithmetic of the engine. Compare the first
number with what the engine says:

    printf 'stat\\nnew\\nnnueeval net <net file>\\nquit\\n' | ./build/Release/Qapla

They have to be equal. Both sides compute in integers, so a difference of one is a
difference, and it means the features, the file layout or the quantization have
drifted apart.
"""

import sys

import format as fmt
import netfile


def value_of(network, board):
    own = fmt.WHITE if board.white_to_move else fmt.BLACK
    other = fmt.BLACK if board.white_to_move else fmt.WHITE
    return netfile.evaluate(network, fmt.features(board, own), fmt.features(board, other))


def main():
    if len(sys.argv) < 2:
        print('usage: verify.py <net file> [<games file>]')
        raise SystemExit(1)
    network = netfile.read(sys.argv[1])
    print('shape %d, start position %d' % (netfile.architecture_id(),
                                           value_of(network, fmt.Board())))
    if len(sys.argv) > 2:
        for index, (board, stored, result) in enumerate(
                fmt.read_positions(sys.argv[2], 5)):
            print('position %d: net %d, stored %d, result %d'
                  % (index, value_of(network, board), stored, result))


if __name__ == '__main__':
    main()
