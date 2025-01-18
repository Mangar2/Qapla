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
 * Scans a fen string
 */

#ifndef __FENSCANNER_H
#define __FENSCANNER_H

#include "../basics/types.h"
#include "ichessboard.h"
#include <string>
#include <array>

using namespace ChessBasics;
using namespace std;

namespace QaplaInterface {
	class FenScanner {
	public:
		FenScanner() {
			error = true;
		}

		bool setBoard(string fen, IChessBoard* chessBoard) {
			chessBoard->clearBoard();
			string::iterator fenIterator = fen.begin();
			error = false;
			scanPieceSector(fen, fenIterator, chessBoard);
			skipBlank(fen, fenIterator);
			scanSideToMove(fen, fenIterator, chessBoard);
			if (!skipBlank(fen, fenIterator)) return !error;
			scanCastlingRights(fen, fenIterator, chessBoard);
			if (!skipBlank(fen, fenIterator)) return !error;
			scanEPField(fen, fenIterator, chessBoard);
			if (!skipBlank(fen, fenIterator)) return !error;
			scanHalfMovesWithouthPawnMoveOrCapture(fen, fenIterator, chessBoard);
			if (!skipBlank(fen, fenIterator)) return !error;
			scanPlayedMovesInGame(fen, fenIterator, chessBoard);
			return !error;
		}

	private:

		string::iterator fenIterator;

		bool error;

		/**
		 * Scans the piece sector of a fen string
		 */
		void scanPieceSector(const string& fen, string::iterator& fenIterator, IChessBoard* chessBoard) {

			int16_t rank = 0;
			int16_t file = 7;

			for (; fenIterator != fen.end() && !error && file >= 0; ++fenIterator) {
				char curChar = *fenIterator;
				if (curChar == 0 || curChar == ' ') {
					break;
				}
				else if (curChar == '/') {
					error = rank != NORTH;
					rank = 0;
					file--;
				}
				else if (isPieceChar(curChar)) {
					chessBoard->setPiece(rank, file, curChar);
					rank++;
				}
				else if (isColChar(curChar)) {
					rank += (curChar - '0');
				}
				else {
					error = true;
				}
			}

			if (rank != NORTH || file != 0) {
				error = true;
			}

		}

		/**
		 * Skips a mandatory blank
		 */
		bool skipBlank(const string& fen, string::iterator& fenIterator) {
			bool hasBlank = false;
			if (fenIterator != fen.end()) {
				if (*fenIterator == ' ') {
					++fenIterator;
					hasBlank = true;
				}
			}
			return hasBlank;
		}

		/**
		 * Scans the side to move, either "w" or "b", default white
		 */
		void scanSideToMove(const string& fen, string::iterator& fenIterator, IChessBoard* chessBoard) {
			if (fenIterator != fen.end()) {
				chessBoard->setWhiteToMove(*fenIterator == 'w');
				if (*fenIterator != 'w' && *fenIterator != 'b') {
					error = true;
				}
				++fenIterator;
			}
		}

		/**
		 * Scans the castling rights section 'K', 'Q' for white rights and 'k', 'q' for black rights
		 * Or '-' for no castling rights
		 */
		void scanCastlingRights(const string& fen, string::iterator& fenIterator, IChessBoard* chessBoard) {
			bool castlingRightsFound = false;

			// Default: every castle right activated
			if (fenIterator == fen.end()) {
				chessBoard->setWhiteKingSideCastlingRight(true);
				chessBoard->setWhiteQueenSideCastlingRight(true);
				chessBoard->setBlackKingSideCastlingRight(true);
				chessBoard->setBlackKingSideCastlingRight(true);
			}
			else {
				if (*fenIterator == 'K') {
					chessBoard->setWhiteKingSideCastlingRight(true);
					castlingRightsFound = true;
					++fenIterator;
				}
				if (*fenIterator == 'Q') {
					chessBoard->setWhiteQueenSideCastlingRight(true);
					castlingRightsFound = true;
					++fenIterator;
				}
				if (*fenIterator == 'k') {
					chessBoard->setBlackKingSideCastlingRight(true);
					castlingRightsFound = true;
					++fenIterator;
				}
				if (*fenIterator == 'q') {
					chessBoard->setBlackQueenSideCastlingRight(true);
					castlingRightsFound = true;
					++fenIterator;
				}
				if (*fenIterator == '-') {
					if (castlingRightsFound) {
						error = true;
					}
					++fenIterator;
				}
				else if (!castlingRightsFound) {
					error = true;
				}
			}
		}

		/**
		 * Scans an EN-Passant-Field
		 */
		void scanEPField(const string& fen, string::iterator& fenIterator, IChessBoard* chessBoard) {
			uint32_t epFile = -1;
			uint32_t epRank = -1;
			if (fenIterator != fen.end() && *fenIterator >= 'a' && *fenIterator <= 'h') {
				epFile = *fenIterator - 'a';
				++fenIterator;
			}
			if (fenIterator != fen.end() && *fenIterator >= '1' && *fenIterator <= '8') {
				epRank = *fenIterator - '1';
				++fenIterator;
			}
			if (epFile != -1 && epRank != -1) {
				chessBoard->setEPSquare(epFile, epRank);
			}
		}

		/**
		 * Scans a positive integer in the fen
		 */
		uint32_t scanInteger(const string& fen, string::iterator& fenIterator) {
			uint32_t result = 0;
			while (fenIterator != fen.end() && *fenIterator >= '0' && *fenIterator <= '9') {
				result *= 10;
				result += *fenIterator - '0';
				++fenIterator;
			}
			return result;
		}

		void scanHalfMovesWithouthPawnMoveOrCapture(const string& fen, string::iterator& fenIterator, IChessBoard* chessBoard) {
			chessBoard->setHalfmovesWithouthPawnMoveOrCapture((uint8_t)scanInteger(fen, fenIterator));
		}

		void scanPlayedMovesInGame(const string& fen, string::iterator& fenIterator, IChessBoard* chessBoard) {
			chessBoard->setPlayedMovesInGame(scanInteger(fen, fenIterator));
		}

		bool isPieceChar(char pieceChar) {
			string supportedChars = "PpNnBbRrQqKk";
			return supportedChars.find(pieceChar) != string::npos;
		}

		bool isColChar(char colChar) {
			string supportedChars = "12345678";
			return supportedChars.find(colChar) != string::npos;
		}

	};
}

#endif // __FENSCANNER_H