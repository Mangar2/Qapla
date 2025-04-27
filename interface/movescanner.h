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
 * Scans a move string
 */

#ifndef __MOVESCANNER_H
#define __MOVESCANNER_H

#include <string>

using namespace std;

namespace QaplaInterface {

	static bool isPieceChar(char pieceChar) {
		return string("NnBbRrQqKk").find(pieceChar) != string::npos;
	}

	static bool isCastleNotationChar(char ch) {
		return (ch == '0' || ch == 'O');
	}

	static bool isCheckSign(char ch) {
		return ch == '+';
	}

	static bool isMateSign(char ch) {
		return ch == '#';
	}

	static bool isPromoteChar(char ch) {
		return ch == '=';
	}

	static bool isRankChar(char rank) {
		return rank >= '1' && rank <= '8';
	}

	static bool isFileChar(char file) {
		return file >= 'a' && file <= 'h';
	}

	static bool isCaptureChar(char capture) {
		return capture == 'x' || capture == ':';
	}

	static uint32_t charToRank(char rank) {
		return (uint32_t)(rank - '1');
	}

	static uint32_t charToFile(char file) {
		return (uint32_t)(file - 'a');
	}

	class MoveScanner {
	public:
		MoveScanner(string move)
		{
			scanMove(move);
		}

	private:

		/*
		 * Scans the move
		 */
		void scanMove(string move) {
			piece = 0;
			promote = 0;
			int32_t curIndex = static_cast<int32_t>(move.size()) - 1;
			// It is more simple to read the move from the end instead from the beginning. 
			while (curIndex >= 0 && move[curIndex] == ' ') {
				--curIndex;
			}
			if (!handleCastleNotation(move)) {

				skipCheckAndMateSigns(move, curIndex);
				promote = getPiece(move, curIndex);
				skipEPInfo(move, curIndex);
				skipCaptureChar(move, curIndex);
				// destination rank/file is always last and mandatory
				destinationRank = getRank(move, curIndex);
				destinationFile = getFile(move, curIndex);
				skipCaptureChar(move, curIndex);
				// departure rank and file is optional
				departureRank = getRank(move, curIndex);
				departureFile = getFile(move, curIndex);
				piece = getPiece(move, curIndex);
				legal = (curIndex == -1);
			}

			// if no piece is given and the from position is not fully qualified, it is always a pawn
			if (piece == 0 && (departureFile == NO_POS || departureRank == NO_POS)) {
				piece = 'P';
			}

		}

		bool handleCastleNotation(const string& move) {

			// Look for castling notations
			bool isCastleNotation = false;
			if (isCastleNotationChar(move[0]))
			{
				isCastleNotation = true;
				// We do not know the fileor to move thus always note it as white castle move

				departureFile = 4;
				departureRank = NO_POS;
				destinationFile = NO_POS;
				destinationRank = NO_POS;
				piece = 'K';

				if (move[1] == '-' && isCastleNotationChar(move[2])) {

					// Castle Queen side
					if (move[3] == '-' && isCastleNotationChar(move[4])) {
						destinationFile = 2;
					}
					else {
						destinationFile = 6;
					}
				}

			}
			return isCastleNotation;
		}

		/**
		 * Skips the optional check or mate signs, they can be ignored
		 */
		void skipCheckAndMateSigns(const string& move, int32_t& curIndex) {
			while (curIndex >= 0 && (isCheckSign(move[curIndex]) || isMateSign(move[curIndex]))) {
				--curIndex;
			}
		}

		/**
		 * Gets a piece from the move string
		 * Promotion FIDE standard is "e8=Q", but it might also be noted without '=' as "e8Q"
		 * lower case letters are also supported to align with winboard standard
		 */
		char getPiece(const string& move, int32_t& curIndex) {
			char p = 0;
			if (curIndex >= 0 && isPieceChar(move[curIndex])) {
				p = move[curIndex];
				curIndex--;
				if (curIndex >= 0 && isPromoteChar(move[curIndex])) {
					curIndex--;
				}
			}
			return p;
		}

		/**
		 * Skips an en passant "e.p." notation that is optional
		 */
		void skipEPInfo(const string& move, int32_t& curIndex) {
			if (curIndex > 4 &&
				move[curIndex - 3] == 'e' &&
				move[curIndex - 2] == '.' &&
				move[curIndex - 1] == 'p' &&
				move[curIndex - 0] == '.'
				) {
				curIndex -= 4;
			}
		}

		/**
		 * Capture notations are optional and can be scipped
		 * Examples "Be5:", "Bxe5", "B:e5"
		 */
		void skipCaptureChar(const string& move, int32_t& curIndex) {
			
			if (curIndex >= 0 && isCaptureChar(move[curIndex])) {
				curIndex--;
			}
		}

		/**
		 * Retrieves the rank of the move 
		 */
		int32_t getRank(const string& move, int32_t& curIndex) {
			int32_t rank = NO_POS;
			if (curIndex >= 0 && isRankChar(move[curIndex])) {
				rank = charToRank(move[curIndex]);
				curIndex--;
			}
			return rank;
		}

		/**
		 * Retrieves the file of the move
		 */
		int32_t getFile(const string& move, int32_t& curIndex) {
			// the target rank is always last now like in "e8=Q". 
			int32_t file = NO_POS;
			if (curIndex >= 0 && isFileChar(move[curIndex])) {
				file = charToFile(move[curIndex]);
				curIndex--;
			}
			return file;
		}

	public:

		bool isLegal() {
			return legal;
		}

		char piece;
		char promote;
		int32_t departureFile;
		int32_t departureRank;
		int32_t destinationFile;
		int32_t destinationRank;
		bool legal;

		static const int32_t NO_POS = -1;
	};

}

#endif //__MOVESCANNER_H