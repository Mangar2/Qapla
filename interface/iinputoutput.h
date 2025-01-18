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
 * Interface for Input/Output handling - usually to receive token from the console and to print
 * text to the console. Still the interface is independent and can be used for any input/output.
 */

#ifndef IINPUTOUTPUT__H
#define IINPUTOUTPUT__H

#include <string>

using namespace std;

namespace QaplaInterface {

	class IInputOutput {
	public:
		/**
		 * Wait until the input provides the next token
		 * @param getEOL true, to get the end of line separator as token
		 */
		virtual string getNextTokenBlocking(bool getEOL = false) = 0;

		/**
		 * Wait until the input provides a full line (with End-Of-Line tag)
		 */
		virtual string getToEOLBlocking() = 0;

		/**
		 * Polls for the next token, returns "" empty string, if no token is available
		 */
		virtual string getNextTokenNonBlocking() = 0;
		virtual string getNextTokenNonBlocking(const string& tokenSeparator) = 0;

		/**
		 * Gets the current token already received
		 */
		virtual string getCurrentToken() = 0;

		/**
		 * Gets the current token in 64 bit unsigned integer format
		 */
		virtual uint64_t getCurrentTokenAsUnsignedInt() = 0;

		/**
		 * Prints a string and moves to the next line
		 */
		virtual void println(const string& string) = 0;

		/**
		 * Prints a string
		 */
		virtual void print(const string& string) = 0;

	};

}
#endif