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
 * List of pieces in a bitbase. Pieces can be added/deleted and sorted
 */

#ifndef __PIECELIST_H
#define __PIECELIST_H

#include "../movegenerator/movegenerator.h"
#include "../movegenerator/bitboardmasks.h"
#include <ctype.h>
#include <vector>
#include <string>

using namespace std;
using namespace QaplaMoveGenerator;

namespace QaplaBitbase {

	class PieceList {
	public:
		PieceList() {
			clear();
		}

		/**
		 * Creates a list of pieces initialized by a piece string
		 * @param pieceString: String with the list of current pieces.
		 * @example PieceList("KRPKP") for king with rook and pawn vs. King with pawn
		 */
		PieceList(string pieceString) {
			clear();
			bool whitePiece = true;
			addPiece(WHITE_KING);
			addPiece(BLACK_KING);
			if (pieceString[0] == 'K') {
				for (uint32_t stringPos = 1; pieceString[stringPos] != 0; stringPos++) {
					if (pieceString[stringPos] == 'K') {
						whitePiece = false;
					}
					else {
						Piece piece = getPieceType(charToPiece(pieceString[stringPos]));
						piece += (whitePiece ? WHITE : BLACK);
						addPiece(piece);
					}
				}
			}
			bubbleSort<true>();
		}

		/**
		 * Creates a new piece list initialized by a board position
		 * @param board current board position
		 */
		PieceList(const Board& position) {
			clear();
			addPiecesFromBitBoard(position.getPieceBB(WHITE_KING), WHITE_KING);
			addPiecesFromBitBoard(position.getPieceBB(BLACK_KING), BLACK_KING);
			for (Piece piece = WHITE_PAWN; piece <= BLACK_QUEEN; ++piece) {
				addPiecesFromBitBoard(position.getPieceBB(piece), piece);
			}
		}

		/**
		 * Clears the piece list
		 */
		void clear() {
			_numberOfPieces = 0;
			_numberOfPawns = 0;
		}

		/**
		 * Gets the piece string of one color
		 */
		template<Piece COLOR>
		string getPieceStringOfColor() {
			string result = "K";
			const uint32_t indexWithoutKings = 2;
			assert(getNumberOfPieces() >= 2);
			for (uint32_t pieceNo = getNumberOfPieces() - 1; pieceNo >= indexWithoutKings; pieceNo--) {
				if (getPieceColor(getPiece(pieceNo)) == COLOR) {
					result += static_cast<char>(toupper(pieceToChar(getPieceType(getPiece(pieceNo)))));
				}
			}
			return result;
		}

		/**
		 * Gets the full piece string
		 */
		string getPieceString() {
			string result = getPieceStringOfColor<WHITE>() + getPieceStringOfColor<BLACK>();
			return result;
		}

		/**
		 * Adds a piece to the list
		 */
		void addPiece(Piece piece) {
			_pieces[_numberOfPieces] = piece;
			_numberOfPieces++;
			if (isPawn(piece)) {
				_numberOfPawns++;
			}
		}

		void addPiece(Piece piece, Square square) {
			_pieceSquares[_numberOfPieces] = square;
			addPiece(piece);
		}

		/**
		 * Removes a piece from the list
		 */
		void removePiece(uint32_t pieceNo) {
			if (pieceNo < _numberOfPieces) {
				_numberOfPieces--;
				if (isPawn(_pieces[pieceNo])) {
					_numberOfPawns--;
				}
				for (; pieceNo < _numberOfPieces; pieceNo++) {
					_pieces[pieceNo] = _pieces[pieceNo + 1];
				}
			}
		}

		/**
		 * Promotes a pawn -> removing the pawn from the list and add the
		 * promoted piece to the list
		 */
		void promotePawn(uint32_t pieceNo, Piece promotePieceType = QUEEN) {
			if (pieceNo < _numberOfPieces && isPawn(_pieces[pieceNo])) {
				_pieces[pieceNo] += promotePieceType - PAWN;
				_numberOfPawns--;
				bubbleSort<true>();
			}
		}

		/**
		 * Switches the side -> black to white
		 */
		void toSymetric() {
			for (uint32_t pieceNo = 0; pieceNo < getNumberOfPieces(); ++pieceNo) {
				_pieces[pieceNo] = switchColor(_pieces[pieceNo]);
				_pieceSquares[pieceNo] = switchSide(_pieceSquares[pieceNo]);
			}
			// White king to index 0, Black king to index 1
			swap<false>(0, 1);
			// Bring black pieces behind white pieces
			bubbleSort<false>();
		}

		/**
		 * Gets a piece
		 */
		Piece getPiece(uint32_t pieceNo) const {
			return pieceNo < _numberOfPieces ? _pieces[pieceNo] : NO_PIECE;
		}

		/**
		 * Gets the Square of a piece
	      */
		const Square getSquare(uint32_t pieceNo) const {
			return pieceNo < _numberOfPieces ? _pieceSquares[pieceNo] : NO_SQUARE;
		}

		/**
		 * Sets the square of a piece
		 */
		void setSquare(uint32_t pieceNo, Square square) {
			_pieceSquares[pieceNo] = square;
		}

		/**
		 * Gets the number of pawns
		 */
		uint32_t getNumberOfPawns() const { return _numberOfPawns; }

		/**
		 * Gets the number of pieces (including pawns)
		 */
		uint32_t getNumberOfPieces() const { return _numberOfPieces; }

		/** 
		 * Gets the number of identical pieces (i.e. white queens) starting at the given position
		 */
		uint32_t getNumberOfSamePieces(uint32_t pieceNo) const { 
			Piece piece = getPiece(pieceNo);
			uint32_t samePieceCount = 1;
			for (pieceNo++; pieceNo < _numberOfPieces && piece == getPiece(pieceNo); pieceNo++) {
				samePieceCount++;
			}
			return samePieceCount; 
		}

		/**
		 * Gets the number of pieces without pawns
		 */
		uint32_t getNumberOfPiecesWithoutPawns() const { return _numberOfPieces - _numberOfPawns; }

	private:
		/**
		 * Adds all pieces flagged with '1' on a bitboard
		 */
		void addPiecesFromBitBoard(bitBoard_t piecesBitBoard, Piece piece) {
			for (; piecesBitBoard != 0; piecesBitBoard &= piecesBitBoard - 1) {
				_pieceSquares[_numberOfPieces] = (lsb(piecesBitBoard));
				addPiece(piece);
			}
		}

		/**
		 * Swap two pieces and squares in the piece list
		 */
		template <bool ONLY_PIECES>
		void swap(uint32_t index1, uint32_t index2) {
			Piece piece = _pieces[index1];
			_pieces[index1] = _pieces[index2];
			_pieces[index2] = piece;
			if (!ONLY_PIECES) {
				Square square = _pieceSquares[index1];
				_pieceSquares[index1] = _pieceSquares[index2];
				_pieceSquares[index2] = square;
			}
		}

		/**
		 * Sort the pieces
		 */
		template <bool ONLY_PIECES>
		void bubbleSort() {
			if (_numberOfPieces == 0) return;
			const int32_t indexWithoutKings = 2;
			for (int32_t outerLoop = _numberOfPieces - 1; outerLoop > indexWithoutKings; outerLoop--) {
				for (int32_t innerLoop = indexWithoutKings + 1; innerLoop <= outerLoop; innerLoop++) {
					if (_pieces[innerLoop - 1] > _pieces[innerLoop]) {
						swap<ONLY_PIECES>(innerLoop - 1, innerLoop);
					}
				}
			}
		}

		uint32_t _numberOfPieces;
		uint32_t _numberOfPawns;
		static const uint32_t MAX_PIECES_COUNT = 10;
		array<Piece, MAX_PIECES_COUNT> _pieces{};
		array<Square, MAX_PIECES_COUNT> _pieceSquares{};
	};

}

#endif // __PIECELIST_H