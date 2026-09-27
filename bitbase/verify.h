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
 * @copyright Copyright (c) 2025 Volker Böhm
 * @Overview
 * Tool to veryfy generated bitbases
 */


#ifndef __BITBASEVERIFY_H
#define __BITBASEVERIFY_H

#include <iostream>
#include <atomic>
#include <mutex>
#include "bitbase.h"
#include "boardaccess.h"
#include "workpackage.h"
#include "generationstate.h"
#include "bitbase-reader.h"
#include "../search/clockmanager.h"

using namespace QaplaMoveGenerator;
using namespace QaplaSearch;

namespace QaplaBitbase {
	class Verify {
	public:
		Verify() {
		}

		/**
		 * Computes a bitbase for a piece string (example KPK)
		 */
		void verifyBitbase(string pieceString) {
			PieceList list(pieceString);
			verifyBitbase(list);
		}

		/**
		 * Computes a bitbase for a piece string (example KPK) and
		 * all other bitbases needed
		 */
		void verifyBitbaseRec(string pieceString, uint32_t cores = 1, bool uncompressed = false,
				int traceLevel = 0, int debugLevel = 0)
		{

			_cores = std::min(MAX_THREADS, cores);
			_uncompressed = uncompressed;
			_traceLevel = traceLevel;
			_debugLevel = debugLevel;
			ClockManager clock;
			clock.setStartTime();
			_verified.clear();
			if (pieceString == "3") {
				verifyBitbaseRec("K*K", true);
			} else if (pieceString == "4") {
				verifyBitbaseRec("K*K", true);
				verifyBitbaseRec("K*K*", true);
				verifyBitbaseRec("K**K", true);
			}
			else if (pieceString == "5") {
				verifyBitbaseRec("K*K", true);
				verifyBitbaseRec("K*K*", true);
				verifyBitbaseRec("K**K", true);
				verifyBitbaseRec("K**K*", true);
				verifyBitbaseRec("K*K**", true);
				verifyBitbaseRec("K***K", true);
			}
			else if (pieceString == "6") {
				verifyBitbaseRec("K*K", true);
				verifyBitbaseRec("K*K*", true);
				verifyBitbaseRec("K**K", true);
				verifyBitbaseRec("K**K*", true);
				verifyBitbaseRec("K*K**", true);
				verifyBitbaseRec("K***K", true);
				verifyBitbaseRec("K**K**", true);
				verifyBitbaseRec("K***K*", true);
				verifyBitbaseRec("K*K***", true);
				verifyBitbaseRec("K****K", true);
			}
			else {
				verifyBitbaseRec(pieceString, true);
			}
			cout << endl << "All Bitbases verified!" << endl;
			printTimeSpent(clock, 0);
			cout << endl;
		}

	private:

		/**
		 * Prints the time spent so far
		 */
		void printTimeSpent(ClockManager& clock, int minTraceLevel = 2) {
			if (_traceLevel < minTraceLevel) {
				return;
			}
			uint64_t timeInMilliseconds = clock.computeTimeSpentInMilliseconds();
			cout << "Time spent: " << (timeInMilliseconds / (60 * 60 * 1000))
				<< ":" << ((timeInMilliseconds / (60 * 1000)) % 60)
				<< ":" << ((timeInMilliseconds / 1000) % 60)
				<< "." << timeInMilliseconds % 1000 << " ";
		}

		/**
		 * Join all running threads
		 */
		void joinThreads() {
			for (auto& thr : _threads) {
				if (thr.joinable()) {
					thr.join();
				}
			}
		}

		/**
		 * Populates a position from a bitbase index for the squares and a piece list for the piece types
		 */
		bool setPosition(MoveGenerator& position, const PieceList& pieceList)
		{
			bool result = true;
			for (uint32_t pieceNo = 0; pieceNo < pieceList.getNumberOfPieces(); ++pieceNo) {
				Square square = pieceList.getSquare(pieceNo);
				if (position[square] != NO_PIECE) {
					result = false;
					break;
				}
				position.unsafeSetPiece(square, pieceList.getPiece(pieceNo));
			}
			if (result) {
				position.computeAttackMasksForBothColors();
			}
			return result;
		}

		/**
		 * Computes a single position
		 */
		BitbaseResult computePosition(MoveGenerator& position, bool verbose = false) {
			MoveList moveList;
			Move move;
			position.genMovesOfMovingColor(moveList);
			// Mate/Stalemate
			if (moveList.getTotalMoveAmount() == 0) {
				if (verbose) cout << "Mate or Stalemate" << endl;
				if (position.isWhiteToMove()) {
					return position.isInCheck() ? BitbaseResult::Loss : BitbaseResult::Draw;
				} 
				else {
					return position.isInCheck() ? BitbaseResult::Win : BitbaseResult::Draw;
				}
			}

			// White maximizes (Win > Draw > Loss), Black minimizes (Loss < Draw < Win from white's view).
			// DrawOrLoss (1-bit legacy) equals Draw=0 and is treated as Draw.
			BitbaseResult result = position.isWhiteToMove() ? BitbaseResult::Loss : BitbaseResult::Win;
			const PositionSnapshot snapshot = position.getSnapshot();

			for (uint32_t moveNo = 0; moveNo < moveList.getTotalMoveAmount(); moveNo++) {
				move = moveList.getMove(moveNo);

				position.doMove(move);
				BitbaseResult cur = BitbaseReader::getValueFromSingleBitbase(position);
				
				if (verbose) {
					uint64_t index = BoardAccess::getIndex<0>(position);
					cout << move.getLAN() << " with index: " << index << " " << to_string(cur) << endl;
				}
				// Always undo move before throwing an exception to keep the position in a consistent state
				position.undoMove(move, snapshot);

				if (cur == BitbaseResult::Unknown) {
					position.doMove(move);
					auto fen = position.getFen(0);
					position.undoMove(move, snapshot);
					throw "Bitbase not available for fen:  " + fen;
				}

				if (position.isWhiteToMove()) {
					// White maximizes: Win > Draw > Loss
					if (cur == BitbaseResult::Win) { result = cur; break; }
					if (cur == BitbaseResult::Draw) result = cur;
				} else {
					// Black minimizes from white's view: Loss < Draw < Win
					if (cur == BitbaseResult::Loss) { result = cur; break; }
					if (cur == BitbaseResult::Draw) result = cur;
				}
			}
			return result;
		}

		/**
		 * verifies a position 
		 */
		void verifyPosition(MoveGenerator& position) {
			if (position.isLegal()) {
				BitbaseResult computedResult = computePosition(position);
				BitbaseResult expectedResult = BitbaseReader::getValueFromSingleBitbase(position);
				if (computedResult != expectedResult) {
					uint64_t errNo = ++_errors;
					if (errNo <= 10) {
						std::lock_guard<std::mutex> lock(_mtxOutput);
						cout << "verify failed on " << endl;
						position.print();
						cout << "Bitbase info: " << to_string(expectedResult)
							<< " Computed: " << to_string(computedResult)
							<< " Index: " << BoardAccess::getIndex<0>(position)
							<< endl;
						try {
							computePosition(position, true);
						}
						catch (string s) {
							cout << "Error during verify: " << s << endl;
						}
					}
				}
			}
		}


		/**
		 * verifies a position for black and white based on a piece list having squares and pieces
		 */
		void verifyPosition(const PieceList& pieceList) {
			MoveGenerator position;
			static uint64_t exceptionCounter = 0;
			try {
				if (setPosition(position, pieceList)) {
					position.setWhiteToMove(true);
					verifyPosition(position);
					position.setWhiteToMove(false);
					verifyPosition(position);
				}
			}
			catch (string s) {
				exceptionCounter++;
				if (exceptionCounter < 10) {
					 cout << "Exception during verifyPosition: " << s << endl;
					 if (exceptionCounter == 9) {
						 cout << "Further exceptions will be suppressed." << endl;
					 }
				}
			}
			return;
		}

		/**
		 * Verifies many positions classified by a pieceList
		 * @param pieceList list of pieces in the bitbase
		 */
		void verifyPositionRec(PieceList pieceList, uint32_t pieceNo) {
			Square square = pieceList.getSquare(pieceNo);
			Piece piece = pieceList.getPiece(pieceNo);
			if (isPawn(piece) && square < A2) {
				square = A2;
				pieceList.setSquare(pieceNo, square);
			}
			if (square > H8 || (isPawn(piece) && square > H7)) {
				return;
			}
			if (pieceNo + 1 < pieceList.getNumberOfPieces()) {
				verifyPositionRec(pieceList, pieceNo + 1);
			}
			else {
				verifyPosition(pieceList);
			}
			pieceList.setSquare(pieceNo, square + 1);
			verifyPositionRec(pieceList, pieceNo);
		}

		/**
		 * Verifies many positions classified by a pieceList
		 * @param pieceList list of pieces in the bitbase
		 */
		void verifyPositions(PieceList pieceList) {
			int work = ((int(BOARD_SIZE) + _cores - 1)/ _cores);
			for (uint32_t threadNo = 0; threadNo < _cores; ++threadNo) {
				Square first = Square(threadNo * work);
				Square last = Square((threadNo + 1) * work - 1);
				if (last > H8) {
					last = H8;
				}
				_threads[threadNo] = thread([this, first, last, pieceList]() {
					try {
						PieceList pieceListCopy(pieceList);
						for (Square square = first; square <= last; ++square) {
							pieceListCopy.setSquare(0, square);
							verifyPositionRec(pieceListCopy, 1);
							{
								std::lock_guard<std::mutex> lock(_mtxOutput);
								cout << '.';
							}
						}
					}
					catch (string s) {
						std::lock_guard<std::mutex> lock(_mtxOutput);
						cout << "Exception during verifyPositions, thread terminated: " << s << endl;
					}
				});
			}
			joinThreads();
		}

		/**
		 * Verifies a bitbase
		 * @param pieceList list of pieces in the bitbase
		 */
		void verifyBitbase(PieceList& pieceList) {
			ClockManager clock;
			clock.setStartTime();
			try {
				for (uint32_t pieceNo = 0; pieceNo < pieceList.getNumberOfPieces(); pieceNo++) {
					pieceList.setSquare(pieceNo, isPawn(pieceList.getPiece(pieceNo)) ? A2 : A1);
				}
				string pieceString = pieceList.getPieceString();
				string loadString = pieceString;
				replace(loadString.begin(), loadString.end(), 'P', '*');
				BitbaseReader::loadBitbaseRec(loadString, true);
				for (uint32_t pieceNo = 2; pieceNo < pieceList.getNumberOfPieces(); pieceNo++) {
					PieceList loadList(pieceList);
					loadList.removePiece(pieceNo);
					string loadString = loadList.getPieceString();
					replace(loadString.begin(), loadString.end(), 'P', '*');
					BitbaseReader::loadBitbaseRec(loadString, true);
				}
				_errors = 0;
				cout << pieceString << " Verifying with " << _cores << " cores ";
				verifyPositions(pieceList);
				cout << " Errors: " << _errors << " ";
				printTimeSpent(clock, 0);
				cout << endl;
			}
			catch (string s) {
				auto pieceString = pieceList.getPieceString();
				 cout << "Exception during verifyBitbase for " << pieceString << ": " << s << endl;
				 if (++_exceptionCounter == 10) {
					 cout << "Further exceptions will be suppressed." << endl;
				 }
				 // Continue with other bitbases even if one fails, to get a more complete picture of potential issues.
				 // Do not re-throw the exception to avoid terminating the entire verification process.
				 // throw; --- IGNORE ---
				cout << "Exception during verifyBitbase: " << s << endl;
			}

		}

		/**
		 * Recursively computes bitbases based on a bitbase string
		 * For KQKP it will compute KQK, KQKQ, KQKR, KQKB, KQKN, ...
		 * so that any bitbase KQKP can get to is available
		 */
		void verifyBitbaseRec(string pieceString, bool first) {
			size_t index = pieceString.find('*');
			Bitbase::setCacheSize(1024);
			if (index != string::npos) {
				for (auto ch : { 'Q', 'R', 'B', 'N', 'P' }) {
					pieceString[index] = ch;
					verifyBitbaseRec(pieceString, false);
				}
			} 
			else {
				PieceList list(pieceString);
				string corrected = list.getPieceString();
				if (find(_verified.begin(), _verified.end(), corrected) == _verified.end()) {
					try {
						verifyBitbase(list);
						_verified.push_back(pieceString);
					}
					catch (string s) {
						cout << s << endl;
					}
				}
			}
		}

		uint32_t _cores;
		bool _uncompressed;
		int _traceLevel;
		int _debugLevel;
		uint64_t _exceptionCounter = 0;
		std::atomic<uint64_t> _errors{0};
		std::mutex _mtxOutput;
		vector<string> _verified;

		static constexpr uint32_t MAX_THREADS = 64;
		array<thread, MAX_THREADS> _threads;
	};

}

#endif // __BITBASEVERIFY_H
