"""The two file formats of the engine, read in Python.

This module is the counterpart of src/book/packed-move.h, src/nnue-data/game-file.h
and src/nnue/nnue-features.h. Everything that has to agree between the engine and
the training lives here, so that there is one place to look when something drifts:

* the eleven bit move code and how it is unpacked without a move generator,
* the three byte record of a played game,
* the piece encoding of Qapla and the HalfKA feature index built from it.

It needs nothing but the standard library. The board below applies moves and does
not generate or check them - it does not have to, every move in a game file was
played by the engine and is legal.
"""

from array import array

# --- the piece encoding of Qapla, see basics/types.h -------------------------

NO_PIECE = 0
WHITE, BLACK = 0, 1
PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING = 2, 4, 6, 8, 10, 12
MIN_PIECE = PAWN                     # white pawn, the first real piece
WHITE_KING, BLACK_KING = KING, KING + 1

# --- the packed move, see src/book/packed-move.h -----------------------------

DIRECTIONS = (
    +2 * 8 - 1, +2 * 8 + 1, +1 * 8 + 2, -1 * 8 + 2,      # knight
    +1 * 8 - 2, -1 * 8 - 2, -2 * 8 - 1, -2 * 8 + 1,
    +0 * 8 + 1, +0 * 8 - 1, +1 * 8 + 0, -1 * 8 + 0,      # rook
    +1 * 8 + 1, -1 * 8 + 1, +1 * 8 - 1, -1 * 8 - 1,      # bishop
)
KNIGHT_DIRECTIONS = 8

# Index 0 and 7 both mean queen: those are the values the rank bits of the
# destination square have anyway, so a queen promotion leaves them alone.
PROMOTION_PIECES = (
    QUEEN + BLACK, ROOK + BLACK, BISHOP + BLACK, KNIGHT + BLACK,
    KNIGHT + WHITE, BISHOP + WHITE, ROOK + WHITE, QUEEN + WHITE,
)

TO_MASK = 0x3F
TO_RANK_MASK = 0x38
DIRECTION_SHIFT = 6
DIRECTION_MASK = 0x3C0
PROMOTION_FLAG = 0x400


def unpack_move(packed, squares):
    """Returns (from, to, promotion) of a packed move in the given placement.

    The departure square is the first occupied square when walking from the
    destination against the direction of the move - for a slider that square is
    necessarily the moving piece, anything in between would have blocked it.
    """
    bits = packed
    promotion = NO_PIECE
    if bits & PROMOTION_FLAG:
        code = (bits & TO_RANK_MASK) >> 3
        promotion = PROMOTION_PIECES[code]
        # Restore the rank of the destination, which carried the promotion piece.
        bits = (bits | TO_RANK_MASK) if code > 3 else (bits & ~TO_RANK_MASK)
    to = bits & TO_MASK
    direction = (bits & DIRECTION_MASK) >> DIRECTION_SHIFT
    if direction < KNIGHT_DIRECTIONS:
        return to + DIRECTIONS[direction], to, promotion
    step = DIRECTIONS[direction]
    square = to - step
    while 0 <= square < 64 and squares[square] == NO_PIECE:
        square -= step
    return square, to, promotion


# --- a board that only replays -----------------------------------------------

START_PLACEMENT = None


def _start_placement():
    global START_PLACEMENT
    if START_PLACEMENT is None:
        back = (ROOK, KNIGHT, BISHOP, QUEEN, KING, BISHOP, KNIGHT, ROOK)
        squares = array('b', [NO_PIECE]) * 64
        for file in range(8):
            squares[file] = back[file] + WHITE
            squares[8 + file] = PAWN + WHITE
            squares[48 + file] = PAWN + BLACK
            squares[56 + file] = back[file] + BLACK
        START_PLACEMENT = squares
    return START_PLACEMENT


class Board:
    """Piece placement, king squares and side to move. Applies moves, nothing else."""

    __slots__ = ('squares', 'kings', 'white_to_move')

    def __init__(self):
        self.squares = array('b', _start_placement())
        self.kings = [4, 60]
        self.white_to_move = True

    def apply(self, departure, destination, promotion):
        squares = self.squares
        piece = squares[departure]
        kind = piece & ~1
        if kind == KING:
            self.kings[piece & 1] = destination
            step = destination - departure
            if step == 2:            # castling short, the rook follows
                squares[destination - 1] = squares[destination + 1]
                squares[destination + 1] = NO_PIECE
            elif step == -2:         # castling long
                squares[destination + 1] = squares[destination - 2]
                squares[destination - 2] = NO_PIECE
        elif kind == PAWN and (departure & 7) != (destination & 7) \
                and squares[destination] == NO_PIECE:
            # A pawn moving sideways to an empty square captures en passant.
            squares[(departure & ~7) | (destination & 7)] = NO_PIECE
        squares[destination] = promotion if promotion != NO_PIECE else piece
        squares[departure] = NO_PIECE
        self.white_to_move = not self.white_to_move


# --- the features, see src/nnue/nnue-features.h -------------------------------

SQUARE_COUNT = 64
PIECE_PLANES = 11
FEATURE_COUNT = PIECE_PLANES * SQUARE_COUNT * SQUARE_COUNT     # 45056
MAX_ACTIVE_FEATURES = 32


def features(board, perspective):
    """The active features of one perspective, HalfKA in its second shape.

    Eleven planes: the own king needs none, its square is already the first part
    of the index. Seen from black every square is mirrored top to bottom and every
    colour is the other one - and because the colour is the lowest bit of a piece,
    that second part is an exclusive or with one.
    """
    flip = 0x38 if perspective == BLACK else 0
    king = board.kings[perspective] ^ flip
    base = king * PIECE_PLANES
    squares = board.squares
    result = []
    for square in range(64):
        piece = squares[square]
        if piece == NO_PIECE:
            continue
        relative = (piece - MIN_PIECE) ^ perspective
        if relative == 10:                       # the own king
            continue
        plane = 10 if relative == 11 else relative
        result.append((base + plane) * SQUARE_COUNT + (square ^ flip))
    return result


# --- the game file, see src/nnue-data/game-file.h -----------------------------

NO_GAME_VALUE = -1024
RESULT_LOSS, RESULT_DRAW, RESULT_WIN = 0, 1, 2


def _unpack_record(record):
    move = record & 0x7FF
    result = (record >> 11) & 0x3
    value = record >> 13
    if value >= 1 << 10:                         # eleven bits, two's complement
        value -= 1 << 11
    return move, value, result


def read_games(path):
    """Yields one list of (packed move, value, result) per game."""
    with open(path, 'rb') as stream:
        while True:
            header = stream.read(1)
            if not header:
                return
            count = header[0]
            if count == 0:
                return
            data = stream.read(3 * count)
            if len(data) != 3 * count:
                return
            yield [_unpack_record(data[i] | (data[i + 1] << 8) | (data[i + 2] << 16))
                   for i in range(0, len(data), 3)]


def read_positions(path, max_positions=None):
    """Yields (board, value, result) for every position that carries a value.

    The board is replayed from the initial position, which is why the moves of the
    book line are in the file at all - they have no value and are only walked
    through. The value and the result are seen from the side to move.
    """
    produced = 0
    for game in read_games(path):
        board = Board()
        for packed, value, result in game:
            if value != NO_GAME_VALUE:
                yield board, value, result
                produced += 1
                if max_positions is not None and produced >= max_positions:
                    return
            departure, destination, promotion = unpack_move(packed, board.squares)
            board.apply(departure, destination, promotion)
