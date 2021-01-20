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
 * Implements a Game information for pgn i/o
 */

#ifndef __PGNGAME_H
#define __PGNGAME_H

#include <string>
#include <vector>
#include <map>
#include "pgnfiletokenizer.h"

using namespace std;

class PGNTokenizer;

enum class Variant { STANDARD, CHESS_960, UNKNOWN };
enum class Result { WHITE_WIN, BLACK_WIN, DRAW, UNTERMINATED };
enum class Cause { 
	UNKNOWN, RESIGNED, LOSS_ON_TIME, DRAW_AGREEMENG, ILLEGAL_MOVE, NO_CAUSE, ADJUCATE, 
	FIFTY_MOVES_RULE, WHITE_MATES, BLACK_MATES, REPETITION, NOT_ENOUGH_MATERIAL
};

namespace ChessPGN {

	class PGNGame 
	{
	public:
		PGNGame();

		/**
		 * Initializes the members
		 */
		void init();

		// Sets the game from a PGN
		// Parameters			:
		// aPGNTokenizer		: tokenizer to get pgn token
		bool setGame(PGNTokenizer& aPGNTokenizer);

		/**
		 * Gets the number of half moves in the pgn (not including fen start moves)
		 */
		size_t getHalfmovesCount() const
		{
			return _moves.size();
		}

		/**
		 * Gets a move of the game
		 */
		const vector<string>& getMoves() const
		{
			return _moves;
		}

		// Gets the clock
		const string& getTag(const string& tagName) const {
			return _tags.at(tagName);
		}

		/**
		 * Gets the result of the game
		 */
		Result getResult() const { return _result; }

		// ---------------------- Helpers -----------------------------------------
	protected:

		// Sets game from a tag section
		bool parseTagSection(PGNTokenizer& aTokenizer);

		// Parses an Annotation section (may be recursive)
		void parseRAV(PGNTokenizer& aTokenizer);

		// Sets game from moves section
		bool parseGameSection(PGNTokenizer& aTokenizer);

	private:
		Cause parseCause(string cause);

		// Moves of the game
		vector<string> _moves;
		map<string, string> _tags;
		Result _result;
		Cause _gameEndCause;
	};
}

#endif // __PGNGAME_H
