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
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

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
using QaplaInterface::IChessBoard;
using QaplaMoveGenerator::MoveGenerator;

namespace {

	/** How the game ended, seen from white. */
	enum class Outcome { WHITE_WINS, DRAW, BLACK_WINS };

	/** What ended the game, which decides how its result is read. */
	enum class Ending { MATE, DRAW_RULE, VALUE, LENGTH, FAILED };

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

	/** A game a worker has played and handed over. */
	struct PlayedGame {
		std::vector<GameMove> moves;
		size_t bookPlies = 0;
		Ending ending = Ending::FAILED;
		Outcome outcome = Outcome::DRAW;
	};

	/**
	 * Sets an option of the engine. The options live in the option providers, not
	 * in setOption of the board - that one knows only the bitbase path.
	 * Returns false if no provider knows the option.
	 */
	bool setEngineOption(IChessBoard* board, const std::string& name, const std::string& value) {
		for (auto* provider : board->getUciOptionProviders()) {
			if (provider->setUciOption(name, value)) return true;
		}
		return false;
	}

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

	/**
	 * Plays games on an engine instance of its own, so that several of them run
	 * side by side without sharing a board or a hash table.
	 */
	class GamePlayer {
	public:
		GamePlayer(IChessBoard& prototype, const GameGenerator::Settings& settings)
			: _board(prototype.createNew()), _settings(settings) {
			_board->initialize();
			setEngineOption(_board.get(), "Threads", "1");
			setEngineOption(_board.get(), "Hash", std::to_string(settings.hashInMegabytes));
			_clock.setSearchDepthLimit(settings.searchDepth);
			// Analyse mode, otherwise the clock ends the root move loop at its time
			// budget - which a clock setting without a time control leaves at
			// nothing - before the value of the iteration is stored, and every game
			// would be labelled with -MAX_VALUE.
			_clock.setAnalyseMode();
			_board->setClock(_clock);
		}

		/**
		 * Plays one game, starting at the end of the book line.
		 */
		void play(const std::vector<BookMove>& line, PlayedGame& game) {
			game.moves.clear();
			game.bookPlies = line.size();
			game.ending = Ending::FAILED;
			game.outcome = Outcome::DRAW;

			setToStartPosition(_position);
			LineWalker walker(_position);
			// The moves of the book line have no value of their own, but they have
			// to be in the file: without them the reader cannot replay the game from
			// the start position, and a packed move needs the board to be unpacked.
			for (const BookMove& bookMove : line) {
				const auto packed = packMove(bookMove);
				if (!packed || !walker.play(bookMove)) return;
				game.moves.push_back(GameMove{ .move = *packed, .value = NO_GAME_VALUE });
			}

			ChessInterface::setPositionByFen(_position.getFen(int(line.size()) / 2 + 1), _board.get());
			Ending ending = Ending::LENGTH;
			Outcome outcome = Outcome::DRAW;
			value_t finalValue = 0;
			bool finalSideIsWhite = _position.isWhiteToMove();

			while (game.moves.size() < _settings.maxHalfMoves) {
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
				finalSideIsWhite = _position.isWhiteToMove();
				if (!fitsInRecord(finalValue)) {
					// Decided, and a value that could not be stored anyway. A mate
					// value ends the game here too, it is far outside the range.
					ending = Ending::VALUE;
					break;
				}

				const Move move = findMoveByNotation(_position, info.currentConsideredMove);
				const BookMove bookMove{ .from = move.getDeparture(), .to = move.getDestination(),
					.promotion = move.isPromote() ? move.getPromotion() : NO_PIECE };
				const auto packed = move.isEmpty() ? std::nullopt : packMove(bookMove);
				if (!packed || !walker.play(bookMove)) {
					std::cout << "Error (cannot replay move): " << info.currentConsideredMove
						<< " in " << _position.getFen(1) << std::endl;
					return;
				}
				game.moves.push_back(GameMove{ .move = *packed, .value = finalValue });
				ChessInterface::setMove(info.currentConsideredMove, _board.get());
			}

			// Mate and the draw rules are the result of the game. Everything else -
			// the value leaving the range, or the game being broken off at its
			// length - is decided by the value of the last position.
			if (ending == Ending::VALUE || ending == Ending::LENGTH) {
				if (finalValue >= _settings.winThreshold) {
					outcome = finalSideIsWhite ? Outcome::WHITE_WINS : Outcome::BLACK_WINS;
				}
				else if (finalValue <= -_settings.winThreshold) {
					outcome = finalSideIsWhite ? Outcome::BLACK_WINS : Outcome::WHITE_WINS;
				}
			}
			game.ending = ending;
			game.outcome = outcome;
		}

	private:
		std::unique_ptr<IChessBoard> _board;
		const GameGenerator::Settings& _settings;
		MoveGenerator _position;
		ClockSetting _clock;
	};

	/**
	 * The lines waiting to be played. Bounded, so that the thread reading the
	 * library cannot run arbitrarily far ahead of the workers.
	 */
	class LineQueue {
	public:
		explicit LineQueue(size_t capacity) : _capacity(capacity) {}

		void push(std::vector<BookMove>&& line) {
			std::unique_lock<std::mutex> lock(_mutex);
			_notFull.wait(lock, [this] { return _lines.size() < _capacity; });
			_lines.push_back(std::move(line));
			lock.unlock();
			_notEmpty.notify_one();
		}

		/**
		 * Takes the next line. Returns false once the queue is empty and nothing
		 * more will come.
		 */
		bool pop(std::vector<BookMove>& line) {
			std::unique_lock<std::mutex> lock(_mutex);
			_notEmpty.wait(lock, [this] { return !_lines.empty() || _finished; });
			if (_lines.empty()) return false;
			line = std::move(_lines.front());
			_lines.pop_front();
			lock.unlock();
			_notFull.notify_one();
			return true;
		}

		/** No more lines will be pushed. */
		void finish() {
			{
				std::lock_guard<std::mutex> lock(_mutex);
				_finished = true;
			}
			_notEmpty.notify_all();
		}

	private:
		const size_t _capacity;
		std::mutex _mutex;
		std::condition_variable _notEmpty;
		std::condition_variable _notFull;
		std::deque<std::vector<BookMove>> _lines;
		bool _finished = false;
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

	const uint32_t hardwareThreads = std::max(1u, std::thread::hardware_concurrency());
	const uint32_t workerCount = settings.threads != 0 ? settings.threads : hardwareThreads;
	std::cout << "Playing on " << workerCount << " threads, " << settings.hashInMegabytes
		<< " MB hash each, " << hardwareThreads << " cores available" << std::endl;

	Statistics statistics;
	std::mutex writeMutex;
	const auto startTime = std::chrono::steady_clock::now();

	/**
	 * Takes a finished game, counts it and writes it. One thread at a time.
	 */
	const auto collect = [&](const PlayedGame& game) {
		std::lock_guard<std::mutex> lock(writeMutex);
		if (game.ending == Ending::FAILED || game.moves.size() <= game.bookPlies) {
			statistics.skipped++;
			return;
		}
		switch (game.ending) {
		case Ending::MATE: statistics.endedByMate++; break;
		case Ending::DRAW_RULE: statistics.endedByDrawRule++; break;
		case Ending::VALUE: statistics.endedByValue++; break;
		default: statistics.endedByLength++; break;
		}
		std::vector<GameMove> moves = game.moves;
		setResult(moves, game.outcome);
		writeGame(output, moves);
		statistics.games++;
		statistics.moves += moves.size();
		statistics.movesWithValue += moves.size() - game.bookPlies;
		switch (game.outcome) {
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

	LineQueue queue(size_t(workerCount) * 2);
	std::vector<std::thread> workers;
	for (uint32_t index = 0; index < workerCount; index++) {
		workers.emplace_back([&] {
			GamePlayer player(*_board, settings);
			PlayedGame game;
			std::vector<BookMove> line;
			while (queue.pop(line)) {
				player.play(line, game);
				collect(game);
			}
		});
	}

	// The library is read here and only here: the workers get whole lines, they
	// never touch the book.
	MoveGenerator position;
	setToStartPosition(position);
	LineWalker walker(position);
	uint64_t leavesSeen = 0;
	const uint64_t lastLeaf = settings.games == 0
		? ~uint64_t(0) : settings.firstLeaf + settings.games;

	const auto walkLeaves = [&](auto&& self, PositionBook::NodeIndex node) -> void {
		if (leavesSeen >= lastLeaf) return;
		auto child = book.firstChild(node);
		if (child == PositionBook::NO_NODE) {
			if (leavesSeen >= settings.firstLeaf) queue.push(std::vector<BookMove>(walker.path()));
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
	queue.finish();
	for (std::thread& worker : workers) worker.join();
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
