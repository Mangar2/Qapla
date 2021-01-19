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

		PGNFileTokenizer() {}

		// Set the file name and opens the file
		virtual void openFile(const string& fileName)
		{
			_file.close();
			_file.open(fileName, ios_base::in);
			readNextString();
			getNextToken();
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

	protected:
		/**
		 * Reads the next string from the file
		 */
		virtual void readNextString()
		{
			_file >> _pgnString;
			_pgnString += " ";
			_pos = 0;
			_lastPos = 0;
		}
		fstream	_file;
	};

}

#endif // __PGNFILETOKENIZER_H
