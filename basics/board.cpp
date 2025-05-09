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
 * @author Volker B�hm
 * @copyright Copyright (c) 2021 Volker B�hm
 */

#include <iomanip>
#include "bits.h"
#include "board.h"
#include "pst.h"

using namespace QaplaBasics;

Board::Board() {
	clear();
	initClearCastleMask();
}

void Board::clear() {
	clearBB();
	_pieceSignature.clear();
	_materialBalance.clear();
	_pstBonus = 0;
	kingSquares[WHITE] = E1;
	kingSquares[BLACK] = E8;
	_kingStartSquare = { E1, E8 };
	_queenRookStartSquare = { A1, A8 };
	_kingRookStartSquare = { H1, H8 };
	_startHalfmoves = 0;
	_boardState.initialize();
	_board.fill(NO_PIECE);
}

void Board::initClearCastleMask() {
	_clearCastleFlagMask.fill(0xFFFF);
	_clearCastleFlagMask[_queenRookStartSquare[WHITE]] = static_cast<uint16_t>(~BoardState::WHITE_QUEEN_SIDE_CASTLE_BIT);
	_clearCastleFlagMask[_kingRookStartSquare[WHITE]] = static_cast<uint16_t>(~BoardState::WHITE_KING_SIDE_CASTLE_BIT);
	_clearCastleFlagMask[_queenRookStartSquare[BLACK]] = static_cast<uint16_t>(~BoardState::BLACK_QUEEN_SIDE_CASTLE_BIT);
	_clearCastleFlagMask[_kingRookStartSquare[BLACK]] = static_cast<uint16_t>(~BoardState::BLACK_KING_SIDE_CASTLE_BIT);
	_clearCastleFlagMask[_kingStartSquare[WHITE]] =
		static_cast<uint16_t>(~(BoardState::WHITE_QUEEN_SIDE_CASTLE_BIT + BoardState::WHITE_KING_SIDE_CASTLE_BIT));
	_clearCastleFlagMask[_kingStartSquare[BLACK]] =
		static_cast<uint16_t>(~(BoardState::BLACK_QUEEN_SIDE_CASTLE_BIT + BoardState::BLACK_KING_SIDE_CASTLE_BIT));
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
	setEP(Square(getEP() ^ 0x38));
	setWhiteToMove(!board.isWhiteToMove());
}

void Board::removePiece(Square squareOfPiece) {
	Piece pieceToRemove = _board[squareOfPiece];
	removePieceBB(squareOfPiece, pieceToRemove);
	_boardState.updateHash(squareOfPiece, pieceToRemove);
	_board[squareOfPiece] = NO_PIECE;
	_pieceSignature.removePiece(pieceToRemove, bitBoardsPiece[pieceToRemove]);
	_materialBalance.removePiece(pieceToRemove);
	_pstBonus -= PST::getValue(squareOfPiece, pieceToRemove);
}

void Board::addPiece(Square squareOfPiece, Piece pieceToAdd) {
	_pieceSignature.addPiece(pieceToAdd);
	addPieceBB(squareOfPiece, pieceToAdd);
	_boardState.updateHash(squareOfPiece, pieceToAdd);
	_board[squareOfPiece] = pieceToAdd;
	_materialBalance.addPiece(pieceToAdd);
	_pstBonus += PST::getValue(squareOfPiece, pieceToAdd);
}

void Board::movePiece(Square departure, Square destination) {
	Piece pieceToMove = _board[departure];
	if (isKing(pieceToMove)) {
		kingSquares[getPieceColor(pieceToMove)] = destination;
	}
	_pstBonus += PST::getValue(destination, pieceToMove) -
		PST::getValue(departure, pieceToMove);
	movePieceBB(departure, destination, pieceToMove);
	_boardState.updateHash(departure, pieceToMove);
	_board[departure] = NO_PIECE;
	_boardState.updateHash(destination, pieceToMove);
	_board[destination] = pieceToMove;
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
		if (_kingRookStartSquare[WHITE] != F1) {
			movePiece(_kingRookStartSquare[WHITE], F1);
		}
		break;
	case Move::WHITE_CASTLES_QUEEN_SIDE:
		if (_queenRookStartSquare[WHITE] != D1) {
			movePiece(_queenRookStartSquare[WHITE], D1);
		}
		break;
	case Move::BLACK_CASTLES_KING_SIDE:
		if (_kingRookStartSquare[BLACK] != F8) {
			movePiece(_kingRookStartSquare[BLACK], F8);
		}
		break;
	case Move::BLACK_CASTLES_QUEEN_SIDE:
		if (_queenRookStartSquare[BLACK] != D8) {
			movePiece(_queenRookStartSquare[BLACK], D8);
		}
		break;
	}
}

void Board::doMove(Move move) {
	assert(assertMove(move));

	Square departure = move.getDeparture();
	Square destination = move.getDestination();
	updateStateOnDoMove(departure, destination);

	if (move.isCaptureMoveButNotEP())
	{
		removePiece(destination);
	}
	movePiece(departure, destination);

	if (move.getAction() != 0) {
		doMoveSpecialities(move);
	}

	assert(_board[departure] == NO_PIECE || move.isCastleMove());
	assert(_board[destination] != NO_PIECE);

}


/**
 * Update all based for doMove
 * @param departure departure position of the move
 * @param destination destination position of the move
 */
void Board::updateStateOnDoMove(Square departure, Square destination) {
	_whiteToMove = !_whiteToMove;
	_boardState.clearEP();
	_boardState.disableCastlingRightsByMask(
		_clearCastleFlagMask[departure] & _clearCastleFlagMask[destination]);
	_boardState.halfmovesWithoutPawnMoveOrCapture++;
	bool isCapture = _board[destination] != NO_PIECE;
	bool isPawnMove = isPawn(_board[departure]);
	bool isMoveTwoRanks = ((departure - destination) & 0x0F) == 0;
	if (isCapture || isPawnMove) {
		_boardState.halfmovesWithoutPawnMoveOrCapture = 0;
		_boardState.fenHalfmovesWithoutPawnMoveOrCapture = 0;
	}
	if (isPawnMove && isMoveTwoRanks) {
		_boardState.setEP(destination);
	}
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
		assert(_board[G1] == WHITE_KING);
		removePiece(G1);
		if (_kingRookStartSquare[WHITE] != F1) {
			movePiece(F1, _kingRookStartSquare[WHITE]);
		}
		addPiece(_kingStartSquare[WHITE], WHITE_KING);
		kingSquares[WHITE] = _kingStartSquare[WHITE];
		break;
	case Move::BLACK_CASTLES_KING_SIDE:
		assert(_board[G8] == BLACK_KING);
		removePiece(G8);
		if (_kingRookStartSquare[BLACK] != F8) {
			movePiece(F8, _kingRookStartSquare[BLACK]);
		}
		addPiece(_kingStartSquare[BLACK], BLACK_KING);
		kingSquares[BLACK] = _kingStartSquare[BLACK];
		break;

	case Move::WHITE_CASTLES_QUEEN_SIDE:
		assert(_board[C1] == WHITE_KING);
		removePiece(C1);
		if (_queenRookStartSquare[WHITE] != D1) {
			movePiece(D1, _queenRookStartSquare[WHITE]);
		}
		addPiece(_kingStartSquare[WHITE], WHITE_KING);
		kingSquares[WHITE] = _kingStartSquare[WHITE];
		break;

	case Move::BLACK_CASTLES_QUEEN_SIDE:
		assert(_board[C8] == BLACK_KING);
		removePiece(C8);
		if (_queenRookStartSquare[BLACK] != D8) {
			movePiece(D8, _queenRookStartSquare[BLACK]);
		}
		addPiece(_kingStartSquare[BLACK], BLACK_KING);
		kingSquares[BLACK] = _kingStartSquare[BLACK];
		break;
	}

}

void Board::undoMove(Move move, BoardState recentBoardState) {

	Square departure = move.getDeparture();
	Square destination = move.getDestination();
	Piece capture = move.getCapture();
	static uint64_t amount = 0;
	if (move.getAction() != 0) {
		undoMoveSpecialities(move);
	} 

	if (!move.isCastleMove())
	{
		assert(_board[destination] == move.getMovingPiece());
		movePiece(destination, departure);
		if (move.isCaptureMoveButNotEP()) {
			addPiece(destination, capture);
		}
	}
	_whiteToMove = !_whiteToMove;
	_boardState = recentBoardState;
	assert(_board[departure] != NO_PIECE);
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

	return result;
}

void Board::printPst(Piece piece) const {
	auto pieceBB = getPieceBB(piece);
	if (!pieceBB) return;

	std::cout << " " << colorToString(getPieceColor(piece)) 
		<< " " << pieceToChar(piece) << " PST: " 
		<< std::right << std::setw(19);

	EvalValue total;

	for (auto bb = pieceBB; bb; bb &= bb - 1)
	{
		const auto square = lsb(bb);
		total += PST::getValue(square, piece);
	}
	cout << total << " (";

	for (auto bb = pieceBB; bb; bb &= bb - 1)
	{
		const auto square = lsb(bb);
		const auto value = PST::getValue(square, piece);
		cout << squareToString(square) << value << " ";
	}
	cout << ")" << endl;
}


void Board::printPst() const {
	for (Piece piece = MIN_PIECE; piece <= BLACK_KING; ++piece) {
		printPst(piece);
	}
}


void Board::printFen() const {
	cout << getFen() << endl;
}

void Board::print() const {
	for (Rank rank = Rank::R8; rank >= Rank::R1; --rank) {
		for (File file = File::A; file <= File::H; ++file) {
			Piece piece = operator[](computeSquare(file, rank));
			std::cout << " " << pieceToChar(piece) << " ";
		}
		std::cout << std::endl;
	}
	std::cout << "hash: " << computeBoardHash() << std::endl;
	printFen();
	//printf("White King: %ld, Black King: %ld\n", kingPos[WHITE], kingPos[BLACK]);
}

bool Board::assertMove(Move move) const {
	assert(move.getMovingPiece() != NO_PIECE);
	assert(move.getDeparture() != move.getDestination());
	if (!(move.getMovingPiece() == operator[](move.getDeparture()))) {
		move.print();
	}
	assert(move.getMovingPiece() == operator[](move.getDeparture()));
	assert((move.getCapture() == operator[](move.getDestination())) || move.isCastleMove() || move.isEPMove());
	return true;
}
