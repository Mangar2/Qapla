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

#include <iostream>
#include "../search/clockmanager.h"
#include "../movegenerator/movegenerator.h"
#include "../search/moveprovider.h"
#include "bitbaseindex.h"
#include "bitbasereader.h"
#include "bitbasegenerator.h"

using namespace std;
using namespace ChessMoveGenerator;
using namespace ChessSearch;
using namespace QaplaBitbase;

/**
 * Clears the situation counter
 */
void BitbaseGenerator::clear() {
	_numberOfDirectLoss = 0;
	_numberOfDirectDraw = 0;
	_numberOfIllegalPositions = 0;
}

/**
 * Searches all captures and look up the position after capture in a bitboard.
 */
value_t BitbaseGenerator::initialSearch(MoveGenerator& position)
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
 * Computes a position value by probing all moves and lookup the result in this bitmap
 * Captures are excluded, they have been tested in the initial search. 
 */
bool BitbaseGenerator::computeValue(MoveGenerator& position, Bitbase& bitbase, bool verbose) {
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

/**
 * Sets the bitbase index for a position by computing the position value from the bitbase itself
 * @returns 1, if the position is a win (now) and 0, if it is still unknown
 */
uint32_t BitbaseGenerator::computePosition(uint64_t index, MoveGenerator& position, Bitbase& bitbase, Bitbase& computedPositions) {
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
 * Prints the difference of two bitbases
 */
void BitbaseGenerator::compareBitbases(string pieceString, Bitbase& newBitbase, Bitbase& oldBitbase) {
	MoveGenerator position;
	PieceList pieceList(pieceString);
	uint64_t sizeInBit = newBitbase.getSizeInBit();
	uint64_t differences = 0;
	cout << " comparing bitbases for " << pieceString << endl;
	for (uint64_t index = 0; index < sizeInBit; index++) {
		bool newResult = newBitbase.getBit(index);
		bool oldResult = oldBitbase.getBit(index);
		if (newResult != oldResult) {
			BitbaseIndex bitbaseIndex(index, pieceList);
			addPiecesToPosition(position, bitbaseIndex, pieceList);
			differences++;
			if (differences < 10) {
				printf("new: %s, old: %s\n", newResult ? "won" : "not won", oldResult ? "won" : "not won");
				printf("index: %lld\n", index);
				position.print();
			}
			position.clear();
		}
	}
	cout << " Compare for " << pieceString << " amount of differences: " << differences << endl;
}

/**
 * Prints the time spent so far
 */
void BitbaseGenerator::printTimeSpent(ClockManager& clock) {
	uint64_t timeInMilliseconds = clock.computeTimeSpentInMilliseconds();
	cout << "Time spent: " << (timeInMilliseconds / (60 * 60 * 1000)) 
		<< ":" << ((timeInMilliseconds / (60 * 1000)) % 60) 
		<< ":" << ((timeInMilliseconds / 1000) % 60) 
		<< "." << timeInMilliseconds % 1000 << endl;
}

/**
 * Debugging, shows the position identified by the globally defined debug index
 */
void BitbaseGenerator::showDebugIndex(string pieceString) {
	MoveGenerator position;
	Bitbase bitbase;
	bitbase.readFromFile(pieceString);
	PieceList pieceList(pieceString);
	uint64_t index;
	while (true) {
		cin >> index;
		printf("%lld\n", index);
		BitbaseIndex bitbaseIndex(index, pieceList);
		if (bitbaseIndex.isLegal()) {
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

/**
 * Marks one candidate identified by a partially filled move and a destination square
 */
void BitbaseGenerator::markCandidate(bool wtm, Bitbase& bitbase, PieceList& list, 
	Move move, Square destination, bool verbose) 
{
	move.setDestination(destination);
	uint64_t index = BoardAccess::getIndex(!wtm, list, move);
	bitbase.setBit(index);
	if (verbose) {
		cout << "New candidate, index: " << index << " move " << move.getLAN() << endl;
	}
}

/**
 * Mark candidates for a dedicated piece identified by a partially filled move
 */
void BitbaseGenerator::markCandidates(const MoveGenerator& position, Bitbase& bitbase, 
	PieceList& list, Move move, bool verbose)
{
	bitBoard_t attackBB = position.pieceAttackMask[move.getDeparture()];
	const bool wtm = position.isWhiteToMove();
	if (move.getMovingPiece() == WHITE_KING) {
		attackBB &= ~position.pieceAttackMask[position.getKingSquare<BLACK>()];
	}
	if (move.getMovingPiece() == BLACK_KING) {
		attackBB &= ~position.pieceAttackMask[position.getKingSquare<WHITE>()];
	}
	if (move.getMovingPiece() == WHITE_PAWN && move.getDeparture() >= A3) {
		markCandidate(wtm, bitbase, list, move, move.getDeparture() - NORTH, verbose);
		if (getRank(move.getDeparture()) == Rank::R4) {
			markCandidate(wtm, bitbase, list, move, move.getDeparture() - 2 * NORTH, verbose);
		}
	}
	if (move.getMovingPiece() == BLACK_PAWN && move.getDeparture() <= H6) {
		markCandidate(wtm, bitbase, list, move, move.getDeparture() + NORTH, verbose);
		if (getRank(move.getDeparture()) == Rank::R5) {
			markCandidate(wtm, bitbase, list, move, move.getDeparture() + 2 * NORTH, verbose);
		}
	}
	if (getPieceType(move.getMovingPiece()) != PAWN) {
		for (; attackBB; attackBB &= attackBB - 1) {
			const Square destination = lsb(attackBB);
			const bool occupied = position.getAllPiecesBB() & (1ULL << destination);
			if (occupied) {
				continue;
			}
			markCandidate(wtm, bitbase, list, move, destination, verbose);
		}
	}
}

/**
 * Mark all candidate positions we need to look at after a new bitbase position is set to 1
 * Candidate positions are computed by running through the attack masks of every piece and
 * computing reverse moves (ignoring all special cases like check, captures, ...)
 */
void BitbaseGenerator::markCandidates(MoveGenerator& position, Bitbase& bitbase, bool verbose) {
	PieceList pieceList(position);
	position.computeAttackMasksForBothColors();
	Piece piece = PAWN + int(position.isWhiteToMove());
	if (verbose) {
		position.print();
	}
	for (; piece <= BLACK_KING; piece += 2) {
		bitBoard_t pieceBB = position.getPieceBB(piece);
		Move move;
		move.setMovingPiece(piece);
		for (; pieceBB; pieceBB &= pieceBB - 1) {
			Square departure = lsb(pieceBB);
			move.setDeparture(departure);
			markCandidates(position, bitbase, pieceList, move, verbose);
		}
	}
}

/**
 * Populates a position from a bitbase index for the squares and a piece list for the piece types
 */
void BitbaseGenerator::addPiecesToPosition(
	MoveGenerator& position, const BitbaseIndex& bitbaseIndex, const PieceList& pieceList)
{
	position.setPiece(bitbaseIndex.getSquare(0), WHITE_KING);
	position.setPiece(bitbaseIndex.getSquare(1), BLACK_KING);
	const uint32_t kingAmount = 2;
	for (uint32_t pieceNo = kingAmount; pieceNo < pieceList.getNumberOfPieces(); pieceNo++) {
		position.setPiece(bitbaseIndex.getSquare(pieceNo), pieceList.getPiece(pieceNo));
	}
	position.setWhiteToMove(bitbaseIndex.isWhiteToMove());
}

/**
 * Compute the bitbase by checking each position for an update as long as no further update is found
 * @param wonPositions bitbase with all positions winning in one ply
 * @param computedPositions positions that are decided in one ply (illegal, win, draw, loss)
 * @param pieceList list of pieces of the bitbase
 * @param clock time measurement for the bitbase generation
 */
void BitbaseGenerator::computeBitbase(
	Bitbase& wonPositions, Bitbase& computedPositions, PieceList& pieceList, ClockManager& clock)
{
	BitbaseIndex bitbaseIndexType(pieceList);
	Bitbase probePositions(bitbaseIndexType);
	const uint64_t bitbaseSizeInBit = bitbaseIndexType.getSizeInBit();

	for (uint32_t loopCount = 0; loopCount < 1024; loopCount++) {

		MoveGenerator position;
		uint64_t bitsChanged = 0;
		bool first = true;
		for (uint64_t index = 0; index < bitbaseSizeInBit; index++) {
			printInfo(index, bitbaseSizeInBit, bitsChanged);

			if (!computedPositions.getBit(index)) {
				if (loopCount > 0 && !probePositions.getBit(index)) continue;
				BitbaseIndex bitbaseIndex(index, pieceList);
				if (!bitbaseIndex.isLegal()) {
					cout << " Error, programming bug, should already be set in computedPositions " << endl;
					continue;
				}

				position.clear();
				addPiecesToPosition(position, bitbaseIndex, pieceList);
				assert(index == BoardAccess::getIndex<0>(position));

				uint32_t success = computePosition(index, position, wonPositions, computedPositions);
				if (loopCount > 0 && success && !probePositions.getBit(index)) {
					cout << endl << "index: " << index << " fen: " << position.getFen() << endl;
					computePosition(index, position, wonPositions, computedPositions);
				}
				probePositions.clearBit(index);
				if (success) {
					markCandidates(position, probePositions, index == debugIndex);
				}
				bitsChanged += success;
				if (first && success) {
					first = false;
				}
			}
		}

		cout << endl << "Loop: " << loopCount << " positions set: " << bitsChanged << endl;
		printTimeSpent(clock);
		if (bitsChanged == 0) {
			break;
		}
	}
}

/**
 * Initially probe alle positions for a mate- draw or capture situation
 */
uint32_t BitbaseGenerator::initialComputePosition(uint64_t index, MoveGenerator& position,
	Bitbase& bitbase, Bitbase& computedPositions) 
{
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

/**
 * @returns true, if the bitbase has two pieces of the same kind that are badly ordered
 * (the first piece is on a larger square than the second piece).
 */
bool BitbaseGenerator::hasUnorderdDoublePiece(const PieceList& pieceList, const BitbaseIndex& bitbaseIndex) {
	bool unordered = false;
	Piece lastPiece = Piece::NO_PIECE;
	for (uint32_t index = 2; index < pieceList.getNumberOfPieces(); index++) {
		const bool samePiece = pieceList.getPiece(index) == pieceList.getPiece(index - 1);
		if (samePiece) {
			unordered = bitbaseIndex.getSquare(index) <= bitbaseIndex.getSquare(index - 1);
			if (unordered) {
				break;
			}
		}
	}
	return unordered;
};

/**
 * Computes a bitbase for a set of pieces described by a piece list.
 */
void BitbaseGenerator::computeBitbase(PieceList& pieceList) {
	MoveGenerator position;
	string pieceString = pieceList.getPieceString();
	printf("Computing bitbase for %s\n", pieceString.c_str());
	if (BitbaseReader::isBitbaseAvailable(pieceString)) {
		printf("Bitbase already loaded\n");
		return;
	}
	BitbaseIndex bitbaseIndexType(pieceList);
	uint32_t bitsChanged = 0;
	const uint64_t sizeInBit = bitbaseIndexType.getSizeInBit();
	Bitbase wonPositions(sizeInBit);
	Bitbase computedPositions(sizeInBit);
	ClockManager clock;

	clock.setStartTime();
	clear();
	for (uint64_t index = 0; index < sizeInBit; index++) {
		printInfo(index, sizeInBit, bitsChanged);
		BitbaseIndex bitbaseIndex(index, pieceList);
		if (bitbaseIndex.isLegal() && !hasUnorderdDoublePiece(pieceList, bitbaseIndex)) {
			position.clear();
			addPiecesToPosition(position, bitbaseIndex, pieceList);
			uint64_t testIndex = BoardAccess::getIndex<0>(position);
			if (testIndex != index) {
				cout << "Fatal error in index: " << index << " index computing has a bug " << endl;
			}
			bitsChanged += initialComputePosition(index, position, wonPositions, computedPositions);
		}
		else {
			computedPositions.setBit(index);
		}
	}
	cout << endl
		<< "Number of direct loss: " << _numberOfDirectLoss << endl
		<< "Number of direct draw: " << _numberOfDirectDraw << endl
		<< "Number of illegal positions: " << _numberOfIllegalPositions << endl
		<< "Initial winning positions: " << bitsChanged << endl;
	printTimeSpent(clock);
	computeBitbase(wonPositions, computedPositions, pieceList, clock);

	cout
		<< wonPositions
		<< " illigal positions " << _numberOfIllegalPositions << endl;

	printTimeSpent(clock);
	string fileName = pieceString + string(".btb");
	wonPositions.storeToFile(fileName.c_str());
	BitbaseReader::setBitbase(pieceString, wonPositions);
}

/**
 * Recursively computes bitbases based on a bitbase string
 * For KQKP it will compute KQK, KQKQ, KQKR, KQKB, KQKN, ...
 * so that any bitbase KQKP can get to is available
 */
void BitbaseGenerator::computeBitbaseRec(PieceList& pieceList) {
	if (pieceList.getNumberOfPieces() <= 2) return;
	string pieceString = pieceList.getPieceString();
	compareFiles(pieceString);
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
