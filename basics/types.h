/**
 * @license
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Volker Böhm
 * @copyright Copyright (c) 2021 Volker Böhm
 * @Overview
 * Defines basic types for a chess engine like piece and move
 */

#ifndef __TYPES_H
#define __TYPES_H

#include <string>
#include <iostream>
#include <iomanip>

using namespace std;

namespace ChessBasics {

	typedef int32_t square_t;
	typedef uint64_t bitBoard_t;

	/**
	 * Chess board Squares
	 */
	enum Square {
		A1, B1, C1, D1, E1, F1, G1, H1,
		A2, B2, C2, D2, E2, F2, G2, H2,
		A3, B3, C3, D3, E3, F3, G3, H3,
		A4, B4, C4, D4, E4, F4, G4, H4,
		A5, B5, C5, D5, E5, F5, G5, H5,
		A6, B6, C6, D6, E6, F6, G6, H6,
		A7, B7, C7, D7, E7, F7, G7, H7,
		A8, B8, C8, D8, E8, F8, G8, H8,
		BOARD_SIZE,
		NO_SQUARE,
		NORTH = 8,
		EAST = 1,
		SOUTH = -NORTH,
		WEST = -EAST,
		NE = NORTH + EAST,
		NW = NORTH + WEST,
		SE = SOUTH + EAST,
		SW = SOUTH + WEST
	};

	constexpr Square operator*(Square a, int32_t b) { return Square(int32_t(a) * b); }
	constexpr Square operator+(Square a, int32_t b) { return Square(int32_t(a) + b); }
	constexpr Square operator-(Square a, int32_t b) { return Square(int32_t(a) - b); }
	constexpr Square operator-(Square square) { return Square(-int32_t(square)); }
	constexpr bool operator<(Square a, Square b) { return int32_t(a) < int32_t(b); }
	inline Square operator^=(Square& square, int32_t a) { return square = Square(square ^ a); }
	inline Square& operator++(Square& square) { return square = Square(square + 1); }
	inline Square& operator--(Square& square) { return square = Square(square - 1); }
	inline Square& operator+=(Square& a, int32_t b) { return a = Square(a + b); }

	/**
	 * Names of chess board files
	 */
	enum class File {
		A, B, C, D, E, F, G, H, COUNT, NONE
	};

	constexpr File operator+(File a, int32_t b) { return File(int32_t(a) + b); }
	constexpr File operator-(File a, int32_t b) { return File(int32_t(a) - b); }
	constexpr File operator-(File a, File b) { return File(int32_t(a) - int32_t(b)); }
	inline File& operator++(File& file) { return file = File(file + 1);  }
	inline File& operator--(File& file) { return file = File(file - 1); }
	inline File& operator+=(File& a, int32_t b) { return a = File(a + b); }

	/**
	 * Names of chess board ranks
	 */
	enum class Rank {
		R1, R2, R3, R4, R5, R6, R7, R8, COUNT, NONE
	};

	constexpr Rank operator+(Rank a, int32_t b) { return Rank(int32_t(a) + b); }
	constexpr Rank operator-(Rank a, int32_t b) { return Rank(int32_t(a) - b); }
	constexpr Rank operator-(Rank a, Rank b) { return Rank(int32_t(a) - int32_t(b)); }
	inline Rank& operator++(Rank& rank) { return rank = Rank(rank + 1); }
	inline Rank& operator--(Rank& rank) { return rank = Rank(rank - 1); }
	inline Rank& operator+=(Rank& a, int32_t b) { return a = Rank(a + b); }

	/**
	 * Gets the rank of a square
	 */
	static Rank getRank(Square square) {
		return Rank(square / NORTH);
	}

	/**
	 * Gets the opposit rank of a square
	 */
	static Rank getOppositRank(Square square) {
		return Rank::R8 - Square(getRank(square));
	}

	/**
	 * Gets the file of a square
	 */
	static File getFile(Square square) {
		return File(square % NORTH);
	}

	/**
	 * Calculates a square from rank and file
	 */
	static Square computeSquare(File file, Rank rank) {
		return Square(Square(file) + Square(rank) * NORTH);
	}

	/**
	 * Checks, wether a square is inside the board
	 */
	static bool isInBoard(Square square)
	{
		return (square >= A1) && (square <= H8);
	}

	/**
	 * Checks, wether a rank is inside the board
	 */
	static bool isRankInBoard(Rank rank)
	{
		return (rank >= Rank::R1) && (rank <= Rank::R8);
	}

	/**
	 * Checks, wether a file is inside the board
	 */
	static bool isFileInBoard(File file)
	{
		return (file >= File::A) && (file <= File::H);
	}

	/**
	 * Checks, wether file and rank is inside the board
	 */
	static bool isInBoard(Rank rank, File file)
	{
		return isRankInBoard(rank) && isFileInBoard(file);
	}

	/**
	 * Chess piece definition
	 */
	enum Piece {
		NO_PIECE = 0x00,
		WHITE = 0x00,
		BLACK = 0x01,
		PAWN = 0x02,
		KNIGHT = 0x04,
		BISHOP = 0x06,
		ROOK = 0x08,
		QUEEN = 0x0A,
		KING = 0x0C,
		WHITE_PAWN = PAWN + WHITE,
		BLACK_PAWN = PAWN + BLACK,
		WHITE_KNIGHT = KNIGHT + WHITE,
		BLACK_KNIGHT = KNIGHT + BLACK,
		WHITE_BISHOP = BISHOP + WHITE,
		BLACK_BISHOP = BISHOP + BLACK,
		WHITE_ROOK = ROOK + WHITE,
		BLACK_ROOK = ROOK + BLACK,
		WHITE_QUEEN = QUEEN + WHITE,
		BLACK_QUEEN = QUEEN + BLACK,
		WHITE_KING = KING + WHITE,
		BLACK_KING = KING + BLACK,
		MIN_PIECE = WHITE_PAWN,
		PIECE_AMOUNT = BLACK_KING + 1,
		PIECE_MASK = 0x0F,
		COLOR_AMOUNT = 0x02,
		COLOR_MASK = 0x01
	};
	
	constexpr Piece operator+(Piece a, int32_t b) { return Piece(int32_t(a) + b); }
	constexpr Piece operator-(Piece a, int32_t b) { return Piece(int32_t(a) - b); }
	inline Piece& operator++(Piece& piece) { return piece = Piece(piece + 1); }
	inline Piece& operator+=(Piece& a, int32_t b) { return a = Piece(a + b); }
	inline Piece& operator-=(Piece& a, int32_t b) { return a = Piece(a - b); }

	/**
	 * Checks, if a piece is a pawn
	 */
	constexpr auto isPawn(Piece piece) { return (piece & ~COLOR_MASK) == PAWN; }

	/**
     * Checks, if a piece is a pawn
     */
	constexpr auto isKing(Piece piece) { return (piece & ~COLOR_MASK) == KING; }

	/**
	 * Gets the color of a piece (WHITE or BLACK)
	 */
	constexpr Piece getPieceColor(Piece piece) {
		return Piece(piece & COLOR_MASK);
	}

	/**
	 * Switches the color of a pice (BLACK to WHITE | WHITE to BLACK)
	 */
	constexpr Piece switchColor(Piece piece) {
		return Piece(piece ^ COLOR_MASK);
	}

	/**
	 * Gets the type of piece by removing the color information
	 */
	constexpr Piece getPieceType(Piece piece) {
		return Piece(piece & ~COLOR_MASK);
	}

	/**
	 * Computes the string representation of a board square
	 * @param square Square in internal representation
	 */
	static auto squareToString(square_t square) {
		std::string result = "?";
		result = "";
		if (square >= Square::A1 && square <= Square::H8) {
			result += ('a' + (square % NORTH));
			result += ('1' + (square / NORTH));
		}
		return result;
	}

	/**
	 * Computes the internal representation of a board square from a string
	 * @param squareAsString standard chess notation of a square
	 * @expampe stringToSquare("e1")
	 */
	static auto stringToSquare(std::string squareAsString) {
		square_t result =
			squareAsString[0] * EAST +
			squareAsString[1] * NORTH;
		return result;
	}

	/**
	 * Computes the char representation of a chess piece
	 */
	static auto pieceToChar(Piece piece) {
		switch (piece)
		{
		case BLACK_PAWN: return 'p';
		case BLACK_KNIGHT: return 'n';
		case BLACK_BISHOP: return 'b';
		case BLACK_ROOK: return 'r';
		case BLACK_QUEEN: return 'q';
		case BLACK_KING: return 'k';
		case WHITE_PAWN: return 'P';
		case WHITE_KNIGHT: return 'N';
		case WHITE_BISHOP: return 'B';
		case WHITE_ROOK: return 'R';
		case WHITE_QUEEN: return 'Q';
		case WHITE_KING: return 'K';
		default:return '.';
		}
	}

	/**
	 * Computes the piece number from a piece symbol
	 */
	static auto charToPiece(char piece) {
		switch (piece)
		{
		case 'p':return BLACK_PAWN;
		case 'n':return BLACK_KNIGHT;
		case 'b':return BLACK_BISHOP;
		case 'r':return BLACK_ROOK;
		case 'q':return BLACK_QUEEN;
		case 'k':return BLACK_KING;
		case 'P':return WHITE_PAWN;
		case 'N':return WHITE_KNIGHT;
		case 'B':return WHITE_BISHOP;
		case 'R':return WHITE_ROOK;
		case 'Q':return WHITE_QUEEN;
		case 'K':return WHITE_KING;
		default:return NO_PIECE;
		}
	}

	/**
	 * Converts a Color to a string
	 */
	static string colorToString(Piece color) {
		return color == WHITE ? "White" : "Black";
	}

	/**
	 * Computes the piece symbol for a promotion used for LAN (Long Algebraic Notation)
	 * @example e2e1q
	 */
	static char pieceToPromoteChar(Piece piece) {
		switch (piece)
		{
		case WHITE_KNIGHT:
		case BLACK_KNIGHT:
			return 'n';
		case WHITE_BISHOP:
		case BLACK_BISHOP:
			return 'b';
		case WHITE_ROOK:
		case BLACK_ROOK:
			return 'r';
		case WHITE_QUEEN:
		case BLACK_QUEEN:
			return 'q';
		default:
			return ' ';
		}
	}

}

#endif __TYPES_H
