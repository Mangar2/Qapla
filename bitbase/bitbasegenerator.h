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
#include "../search/quiescencesearch.h"
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
				positionValue = BitbaseReader::getValueFromBitbase(position, position.pieceSignature, 0);
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


		bool computeValue(MoveGenerator& position, Bitbase& bitBase, bool verbose) {
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
					index = BoardAccess::getIndex(!whiteToMove, pieceList, move);
					result = bitBase.getBit(index);
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

		uint32_t computePosition(uint64_t index, MoveGenerator& position, Bitbase& bitBase, Bitbase& computedPositions) {
			uint32_t result = 0;
			if (computeValue(position, bitBase, false)) {
				if (index == debugIndex) {
					computeValue(position, bitBase, true);
				}
				bitBase.setBit(index);
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

		uint32_t initialComputePosition(uint64_t index, MoveGenerator& position, Eval& eval, Bitbase& bitBase, Bitbase& computedPositions) {
			uint32_t result = 0;
			MoveList moveList;
			bool kingInCheck = false;
			if (index == debugIndex) {
				kingInCheck = false;
			}

			if (position.isIllegalPosition2()) {
				amountOfIllegalPositions++;
				computedPositions.setBit(index);
				return 0;
			}

			position.genMovesOfMovingColor(moveList);
			if (moveList.getTotalMoveAmount() > 0) {
				value_t positionValue;

				positionValue = initialSearch(position);
				if (positionValue == 1) {
					bitBase.setBit(index);
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
					bitBase.setBit(index);
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

		void addPiecesToBoard(MoveGenerator& position, const BitbaseIndex& bitBaseIndex, const PieceList& pieceList) {
			position.setPiece(bitBaseIndex.getPiecePos(0), WHITE_KING);
			position.setPiece(bitBaseIndex.getPiecePos(1), BLACK_KING);
			for (uint32_t pieceNo = 0; pieceNo < pieceList.getPieceAmount(); pieceNo++) {
				position.setPiece(bitBaseIndex.getPiecePos(2 + pieceNo), pieceList.getPiece(pieceNo));
			}
			position.setWhiteToMove(bitBaseIndex.getWhiteToMove());
		}

		void removePiecesFromBoard(MoveGenerator& position, const BitbaseIndex& bitBaseIndex) {
			for (uint32_t pieceNo = 0; pieceNo < bitBaseIndex.getPieceAmount(); pieceNo++) {
				position.removePieceFromPosition(bitBaseIndex.getPiecePos(pieceNo));
			}
		}

		void compareBitbases(const char* pieceString, Bitbase& bitbase1, Bitbase& bitbase2) {
			MoveGenerator position;
			PieceList pieceList(pieceString);
			uint64_t sizeInBit = bitbase1.getSizeInBit();
			for (uint64_t index = 0; index < sizeInBit; index++) {
				bool newResult = bitbase1.getBit(index);
				bool oldResult = bitbase2.getBit(index);
				if (bitbase1.getBit(index) != bitbase2.getBit(index)) {
					BitbaseIndex bitBaseIndex;
					bitBaseIndex.setPiecePositionsByIndex(index, pieceList);
					addPiecesToBoard(position, bitBaseIndex, pieceList);
					printf("new: %s, old: %s\n", newResult ? "won" : "not won", oldResult ? "won" : "not won");
					printf("index: %lld\n", index);
					position.print();
					position.clear();
				}
			}
		}

		void compareFiles(string pieceString) {
			Bitbase bitBase1;
			Bitbase bitBase2;
			bitBase1.readFromFile(pieceString);
			bitBase2.readFromFile("generated\\" + pieceString);
			compareBitbases(pieceString.c_str(), bitBase1, bitBase2);
		}

		void printTimeSpent(ThinkingTimeManager& timeManager) {
			uint64_t timeInMilliseconds = timeManager.getTimeSpentInMilliseconds();
			printf("Time spend: %lld:%lld:%lld.%lld\n",
				timeInMilliseconds / (60 * 60 * 1000),
				(timeInMilliseconds / (60 * 1000)) % 60,
				(timeInMilliseconds / 1000) % 60,
				timeInMilliseconds % 1000);
		}


		void computeBitbase(const char* pieceString) {
			PieceList pieceList(pieceString);
			computeBitbaseRec(pieceList);
		}

		void computeBitbaseRec(PieceList& pieceList) {
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

		void computeBitbase(PieceList& pieceList) {
			MoveGenerator position;
			string pieceString = pieceList.getPieceString();
			printf("Computing bitbase for %s\n", pieceString.c_str());
			if (BitbaseReader::isBitbaseAvailable(pieceString)) {
				printf("Bitbase already loaded\n");
				return;
			}
			BitbaseIndex bitBaseIndex;
			uint32_t bitsChanged = 0;
			bitBaseIndex.setPiecePositionsByIndex(0, pieceList);
			uint64_t sizeInBit = bitBaseIndex.getSizeInBit();
			Bitbase bitBase(bitBaseIndex);
			Bitbase computedPositions(bitBaseIndex);
			ThinkingTimeManager timeManager;

			timeManager.startTimer();
			position.clear();
			amountOfIllegalPositions = 0;
			amountOfDirectDrawOrLoss = 0;
			for (uint64_t index = 0; index < sizeInBit; index++) {
				printInfo(index, sizeInBit, bitsChanged);
				if (bitBaseIndex.setPiecePositionsByIndex(index, pieceList)) {
					addPiecesToBoard(position, bitBaseIndex, pieceList);
					bitsChanged += initialComputePosition(index, position, eval, bitBase, computedPositions);
					removePiecesFromBoard(position, bitBaseIndex);
				}
			}
			printf("\nInitial, positions set: %ld\n", bitsChanged);
			printTimeSpent(timeManager);

			for (uint32_t loopCount = 0; loopCount < 100; loopCount++) {
				bitsChanged = 0;
				for (uint64_t index = 0; index < sizeInBit; index++) {
					printInfo(index, sizeInBit, bitsChanged);

					if (!computedPositions.getBit(index) && bitBaseIndex.setPiecePositionsByIndex(index, pieceList)) {
						addPiecesToBoard(position, bitBaseIndex, pieceList);
						assert(index == BoardAccess::getIndex(position));
						bitsChanged += computePosition(index, position, bitBase, computedPositions);
						//if (bitsChanged == 0) { position.printBoard(); }
						removePiecesFromBoard(position, bitBaseIndex);
					}
				}

				printf("\nLoop: %ld, positions set: %ld\n", loopCount, bitsChanged);
				printTimeSpent(timeManager);
				if (bitsChanged == 0) {
					break;
				}
			}
			//compareToFile(pieceString, bitBase);
			bitBase.printStatistic();
			printf("Illegal positions: %lld, draw or loss found in quiescense: %lld, sum: %lld \n",
				amountOfIllegalPositions, amountOfDirectDrawOrLoss, amountOfDirectDrawOrLoss + amountOfIllegalPositions);
			printTimeSpent(timeManager);
			string fileName = pieceString + string(".btb");
			bitBase.storeToFile(fileName.c_str());
			BitbaseReader::setBitbase(pieceString, bitBase);
			//testAllKPK(bitBase);
		}

		void testBoard(MoveGenerator& position, Square wk, Square bk, bool wtm, bool expected, Bitbase& bitBase) {
			position.setPiece(wk, WHITE_KING);
			position.setPiece(bk, BLACK_KING);
			position.setWhiteToMove(wtm);
			uint64_t checkIndex = BoardAccess::getIndex(position);
			if (bitBase.getBit(checkIndex) == expected) {
				printf("Test OK\n");
			}
			else {
				printf("%s, index:%lld\n", position.isWhiteToMove() ? "white" : "black", checkIndex);
				printf("Test failed\n");
				position.print();
				computeValue(position, bitBase, true);
				showDebugIndex("KRKP");
			}
		}

		struct piecePos {
			Piece piece;
			Square pos;
		};

		void testKQKR(Square wk, Square bk, Square q, Square r, bool wtm, bool expected, Bitbase& bitBase) {
			MoveGenerator position;
			position.setPiece(q, WHITE_QUEEN);
			position.setPiece(r, BLACK_ROOK);
			testBoard(position, wk, bk, wtm, expected, bitBase);
		}

		void testKRKQ(Square wk, Square bk, Square r, Square q, bool wtm, bool expected, Bitbase& bitBase) {
			MoveGenerator position;
			position.setPiece(q, BLACK_QUEEN);
			position.setPiece(r, WHITE_ROOK);
			testBoard(position, wk, bk, wtm, expected, bitBase);
		}


		void testAllKQKR() {
			Bitbase bitBase;
			bitBase.readFromFile("KQKR.btb_generated");
			testKQKR(B7, B1, C8, A2, false, false, bitBase);
			testKQKR(B7, B1, D8, A2, false, true, bitBase);
			testKQKR(B7, B1, C8, A3, false, true, bitBase);
			testKQKR(B7, B1, C8, A2, true, true, bitBase);
		}


		void testKPK(Square wk, Square bk, Square p, bool wtm, bool expected, Bitbase& bitBase) {
			MoveGenerator position;
			position.setPiece(p, WHITE_PAWN);
			testBoard(position, wk, bk, wtm, expected, bitBase);
		}

		void testAllKPK(Bitbase& bitBase) {
			testKPK(E5, E8, E7, false, false, bitBase);
			testKPK(D6, E8, E7, true, false, bitBase);
			testKPK(A2, A5, E2, true, true, bitBase);
			testKPK(A2, A5, E2, false, false, bitBase);
			testKPK(A3, A5, E2, true, false, bitBase);
			testKPK(A3, A5, E2, false, true, bitBase);
		}

		void testAllKPK() {
			Bitbase bitBase;
			bitBase.readFromFile("KPK");
			testAllKPK(bitBase);
		}

		void testKRKP(Square wk, Square bk, Square p, Square r, bool wtm, bool expected, Bitbase& bitBase) {
			MoveGenerator position;
			position.setPiece(wk, WHITE_KING);
			position.setPiece(bk, BLACK_KING);
			position.setPiece(p, BLACK_PAWN);
			position.setPiece(r, WHITE_ROOK);
			position.setWhiteToMove(wtm);
			uint64_t checkIndex = BoardAccess::getIndex(position);
			if (bitBase.getBit(checkIndex) == expected) {
				printf("Test OK\n");
			}
			else {
				position.print();
				printf("%s, index:%lld\n", position.isWhiteToMove() ? "white" : "black", checkIndex);
				printf("Test failed\n");
				computeValue(position, bitBase, true);
				showDebugIndex("KRKP");

			}
		}

		void showDebugIndex(string pieceString) {
			MoveGenerator position;
			Bitbase bitBase;
			bitBase.readFromFile(pieceString);
			PieceList pieceList(pieceString.c_str());
			BitbaseIndex bitBaseIndex;
			uint32_t index;
			while (true) {
				cin >> index;
				printf("%ld\n", index);
				if (bitBaseIndex.setPiecePositionsByIndex(index, pieceList)) {
					position.clear();
					addPiecesToBoard(position, bitBaseIndex, pieceList);
					printf("index:%lld, result for white %s\n", index, bitBase.getBit(index) ? "win" : "draw");

					computeValue(position, bitBase, true);
				}
				else {
					break;
				}
			}
		}


		void testAllKRKP() {
			Bitbase bitBase;
			bitBase.readFromFile("KRKP");
			testKRKP(F2, D4, F6, F3, true, true, bitBase);
			testKRKP(F2, D4, F6, F3, false, true, bitBase);
			testKRKP(A7, G4, F2, B5, false, false, bitBase);
			testKRKP(A7, G4, F2, B5, true, false, bitBase);
			testKRKP(A7, G4, F5, B5, true, true, bitBase);
			testKRKP(A7, G4, F5, B5, false, false, bitBase);
			testKRKP(C3, C1, D2, A2, false, false, bitBase);
		}

		void testAllKRKQ() {
			Bitbase bitBase;
			bitBase.readFromFile("KRKQ");
			testKRKQ(C3, C1, A2, D1, false, false, bitBase);
		}


		Eval eval;
		uint64_t amountOfIllegalPositions;
		uint64_t amountOfDirectDrawOrLoss;
	};

}

#endif // __BITBASEGENERATOR_H