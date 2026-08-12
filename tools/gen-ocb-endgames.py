#!/usr/bin/env python3
"""
Generates a start position set of opposite coloured bishop endgames.

Each position holds one bishop per side on squares of opposite colour, pawns on both
sides and a material surplus of one or two pawns for one of them - the case todo item 10
is about. The standard opening book produces this type far too rarely to measure anything
on it, so the scaling factor is measured on this set instead.

Legality is checked by hand, which is enough for this material: no square is occupied
twice, the kings do not touch, the side not to move is not in check, and no pawn stands
on a rank it could not be on. White is always to move, castling and en passant are empty.

Usage: python3 tools/gen-ocb-endgames.py <count> > test/opening/ocb-endgames.raw
"""

import random
import sys

FILES = "abcdefgh"


def square(file_index, rank_index):
    """rank_index 0 is rank 1."""
    return file_index, rank_index


def is_light(sq):
    file_index, rank_index = sq
    return (file_index + rank_index) % 2 == 1


def king_distance(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def bishop_attacks(sq, occupied):
    result = set()
    for df, dr in ((1, 1), (1, -1), (-1, 1), (-1, -1)):
        f, r = sq[0] + df, sq[1] + dr
        while 0 <= f < 8 and 0 <= r < 8:
            result.add((f, r))
            if (f, r) in occupied:
                break
            f += df
            r += dr
    return result


def pawn_attacks(sq, white):
    direction = 1 if white else -1
    result = set()
    for df in (-1, 1):
        f, r = sq[0] + df, sq[1] + direction
        if 0 <= f < 8 and 0 <= r < 8:
            result.add((f, r))
    return result


def make_position(rng):
    """Returns a FEN or None if the drawn placement is not legal."""
    white_pawn_count = rng.randint(2, 5)
    surplus = rng.choice((1, 1, 2))
    black_pawn_count = white_pawn_count - surplus
    if black_pawn_count < 1:
        return None
    if rng.random() < 0.5:
        white_pawn_count, black_pawn_count = black_pawn_count, white_pawn_count

    occupied = {}

    def place(sq, piece):
        if sq in occupied:
            return False
        occupied[sq] = piece
        return True

    # Pawns: at most one per file and side, ranks 2 to 6 for white and 3 to 7 for black,
    # so that no side starts with a promotion already on the board.
    white_files = rng.sample(range(8), white_pawn_count)
    black_files = rng.sample(range(8), black_pawn_count)
    for f in white_files:
        if not place(square(f, rng.randint(1, 5)), "P"):
            return None
    for f in black_files:
        if not place(square(f, rng.randint(2, 6)), "p"):
            return None

    # Bishops on opposite coloured squares
    white_bishop_light = rng.random() < 0.5
    free = [(f, r) for f in range(8) for r in range(8) if (f, r) not in occupied]
    white_squares = [s for s in free if is_light(s) == white_bishop_light]
    black_squares = [s for s in free if is_light(s) != white_bishop_light]
    if not white_squares or not black_squares:
        return None
    if not place(rng.choice(white_squares), "B"):
        return None
    if not place(rng.choice(black_squares), "b"):
        return None

    # Kings, not touching each other
    free = [(f, r) for f in range(8) for r in range(8) if (f, r) not in occupied]
    white_king = rng.choice(free)
    place(white_king, "K")
    free = [s for s in free if king_distance(s, white_king) > 1 and s not in occupied]
    if not free:
        return None
    black_king = rng.choice(free)
    place(black_king, "k")

    # White is to move, so black must not be in check
    occupied_squares = set(occupied)
    attacked_by_white = set()
    for sq, piece in occupied.items():
        if piece == "B":
            attacked_by_white |= bishop_attacks(sq, occupied_squares)
        elif piece == "P":
            attacked_by_white |= pawn_attacks(sq, True)
        elif piece == "K":
            for df in (-1, 0, 1):
                for dr in (-1, 0, 1):
                    attacked_by_white.add((sq[0] + df, sq[1] + dr))
    if black_king in attacked_by_white:
        return None

    rows = []
    for rank_index in range(7, -1, -1):
        row = ""
        empty = 0
        for file_index in range(8):
            piece = occupied.get((file_index, rank_index))
            if piece is None:
                empty += 1
                continue
            if empty:
                row += str(empty)
                empty = 0
            row += piece
        if empty:
            row += str(empty)
        rows.append(row)
    return "/".join(rows) + " w - - 0 1"


def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    rng = random.Random(20260811)
    seen = set()
    while len(seen) < count:
        fen = make_position(rng)
        if fen is not None and fen not in seen:
            seen.add(fen)
            print(fen)


if __name__ == "__main__":
    main()
