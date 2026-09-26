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


def pack_move(departure, destination, promotion=NO_PIECE):
    """The eleven bit code of a move, or None if the format cannot express it.

    The counterpart of unpack_move and of packMove in src/book/packed-move.h. Needed by
    the converter that turns a pgn into a game file; the round trip against unpack_move
    is what keeps the two in step.
    """
    file_delta = (destination & 7) - (departure & 7)
    rank_delta = (destination >> 3) - (departure >> 3)
    if file_delta == 0 and rank_delta == 0:
        return None

    packed = None
    if abs(file_delta) * abs(rank_delta) == 2:
        # A knight, whose direction points back at the departure square.
        backwards = departure - destination
        for index in range(KNIGHT_DIRECTIONS):
            if DIRECTIONS[index] == backwards:
                packed = destination | (index << DIRECTION_SHIFT)
                break
    elif file_delta == 0 or rank_delta == 0 or abs(file_delta) == abs(rank_delta):
        def sign(value):
            return (value > 0) - (value < 0)
        step = sign(rank_delta) * 8 + sign(file_delta)
        for index in range(KNIGHT_DIRECTIONS, len(DIRECTIONS)):
            if DIRECTIONS[index] == step:
                packed = destination | (index << DIRECTION_SHIFT)
                break
    if packed is None:
        return None
    if promotion == NO_PIECE:
        return packed

    # The rank bits of the destination are redundant for a promotion and carry the
    # piece instead; a queen leaves them as they are.
    rank_bits = packed & TO_RANK_MASK
    last_rank = rank_bits == TO_RANK_MASK
    if not last_rank and rank_bits != 0:
        return None
    kind = promotion & ~1
    codes = {QUEEN: 7 if last_rank else 0, ROOK: 6 if last_rank else 1,
             BISHOP: 5 if last_rank else 2, KNIGHT: 4 if last_rank else 3}
    if kind not in codes:
        return None
    return (packed & ~TO_RANK_MASK) | (codes[kind] << 3) | PROMOTION_FLAG


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

GAME_FILE_MAGIC = b'QAPLAGM2'
GAME_FILE_VERSION = 2

# The value of a record is a win probability, stored as a code. Zero means the move
# has none, which happens for the moves of an opening line that were never searched.
NO_GAME_VALUE = 0
MIN_VALUE_CODE = 1
MAX_VALUE_CODE = (1 << 11) - 1

RESULT_LOSS, RESULT_DRAW, RESULT_WIN, RESULT_NONE = 0, 1, 2, 3

# The scale the engine's value is turned into a probability with, NET_VALUE_SCALE of
# src/nnue/nnue-arch.h. Only the converter needs it; a file already holds probabilities.
VALUE_SCALE = 400


def probability_of_code(code):
    """The probability of a code, or None for a move without a value."""
    if code == NO_GAME_VALUE:
        return None
    return (code - MIN_VALUE_CODE) / (MAX_VALUE_CODE - MIN_VALUE_CODE)


def code_of_probability(probability):
    span = MAX_VALUE_CODE - MIN_VALUE_CODE
    clamped = 0.0 if probability < 0.0 else (1.0 if probability > 1.0 else probability)
    # floor(x + 0.5) and not round(), which rounds halves to even in Python and would
    # differ from lround() in the engine by one code.
    return MIN_VALUE_CODE + int(clamped * span + 0.5)


def code_of_value(value):
    """The code of a value in the unit of the engine, where a pawn is 80 to 95."""
    import math
    return code_of_probability(1.0 / (1.0 + math.exp(-value / VALUE_SCALE)))


def _unpack_record(record):
    return record & 0x7FF, (record >> 13) & MAX_VALUE_CODE, (record >> 11) & 0x3


def pack_record(move, value, result):
    return (move & 0x7FF) | ((result & 0x3) << 11) | ((value & MAX_VALUE_CODE) << 13)


def read_games(path):
    """Yields one list of (packed move, value code, result) per game."""
    with open(path, 'rb') as stream:
        header = stream.read(len(GAME_FILE_MAGIC) + 4)
        if len(header) != len(GAME_FILE_MAGIC) + 4 \
                or header[:len(GAME_FILE_MAGIC)] != GAME_FILE_MAGIC \
                or int.from_bytes(header[len(GAME_FILE_MAGIC):], 'little') != GAME_FILE_VERSION:
            raise ValueError('%s is not a game file of version %d - a file of the older '
                             'format holds pawns where this expects probabilities'
                             % (path, GAME_FILE_VERSION))
        while True:
            count_byte = stream.read(1)
            if not count_byte:
                return
            count = count_byte[0]
            if count == 0:
                return
            data = stream.read(3 * count)
            if len(data) != 3 * count:
                return
            yield [_unpack_record(data[i] | (data[i + 1] << 8) | (data[i + 2] << 16))
                   for i in range(0, len(data), 3)]


def write_games(path, games):
    """Writes games, each a list of (packed move, value code, result)."""
    with open(path, 'wb') as stream:
        stream.write(GAME_FILE_MAGIC)
        stream.write(GAME_FILE_VERSION.to_bytes(4, 'little'))
        for moves in games:
            if not moves or len(moves) > 255:
                continue
            stream.write(bytes([len(moves)]))
            for move, value, result in moves:
                record = pack_record(move, value, result)
                stream.write(bytes([record & 0xFF, (record >> 8) & 0xFF, (record >> 16) & 0xFF]))


def read_positions(path, max_positions=None):
    """Yields (board, value code, result) for every position that carries a value.

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
