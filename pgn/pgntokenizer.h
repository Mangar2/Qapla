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
 * Implements a tokeniszer for PGN files
 */

#ifndef __PGNTOKENIZER_H
#define __PGNTOKENIZER_H

#include <array>
#include <string>

using namespace std;

namespace ChessPGN {

	class PGNTokenizer
	{
	public:
		PGNTokenizer(void);

		PGNTokenizer(const string& aStr)
		{
			setPGNString(aStr);
		}

		// ---------------------- Getter ------------------------------------------

		// Gets the positiion
		virtual size_t getPos()
		{
			return _lastPos;
		}

		// Fetches the next token
		const string& getNextToken();

		// Gets current token
		const string& getCurToken() const
		{
			return _token;
		}


		// ---------------------- Setter ------------------------------------------
		// Sets the position
		virtual void setPos(int aPos)
		{
			_pos = aPos;
			getNextToken();
		}

		// Sets the string
		void setPGNString(const string& pgn)
		{
			_pgnString = pgn;
			setPos(0);
		}


		// ---------------------- Helpers -----------------------------------------
	protected:
		// Gets the next char and sets the position to it
		virtual void nextChar()
		{
			_pos++;
		}
		virtual void skipToEOL() {
			findChar('\n', false);
		}

	private:

		// Next token helper
		void skipSpaces();

		// Scans until a char is found
		void findChar(char aCh, bool aSkipEOL);

		// Skips an annotation
		void skipAnnotation();

		// Scans all digits
		void skipDigits();

		// True if a char is a SymbolContinuation
		bool isSymbolContinuation(char aCh);

		// Scan symbol continuation chars
		void skipSymbolContinuation();

	protected:
		// Current PGN string to tokenize
		string _pgnString;
		// Current token
		string _token;
		// Current position in the PGN string
		size_t    _pos;
		size_t    _lastPos;

		enum class CharType {
			NOTHING, SYMBOL_CONTINUATION, SPACE
		};
		array<CharType, 256> _charType;
	};
}

#endif // __PGNTOKENIZER_H
