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
 * Plays the training games: every leaf of the position library in turn, from
 * there to the end of the game, every move chosen by a search of the given
 * depth, every position stored with the value that search returned.
 *
 * One game per leaf and no more: the search is deterministic, so a second game
 * from the same position would be the same game move for move. The variety has
 * to be in the library, which is what it is built for.
 *
 * One thread reads the lines out of the library and hands them to a pool of
 * workers, every worker with an engine instance of its own - a board is not
 * shared, and neither is a hash table. The games are written as they finish, so
 * their order in the file depends on the timing while their content does not:
 * every game follows from its start position alone.
 *
 * A run can be cut into pieces with firstLeaf and games, which makes it
 * resumable and lets several runs share a machine.
 *
 * Needs no special build: the games are played through the ordinary interface of
 * the engine, not through the search observer.
 */

#pragma once

#include <cstdint>
#include <string>

#include "../../interface/ichessboard.h"
#include "game-file.h"

namespace QaplaNnueData {

	class GameGenerator {
	public:
		struct Settings {
			/** The position library, its leaves are the start positions. */
			std::string bookFile = "test/nnue/start-positions-100k.bok";

			/** The file of games to write. */
			std::string outputFile = "test/nnue/training-games.gam";

			/** Depth of the searches, counted as the engine reports it. */
			uint32_t searchDepth = 8;

			/**
			 * From this value on a game counts as won for the side to move, in
			 * the internal unit of the engine where a pawn is 80 to 95.
			 */
			QaplaBasics::value_t winThreshold = 500;

			/** A game is broken off after that many half moves, the book line included. */
			uint32_t maxHalfMoves = MAX_HALF_MOVES;

			/** Leaves to skip, so a run can continue where another one stopped. */
			uint64_t firstLeaf = 0;

			/** Leaves to play, 0 for all of them. */
			uint64_t games = 0;

			/** Games played at the same time, 0 for one per core. */
			uint32_t threads = 0;

			/** Hash of every worker in megabytes - it is paid once per thread. */
			uint32_t hashInMegabytes = 32;
		};

		explicit GameGenerator(QaplaInterface::IChessBoard* board) : _board(board) {}

		/**
		 * Plays the games and writes the file.
		 * Returns false if the library cannot be read or the file cannot be
		 * written.
		 */
		bool generate(const Settings& settings);

	private:
		QaplaInterface::IChessBoard* _board;
	};
}
