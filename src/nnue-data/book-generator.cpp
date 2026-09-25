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
 * @copyright Copyright (c) 2026 Volker Böhm
 * @Overview
 * Builds the library of start positions, see book-generator.h
 */

#include <filesystem>
#include <iostream>
#include <random>
#include <vector>

#include "book-generator.h"
#include "../book/board-position.h"
#include "../../basics/movelist.h"
#include "../../interface/chessinterface.h"
#include "../../movegenerator/movegenerator.h"
#include "../../search/search.h"

using namespace QaplaNnueData;
using namespace QaplaBook;
using namespace QaplaBasics;
using QaplaInterface::ChessInterface;
using QaplaInterface::ClockSetting;
using QaplaMoveGenerator::MoveGenerator;
using QaplaSearch::ply_t;

namespace {

	/**
	 * Sets a board to the position a game starts at.
	 */
	void setToStartPosition(MoveGenerator& position) {
		static constexpr std::array<Piece, 8> BACK_RANK = {
			ROOK, KNIGHT, BISHOP, QUEEN, KING, BISHOP, KNIGHT, ROOK };
		position.clear();
		for (uint32_t file = 0; file < 8; file++) {
			const File currentFile = File(file);
			position.setPiece(computeSquare(currentFile, Rank::R1), BACK_RANK[file] + WHITE);
			position.setPiece(computeSquare(currentFile, Rank::R2), WHITE_PAWN);
			position.setPiece(computeSquare(currentFile, Rank::R7), BLACK_PAWN);
			position.setPiece(computeSquare(currentFile, Rank::R8), BACK_RANK[file] + BLACK);
		}
		position.setWhiteToMove(true);
		for (const Piece color : { WHITE, BLACK }) {
			position.setCastlingRight(color, true, true);
			position.setCastlingRight(color, false, true);
		}
		position.computeAttackMasksForBothColors();
	}

	/**
	 * The move of the position that plays the two squares of a book move, or an
	 * empty move if there is none. The move list is pseudo legal, the caller
	 * checks legality after playing it.
	 */
	Move findMove(MoveGenerator& position, const BookMove& bookMove) {
		MoveList moveList;
		position.computeAttackMasksForBothColors();
		position.genMovesOfMovingColor(moveList);
		for (uint32_t index = 0; index < moveList.getTotalMoveAmount(); index++) {
			const Move move = moveList[index];
			if (move.getDeparture() != bookMove.from) continue;
			if (move.getDestination() != bookMove.to) continue;
			const Piece promotion = move.isPromote() ? move.getPromotion() : NO_PIECE;
			if (promotion != bookMove.promotion) continue;
			return move;
		}
		return Move::EMPTY_MOVE;
	}

	/**
	 * Walks a board along a line of book moves and back. It keeps the snapshots
	 * the board needs to take a move back.
	 */
	class LineWalker {
	public:
		explicit LineWalker(MoveGenerator& position) : _position(position) {}

		/**
		 * Plays a book move. Returns false if the move is not legal in the
		 * position, in which case nothing has been played.
		 */
		bool play(const BookMove& bookMove) {
			const Move move = findMove(_position, bookMove);
			if (move.isEmpty()) return false;
			const PositionSnapshot snapshot = _position.getSnapshot();
			_position.doMove(move);
			if (!_position.isLegal()) {
				_position.undoMove(move, snapshot);
				_position.computeAttackMasksForBothColors();
				return false;
			}
			_path.push_back(bookMove);
			_moves.push_back(move);
			_snapshots.push_back(snapshot);
			return true;
		}

		/**
		 * Takes the whole line back, the board stands at the start position
		 * afterwards.
		 */
		void unplayAll() {
			while (!_moves.empty()) {
				_position.undoMove(_moves.back(), _snapshots.back());
				// undoMove does not restore the attack masks and the move
				// generation reads them.
				_position.computeAttackMasksForBothColors();
				_moves.pop_back();
				_snapshots.pop_back();
				_path.pop_back();
			}
		}

		const std::vector<BookMove>& path() const {
			return _path;
		}

		ply_t plies() const {
			return ply_t(_path.size());
		}

	private:
		MoveGenerator& _position;
		std::vector<BookMove> _path;
		std::vector<Move> _moves;
		std::vector<PositionSnapshot> _snapshots;
	};

	/**
	 * Adds every legal move of the position as a move of the book.
	 */
	uint32_t addAllMoves(PositionBook& book, MoveGenerator& position) {
		MoveList moveList;
		position.computeAttackMasksForBothColors();
		position.genMovesOfMovingColor(moveList);
		uint32_t added = 0;
		for (uint32_t index = 0; index < moveList.getTotalMoveAmount(); index++) {
			const Move move = moveList[index];
			const PositionSnapshot snapshot = position.getSnapshot();
			position.doMove(move);
			const bool isLegal = position.isLegal();
			position.undoMove(move, snapshot);
			position.computeAttackMasksForBothColors();
			if (!isLegal) continue;
			book.addMove(PositionBook::ROOT,
				BookMove{ .from = move.getDeparture(), .to = move.getDestination(),
					.promotion = move.isPromote() ? move.getPromotion() : NO_PIECE });
			added++;
		}
		return added;
	}
}

bool BookGenerator::generate(const Settings& settings) {
	if (!QaplaSearch::SearchObserver::isCompiledIn()) {
		std::cout << "Error (not compiled in): the search observer is missing, build with "
			<< "EXTRA_DEFINES=-DQAPLA_GENERATE_NNUE_DATA" << std::endl;
		return false;
	}
	if (settings.searchDepth < 2) {
		std::cout << "Error (search depth too small): need at least 2" << std::endl;
		return false;
	}

	PositionBook book;
	if (!settings.inputFile.empty()) {
		if (!book.readFromFile(settings.inputFile)) {
			std::cout << "Error (cannot read book): " << settings.inputFile << std::endl;
			return false;
		}
		std::cout << "Read " << book.size() << " moves, " << book.leafCount()
			<< " leaves from " << settings.inputFile << std::endl;
	}

	MoveGenerator position;
	setToStartPosition(position);
	// Steps one and two build a book, they are not repeated for one that is
	// already there: the search is deterministic, so searching the same first
	// moves again walks the same lines and adds nothing.
	const bool isNewBook = book.size() == 0;
	if (isNewBook) {
		const uint32_t firstMoves = addAllMoves(book, position);
		std::cout << "Step 1: " << firstMoves << " first moves" << std::endl;
	}

	const uint64_t leavesAtStart = book.leafCount();
	LineCollector collector(book, LineCollector::Settings{
		// The engine reports one more than the remaining depth of its root.
		.rootRemainingDepth = ply_t(settings.searchDepth) - 1,
		.collectRemainingDepth = settings.collectRemainingDepth,
		.margin = settings.margin,
		.maxPly = settings.maxPly,
		.maxNewLeaves = settings.leaves });
	QaplaSearch::Search::setObserver(&collector);

	// One observer for the whole process, so the search must not spread over
	// threads while it is watched.
	_board->setOption("Threads", "1");
	ClockSetting clock;
	clock.setSearchDepthLimit(settings.searchDepth);

	uint64_t searches = 0;
	const auto searchAtCurrentPosition = [&](const std::vector<BookMove>& path) {
		collector.setPrefix(path);
		// A new game empties the hash. With a hash still holding the previous
		// search, most nodes would be cut at the hash and never reach the depth
		// the lines are collected at.
		_board->newGame();
		ChessInterface::setPositionByFen(position.getFen(int(path.size()) / 2 + 1), _board);
		_board->setClock(clock);
		_board->computeMove("", false);
		searches++;
	};

	// Step 2: every first move once. The children of the root do not change
	// while the collector inserts - a line starting with a move that is already
	// there finds that move instead of adding one - so walking them is safe.
	LineWalker walker(position);
	if (isNewBook) {
		for (auto child = book.firstChild(PositionBook::ROOT);
			child != PositionBook::NO_NODE && !collector.isFull();
			child = book.nextSibling(child)) {
			if (!walker.play(book.move(child, BoardPosition(position)))) continue;
			searchAtCurrentPosition(walker.path());
			walker.unplayAll();
		}
		std::cout << "Step 2: " << searches << " searches, "
			<< collector.counters().newLeaves << " leaves" << std::endl;
	}

	// Step 3: random leaves until the wanted number of leaves is there.
	std::mt19937_64 random(settings.seed != 0 ? settings.seed : std::random_device{}());
	// A search of a leaf that has been searched before yields nothing new. Once
	// that happens over and over, the book is saturated at this depth and the run
	// stops instead of turning in circles.
	constexpr uint64_t MAX_FRUITLESS_ROUNDS = 100;
	uint64_t fruitlessRounds = 0;
	while (!collector.isFull() && fruitlessRounds < MAX_FRUITLESS_ROUNDS) {
		auto node = PositionBook::ROOT;
		bool walkFailed = false;
		while (!walkFailed && walker.plies() < settings.maxPly) {
			uint32_t children = 0;
			for (auto child = book.firstChild(node); child != PositionBook::NO_NODE;
				child = book.nextSibling(child)) {
				children++;
			}
			if (children == 0) break;
			auto child = book.firstChild(node);
			for (uint32_t step = uint32_t(random() % children); step > 0; step--) {
				child = book.nextSibling(child);
			}
			// A book move that is not legal here means the book does not fit the
			// position, which can only happen if it was written by something else.
			walkFailed = !walker.play(book.move(child, BoardPosition(position)));
			node = child;
		}
		if (walkFailed || walker.plies() == 0) {
			fruitlessRounds++;
		}
		else {
			const uint64_t leavesBefore = collector.counters().newLeaves;
			searchAtCurrentPosition(walker.path());
			fruitlessRounds = collector.counters().newLeaves > leavesBefore ? 0 : fruitlessRounds + 1;
		}
		walker.unplayAll();
	}
	if (fruitlessRounds >= MAX_FRUITLESS_ROUNDS) {
		std::cout << "Stopped: " << MAX_FRUITLESS_ROUNDS
			<< " searches in a row added nothing, the book is saturated" << std::endl;
	}
	QaplaSearch::Search::setObserver(nullptr);

	const auto& counters = collector.counters();
	std::cout << "Searches: " << searches
		<< ", candidates: " << counters.candidates
		<< ", rejected in check: " << counters.rejectedInCheck
		<< ", mate: " << counters.rejectedMate
		<< ", off margin: " << counters.rejectedOffMargin
		<< ", null move: " << counters.rejectedNullMove
		<< ", too long: " << counters.rejectedTooLong
		<< ", lines: " << counters.added
		<< ", new leaves: " << counters.newLeaves << std::endl;
	std::cout << "Book: " << book.size() << " moves, " << book.leafCount()
		<< " leaves, " << (book.leafCount() - leavesAtStart) << " of them new" << std::endl;

	book.header().author = "Qapla";
	book.header().name = "NNUE start positions";
	try {
		const std::filesystem::path path(settings.outputFile);
		if (path.has_parent_path() && !path.parent_path().empty()) {
			std::filesystem::create_directories(path.parent_path());
		}
		book.writeToFile(path);
	}
	catch (const std::exception& error) {
		std::cout << "Error (cannot write book): " << error.what() << std::endl;
		return false;
	}
	std::cout << "Written " << settings.outputFile << std::endl;
	return true;
}
