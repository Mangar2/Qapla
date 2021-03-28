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
#include <algorithm>
#include <thread>
#include "../search/clockmanager.h"
#include "../movegenerator/movegenerator.h"
#include "../search/moveprovider.h"
#include "bitbaseindex.h"
#include "generationstate.h"
#include "bitbasereader.h"
#include "bitbasegenerator.h"


using namespace std;
using namespace ChessMoveGenerator;
using namespace ChessSearch;
using namespace QaplaBitbase;

/**
 * Computes a position value by probing all moves and lookup the result in this bitmap
 * Captures are excluded, they have been tested in the initial search. 
 */
bool BitbaseGenerator::computeValue(MoveGenerator& position, const Bitbase& bitbase, bool verbose) {
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
uint32_t BitbaseGenerator::computePosition(uint64_t index, MoveGenerator& position, GenerationState& state) {
	uint32_t result = 0;
	if (computeValue(position, state.getWonPositions(), false)) {
		if (index == _debugIndex) {
			computeValue(position, state.getWonPositions(), true);
		}
		state.setWin(index);
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
	if (differences > 0 || _traceLevel > 0) {
		cout << "Compare for " << pieceString << " amount of differences: " << differences << endl;
	}
}

/**
 * Prints the time spent so far
 */
void BitbaseGenerator::printTimeSpent(ClockManager& clock, int minTraceLevel) {
	if (_traceLevel < minTraceLevel) {
		return;
	}
	uint64_t timeInMilliseconds = clock.computeTimeSpentInMilliseconds();
	cout << endl << "Time spent: " << (timeInMilliseconds / (60 * 60 * 1000)) 
		<< ":" << ((timeInMilliseconds / (60 * 1000)) % 60) 
		<< ":" << ((timeInMilliseconds / 1000) % 60) 
		<< "." << timeInMilliseconds % 1000 << " ";
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
uint64_t BitbaseGenerator::computeCandidateIndex(bool wtm, const PieceList& list, Move move, 
	Square destination, bool verbose)
{
	move.setDestination(destination);
	uint64_t index = BoardAccess::getIndex(!wtm, list, move);
	if (DEBUG_LEVEL > 0 && verbose) {
		cout << "New candidate, index: " << index << " move " << move.getLAN() << endl;
	}
	return index;
}

/**
 * Mark candidates for a dedicated piece identified by a partially filled move
 */
void BitbaseGenerator::computeCandidates(vector<uint64_t>& candidates, const MoveGenerator& position,
	const PieceList& list, Move move, bool verbose)
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
		candidates.push_back(
			computeCandidateIndex(wtm, list, move, move.getDeparture() - NORTH, verbose));
		if (getRank(move.getDeparture()) == Rank::R4) {
			candidates.push_back(
				computeCandidateIndex(wtm, list, move, move.getDeparture() - 2 * NORTH, verbose));
		}
	}
	if (move.getMovingPiece() == BLACK_PAWN && move.getDeparture() <= H6) {
		candidates.push_back(
			computeCandidateIndex(wtm, list, move, move.getDeparture() + NORTH, verbose));
		if (getRank(move.getDeparture()) == Rank::R5) {
			candidates.push_back(
				computeCandidateIndex(wtm, list, move, move.getDeparture() + 2 * NORTH, verbose));
		}
	}
	if (getPieceType(move.getMovingPiece()) != PAWN) {
		for (; attackBB; attackBB &= attackBB - 1) {
			const Square destination = lsb(attackBB);
			const bool occupied = position.getAllPiecesBB() & (1ULL << destination);
			if (occupied) {
				continue;
			}
			candidates.push_back(computeCandidateIndex(wtm, list, move, destination, verbose));
		}
	}
}

/**
 * Computes all candidate positions we need to look at after a new bitbase position is set to 1
 * Candidate positions are computed by running through the attack masks of every piece and
 * computing reverse moves (ignoring all special cases like check, captures, ...)
 */
void BitbaseGenerator::computeCandidates(vector<uint64_t>& candidates, MoveGenerator& position, bool verbose) {
	PieceList pieceList(position);
	position.computeAttackMasksForBothColors();
	Piece piece = PAWN + int(position.isWhiteToMove());
	if (verbose) {
		position.print();
	}
	for (; piece <= BLACK_KING; piece += 2) {
		bitBoard_t pieceBB = position.getPieceBB(piece);
		for (; pieceBB; pieceBB &= pieceBB - 1) {
			Move move;
			move.setMovingPiece(piece);
			Square departure = lsb(pieceBB);
			move.setDeparture(departure);
			computeCandidates(candidates, position, pieceList, move, verbose);
		}
	}
}

/**
 * Populates a position from a bitbase index for the squares and a piece list for the piece types
 */
void BitbaseGenerator::addPiecesToPosition(
	MoveGenerator& position, const BitbaseIndex& bitbaseIndex, const PieceList& pieceList)
{
	position.unsafeSetPiece(bitbaseIndex.getSquare(0), WHITE_KING);
	position.unsafeSetPiece(bitbaseIndex.getSquare(1), BLACK_KING);
	const uint32_t kingAmount = 2;
	for (uint32_t pieceNo = kingAmount; pieceNo < pieceList.getNumberOfPieces(); pieceNo++) {
		position.unsafeSetPiece(bitbaseIndex.getSquare(pieceNo), pieceList.getPiece(pieceNo));
	}
	position.computeAttackMasksForBothColors();
	position.setWhiteToMove(bitbaseIndex.isWhiteToMove());
}

/**
 * Computes a workpackage for a compute-bitbase loop
 * @param work list of indexes to work on
 * @param candidates resulting candidates for further checks
 */
void BitbaseGenerator::computeWorkpackage(Workpackage& workpackage, GenerationState& state, bool onlyCandidates) 
{
	MoveGenerator position;
	vector<uint64_t> candidates;

	static const uint64_t packageSize = 50000;
	pair<uint64_t, uint64_t> package = workpackage.getNextPackageToExamine(packageSize);
	while (package.first < package.second) {
		for (uint64_t workNo = package.first; workNo < package.second; ++workNo) {
			uint64_t index = workpackage.getIndex(workNo);
			BitbaseIndex bitbaseIndex(index, state.getPieceList());
			if (!bitbaseIndex.isLegal()) {
				cout << " Error, programming bug, should already be set in computedPositions index: " << index << endl;
				exit(1);
			}

			position.clear();
			addPiecesToPosition(position, bitbaseIndex, state.getPieceList());
			if (DEBUG_LEVEL > 0 && index != BoardAccess::getIndex<0>(position)) {
				cout << "Error, programming bug, index is not correct " << index << endl;
				exit(1);
			}

			const auto success = computePosition(index, position, state);

			if (success) {
				computeCandidates(candidates, position, index == _debugIndex);
			}
		}
		if (candidates.size() > packageSize) {
			state.setCandidatesTreadSafe(candidates, _debugIndex);
			candidates.clear();
		}
		package = workpackage.getNextPackageToExamine(packageSize);
	}
	state.setCandidatesTreadSafe(candidates, _debugIndex);
}

/**
 * Compute the bitbase by checking each position for an update as long as no further update is found
 * @param state Current computation state
 * @param clock time measurement for the bitbase generation
 */
void BitbaseGenerator::computeBitbase(GenerationState& state, ClockManager& clock) {
	const uint64_t bitbaseSizeInBit = state.getSizeInBit();

	for (uint32_t loopCount = 0; loopCount < 1024; loopCount++) {
		for (auto& candidate : _candidates) {
			candidate.clear();
		}
		uint64_t index = 0;
		uint32_t threadNo = 0;
		Workpackage workpackage(state);
		state.clearAllCandidates();
		for (uint32_t threadNo = 0; threadNo < _cores; ++threadNo) {
			_threads[threadNo] = thread([this, &workpackage, loopCount, &state]() {
				computeWorkpackage(workpackage, state, true);
			});
		}
		/*
		computeWorkpackage(work, _candidates[0], state, loopCount == 0);
		*/
		joinThreads();
		cout << ".";
		if (!state.hasCandidates()) {
			break;
		}
	}
}

/**
 * Searches all captures and look up the position after capture in a bitboard.
 */
Result BitbaseGenerator::initialSearch(MoveGenerator& position, MoveList& moveList)
{
	Move move;

	// The side to move starts with a most negative value (Loss)
	Result result = position.isWhiteToMove() ? Result::Loss : Result::Win;
	BoardState boardState = position.getBoardState();

	for (uint32_t moveNo = 0; moveNo < moveList.getTotalMoveAmount(); moveNo++) {
		move = moveList.getMove(moveNo);
		// As we do not have any information about the current bitboard, any silent move
		// leads to an unknown situation
		if (!move.isCaptureOrPromote()) {
			result = Result::Unknown;
			continue;
		}
		position.doMove(move);
		Result cur = BitbaseReader::getValueFromSingleBitbase(position);
		position.undoMove(move, boardState);

		if (!position.isWhiteToMove()) {
			// Seeking for Result::Loss or draw - white view
			if (cur == Result::Unknown) {
				result = cur;
			} else if (cur != Result::Win) {
				result = cur;
				break;
			}
		}
		else {
			// Seeking for Result::Win
			if (cur == Result::Win) {
				result = cur;
				break;
			}
			if (cur == Result::Draw) {
				result = cur;
			}
		}
	}
	return result;
}


/**
 * Sets a situation to mate or stalemate
 */
bool BitbaseGenerator::setMateOrStalemate(ChessMoveGenerator::MoveGenerator& position, const uint64_t index, 
	QaplaBitbase::GenerationState& state)
{
	bool result = false;
	if (!position.isWhiteToMove() && position.isInCheck()) {
		if (DEBUG_LEVEL > 0 && index == _debugIndex) {
			cout << _debugIndex << " , Fen: " << position.getFen() << " is win by mate (move generator) " << endl;
		}
		state.setWin(index);
		result = true;
	}
	else if (position.isWhiteToMove() && position.isInCheck()) {
		if (DEBUG_LEVEL > 0 && index == _debugIndex) {
			cout << _debugIndex << " , Fen: " << position.getFen() << " is loss by mate (move generator) " << endl;
		}
		state.setLoss(index);
		result = true;
	}
	else {
		if (DEBUG_LEVEL > 0 && index == _debugIndex) {
			cout << _debugIndex << " , Fen: " << position.getFen() << " is stalemate (move generator) " << endl;
		}
		state.setDraw(index);
		result = true;
	}
	return result;
}

/**
 * Initially probe alle positions for a mate- draw or capture situation
 */
bool BitbaseGenerator::initialComputePosition(uint64_t index, MoveGenerator& position, GenerationState& state) {
	MoveList moveList;
	bool result = false;
	bool kingInCheck = false;
	if (DEBUG_LEVEL > 0 && index == _debugIndex) {
		kingInCheck = false;
	}

	// Exclude all illegal positions (king not to move is in check) from future search
	if (!position.isLegalPosition()) {
		if (DEBUG_LEVEL > 0 && index == _debugIndex) {
			cout << _debugIndex << " , Fen: " << position.getFen() << " is illegal (move generator) " << endl;
		}
		state.setIllegal(index);
		return false;
	}

	position.genMovesOfMovingColor(moveList);
	if (moveList.getTotalMoveAmount() > 0) {
		// Compute all captures and look up the positions in other bitboards
		Result positionValue = initialSearch(position, moveList);
		if (DEBUG_LEVEL > 0 && index == _debugIndex) {
			cout << _debugIndex << " " << int(positionValue) << endl;
		}
		if (positionValue == Result::Win) {
			if (DEBUG_LEVEL > 0 && index == _debugIndex) {
				cout << _debugIndex << " , Fen: " << position.getFen() << " is a win (initial search) " << endl;
			}
			state.setWin(index);
			result = true;
		}
		else if (positionValue != Result::Unknown) {
			if (DEBUG_LEVEL > 0 && index == _debugIndex) {
				cout << _debugIndex << " , Fen: " << position.getFen() << " is a loss or draw (initial search) " << endl;
			}
			state.setDraw(index);
			result = true;
		}
	}
	else {
		result = setMateOrStalemate(position, index, state);
	}
	return result;
}

/**
 * Computes a workpackage of initial positions for a bitbase; 
 */
void BitbaseGenerator::computeInitialWorkpackage(Workpackage& workpackage, GenerationState& state) {
	MoveGenerator position;
	vector<uint64_t> candidates;

	uint64_t packageSize = min(50000ULL, (state.getSizeInBit() + 5) / 5);
	pair<uint64_t, uint64_t> package = workpackage.getNextPackageToExamine(packageSize, state.getSizeInBit());
	while (package.first < package.second) {
		for (uint64_t index = package.first; index < package.second; ++index) {
			BitbaseIndex bitbaseIndex(index, state.getPieceList());
			if (bitbaseIndex.isLegal()) {
				position.clear();
				addPiecesToPosition(position, bitbaseIndex, state.getPieceList());
				if (DEBUG_LEVEL > 0) {
					uint64_t testIndex = BoardAccess::getIndex<0>(position);
					if (testIndex != index) {
						cout << "Fatal error in index: " << index << " index computing has a bug " << endl;
						cout << "Created board has index: " << testIndex << endl;
						position.print();
						exit(1);
					}
				}
				bool isDecided = initialComputePosition(index, position, state);
				if (isDecided) {
					computeCandidates(candidates, position, index == _debugIndex);
				}
			}
			else {
				if (DEBUG_LEVEL > 0 && index == _debugIndex) {
					cout << _debugIndex << " Is an illegal index (bitbaseIndex) " << endl;
				}
				state.setIllegal(index);
			}
		}
		state.setCandidatesTreadSafe(candidates, _debugIndex);
		candidates.clear();
		package = workpackage.getNextPackageToExamine(packageSize, state.getSizeInBit());
	}
}

/**
 * Computes a bitbase for a set of pieces described by a piece list.
 */
void BitbaseGenerator::computeBitbase(PieceList& pieceList) {
	MoveGenerator position;
	string pieceString = pieceList.getPieceString();
	if (BitbaseReader::isBitbaseAvailable(pieceString)) {
		cout << "Bitbase " << pieceString << " already loaded" << endl;
		return;
	}

	if (_traceLevel > 1) cout << endl;
	cout << pieceString << " using " << _cores << " threads ";

	GenerationState state(pieceList);
	ClockManager clock;
	clock.setStartTime();

	Workpackage workpackage(state);
	state.clearAllCandidates();
	for (uint32_t threadNo = 0; threadNo < _cores; ++threadNo) {
		_threads[threadNo] = thread([this, &workpackage, &state]() {
			computeInitialWorkpackage(workpackage, state);
		});
	}
	joinThreads();
	printTimeSpent(clock, 2);
	printStatistic(state, 2);

	computeBitbase(state, clock);

	printTimeSpent(clock, 2);
	string fileName = pieceString + string(".btb");
	cout << "c";
	state.storeToFile(fileName);
	printTimeSpent(clock, 1);
	printStatistic(state, 1);
	cout << endl;
	BitbaseReader::setBitbase(pieceString, state.getWonPositions());
}

/**
 * Recursively computes bitbases based on a bitbase string
 * For KQKP it will compute KQK, KQKQ, KQKR, KQKB, KQKN, ...
 * so that any bitbase KQKP can get to is available
 */
void BitbaseGenerator::computeBitbaseRec(PieceList& pieceList, bool first) {
	if (pieceList.getNumberOfPieces() <= 2) return;
	string pieceString = pieceList.getPieceString();
	if (!BitbaseReader::isBitbaseAvailable(pieceString)) {
		BitbaseReader::loadBitbase(pieceString);
		if (DEBUG_LEVEL > 1) {
			compareFiles(pieceString);
		}
	}
	for (uint32_t pieceNo = 2; pieceNo < pieceList.getNumberOfPieces(); pieceNo++) {
		PieceList newPieceList(pieceList);
		if (isPawn(newPieceList.getPiece(pieceNo))) {
			for (Piece piece = QUEEN; piece >= KNIGHT; piece -= 2) {
				newPieceList.promotePawn(pieceNo, piece);
				computeBitbaseRec(newPieceList, false);
				newPieceList = pieceList;
			}
		}
		newPieceList.removePiece(pieceNo);
		computeBitbaseRec(newPieceList, false);
	}
	
	if (!BitbaseReader::isBitbaseAvailable(pieceString)) {
		computeBitbase(pieceList);
		if (DEBUG_LEVEL > 1) {
			compareFiles(pieceString);
		}
	} 

	if (first && DEBUG_LEVEL > 0) {
		compareFiles(pieceString);
	}
}
