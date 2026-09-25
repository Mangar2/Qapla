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
 * Builds the library of start positions the NNUE training games are played
 * from, as a book file, see src/book.
 *
 * The library is grown in three steps:
 * 1. Every legal first move of white becomes a move of the book.
 * 2. Every one of those moves is searched, and every line the search walks
 *    that ends at the collecting depth goes into the book, see LineCollector.
 *    That gives a very broad tree of positions the search really visits.
 * 3. From then on a leaf of the book is picked by walking down from the root and
 *    choosing a random move at every node, and that leaf is searched the same
 *    way. Repeated until the wanted number of leaves is reached.
 *
 * Step three is a random walk over the positions of a game in which every step
 * is a line a real search considered, not a line of random moves. The tree it
 * produces is broad at the top and reaches into the middle game, and its leaves
 * are the start positions of the training games.
 *
 * The searches need the search observer, so a build without
 * QAPLA_GENERATE_NNUE_DATA refuses to run - see search/search-config.h. They run
 * single threaded, because the observer is one per process.
 */

#pragma once

#include <cstdint>
#include <string>

#include "../../interface/ichessboard.h"
#include "line-collector.h"
#include "nnue-book.h"

namespace QaplaNnueData {

	class BookGenerator {
	public:
		struct Settings {
			/** The book file to write. */
			std::string outputFile = "test/nnue/start-positions.bok";

			/** A book to add to, empty to start a new one. */
			std::string inputFile;

			/** Leaves to add in this run. */
			uint64_t leaves = 10000;

			/** Depth of the searches, counted as the engine reports it. */
			uint32_t searchDepth = 8;

			/** Plies that have to be left below a node for its line to be taken. */
			QaplaSearch::ply_t collectRemainingDepth = 2;

			/**
			 * How far the value of a position may be from the root value, in the
			 * internal unit of the engine where a pawn is 80 to 95.
			 */
			QaplaBasics::value_t margin = 100;

			/** Longest line of the book, counted from the start position. */
			QaplaSearch::ply_t maxPly = 120;

			/** Seed of the random walk, 0 to seed from the system. */
			uint64_t seed = 0;
		};

		explicit BookGenerator(QaplaInterface::IChessBoard* board) : _board(board) {}

		/**
		 * Runs the three steps and writes the book.
		 * Returns false if the build cannot collect, the input book cannot be
		 * read or the output cannot be written.
		 */
		bool generate(const Settings& settings);

	private:
		QaplaInterface::IChessBoard* _board;
	};
}
