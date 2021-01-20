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

#ifndef __PGNFILETOKENIZER_H
#define __PGNFILETOKENIZER_H

#include <string>
#include <fstream>
#include "pgntokenizer.h"

namespace ChessPGN {

	class PGNFileTokenizer :
		public PGNTokenizer
	{
	public:

		PGNFileTokenizer(const string fileName) {
			_stream.open(fileName);
			readNextString();
			getNextToken();
		}

		~PGNFileTokenizer() {
			_stream.close();
		}

		/**
		 * Sets the position to the next char, reads a new line, if needed
		 */
		virtual void nextChar()
		{
			_pos++;
			if (_pgnString.length() <= _pos) {
				readNextString();
			}
		}

		/**
		 * Sets the position to the end of the line
		 */
		virtual void skipToEOL() {
			_pos = _pgnString.length();
		}

	protected:
		/**
		 * Reads the next string from the file
		 */
		virtual void readNextString()
		{
			getline(_stream, _pgnString);
			_pos = 0;
			_lastPos = 0;
			// hack stop parsing at end of line
			if (!_stream.eof()) {
				_pgnString += " ";
			}
		}
		ifstream	_stream;
	};

}

#endif // __PGNFILETOKENIZER_H
