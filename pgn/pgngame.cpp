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

#include "pgngame.h"
#include "PGNTokenizer.h"
#include <algorithm>

using namespace ChessPGN;

// -------------------------- CTor --------------------------------------------
PGNGame::PGNGame()
{
	init();
}

// -------------------------- InitInfo ----------------------------------------
void PGNGame::init()
{
	_moves.clear();
	_tags.clear();
	_gameEndCause = Cause::UNKNOWN;
	_result = Result::UNTERMINATED;
}

// -------------------------- ParseTagSection ----------------------------------
bool PGNGame::parseTagSection(PGNTokenizer& tokenizer)
{
	string line;
	string name;
	bool res = false;

	// Tag section contains of Tags beginning with "[" and ending with "]"
	while (tokenizer.getCurToken() == "[")
	{
		res = true;
		// Reads the name and checks for a string end tag
		name = tokenizer.getNextToken();
		tokenizer.getNextToken();
		if (name == "]")
		{
			continue;
		}
		line = "";
		// Search for content string. It sould follow immediately
		while (tokenizer.getCurToken()[0] != ']' && tokenizer.getCurToken() != "")
		{
			line += tokenizer.getCurToken();
			tokenizer.getNextToken();
		}

		// Check if it is really a string, remove the '"' if it is
		if ((line.length() >= 2) && 
			(line[0] == '"') && 
			(line[line.length() - 1] == '"'))
		{
			line = line.substr(1, line.length() - 2);
		}
		else
		{
			line = "";
		}
		
		// Now checks for the right tag
		if (name == "Event" || name == "Site" || name == "Date" ||
			name == "Round" || name == "White" || name == "Black" ||
		    name == "Time" || name == "ECO" || name == "Opening" ||
			name == "Variation" || name == "Result" || name == "TimeControl" ||
			name == "WhiteType" || name == "BlackType" || name == "Variant" ||
			name == "PlyCount" || name == "Termination" || name == "FEN")
		{ 
			_tags[name] = line;
		}

		// Whait for opening or closing tags
		while ((tokenizer.getCurToken()[0] != ']') &&
			(tokenizer.getCurToken() != "") &&
			(tokenizer.getCurToken()[0] != '['))
		{
			tokenizer.getNextToken();
		} 
		if (tokenizer.getCurToken()[0] == ']') {
			tokenizer.getNextToken();
		}
	}
	return res;
}

// -------------------------- ParseAnnotation ---------------------------------
void PGNGame::parseRAV(PGNTokenizer& tokenizer)
{
	int mBracketAmount = 1;
	while (mBracketAmount > 0 && tokenizer.getCurToken() != "")
	{
		tokenizer.getNextToken();
		if (tokenizer.getCurToken() == "(") mBracketAmount++;
		else if (tokenizer.getCurToken() == ")") mBracketAmount--;
	}
}

Cause PGNGame::parseCause(string cause) {
	if (cause == "white mates") return Cause::WHITE_MATES;
	else if (cause == "black mates") return Cause::BLACK_MATES;
	else if (cause == "3-fold repetition") return Cause::REPETITION;
	else if (cause == "50 moves rule") return Cause::FIFTY_MOVES_RULE;
	else if (cause == "not enough material") return Cause::NOT_ENOUGH_MATERIAL;
	else return Cause::NO_CAUSE;
}

// -------------------------- ParseGameSection --------------------------------
bool PGNGame::parseGameSection(PGNTokenizer& tokenizer)
{
	bool res = false;
	while (tokenizer.getCurToken() != "")
	{
		string str;
		str = tokenizer.getCurToken();
		if (str == "") break;
		char ch = str[0];

		// Skip move number, it is of no use
		if ((ch >= '0') && (ch <= '9'))
		{
			// 0..n number of "." allowed
			while (tokenizer.getNextToken() == ".");
		}

		str = tokenizer.getCurToken();
		if (str == "") break;
		ch = str[0];

		// Check for Game End Tag
		if (str == "*") _result = Result::UNTERMINATED;
		else if (str == "1-0") _result = Result::WHITE_WIN;
		else if (str == "0-1") _result = Result::BLACK_WIN;
		else if (str == "1/2-1/2") _result = Result::DRAW;
		else if (((ch >= 'A') && (ch <= 'Z')) || // Move SAN
			((ch >= 'a') && (ch <= 'z')))
		{
			_moves.push_back(str);
		}
		else if (ch == '[')
		{
			// Beginning of next game
			break;
		}
		else if (ch == '{')
		{
			_gameEndCause = parseCause(str.substr(1, str.length() - 1));
		}
		else if (ch == '(')
		{
			parseRAV(tokenizer);
		}
		else
		{
			// Unknown
			res = false;
			break;
		}
		tokenizer.getNextToken();
	}  
	return res;
}

// -------------------------- SetGame -----------------------------------------
bool PGNGame::setGame(PGNTokenizer& tokenizer)
{
	// Initializes the game informations
	init();
	// Parse the tag section
	bool hasTag = parseTagSection(tokenizer);

	// Next fetch the moves
	bool hasGame = parseGameSection(tokenizer);
	return hasTag || hasGame;
}
