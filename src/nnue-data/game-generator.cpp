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
 * Plays the training games, see game-generator.h
 */

#include <chrono>
#include <optional>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "game-generator.h"
#include "nnue-book.h"
#include "position-walker.h"
#include "../book/board-position.h"
#include "../../interface/chessinterface.h"

using namespace QaplaNnueData;
using namespace QaplaBook;
using namespace QaplaBasics;
using QaplaInterface::ChessInterface;
using QaplaInterface::ClockSetting;
using QaplaInterface::GameResult;
using QaplaMoveGenerator::MoveGenerator;

namespace {

	/** How the game ended, seen from white. */
	enum class Outcome { WHITE_WINS, DRAW, BLACK_WINS };

	/** What ended the game, which decides how its result is read. */
	enum class Ending { MATE, DRAW_RULE, VALUE, LENGTH, FAILED };

	/**
	 * The move of the position whose long algebraic notation is the given string,
	 * or an empty move if there is none.
	 */
	Move findMoveByNotation(MoveGenerator& position, const std::string& notation) {
		MoveList moveList;
		position.computeAttackMasksForBothColors();
		position.genMovesOfMovingColor(moveList);
		for (uint32_t index = 0; index < moveList.getTotalMoveAmount(); index++) {
			if (moveList[index].getLAN() == notation) return moveList[index];
		}
		return Move::EMPTY_MOVE;
	}

	/**
	 * Fills in the result of every move of the game. It is the same result
	 * everywhere, only seen from the side that is to move - and the move at an
	 * even index is white's, the game starting at the initial position.
	 */
	void setResult(std::vector<GameMove>& moves, Outcome outcome) {
		for (size_t index = 0; index < moves.size(); index++) {
			if (outcome == Outcome::DRAW) {
				moves[index].result = GameValue::DRAW;
				continue;
			}
			const bool whiteToMove = index % 2 == 0;
			const bool sideToMoveWins = whiteToMove == (outcome == Outcome::WHITE_WINS);
			moves[index].result = sideToMoveWins ? GameValue::WIN : GameValue::LOSS;
		}
	}

	/** Counts a few numbers about a run, for the report at the end. */
	struct Statistics {
		uint64_t games = 0;
		uint64_t moves = 0;
		uint64_t movesWithValue = 0;
		uint64_t whiteWins = 0;
		uint64_t draws = 0;
		uint64_t blackWins = 0;
		uint64_t endedByMate = 0;
		uint64_t endedByDrawRule = 0;
		uint64_t endedByValue = 0;
		uint64_t endedByLength = 0;
		uint64_t skipped = 0;
	};
}

bool GameGenerator::generate(const Settings& settings) {
	PositionBook book;
	if (!book.readFromFile(settings.bookFile)) {
		std::cout << "Error (cannot read book): " << settings.bookFile << std::endl;
		return false;
	}
	std::cout << "Read " << book.size() << " moves, " << book.leafCount()
		<< " leaves from " << settings.bookFile << std::endl;

	const std::filesystem::path outputPath(settings.outputFile);
	if (outputPath.has_parent_path() && !outputPath.parent_path().empty()) {
		std::filesystem::create_directories(outputPath.parent_path());
	}
	std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
	if (!output) {
		std::cout << "Error (cannot write games): " << settings.outputFile << std::endl;
		return false;
	}

	_board->setOption("Threads", "1");
	ClockSetting clock;
	clock.setSearchDepthLimit(settings.searchDepth);
	// Analyse mode, otherwise the clock stops the searches at its time budget -
	// which a clock setting without a time control leaves at nothing. The value
	// the game is labelled with then never gets written at all: it stays at
	// -MAX_VALUE, because the root move loop is abandoned before it is set.
	clock.setAnalyseMode();
	_board->setClock(clock);

	MoveGenerator position;
	setToStartPosition(position);
	LineWalker walker(position);
	Statistics statistics;
	std::vector<GameMove> gameMoves;
	const auto startTime = std::chrono::steady_clock::now();
	const uint64_t lastLeaf = settings.games == 0
		? ~uint64_t(0) : settings.firstLeaf + settings.games;
	uint64_t leavesSeen = 0;

	/**
	 * Plays one game from the position the walker stands at and writes it.
	 */
	const auto playGame = [&]() {
		const size_t bookPlies = walker.plies();
		gameMoves.clear();
		// The moves of the book line have no value of their own, but they have to
		// be in the file: without them the reader cannot replay the game from the
		// start position, and a packed move needs the board to be unpacked.
		for (const BookMove& bookMove : walker.path()) {
			const auto packed = packMove(bookMove);
			if (!packed) return;
			gameMoves.push_back(GameMove{ .move = *packed, .value = NO_GAME_VALUE });
		}

		const std::string fen = position.getFen(int(bookPlies) / 2 + 1);
		ChessInterface::setPositionByFen(fen, _board);
		Ending ending = Ending::LENGTH;
		Outcome outcome = Outcome::DRAW;
		value_t finalValue = 0;
		bool finalSideIsWhite = position.isWhiteToMove();

		while (gameMoves.size() < settings.maxHalfMoves) {
			const GameResult result = _board->getGameResult();
			if (result == GameResult::WHITE_WINS_BY_MATE || result == GameResult::BLACK_WINS_BY_MATE) {
				ending = Ending::MATE;
				outcome = result == GameResult::WHITE_WINS_BY_MATE
					? Outcome::WHITE_WINS : Outcome::BLACK_WINS;
				break;
			}
			if (result != GameResult::NOT_ENDED) {
				ending = Ending::DRAW_RULE;
				break;
			}

			_board->computeMove("", false);
			const auto info = _board->getComputingInfo();
			if (!info.error.empty() || info.currentConsideredMove.empty()) {
				// No move to play. getGameResult above should have caught it, so
				// this is the belt to its braces.
				ending = Ending::DRAW_RULE;
				break;
			}
			finalValue = info.valueInCentiPawn;
			finalSideIsWhite = position.isWhiteToMove();
			if (!fitsInRecord(finalValue)) {
				// Decided, and a value that could not be stored anyway. A mate
				// value ends the game here too, it is far outside the range.
				ending = Ending::VALUE;
				break;
			}

			const Move move = findMoveByNotation(position, info.currentConsideredMove);
			const BookMove bookMove{ .from = move.getDeparture(), .to = move.getDestination(),
				.promotion = move.isPromote() ? move.getPromotion() : NO_PIECE };
			const auto packed = move.isEmpty() ? std::nullopt : packMove(bookMove);
			if (!packed || !walker.play(bookMove)) {
				std::cout << "Error (cannot replay move): " << info.currentConsideredMove
					<< " in " << position.getFen(1) << std::endl;
				ending = Ending::FAILED;
				break;
			}
			gameMoves.push_back(GameMove{ .move = *packed, .value = finalValue });
			ChessInterface::setMove(info.currentConsideredMove, _board);
		}

		// Mate and the draw rules are the result of the game. Everything else -
		// the value leaving the range, or the game being broken off at its
		// length - is decided by the value of the last position.
		if (ending == Ending::VALUE || ending == Ending::LENGTH) {
			if (finalValue >= settings.winThreshold) {
				outcome = finalSideIsWhite ? Outcome::WHITE_WINS : Outcome::BLACK_WINS;
			}
			else if (finalValue <= -settings.winThreshold) {
				outcome = finalSideIsWhite ? Outcome::BLACK_WINS : Outcome::WHITE_WINS;
			}
		}

		walker.unplayTo(bookPlies);
		if (ending == Ending::FAILED || gameMoves.size() <= bookPlies) {
			statistics.skipped++;
			return;
		}
		switch (ending) {
		case Ending::MATE: statistics.endedByMate++; break;
		case Ending::DRAW_RULE: statistics.endedByDrawRule++; break;
		case Ending::VALUE: statistics.endedByValue++; break;
		default: statistics.endedByLength++; break;
		}
		setResult(gameMoves, outcome);
		writeGame(output, gameMoves);
		statistics.games++;
		statistics.moves += gameMoves.size();
		statistics.movesWithValue += gameMoves.size() - bookPlies;
		switch (outcome) {
		case Outcome::WHITE_WINS: statistics.whiteWins++; break;
		case Outcome::BLACK_WINS: statistics.blackWins++; break;
		default: statistics.draws++; break;
		}
		if (statistics.games % 100 == 0) {
			const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now() - startTime).count();
			std::cout << statistics.games << " games, " << statistics.movesWithValue
				<< " positions, " << (elapsed / double(statistics.games) / 1000.0)
				<< " s per game, " << (elapsed / 1000) << " s used" << std::endl;
		}
	};

	/**
	 * Walks the library and plays a game at every leaf. The recursion is as deep
	 * as the library, which is a few dozen plies.
	 */
	const auto walkLeaves = [&](auto&& self, PositionBook::NodeIndex node) -> void {
		if (leavesSeen >= lastLeaf) return;
		auto child = book.firstChild(node);
		if (child == PositionBook::NO_NODE) {
			if (leavesSeen >= settings.firstLeaf) playGame();
			leavesSeen++;
			return;
		}
		for (; child != PositionBook::NO_NODE; child = book.nextSibling(child)) {
			if (leavesSeen >= lastLeaf) return;
			if (!walker.play(book.move(child, BoardPosition(position)))) continue;
			self(self, child);
			walker.unplayTo(walker.plies() - 1);
		}
	};
	walkLeaves(walkLeaves, PositionBook::ROOT);
	output.close();

	const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - startTime).count();
	std::cout << "Games: " << statistics.games << ", positions with a value: "
		<< statistics.movesWithValue << ", moves stored: " << statistics.moves
		<< ", skipped: " << statistics.skipped << std::endl;
	std::cout << "Result: " << statistics.whiteWins << " white, " << statistics.draws
		<< " draw, " << statistics.blackWins << " black" << std::endl;
	std::cout << "Ended by: mate " << statistics.endedByMate << ", draw rule "
		<< statistics.endedByDrawRule << ", value out of range " << statistics.endedByValue
		<< ", length " << statistics.endedByLength << std::endl;
	std::cout << "Written " << settings.outputFile << " in " << (elapsed / 1000) << " s"
		<< std::endl;
	return true;
}
