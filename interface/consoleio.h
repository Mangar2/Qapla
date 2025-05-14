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
 * Implements a console io for chess
 */

#ifndef __CONSOLEIO_H
#define __CONSOLEIO_H

#include <string>
#include "iinputoutput.h"

using namespace std;

namespace QaplaInterface {

	class ConsoleIO : public IInputOutput {

		typedef uint16_t bufferSize_t;

	public:
		ConsoleIO() : fatalReadError(false) { token[0] = 0; buffer[0] = 0; }

		virtual bool isFatalReadError() {
			return fatalReadError;
		}

		/**
		 * Waits until a token is avaiable and returns it
		 */
		virtual string getNextTokenBlocking(bool getEOL = false)
		{
			string spaceString = getEOL ? " \t" : " \t\n\r";
			string separator = getEOL ? "\n\t" : "";
			bufferSize_t tokenSize = 0;

			while (tokenSize == 0 && !fatalReadError) {

				tokenSize = readTokenFromBuffer(spaceString, separator);
				removeTokenFromBuffer(tokenSize, spaceString);
				if (tokenSize == 0) {
					readFromStdIn();
				}

			}
			return token;
		}

		/**
		 * Waits until a full line is available and returns it
		 */
		virtual std::string getToEOLBlocking()
		{
			bufferSize_t tokenSize = 0;
			std::string EOLString = "\n\r";

			if (buffer[0] == '\n') {
				removeTokenFromBuffer(0, EOLString);
				return "";
			}

			while (tokenSize == 0 && !fatalReadError) {

				tokenSize = readTokenFromBuffer(EOLString, "");
				if (tokenSize != 0) {
					removeTokenFromBuffer(tokenSize, EOLString);
				}
				else {
					readFromStdIn();
				}

			}
			return token;
		}

		/**
		 * Gets the next token non blocking
		 * @retuns string
		 */
		virtual string getNextTokenNonBlocking()
		{
			return getNextTokenNonBlocking("");
		}

		/**
		 * Gets the next token from the buffer
		 * @param tokenSeparator list of characters separating the token
		 * @returns the next token or NULL, if no token is found
		 */
		virtual string getNextTokenNonBlocking(const string& tokenSeparator)
		{
			string spaceString = " \n\r\t";
			bufferSize_t tokenSize = 0;
			string res;

			tokenSize = readTokenFromBuffer(spaceString, tokenSeparator);
			removeTokenFromBuffer(tokenSize, spaceString);
			if (tokenSize != 0) {
				res = token;
			}
			return res;
		}

		/**
		 * Gets the current token, fetched before with get next token
		 */
		virtual string getCurrentToken() {
			return token;
		}

		/**
		 * Gets the current token, fetched before with get next token
		 * @returns An integer convertion of the token
		 */
		virtual uint64_t getCurrentTokenAsUnsignedInt()
		{
			uint64_t res = 0;
			bufferSize_t index;
			for (index = 0; index < token.size(); index++) {
				if (token[index] < '0' || token[index] > '9') {
					break;
				}
				res *= 10;
				res += token[index] - '0';
			}
			return res;
		}

		/**
		 * Prints a line to std-out
		 */
		virtual void println(const string& stringToPrint)
		{
			fprintf(stdout, "%s\n", stringToPrint.c_str());
			fflush(stdout);
		};

		/**
		 * Prints a string to stdout
		 */
		virtual void print(const string& strintToPrint)
		{
			fprintf(stdout, "%s", strintToPrint.c_str());
			fflush(stdout);
		}


	private:

		/**
		 * Checks, if a character is in a string
		 */
		bool isCharInString(char aChar, const string string) {
			return string.find(aChar) != string::npos;
		}

		/**
		 * Reads from stdin to the internal buffer
		 */
		void readFromStdIn() {
			bufferSize_t endOfBuffer;

			for (endOfBuffer = 0; endOfBuffer < BUFFER_SIZE; endOfBuffer++) {
				if (buffer[endOfBuffer] == 0) {
					break;
				}
			}

			if (fgets(buffer + endOfBuffer, BUFFER_SIZE - endOfBuffer, stdin) == NULL)
			{
				if (feof(stdin))
				{
					fatalReadError = true;
				}
				else
				{
					fatalReadError = true;
				}
			}

		}

		/**
		 * Gets a token from buffer
		 */
		bufferSize_t readTokenFromBuffer(const string spaceString, const string separationString) {
			bufferSize_t index;
			token = "";
			if (isCharInString(buffer[0], separationString)) {
				token += buffer[0];
				return 1;
			}
			for (index = 0; index < BUFFER_SIZE && buffer[index] != 0; index++) {
				if (isCharInString(buffer[index], spaceString + separationString)) {
					break;
				}
				token += buffer[index];
			}
			if (index >= BUFFER_SIZE - 1) {
				// The buffer is full, but no token has been found. This should not happen. If it happens it must be an
				// irregular usage of the engine to make it crash or to test it. So we discard this incomplete token.
				buffer[0] = 0;
				return 0;
			}
			// If the token does not end with a space, it could be incompletely _loaded -> return 0 to indicate
			// that no token has been found.
			return buffer[index] == 0 ? 0 : index;
		}

		/**
		 * Removes a token from the buffer
		 */
		void removeTokenFromBuffer(bufferSize_t tokenSize, const string spaceString) {

			while (isCharInString(buffer[tokenSize], spaceString) && tokenSize < BUFFER_SIZE) {
				tokenSize++;
			}

			for (bufferSize_t aIndex = tokenSize; aIndex < BUFFER_SIZE; aIndex++) {
				buffer[aIndex - tokenSize] = buffer[aIndex];
				if (buffer[aIndex] == 0) {
					break;
				}
			}

		}

		bool fatalReadError;
		static const bufferSize_t BUFFER_SIZE = 2024;
		string token;
		char buffer[BUFFER_SIZE + 1];

	};
}

#endif // __CONSOLEIO_H