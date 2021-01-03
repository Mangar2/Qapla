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
 */

#include "board.h"

using namespace ChessBasics;

Board::Board() {
	clear();
}

void Board::clear() {
	basicBoard.clear();
	clearBB();
	pieceSignature.clear();
	materialBalance.clear();

	kingSquares[WHITE] = E1;
	kingSquares[BLACK] = E8;
}


void Board::setToSymetricBoard(const Board& board) {
	clear();
	for (Square square = A1; square <= H8; ++square) {
		if (board[square] != NO_PIECE) {
			addPiece(Square(square ^ 0x38), Piece(operator[](square) ^ 1));
		}
	}
	setCastlingRight(WHITE, true, isKingSideCastleAllowed<WHITE>());
	setCastlingRight(WHITE, false, isQueenSideCastleAllowed<WHITE>());
	setCastlingRight(BLACK, true, isKingSideCastleAllowed<BLACK>());
	setCastlingRight(BLACK, false, isQueenSideCastleAllowed<BLACK>());
	basicBoard.setEP(Square(getEP() ^ 0x38));
	setWhiteToMove(isWhiteToMove());
}

void Board::removePiece(Square squareOfPiece) {
	Piece pieceToRemove = basicBoard[squareOfPiece];
	removePieceBB(squareOfPiece, pieceToRemove);
	basicBoard.removePiece(squareOfPiece);
	pieceSignature.removePiece(pieceToRemove, bitBoardsPiece[pieceToRemove]);
	materialBalance.removePiece(pieceToRemove);
}

void Board::addPiece(Square squareOfPiece, Piece pieceToAdd) {
	pieceSignature.addPiece(pieceToAdd, bitBoardsPiece[pieceToAdd]);
	addPieceBB(squareOfPiece, pieceToAdd);
	basicBoard.addPiece(squareOfPiece, pieceToAdd);
	materialBalance.addPiece(pieceToAdd);
}

void Board::movePiece(Square departure, Square destination) {
	Piece pieceToMove = basicBoard[departure];
	if (isKing(pieceToMove)) {
		kingSquares[getPieceColor(pieceToMove)] = destination;
	}
	movePieceBB(departure, destination, pieceToMove);
	basicBoard.movePiece(departure, destination);
}

void Board::doMoveSpecialities(Move move) {
	
	Square destination = move.getDestination();
	switch (move.getActionAndMovingPiece())
	{
	case Move::WHITE_PROMOTE:
	case Move::BLACK_PROMOTE:
		removePiece(destination);
		addPiece(destination, move.getPromotion());
		break;
	case Move::WHITE_EP:
		removePiece(destination + SOUTH);
		break;
	case Move::BLACK_EP:
		removePiece(destination + NORTH);
		break;
	case Move::WHITE_CASTLES_KING_SIDE:
		if (basicBoard.kingRookStartSquare[WHITE] != F1) {
			movePiece(basicBoard.kingRookStartSquare[WHITE], F1);
		}
		break;
	case Move::WHITE_CASTLES_QUEEN_SIDE:
		if (basicBoard.queenRookStartSquare[WHITE] != D1) {
			movePiece(basicBoard.queenRookStartSquare[WHITE], D1);
		}
		break;
	case Move::BLACK_CASTLES_KING_SIDE:
		if (basicBoard.kingRookStartSquare[BLACK] != F8) {
			movePiece(basicBoard.kingRookStartSquare[BLACK], F8);
		}
		break;
	case Move::BLACK_CASTLES_QUEEN_SIDE:
		if (basicBoard.queenRookStartSquare[BLACK] != D8) {
			movePiece(basicBoard.queenRookStartSquare[BLACK], D8);
		}
		break;
	}
}

void Board::doMove(Move move) {
	assert(basicBoard.assertMove(move));

	Square departure = move.getDeparture();
	Square destination = move.getDestination();
	basicBoard.updateStateOnDoMove(departure, destination);

	if (move.isCaptureMoveButNotEP())
	{
		removePiece(destination);
	}
	movePiece(departure, destination);

	if (move.getAction() != 0) {
		doMoveSpecialities(move);
	}

	assert(basicBoard[departure] == NO_PIECE || move.isCastleMove());
	assert(basicBoard[destination] != NO_PIECE);

}

void Board::undoMoveSpecialities(Move move) {
	Square destination = move.getDestination();

	switch (move.getActionAndMovingPiece())
	{
	case Move::WHITE_PROMOTE:
		removePiece(destination);
		addPiece(destination, WHITE_PAWN);
		break;
	case Move::BLACK_PROMOTE:
		removePiece(destination);
		addPiece(destination, BLACK_PAWN);
		break;
	case Move::WHITE_EP:
		addPiece(destination + SOUTH, BLACK_PAWN);
		break;
	case Move::BLACK_EP:
		addPiece(destination + NORTH, WHITE_PAWN);
		break;
	case Move::WHITE_CASTLES_KING_SIDE:
		removePiece(G1);
		if (basicBoard.kingRookStartSquare[WHITE] != F1) {
			movePiece(F1, basicBoard.kingRookStartSquare[WHITE]);
		}
		addPiece(basicBoard.kingStartSquare[WHITE], WHITE_KING);
		kingSquares[WHITE] = basicBoard.kingStartSquare[WHITE];
		break;
	case Move::BLACK_CASTLES_KING_SIDE:
		removePiece(G8);
		if (basicBoard.kingRookStartSquare[BLACK] != F8) {
			movePiece(F8, basicBoard.kingRookStartSquare[BLACK]);
		}
		addPiece(basicBoard.kingStartSquare[BLACK], BLACK_KING);
		kingSquares[BLACK] = basicBoard.kingStartSquare[BLACK];
		break;

	case Move::WHITE_CASTLES_QUEEN_SIDE:
		removePiece(C1);
		if (basicBoard.queenRookStartSquare[WHITE] != D1) {
			movePiece(D1, basicBoard.queenRookStartSquare[WHITE]);
		}
		addPiece(basicBoard.kingStartSquare[WHITE], WHITE_KING);
		kingSquares[WHITE] = basicBoard.kingStartSquare[WHITE];
		break;

	case Move::BLACK_CASTLES_QUEEN_SIDE:
		removePiece(C8);
		if (basicBoard.queenRookStartSquare[BLACK] != D8) {
			movePiece(D8, basicBoard.queenRookStartSquare[BLACK]);
		}
		addPiece(basicBoard.kingStartSquare[BLACK], BLACK_KING);
		kingSquares[BLACK] = basicBoard.kingStartSquare[BLACK];
		break;
	}

}


void Board::undoMove(Move move, BoardState boardState) {

	Square departure = move.getDeparture();
	Square destination = move.getDestination();
	Piece capture = move.getCapture();
	static uint64_t amount = 0;
	if (move.getAction() != 0) {
		undoMoveSpecialities(move);
	} 

	if (!move.isCastleMove())
	{
		movePiece(destination, departure);
		if (move.isCaptureMoveButNotEP()) {
			addPiece(destination, capture);
		}
	}

	basicBoard.updateStateOnUndoMove(boardState);
	assert(basicBoard[departure] != NO_PIECE);
}
