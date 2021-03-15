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
#include "pst.h"

using namespace ChessBasics;

Board::Board() {
	clear();
}

void Board::clear() {
	_basicBoard.clear();
	clearBB();
	_pieceSignature.clear();
	_materialBalance.clear();
	_pstBonus = 0;
	kingSquares[WHITE] = E1;
	kingSquares[BLACK] = E8;
}


void Board::setToSymetricBoard(const Board& board) {
	clear();
	for (Square square = A1; square <= H8; ++square) {
		const Piece piece = board[square];
		if (piece != NO_PIECE) {
			setPiece(Square(square ^ 0x38), Piece(piece ^ 1));
		}
	}
	setCastlingRight(WHITE, true, isKingSideCastleAllowed<WHITE>());
	setCastlingRight(WHITE, false, isQueenSideCastleAllowed<WHITE>());
	setCastlingRight(BLACK, true, isKingSideCastleAllowed<BLACK>());
	setCastlingRight(BLACK, false, isQueenSideCastleAllowed<BLACK>());
	_basicBoard.setEP(Square(getEP() ^ 0x38));
	setWhiteToMove(!board.isWhiteToMove());
}

void Board::removePiece(Square squareOfPiece) {
	Piece pieceToRemove = _basicBoard[squareOfPiece];
	removePieceBB(squareOfPiece, pieceToRemove);
	_basicBoard.removePiece(squareOfPiece);
	_pieceSignature.removePiece(pieceToRemove, bitBoardsPiece[pieceToRemove]);
	_materialBalance.removePiece(pieceToRemove);
	_pstBonus -= PST::getValue(squareOfPiece, pieceToRemove);
}

void Board::addPiece(Square squareOfPiece, Piece pieceToAdd) {
	_pieceSignature.addPiece(pieceToAdd, bitBoardsPiece[pieceToAdd]);
	addPieceBB(squareOfPiece, pieceToAdd);
	_basicBoard.addPiece(squareOfPiece, pieceToAdd);
	_materialBalance.addPiece(pieceToAdd);
	_pstBonus += PST::getValue(squareOfPiece, pieceToAdd);
}

void Board::movePiece(Square departure, Square destination) {
	Piece pieceToMove = _basicBoard[departure];
	if (isKing(pieceToMove)) {
		kingSquares[getPieceColor(pieceToMove)] = destination;
	}
	_pstBonus += PST::getValue(destination, pieceToMove) -
		PST::getValue(departure, pieceToMove);
	movePieceBB(departure, destination, pieceToMove);
	_basicBoard.movePiece(departure, destination);
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
		if (_basicBoard.kingRookStartSquare[WHITE] != F1) {
			movePiece(_basicBoard.kingRookStartSquare[WHITE], F1);
		}
		break;
	case Move::WHITE_CASTLES_QUEEN_SIDE:
		if (_basicBoard.queenRookStartSquare[WHITE] != D1) {
			movePiece(_basicBoard.queenRookStartSquare[WHITE], D1);
		}
		break;
	case Move::BLACK_CASTLES_KING_SIDE:
		if (_basicBoard.kingRookStartSquare[BLACK] != F8) {
			movePiece(_basicBoard.kingRookStartSquare[BLACK], F8);
		}
		break;
	case Move::BLACK_CASTLES_QUEEN_SIDE:
		if (_basicBoard.queenRookStartSquare[BLACK] != D8) {
			movePiece(_basicBoard.queenRookStartSquare[BLACK], D8);
		}
		break;
	}
}

void Board::doMove(Move move) {
	assert(_basicBoard.assertMove(move));

	Square departure = move.getDeparture();
	Square destination = move.getDestination();
	_basicBoard.updateStateOnDoMove(departure, destination);

	if (move.isCaptureMoveButNotEP())
	{
		removePiece(destination);
	}
	movePiece(departure, destination);

	if (move.getAction() != 0) {
		doMoveSpecialities(move);
	}

	assert(_basicBoard[departure] == NO_PIECE || move.isCastleMove());
	assert(_basicBoard[destination] != NO_PIECE);

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
		if (_basicBoard.kingRookStartSquare[WHITE] != F1) {
			movePiece(F1, _basicBoard.kingRookStartSquare[WHITE]);
		}
		addPiece(_basicBoard.kingStartSquare[WHITE], WHITE_KING);
		kingSquares[WHITE] = _basicBoard.kingStartSquare[WHITE];
		break;
	case Move::BLACK_CASTLES_KING_SIDE:
		removePiece(G8);
		if (_basicBoard.kingRookStartSquare[BLACK] != F8) {
			movePiece(F8, _basicBoard.kingRookStartSquare[BLACK]);
		}
		addPiece(_basicBoard.kingStartSquare[BLACK], BLACK_KING);
		kingSquares[BLACK] = _basicBoard.kingStartSquare[BLACK];
		break;

	case Move::WHITE_CASTLES_QUEEN_SIDE:
		removePiece(C1);
		if (_basicBoard.queenRookStartSquare[WHITE] != D1) {
			movePiece(D1, _basicBoard.queenRookStartSquare[WHITE]);
		}
		addPiece(_basicBoard.kingStartSquare[WHITE], WHITE_KING);
		kingSquares[WHITE] = _basicBoard.kingStartSquare[WHITE];
		break;

	case Move::BLACK_CASTLES_QUEEN_SIDE:
		removePiece(C8);
		if (_basicBoard.queenRookStartSquare[BLACK] != D8) {
			movePiece(D8, _basicBoard.queenRookStartSquare[BLACK]);
		}
		addPiece(_basicBoard.kingStartSquare[BLACK], BLACK_KING);
		kingSquares[BLACK] = _basicBoard.kingStartSquare[BLACK];
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
		assert(_basicBoard[destination] == move.getMovingPiece());
		movePiece(destination, departure);
		if (move.isCaptureMoveButNotEP()) {
			addPiece(destination, capture);
		}
	}

	_basicBoard.updateStateOnUndoMove(boardState);
	assert(_basicBoard[departure] != NO_PIECE);
}

string Board::getFen() const {
	string result = "";
	File file;
	Rank rank;
	int amoutOfEmptyFields;
	for (rank = Rank::R8; rank >= Rank::R1; --rank)
	{
		amoutOfEmptyFields = 0;
		for (file = File::A; file <= File::H; ++file)
		{
			Square square = computeSquare(file, rank);
			Piece piece = operator[](square);
			if (piece == Piece::NO_PIECE) amoutOfEmptyFields++;
			else
			{
				if (amoutOfEmptyFields > 0) {
					result += std::to_string(amoutOfEmptyFields);
				}
				result.push_back(pieceToChar(piece));
				amoutOfEmptyFields = 0;
			}
		}
		if (amoutOfEmptyFields > 0) {
			result += std::to_string(amoutOfEmptyFields);
		}
		if (rank > Rank::R1) {
			result.push_back('/');
		}
	}

	result += isWhiteToMove()? " w" : " b";
	/*
	if (IsWKCastleAllowed()) printf("K");
	if (IsWQCastleAllowed()) printf("Q");
	if (IsBKCastleAllowed()) printf("k");
	if (IsBQCastleAllowed()) printf("q");

	if (!IsWKCastleAllowed() && !IsWQCastleAllowed() &&
	!IsBKCastleAllowed() && !IsBQCastleAllowed())
	printf("-");

	if (mEP == cNoPos)
	{
	printf(" -");
	}
	else
	{
	aRes = aRes + " " + char('a' + char(Col(mEP)));
	printf(" %c",
	if (mWtm) aRes = aRes + '6';
	else aRes = aRes + '3';
	}
	aRes = aRes + " " + mNonPermChangeAmount + " " +
	(mHalfmovesAmount / 2 + 1);
	*/
	return result;
}


void Board::printFen() const {
	cout << getFen() << endl;
}

void Board::print() const {
	for (Rank rank = Rank::R8; rank >= Rank::R1; --rank) {
		for (File file = File::A; file <= File::H; ++file) {
			Piece piece = operator[](computeSquare(file, rank));
			printf(" %c ", pieceToChar(piece));
		}
		printf("\n");
	}
	printf("hash: %llu\n", computeBoardHash());
	printFen();
	//printf("White King: %ld, Black King: %ld\n", kingPos[WHITE], kingPos[BLACK]);
}
