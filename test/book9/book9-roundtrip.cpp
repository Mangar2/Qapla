// Round trip test of the 9 bit book: random games played with the Qapla move
// generator, every legal move of every position packed and unpacked, the book
// board compared with the Qapla board after every move, and a book of all games
// written, read back and searched.
//
// The Makefile compiles every .cpp of the repository into the engine, so the
// test only exists with BOOK9_ROUNDTRIP_TEST defined. Build and run it from the
// repository root, after make Release:
// c++ -std=c++20 -O2 -pthread -DBOOK9_ROUNDTRIP_TEST -o build/book9-test/book9-roundtrip
//     test/book9/book9-roundtrip.cpp src/book/book-io.cpp
//     $(find build/Release/basics build/Release/movegenerator -name '*.o')
// ./build/book9-test/book9-roundtrip 3000

#ifdef BOOK9_ROUNDTRIP_TEST

#include <cstdio>
#include <random>
#include <vector>

#include "../../src/book9/book.h"
#include "../../src/book9/board-position.h"
#include "../../src/nnue-data/position-walker.h"

using namespace QaplaBasics;
using namespace QaplaBook9;

namespace {
	int failures = 0;
	void fail(const char* what) {
		if (failures++ < 20) std::printf("FAIL: %s\n", what);
	}

	bool sameBoard(const BookBoard& bookBoard, const Board& board) {
		if (bookBoard.isWhiteToMove() != board.isWhiteToMove()) return false;
		for (int32_t square = 0; square < 64; ++square) {
			if (bookBoard.pieceAt(Square(square)) != board[Square(square)]) return false;
		}
		return true;
	}
}

/**
 * Chess960 castling that a standard game never reaches: a king already on its
 * castling square, and a king that castles onto its neighbouring rook.
 */
void checkChess960() {
	BookBoard board;
	board.setPiece(Square::G1, WHITE_KING);
	board.setPiece(Square::H1, WHITE_ROOK);
	board.setPiece(Square::A1, WHITE_ROOK);
	board.setPiece(Square::E8, BLACK_KING);
	// O-O written the Qapla way, g1 to g1, and written as king onto rook.
	const auto kingTarget = packMove(BookMove{ .from = Square::G1, .to = Square::G1 }, board);
	const auto rookTarget = packMove(BookMove{ .from = Square::G1, .to = Square::H1 }, board);
	if (!kingTarget || kingTarget != rookTarget) fail("960: king already on g1");
	BookBoard castled = board;
	castled.doMove(unpackMove(*rookTarget, board));
	if (castled.pieceAt(Square::G1) != WHITE_KING || castled.pieceAt(Square::F1) != WHITE_ROOK
		|| castled.pieceAt(Square::H1) != NO_PIECE) fail("960: O-O from g1 played wrongly");
	// O-O-O from g1: the king travels to c1 over the empty rank.
	const auto queenSide = packMove(BookMove{ .from = Square::G1, .to = Square::C1 }, board);
	if (!queenSide || unpackMove(*queenSide, board).to != Square::A1) fail("960: O-O-O from g1");

	BookBoard neighbour;
	neighbour.setPiece(Square::B1, WHITE_KING);
	neighbour.setPiece(Square::A1, WHITE_ROOK);
	neighbour.setPiece(Square::E8, BLACK_KING);
	const auto packed = packMove(BookMove{ .from = Square::B1, .to = Square::A1 }, neighbour);
	if (!packed) { fail("960: king onto neighbouring rook"); return; }
	neighbour.doMove(unpackMove(*packed, neighbour));
	if (neighbour.pieceAt(Square::C1) != WHITE_KING || neighbour.pieceAt(Square::D1) != WHITE_ROOK
		|| neighbour.pieceAt(Square::A1) != NO_PIECE || neighbour.pieceAt(Square::B1) != NO_PIECE) {
		fail("960: O-O-O from b1 played wrongly");
	}
}

int main(int argc, char** argv) {
	checkChess960();
	const int games = argc > 1 ? std::atoi(argv[1]) : 2000;
	std::mt19937 random(4711);
	Book<> book;
	std::vector<std::vector<BookMove>> lines;
	long long moves = 0, castles = 0, enPassants = 0, promotions = 0;

	for (int game = 0; game < games; ++game) {
		QaplaMoveGenerator::MoveGenerator position;
		QaplaNnueData::setToStartPosition(position);
		BookBoard bookBoard = BookBoard::startPosition();
		std::vector<BookMove> line;
		for (int ply = 0; ply < 300; ++ply) {
			MoveList moveList;
			position.computeAttackMasksForBothColors();
			position.genMovesOfMovingColor(moveList);
			std::vector<Move> legal;
			for (uint32_t index = 0; index < moveList.getTotalMoveAmount(); ++index) {
				const Move move = moveList[index];
				QaplaMoveGenerator::MoveGenerator next = position;
				next.doMove(move);
				if (next.isLegal()) legal.push_back(move);
			}
			if (legal.empty()) break;

			const BoardPosition view(position);
			for (const Move& move : legal) {
				const BookMove bookMove{ .from = move.getDeparture(), .to = move.getDestination(),
					.promotion = move.isPromote() ? move.getPromotion() : NO_PIECE };
				const auto packed = packMove(bookMove, view);
				if (!packed) { fail("move cannot be packed"); continue; }
				if (*packed > PACKED_MOVE_MASK) fail("packed move wider than 9 bits");
				if (packMove(bookMove, bookBoard) != packed) fail("book board packs differently");
				const BookMove unpacked = unpackMove(*packed, view);
				if (toKingTargetCastling(unpacked, view) != bookMove) fail("round trip differs");
				if (move.isCastleMove() != isCastling(unpacked, view)) fail("castling not recognised");
				++moves;
				castles += move.isCastleMove();
				promotions += move.isPromote();
				enPassants += move.getActionAndMovingPiece() == Move::WHITE_EP || move.getActionAndMovingPiece() == Move::BLACK_EP;
			}

			const Move chosen = legal[random() % legal.size()];
			const BookMove bookMove{ .from = chosen.getDeparture(), .to = chosen.getDestination(),
				.promotion = chosen.isPromote() ? chosen.getPromotion() : NO_PIECE };
			line.push_back(bookMove);
			bookBoard.doMove(unpackMove(*packMove(bookMove, bookBoard), bookBoard));
			position.doMove(chosen);
			if (!sameBoard(bookBoard, position)) fail("book board and qapla board differ");
		}
		book.addLine(line);
		lines.push_back(line);
	}

	const std::filesystem::path path = "test/book9/roundtrip.book9";
	book.writeToFile(path);
	Book<> reread;
	reread.readFromFile(path);
	if (reread.size() != book.size()) fail("size after reading differs");
	if (reread.leafCount() != book.leafCount()) fail("leaf count after reading differs");
	for (const auto& line : lines) {
		if (reread.findLine(line) == Book<>::NO_NODE) fail("line not found after reading");
	}
	// Walk one line through children() to check unpacking from the file.
	for (size_t lineIndex = 0; lineIndex < 50 && lineIndex < lines.size(); ++lineIndex) {
		BookBoard board = BookBoard::startPosition();
		auto node = Book<>::ROOT;
		for (const BookMove& expected : lines[lineIndex]) {
			bool found = false;
			for (const auto& child : reread.children(node, board)) {
				if (toKingTargetCastling(child.move, board) == expected) {
					node = child.node;
					board.doMove(child.move);
					found = true;
					break;
				}
			}
			if (!found) { fail("children() misses a move"); break; }
		}
	}
	std::printf("games %d, moves checked %lld (castling %lld, en passant %lld, promotion %lld), "
		"book nodes %zu, failures %d\n", games, moves, castles, enPassants, promotions,
		book.size(), failures);
	return failures == 0 ? 0 : 1;
}

#endif
