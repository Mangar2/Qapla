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
#include "../search/clockmanager.h"
#include "../movegenerator/movegenerator.h"
#include <iostream>

using namespace std;
using namespace ChessMoveGenerator;
using namespace ChessSearch;

namespace QaplaBitbase {
	class BitbaseGenerator {
	public:
		// static const uint64_t debugIndex = 21024;
		static const uint64_t debugIndex = 0xFFFFFFFFFFFFFFFF;
		BitbaseGenerator() {
		}

		/**
		 * Clears the situation counter
		 */
		void clear() {
			_numberOfDirectLoss = 0;
			_numberOfDirectDraw = 0;
			_numberOfIllegalPositions = 0;
		}

		/**
		 * Searches all captures and look up the position after capture in a bitboard.
		 */
		value_t initialSearch(MoveGenerator& position)
		{
			MoveProvider moveProvider;
			Move move;

			value_t result = 0;
			BoardState boardState = position.getBoardState();

			position.computeAttackMasksForBothColors();
			moveProvider.computeCaptures(position, Move::EMPTY_MOVE);

			while (!(move = moveProvider.selectNextMove(position)).isEmpty()) {
				if (!move.isCaptureOrPromote()) {
					continue;
				}
				position.doMove(move);
				const bool isWhiteWin = BitbaseReader::getValueFromSingleBitbase(position) == Result::Win;
				position.undoMove(move, boardState);

				if (position.isWhiteToMove() && isWhiteWin) {
					result = 1;
					break;
				}

				if (!position.isWhiteToMove() && !isWhiteWin) {
					result = -1;
					break;
				}
			}
			return result;
		}

		/**
		 * Marks one candidate identified by a partially filled move and a destination square
		 */
		void markCandidate(bool wtm, Bitbase& bitbase, PieceList& list, Move move, Square destination) {
			move.setDestination(destination);
			uint64_t index = BoardAccess::getIndex(!wtm, list, move);
			bitbase.setBit(index);
		}

		/**
		 * Mark candidates for a dedicated piece identified by a partially filled move
		 */
		void markCandidates(const MoveGenerator& position, Bitbase& bitbase, PieceList& list, Move move) {
			bitBoard_t attackBB = position.pieceAttackMask[move.getDeparture()];
			const bool wtm = position.isWhiteToMove();
			if (move.getMovingPiece() == WHITE_KING) {
				attackBB &= ~position.pieceAttackMask[position.getKingSquare<BLACK>()];
			}
			if (move.getMovingPiece() == BLACK_KING) {
				attackBB &= ~position.pieceAttackMask[position.getKingSquare<WHITE>()];
			}
			if (move.getMovingPiece() == WHITE_PAWN && move.getDeparture() >= A3) {
				markCandidate(wtm, bitbase, list, move, move.getDeparture() - NORTH);
				if (getRank(move.getDeparture()) == Rank::R4) {
					markCandidate(wtm, bitbase, list, move, move.getDeparture() - 2 * NORTH);
				}
			}
			if (move.getMovingPiece() == BLACK_PAWN && move.getDeparture() <= H6) {
				markCandidate(wtm, bitbase, list, move, move.getDeparture() + NORTH);
				if (getRank(move.getDeparture()) == Rank::R5) {
					markCandidate(wtm, bitbase, list, move, move.getDeparture() + 2 * NORTH);
				}
			}
			if (getPieceType(move.getMovingPiece()) != PAWN) {
				for (; attackBB; attackBB &= attackBB - 1) {
					const Square destination = lsb(attackBB);
					const bool occupied = position.getAllPiecesBB() & (1ULL << destination);
					if (occupied) {
						continue;
					}
					markCandidate(wtm, bitbase, list, move, destination);
				}
			}
		}

		/**
		 * Mark all candidate positions we need to look at after a new bitbase position is set to 1
		 * Candidate positions are computed by running through the attack masks of every piece and 
		 * computing reverse moves (ignoring all special cases like check, captures, ...)
		 */
		void markCandidates(MoveGenerator& position, Bitbase& bitbase) {
			PieceList pieceList(position);
			position.computeAttackMasksForBothColors();
			Piece piece = PAWN + int(position.isWhiteToMove());
			for (; piece <= BLACK_KING; piece += 2) {
				bitBoard_t pieceBB = position.getPieceBB(piece);
				Move move;
				move.setMovingPiece(piece);
				for (; pieceBB; pieceBB &= pieceBB - 1) {
					Square departure = lsb(pieceBB);
					move.setDeparture(departure);
					markCandidates(position, bitbase, pieceList, move);
				}
			}
		}

		/**
		 * Computes a position value by probing all moves and lookup the result in this bitmap
		 * Captures are excluded, they have been tested in the initial search. 
		 */
		bool computeValue(MoveGenerator& position, Bitbase& bitbase, bool verbose) {
			MoveList moveList;
			Move move;
			bool whiteToMove = position.isWhiteToMove();
			bool result = position.isWhiteToMove() ? false : true;
			uint64_t index = 0;
			PieceList pieceList(position);

			if (verbose) {
				cout << endl << "index: " << BoardAccess::getIndex<0>(position) << endl;
				position.print();
				printf("%s\n", position.isWhiteToMove() ? "white" : "black");
			}

			position.genMovesOfMovingColor(moveList);

			for (uint32_t moveNo = 0; moveNo < moveList.getTotalMoveAmount(); moveNo++) {
				move = moveList[moveNo];
				if (!move.isCaptureOrPromote()) {
					index = BoardAccess::getIndex(!whiteToMove, pieceList, move);
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
		 * Initially probe alle positions for a mate- draw or capture situation 
		 */
		uint32_t initialComputePosition(uint64_t index, MoveGenerator& position, 
			Bitbase& bitbase, Bitbase& computedPositions) {
			uint32_t result = 0;
			MoveList moveList;
			bool kingInCheck = false;
			if (index == debugIndex) {
				kingInCheck = false;
			}

			// Exclude all illegal positions (king not to move is in check) from future search
			if (!position.isLegalPosition()) {
				_numberOfIllegalPositions++;
				computedPositions.setBit(index);
				return 0;
			}

			position.genMovesOfMovingColor(moveList);
			if (moveList.getTotalMoveAmount() > 0) {
				value_t positionValue;
				// Compute all captures and look up the positions in other bitboards
				positionValue = initialSearch(position);
				if (positionValue == 1) {
					bitbase.setBit(index);
					computedPositions.setBit(index);
					result++;
				}
				else if (positionValue == -1) {
					computedPositions.setBit(index);
					_numberOfDirectLoss++;
				}
			}
			else {
				// Mate or stalemate
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
					_numberOfDirectDraw++;
				}
			}
			return result;
		}


		void printInfo(uint64_t index, uint64_t size, uint64_t bitsChanged, uint32_t step = 1) {
			uint64_t onePercent = size / 100;
			if (index % onePercent >= onePercent - step) {
				printf(".");
			}
		}

		/**
		 * Populates a position from a bitbase index for the squares and a piece list for the piece types
		 */
		void addPiecesToPosition(MoveGenerator& position, const BitbaseIndex& bitbaseIndex, const PieceList& pieceList) {
			position.setPiece(bitbaseIndex.getSquare(0), WHITE_KING);
			position.setPiece(bitbaseIndex.getSquare(1), BLACK_KING);
			const uint32_t kingAmount = 2;
			for (uint32_t pieceNo = kingAmount; pieceNo < pieceList.getNumberOfPieces(); pieceNo++) {
				position.setPiece(bitbaseIndex.getSquare(pieceNo), pieceList.getPiece(pieceNo));
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
					BitbaseIndex bitbaseIndex(pieceList);
					bitbaseIndex.setSquares(index);
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
			uint64_t timeInMilliseconds = clock.computeTimeSpentInMilliseconds();
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
			if (pieceList.getNumberOfPieces() <= 2) return;
			string pieceString = pieceList.getPieceString();
			if (!BitbaseReader::isBitbaseAvailable(pieceString)) {
				BitbaseReader::loadBitbase(pieceString);
			}
			if (!BitbaseReader::isBitbaseAvailable(pieceString)) {
				for (uint32_t pieceNo = 2; pieceNo < pieceList.getNumberOfPieces(); pieceNo++) {
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
				computeBitbase(pieceList);
			}
		}

		void computeBitbase(
			Bitbase& wonPositions, Bitbase& computedPositions, PieceList& pieceList, ClockManager& clock)
		{
			BitbaseIndex bitbaseIndex(pieceList);
			bitbaseIndex.setSquares(0);
			Bitbase probePositions(bitbaseIndex);

			for (uint32_t loopCount = 0; loopCount < 1024; loopCount++) {

				MoveGenerator position;
				uint64_t bitsChanged = 0;
				bool first = true;
				for (uint64_t index = 0; index < bitbaseIndex.getSizeInBit(); index++) {
					printInfo(index, bitbaseIndex.getSizeInBit(), bitsChanged);
					
					if (!computedPositions.getBit(index)) {
						if (loopCount > 0 && !probePositions.getBit(index)) continue;
						bool isLegal = bitbaseIndex.setSquares(index);

						if (!isLegal) continue;
						position.clear();
						addPiecesToPosition(position, bitbaseIndex, pieceList);
						assert(index == BoardAccess::getIndex<0>(position));

						uint32_t success = computePosition(index, position, wonPositions, computedPositions);
						if (loopCount > 0 && success && !probePositions.getBit(index)) {
							cout << endl << "index: " << index << " fen: " << position.getFen() << endl;
							computePosition(index, position, wonPositions, computedPositions);
						}
						probePositions.clearBit(index);
						if (success && index == debugIndex) {
							cout << endl << position.getFen() << endl;
						}
						if (success) {
							markCandidates(position, probePositions);
						}
						bitsChanged += success;
						if (first && success) {
							first = false;
						}
					}
				}

				printf("\nLoop: %ld, positions set: %lld\n", loopCount, bitsChanged);
				printTimeSpent(clock);
				if (bitsChanged == 0) {
					break;
				}
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
			BitbaseIndex bitbaseIndex(pieceList);
			uint32_t bitsChanged = 0;
			uint64_t sizeInBit = bitbaseIndex.getSizeInBit();
			Bitbase wonPositions(bitbaseIndex);
			Bitbase computedPositions(bitbaseIndex);
			ClockManager clock;

			clock.setStartTime();
			clear();
			for (uint64_t index = 0; index < sizeInBit; index++) {
				printInfo(index, sizeInBit, bitsChanged);
				if (bitbaseIndex.setSquares(index)) {
					position.clear();
					addPiecesToPosition(position, bitbaseIndex, pieceList);
					bitsChanged += initialComputePosition(index, position, wonPositions, computedPositions);
				}
			}
			cout << endl
				<< "Number of direct loss: " << _numberOfDirectLoss << endl
				<< "Number of direct draw: " << _numberOfDirectDraw << endl
				<< "Number of illegal positions: " << _numberOfIllegalPositions << endl
				<< "Initial winning positions: " << bitsChanged << endl;
			printTimeSpent(clock);
			computeBitbase(wonPositions, computedPositions, pieceList, clock);

			//compareToFile(pieceString, wonPositions);
			cout 
				<< wonPositions
				<< " illigal positions " << _numberOfIllegalPositions << endl;

			printTimeSpent(clock);
			string fileName = pieceString + string(".btb");
			wonPositions.storeToFile(fileName.c_str());
			BitbaseReader::setBitbase(pieceString, wonPositions);
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
			uint64_t checkIndex = BoardAccess::getIndex<0>(position);
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
			bitbase.readFromFile("KQKR.btb");
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
			uint64_t checkIndex = BoardAccess::getIndex<0>(position);
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
			PieceList pieceList(pieceString);
			BitbaseIndex bitbaseIndex(pieceList);
			uint64_t index;
			while (true) {
				cin >> index;
				printf("%lld\n", index);
				if (bitbaseIndex.setSquares(index)) {
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
		uint64_t _numberOfIllegalPositions;
		uint64_t _numberOfDirectLoss;
		uint64_t _numberOfDirectDraw;
	};

}

#endif // __BITBASEGENERATOR_H