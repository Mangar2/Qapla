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
 * Implements basic algorithms
 * doMove
 * undoMove
 */

#ifndef __BOARD_H
#define __BOARD_H

#include "types.h"
#include "move.h"
#include "basicboard.h"
#include "boardstate.h"
#include "piecesignature.h"
#include "materialbalance.h"

namespace ChessBasics {

	class Board {
	public:
		Board();
		/**
		 * Sets a move on the board
		 */
		void doMove(Move move);
		/*
		 * Undoes a previously made move on the move
		 * @param move move previously made
		 * @param boardState a stored state from the board before doing the move incl. EP-Position
		 */
		void undoMove(Move move, BoardState boardState);
		void clear();
		inline auto getEP() const { return _basicBoard.getEP(); }
		inline auto operator[](Square square) const { return _basicBoard[square]; }
		inline auto isWhiteToMove() const { return _basicBoard.whiteToMove; }
		inline void setWhiteToMove(bool whiteToMove) { _basicBoard.whiteToMove = whiteToMove; }


		/**
		 * Checks, if two positions are identical
		 */
		bool isIdenticalPosition(const Board& boardToCompare) {
			return _basicBoard.isIdenticalPosition(boardToCompare._basicBoard);
		}

		/**
		 * Creates a symetric board exchanging black/white side
		 */
		void setToSymetricBoard(const Board& board);

		/**
		 * Sets a nullmove on the board. A nullmove is a non legal chess move where the 
		 * person to move does nothing and hand over the moving right to the opponent.
		 */
		inline void doNullmove() {
			_basicBoard.clearEP();
			setWhiteToMove(!isWhiteToMove());
		}

		/*
		 * Undoes a previously made nullmove
		 * @param boardState a stored state from the board before doing the nullmove incl. EP-Position
		 */
		inline void undoNullmove(BoardState boardState) {
			setWhiteToMove(!isWhiteToMove());
			_basicBoard.boardState = boardState;
		}

		/**
		 * Checks, if king side castling is allowed
		 */
		template <Piece COLOR>
		inline bool isKingSideCastleAllowed() {
			return _basicBoard.isKingSideCastleAllowed<COLOR>();
		}

		/**
		 * Checks, if queen side castling is allowed
		 */
		template <Piece COLOR>
		inline bool isQueenSideCastleAllowed() {
			return _basicBoard.isQueenSideCastleAllowed<COLOR>();
		}

		/**
		 * Enable/Disable castling right
		 */
		void setCastlingRight(Piece color, bool kingSide, bool allow) {
			_basicBoard.setCastlingRight(color, kingSide, allow);
		}

		/**
		 * Sets the destination of the pawn to capture
		 */
		void setEP(Square destination) {
			_basicBoard.setEP(destination);
		}

		/**
		 * Sets a piece to the board adjusting all state variables
		 */
		void setPiece(Square square, Piece piece) {
			addPiece(square, piece);
			if (piece == WHITE_KING) {
				kingSquares[WHITE] = square;
			}
			if (piece == BLACK_KING) {
				kingSquares[BLACK] = square;
			}
		}

		/**
		 * Computes the hash value of the current board
		 * @returns board hash for the current position
		 */
		inline auto computeBoardHash() const {
			return _basicBoard.computeBoardHash();
		}

		/**
		 * Gets the amount of half moves without pawn move or capture to implement
		 * the 50-moves-draw rule
		 */
		inline auto getHalfmovesWithoutPawnMoveOrCapture() const {
			return _basicBoard.boardState.halfmovesWithoutPawnMoveOrCapture;
		}

		/**
		 * Sets the number of half moves without pawn move or capture
		 */
		void setHalfmovesWithoutPawnMoveOrCapture(uint16_t number) {
			_basicBoard.boardState.halfmovesWithoutPawnMoveOrCapture = number;
		}

		/**
		 * Is the position forced draw due to missing material (in any cases)?
		 * No side has any pawn, no side has more than either a Knight or a Bishop
		 */
		inline auto drawDueToMissingMaterial() const {
			return pieceSignature.drawDueToMissingMaterial();
		}

		/**
		 * Checks if a side has enough material to mate
		 */
		template<Piece COLOR> auto hasEnoughMaterialToMate() const {
			return pieceSignature.hasEnoughMaterialToMate<COLOR>();
		}

		/**
		 * Computes if futility pruning should be applied based on the captured piece
		 */
		inline auto doFutilityOnCapture(Piece capturedPiece) const {
			return pieceSignature.doFutilityOnCapture(capturedPiece);
		}

		/**
		 * Gets the signature of all pieces
		 */
		inline auto getPiecesSignature() const {
			return pieceSignature.getPiecesSignature();
		}

		/**
		 * Gets the absolute value of a piece
		 */
		inline auto getAbsolutePieceValue(Piece piece) const {
			return materialBalance.getAbsolutePieceValue(piece);
		}

		/**
		 * Gets the value of a piece
		 */
		inline auto getPieceValue(Piece piece) const {
			return materialBalance.getPieceValue(piece);
		}

		/**
		 * Gets the value of a piece used for move ordering
		 */
		inline auto getPieceValueForMoveSorting(Piece piece) const {
			return materialBalance.getPieceValueForMoveSorting(piece);
		}

		/**
		 * Gets the material balance value of the board
		 */
		inline auto getMaterialValue() const {
			return materialBalance.getMaterialValue();
		}

		/**
		 * Gets the material balance value of the board
		 * Positive, if the player to move has a better position
		 */
		inline auto getMaterialValue(bool whiteToMove) const {
			return materialBalance.getMaterialValue(whiteToMove);
		}

		/**
		 * Returns true, if the side to move has any range piece
		 */
		inline auto sideToMoveHasQueenRookBishop(bool whiteToMove) const {
			return pieceSignature.sideToMoveHasQueenRookBishop(whiteToMove);
		}

		/**
		 * Gets the bitboard of a piece type
		 */
		inline auto getPieceBB(Piece piece) const {
			return bitBoardsPiece[piece];
		}

		/**
		 * Gets the joint bitboard for all pieces
		 */
		inline auto getAllPiecesBB() const { return bitBoardAllPieces; }

		/**
		 * Gets the square of the king
		 */
		template <Piece COLOR>
		inline auto getKingSquare() const { return kingSquares[COLOR]; }

		/**
		 * Gets the start square of the king rook
		 */
		template <Piece COLOR>
		inline auto getKingRookStartSquare() const { return _basicBoard.kingRookStartSquare[COLOR]; }

		/**
		 * Gets the start square of the king rook
		 */
		template <Piece COLOR>
		inline auto getQueenRookStartSquare() const { return _basicBoard.queenRookStartSquare[COLOR]; }

		BoardState getBoardState() { return _basicBoard.boardState; }

		/**
		 * Prints the board as fen to std-out
		 */
		void printFen() const;

		/**
		 * Prints the board to std-out
		 */
		void print() const;

	protected:
		array<Square, COLOR_AMOUNT> kingSquares;

		array<bitBoard_t, PIECE_AMOUNT> bitBoardsPiece;
		array<bitBoard_t, COLOR_AMOUNT> bitBoardAllPiecesOfOneColor;
		bitBoard_t bitBoardAllPieces;

	private:
		BasicBoard _basicBoard;

		/**
		 * Clears the bitboards
		 */
		void clearBB() {
			bitBoardsPiece.fill(0);
			bitBoardAllPiecesOfOneColor.fill(0);
			bitBoardAllPieces = 0;
		}

		/**
		 * Moves a piece as part of a move
		 */
		void movePiece(Square departure, Square destination);

		/**
		 * Remove current piece as part of a move
	  	 */
		void removePiece(Square squareOfPiece);

		/**
		 * Adds a piece as part of a move (for example for promotions)
		 */
		void addPiece(Square squareOfPiece, Piece pieceToAdd);


		/**
		 * Removes a piece from the bitboards
		 */
		void removePieceBB(Square squareOfPiece, Piece pieceToRemove) {
			bitBoard_t clearMask = ~(1ULL << squareOfPiece);
			bitBoardsPiece[pieceToRemove] &= clearMask;
			bitBoardAllPiecesOfOneColor[getPieceColor(pieceToRemove)] &= clearMask;
			bitBoardAllPieces &= clearMask;
		}

		/**
		 * Adds a piece to the bitboards
		 */
		void addPieceBB(Square squareOfPiece, Piece pieceToAdd) {
			bitBoard_t setMask = (1ULL << squareOfPiece);
			bitBoardsPiece[pieceToAdd] |= setMask;
			bitBoardAllPiecesOfOneColor[getPieceColor(pieceToAdd)] |= setMask;
			bitBoardAllPieces |= setMask;
		}

		/**
		 * Moves a piece on the bitboards
		 */
		void movePieceBB(Square departure, Square destination, Piece pieceToMove) {
			bitBoard_t moveMask = (1ULL << destination) + (1ULL << departure);
			bitBoardsPiece[pieceToMove] ^= moveMask;
			bitBoardAllPiecesOfOneColor[getPieceColor(pieceToMove)] ^= moveMask;
			bitBoardAllPieces ^= moveMask;
		}

		/**
		 * handles EP, Castling, Promotion 
		 */
		void doMoveSpecialities(Move move);
		void undoMoveSpecialities(Move move);

		PieceSignature pieceSignature;
		MaterialBalance materialBalance;

	};
}

#endif __BOARD_H

