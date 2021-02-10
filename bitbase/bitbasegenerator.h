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
 * Tool to generate bitbases
 */


#ifndef __BITBASEGENERATOR_H
#define __BITBASEGENERATOR_H

#include "bitbase.h"
#include "bitbaseindex.h"
#include "bitbasereader.h"
#include "../search/quiescencesearch.h"
#include "../search/clockmanager.h"
#include "../movegenerator/movegenerator.h"
//#include "ThinkingTimeManager.h"
#include <iostream>
using namespace std;
using namespace ChessMoveGenerator;
using namespace ChessSearch;

namespace ChessBitbase {
	class BitbaseGenerator {
	public:
		//static const uint64_t debugIndex =  2194203;
		static const uint64_t debugIndex = 0xFFFFFFFFFFFFFFFF;
		BitbaseGenerator() {
		}

		/**
		 * Searches the best move based on bitbases
		 */
		static value_t search(MoveGenerator& position, Eval& eval, Move lastMove, value_t alpha, value_t beta, int32_t depth)
		{
			MoveProvider moveProvider;
			Move move;
			BoardState boardState = position.getBoardState();
			if (lastMove != 0) {
				position.doMove(lastMove);
			}
			value_t newValue;
			value_t curValue = eval.eval(position, -MAX_VALUE);
			if (!position.isWhiteToMove()) {
				curValue = -curValue;
			}

			if (depth > 0) {
				moveProvider.computeMoves(position, Move::EMPTY_MOVE);
				if (moveProvider.getTotalMoveAmount() == 0) {
					curValue = moveProvider.checkForGameEnd(position, 0);
				}
			}
			else {
				moveProvider.computeCaptures(position, Move::EMPTY_MOVE);
			}

			if (curValue < beta) {
				if (curValue > alpha) {
					alpha = curValue;
				}

				while (!(move = moveProvider.selectNextMove(position)).isEmpty()) {

					if (move.getCapture() == BLACK_KING) {
						curValue = 0;
						break;
					}
					newValue = -search(position, eval, move, -beta, -alpha, depth - 1);

					if (newValue > curValue) {
						curValue = newValue;
						if (newValue >= beta) {
							break;
						}
					}
				}
			}

			if (lastMove != 0) {
				position.undoMove(lastMove, boardState);
			}
			return curValue;

		}

		value_t initialSearch(MoveGenerator& position)
		{
			MoveProvider moveProvider;
			Move move;

			value_t positionValue = 0;
			value_t result = 0;
			BoardState boardState = position.getBoardState();

			position.computeAttackMasksForBothColors();
			moveProvider.computeCaptures(position, Move::EMPTY_MOVE);

			while (!(move = moveProvider.selectNextMove(position)).isEmpty()) {
				if (!move.isCaptureOrPromote()) {
					continue;
				}
				position.doMove(move);
				positionValue = BitbaseReader::getValueFromBitbase(position, 0);
				position.undoMove(move, boardState);

				if (position.isWhiteToMove() && positionValue >= WINNING_BONUS) {
					result = 1;
					break;
				}
				if (!position.isWhiteToMove() && positionValue < WINNING_BONUS) {
					result = -1;
					break;
				}
			}
			return result;
		}

		/**
		 * Quiescense search for bitboard generation
		 */
		value_t quiescense(MoveGenerator& position) {
			value_t result = 0;
			ComputingInfo computingInfo;
			value_t positionValue = QuiescenceSearch::search(position, computingInfo, Move::EMPTY_MOVE, -MAX_VALUE, MAX_VALUE, 0);
			if (!position.isWhiteToMove()) {
				positionValue = -positionValue;
			}
			if (positionValue > WINNING_BONUS) {
				result = 1;
			}

			else if (positionValue <= 1) {
				result = -1;
			}
			else {
				result = 0;
			}

			return result;
		}


		bool computeValue(MoveGenerator& position, Bitbase& bitbase, bool verbose) {
			MoveList moveList;
			Move move;
			bool whiteToMove = position.isWhiteToMove();
			bool result = position.isWhiteToMove() ? false : true;
			uint64_t index = 0;
			PieceList pieceList(position);

			if (verbose) {
				position.print();
				printf("%s\n", position.isWhiteToMove() ? "white" : "black");
			}

			position.genMovesOfMovingColor(moveList);

			for (uint32_t moveNo = 0; moveNo < moveList.getTotalMoveAmount(); moveNo++) {
				move = moveList[moveNo];
				if (!move.isCaptureOrPromote()) {
					index = BoardAccess::computeIndex(!whiteToMove, pieceList, move);
					result = bitbase.getBit(index);
					if (verbose) {
						printf("%s, index: %lld, value: %s\n", move.getLAN().c_str(), 
							index, result ? "win" : "draw or unknown");
					}
				}
				if (whiteToMove && result) {
					break;
				}
				if (!whiteToMove && !result) {
					break;
				}

			}
			return result;
		}

		uint32_t computePosition(uint64_t index, MoveGenerator& position, Bitbase& bitbase, Bitbase& computedPositions) {
			uint32_t result = 0;
			if (computeValue(position, bitbase, false)) {
				if (index == debugIndex) {
					computeValue(position, bitbase, true);
				}
				bitbase.setBit(index);
				computedPositions.setBit(index);
				result++;
			}

			return result;
		}

		/**
		 * Checks, if a piece is a king
		 */
		static bool isKing(Piece piece) {
			return (piece & ~COLOR_MASK) == KING;
		}

		/**
		 * Check for an illigal king capture
		 */
		bool kingCaptureFound(MoveList& moveList) {
			bool result = false;
			for (uint32_t index = 0; index < moveList.getTotalMoveAmount(); index++) {
				if (isKing(moveList[index].getCapture())) {
					return true;
					break;
				}
			}
			return result;
		}

		/**
		 * 
		 */
		uint32_t initialComputePosition(uint64_t index, MoveGenerator& position, 
			Bitbase& bitbase, Bitbase& computedPositions) {
			uint32_t result = 0;
			MoveList moveList;
			bool kingInCheck = false;
			if (index == debugIndex) {
				kingInCheck = false;
			}

			if (!position.isLegalPosition()) {
				amountOfIllegalPositions++;
				computedPositions.setBit(index);
				return 0;
			}

			position.genMovesOfMovingColor(moveList);
			if (moveList.getTotalMoveAmount() > 0) {
				value_t positionValue;

				positionValue = initialSearch(position);
				if (positionValue == 1) {
					bitbase.setBit(index);
					computedPositions.setBit(index);
					result++;
				}
				else if (positionValue == -1) {
					computedPositions.setBit(index);
					amountOfDirectDrawOrLoss++;
				}
			}
			else {
				computedPositions.setBit(index);
				if (!position.isWhiteToMove() && position.isInCheck()) {
					if (index == debugIndex) {
						position.print();
						printf("index: %lld, mate detected\n", index);
					}
					bitbase.setBit(index);
					result++;
				}
				else {
					amountOfDirectDrawOrLoss++;
				}
			}
			return result;
		}


		void printInfo(uint64_t index, uint64_t size, uint32_t bitsChanged, uint32_t step = 1) {
			uint64_t onePercent = size / 100;
			if (index % onePercent >= onePercent - step) {
				printf(".");
			}
		}

		void addPiecesToPosition(MoveGenerator& position, const BitbaseIndex& bitbaseIndex, const PieceList& pieceList) {
			position.setPiece(bitbaseIndex.getPieceSquare(0), WHITE_KING);
			position.setPiece(bitbaseIndex.getPieceSquare(1), BLACK_KING);
			for (uint32_t pieceNo = 0; pieceNo < pieceList.getNumberOfPieces(); pieceNo++) {
				position.setPiece(bitbaseIndex.getPieceSquare(2 + pieceNo), pieceList.getPiece(pieceNo));
			}
			position.setWhiteToMove(bitbaseIndex.isWhiteToMove());
		}

		/**
		 * Prints the difference of two bitbases
		 */
		void compareBitbases(const char* pieceString, Bitbase& bitbase1, Bitbase& bitbase2) {
			MoveGenerator position;
			PieceList pieceList(pieceString);
			uint64_t sizeInBit = bitbase1.getSizeInBit();
			for (uint64_t index = 0; index < sizeInBit; index++) {
				bool newResult = bitbase1.getBit(index);
				bool oldResult = bitbase2.getBit(index);
				if (bitbase1.getBit(index) != bitbase2.getBit(index)) {
					BitbaseIndex bitbaseIndex;
					bitbaseIndex.setPieceSquaresByIndex(index, pieceList);
					addPiecesToPosition(position, bitbaseIndex, pieceList);
					printf("new: %s, old: %s\n", newResult ? "won" : "not won", oldResult ? "won" : "not won");
					printf("index: %lld\n", index);
					position.print();
					position.clear();
				}
			}
		}

		/**
		 * Prints the differences of two bitbases in files
		 */
		void compareFiles(string pieceString) {
			Bitbase bitbase1;
			Bitbase bitbase2;
			bitbase1.readFromFile(pieceString);
			bitbase2.readFromFile("generated\\" + pieceString);
			compareBitbases(pieceString.c_str(), bitbase1, bitbase2);
		}

		/**
		 * Prints the time spent so far
		 */
		void printTimeSpent(ClockManager& clock) {
			uint64_t timeInMilliseconds = clock.getTimeSpentInMilliseconds();
			printf("Time spend: %lld:%lld:%lld.%lld\n",
				timeInMilliseconds / (60 * 60 * 1000),
				(timeInMilliseconds / (60 * 1000)) % 60,
				(timeInMilliseconds / 1000) % 60,
				timeInMilliseconds % 1000);
		}

		/**
		 * Recursively computes bitbases based on a bitbase string
		 * For KQKP it will compute KQK, KQKQ, KQKR, KQKB, KQKN, ... 
		 * so that any bitbase KQKP can get to is available
		 */
		void computeBitbaseRec(PieceList& pieceList) {
			if (pieceList.getNumberOfPieces() == 0) return;
			string pieceString = pieceList.getPieceString();
			if (!BitbaseReader::isBitbaseAvailable(pieceString)) {
				BitbaseReader::loadBitbase(pieceString);
			}
			if (!BitbaseReader::isBitbaseAvailable(pieceString)) {
				if (pieceList.getNumberOfPieces() >= 1) {
					for (uint32_t pieceNo = 0; pieceNo < pieceList.getNumberOfPieces(); pieceNo++) {
						PieceList newPieceList(pieceList);
						if (isPawn(newPieceList.getPiece(pieceNo))) {
							for (Piece piece = QUEEN; piece >= KNIGHT; piece -= 2) {
								newPieceList.promotePawn(pieceNo, piece);
								computeBitbaseRec(newPieceList);
								newPieceList = pieceList;
							}
						}
						newPieceList.removePiece(pieceNo);
						computeBitbaseRec(newPieceList);
					}
				}
				computeBitbase(pieceList);
			}
		}

		/**
		 * Computes a bitbase for a set of pieces described by a piece list.
		 */
		void computeBitbase(PieceList& pieceList) {
			MoveGenerator position;
			string pieceString = pieceList.getPieceString();
			printf("Computing bitbase for %s\n", pieceString.c_str());
			if (BitbaseReader::isBitbaseAvailable(pieceString)) {
				printf("Bitbase already loaded\n");
				return;
			}
			BitbaseIndex bitbaseIndex;
			uint32_t bitsChanged = 0;
			bitbaseIndex.setPieceSquaresByIndex(0, pieceList);
			uint64_t sizeInBit = bitbaseIndex.getSizeInBit();
			Bitbase bitbase(bitbaseIndex);
			Bitbase computedPositions(bitbaseIndex);
			ClockManager clock;

			clock.setStartTime();
			amountOfIllegalPositions = 0;
			amountOfDirectDrawOrLoss = 0;
			for (uint64_t index = 0; index < sizeInBit; index++) {
				printInfo(index, sizeInBit, bitsChanged);
				if (bitbaseIndex.setPieceSquaresByIndex(index, pieceList)) {
					position.clear();
					addPiecesToPosition(position, bitbaseIndex, pieceList);
					bitsChanged += initialComputePosition(index, position, bitbase, computedPositions);
				}
			}
			cout << endl
				<< "Amount of direct draw or loss: " << amountOfDirectDrawOrLoss << endl
				<< "Initial winning positions: " << bitsChanged << endl;
			printTimeSpent(clock);

			for (uint32_t loopCount = 0; loopCount < 100; loopCount++) {
				bitsChanged = 0;
				for (uint64_t index = 0; index < sizeInBit; index++) {
					printInfo(index, sizeInBit, bitsChanged);

					if (!computedPositions.getBit(index) && bitbaseIndex.setPieceSquaresByIndex(index, pieceList)) {
						position.clear();
						addPiecesToPosition(position, bitbaseIndex, pieceList);
						assert(index == BoardAccess::computeIndex<0>(position));
						bitsChanged += computePosition(index, position, bitbase, computedPositions);
						//if (bitsChanged == 0) { position.printBoard(); }
					}
				}

				printf("\nLoop: %ld, positions set: %ld\n", loopCount, bitsChanged);
				printTimeSpent(clock);
				if (bitsChanged == 0) {
					break;
				}
			}
			//compareToFile(pieceString, bitbase);
			bitbase.printStatistic();
			printf("Illegal positions: %lld, draw or loss found in quiescense: %lld, sum: %lld \n",
				amountOfIllegalPositions, amountOfDirectDrawOrLoss, amountOfDirectDrawOrLoss + amountOfIllegalPositions);
			printTimeSpent(clock);
			string fileName = pieceString + string(".btb");
			bitbase.storeToFile(fileName.c_str());
			BitbaseReader::setBitbase(pieceString, bitbase);
		}

		/**
		 * Computes a bitbase for a piece string (example KPK)
		 */
		void computeBitbase(string pieceString) {
			PieceList list(pieceString);
			computeBitbase(list);
		}

		/**
		 * Computes a bitbase for a piece string (example KPK) and 
		 * all other bitbases needed
		 */
		void computeBitbaseRec(string pieceString) {
			PieceList list(pieceString);
			computeBitbaseRec(list);
		}

		/**
		 * Test a position
		 * @param position chess position without kings
		 * @param wk square of white king
		 * @param bk square of black king
		 * @param wtm white to move
		 * @param expected expected value for the position (win or non win)
		 * @param bitbase bit base to test
		 */
		void testBoard(MoveGenerator& position, Square wk, Square bk, bool wtm, bool expected, Bitbase& bitbase) {
			position.setPiece(wk, WHITE_KING);
			position.setPiece(bk, BLACK_KING);
			position.setWhiteToMove(wtm);
			uint64_t checkIndex = BoardAccess::computeIndex<0>(position);
			if (bitbase.getBit(checkIndex) == expected) {
				printf("Test OK\n");
			}
			else {
				printf("%s, index:%lld\n", position.isWhiteToMove() ? "white" : "black", checkIndex);
				printf("Test failed\n");
				position.print();
				computeValue(position, bitbase, true);
				showDebugIndex("KRKP");
			}
		}

		struct piecePos {
			Piece piece;
			Square pos;
		};

		void testKQKR(Square wk, Square bk, Square q, Square r, bool wtm, bool expected, Bitbase& bitbase) {
			MoveGenerator position;
			position.setPiece(q, WHITE_QUEEN);
			position.setPiece(r, BLACK_ROOK);
			testBoard(position, wk, bk, wtm, expected, bitbase);
		}

		void testKRKQ(Square wk, Square bk, Square r, Square q, bool wtm, bool expected, Bitbase& bitbase) {
			MoveGenerator position;
			position.setPiece(q, BLACK_QUEEN);
			position.setPiece(r, WHITE_ROOK);
			testBoard(position, wk, bk, wtm, expected, bitbase);
		}

		void testAllKQKR() {
			Bitbase bitbase;
			bitbase.readFromFile("KQKR.btb_generated");
			testKQKR(B7, B1, C8, A2, false, false, bitbase);
			testKQKR(B7, B1, D8, A2, false, true, bitbase);
			testKQKR(B7, B1, C8, A3, false, true, bitbase);
			testKQKR(B7, B1, C8, A2, true, true, bitbase);
		}

		void testKPK(Square wk, Square bk, Square p, bool wtm, bool expected, Bitbase& bitbase) {
			MoveGenerator position;
			position.setPiece(p, WHITE_PAWN);
			testBoard(position, wk, bk, wtm, expected, bitbase);
		}

		void testAllKPK(Bitbase& bitbase) {
			testKPK(E5, E8, E7, false, false, bitbase);
			testKPK(D6, E8, E7, true, false, bitbase);
			testKPK(A2, A5, E2, true, true, bitbase);
			testKPK(A2, A5, E2, false, false, bitbase);
			testKPK(A3, A5, E2, true, false, bitbase);
			testKPK(A3, A5, E2, false, true, bitbase);
		}

		void testAllKPK() {
			Bitbase bitbase;
			bitbase.readFromFile("KPK");
			testAllKPK(bitbase);
		}

		void testKRKP(Square wk, Square bk, Square p, Square r, bool wtm, bool expected, Bitbase& bitbase) {
			MoveGenerator position;
			position.setPiece(wk, WHITE_KING);
			position.setPiece(bk, BLACK_KING);
			position.setPiece(p, BLACK_PAWN);
			position.setPiece(r, WHITE_ROOK);
			position.setWhiteToMove(wtm);
			uint64_t checkIndex = BoardAccess::computeIndex<0>(position);
			if (bitbase.getBit(checkIndex) == expected) {
				printf("Test OK\n");
			}
			else {
				position.print();
				printf("%s, index:%lld\n", position.isWhiteToMove() ? "white" : "black", checkIndex);
				printf("Test failed\n");
				computeValue(position, bitbase, true);
				showDebugIndex("KRKP");

			}
		}

		void showDebugIndex(string pieceString) {
			MoveGenerator position;
			Bitbase bitbase;
			bitbase.readFromFile(pieceString);
			PieceList pieceList(pieceString.c_str());
			BitbaseIndex bitbaseIndex;
			uint64_t index;
			while (true) {
				cin >> index;
				printf("%lld\n", index);
				if (bitbaseIndex.setPieceSquaresByIndex(index, pieceList)) {
					position.clear();
					addPiecesToPosition(position, bitbaseIndex, pieceList);
					printf("index:%lld, result for white %s\n", index, bitbase.getBit(index) ? "win" : "draw");

					computeValue(position, bitbase, true);
				}
				else {
					break;
				}
			}
		}


		void testAllKRKP() {
			Bitbase bitbase;
			bitbase.readFromFile("KRKP");
			testKRKP(F2, D4, F6, F3, true, true, bitbase);
			testKRKP(F2, D4, F6, F3, false, true, bitbase);
			testKRKP(A7, G4, F2, B5, false, false, bitbase);
			testKRKP(A7, G4, F2, B5, true, false, bitbase);
			testKRKP(A7, G4, F5, B5, true, true, bitbase);
			testKRKP(A7, G4, F5, B5, false, false, bitbase);
			testKRKP(C3, C1, D2, A2, false, false, bitbase);
		}

		void testAllKRKQ() {
			Bitbase bitbase;
			bitbase.readFromFile("KRKQ");
			testKRKQ(C3, C1, A2, D1, false, false, bitbase);
		}


		Eval eval;
		uint64_t amountOfIllegalPositions;
		uint64_t amountOfDirectDrawOrLoss;
	};

}

#endif // __BITBASEGENERATOR_H