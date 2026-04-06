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
 * Tool to generate bitbases
 */

#include <iostream>
#include <algorithm>
#include <thread>

#include "../search/clockmanager.h"
#include "../movegenerator/movegenerator.h"

#include "piecelist.h"
#include "boardaccess.h"
#include "bitbase.h"
#include "reverseindex.h"
#include "generationstate.h"
#include "bitbase-reader.h"
#include "bitbasegenerator.h"

using namespace std;
using namespace QaplaMoveGenerator;
using namespace QaplaSearch;
using namespace QaplaBitbase;

/**
 * Evaluates non-capture, non-promotion moves against already-computed bitbase entries
 * to determine and store the best result achievable from this position.
 * Capture and promotion moves are skipped here — they were resolved during initialization.
 * Stores an intermediate lower-bound value (e.g. Draw) as soon as one is found,
 * even before all successors are resolved. Finalizes the value once all reachable
 * successors are known.
 *
 * @param position Current position to evaluate.
 * @param state Mutable generation state providing the bitbase and storing the result.
 * @param verbose Enables detailed debug output.
 * @returns Final proven result (Win/Loss/Draw), or Unknown if any successor is still unresolved.
 */
BitbaseResult BitbaseGenerator::setComputeValue(
	MoveGenerator& position, QaplaBitbase::GenerationState &state, bool verbose)
{
	MoveList moveList;
	Move move;
	bool whiteToMove = position.isWhiteToMove();
	PieceList pieceList(position);
	auto index = BoardAccess::getIndex<false>(position);
	auto& bitbase = state.getComputedResults();

	bool anyUnknown = false;

	// result contains the value gained so far. It is not a final value, computeValue is only called for non final values.
	BitbaseResult result = bitbase.get2Bits(index);

	if (verbose)
	{
		printDebugInfo(position, index);
	}

	position.genMovesOfMovingColor(moveList);

	for (uint32_t moveNo = 0; moveNo < moveList.getTotalMoveAmount(); moveNo++)
	{
		move = moveList[moveNo];
		// ToDo: check after initializing for a logic gap. As we initialized all positions having a capture or promote
		// we solved the win of the side to move already. If we are here, there is no win for the side to move
		// by promoting or capturing. But we need to solve to detect if we are "draw" or "loss". Example
		// we can force a position to draw by a capture but all non captures will loose - this results in a draw,
		// but we are currently not detecting this.
		if (!move.isCaptureOrPromote())
		{
			const auto moveIndex = BoardAccess::getIndex(!whiteToMove, pieceList, move);
			auto isFinal = state.isPositionComputed(moveIndex);
			if (!isFinal) {
				anyUnknown = true;
				continue;
			}
			auto moveResult = bitbase.get2Bits(moveIndex);
			if (verbose)
			{
				std::cout << move.getLAN() << ", index: " << moveIndex
						  << ", value: " << to_string(result)
						  << std::endl;
			}

			// We store results always from the "strong side" to improve compression.
			// The "strong" side is by definition "white".
			// If the side to move can force a win, we have a final result and stop searching further.
			if (moveResult == BitbaseResult::Win && whiteToMove)
			{
				state.setWin(index);
				return BitbaseResult::Win;
			}
			if (moveResult == BitbaseResult::Loss && !whiteToMove)
			{
				state.setLoss(index);
				return BitbaseResult::Loss;
			}
			// Store that we "gained" a draw so far, still it is not final
			if (moveResult == BitbaseResult::Draw && result != BitbaseResult::Draw)
			{
				result = BitbaseResult::Draw;
				state.setValue(index, BitbaseResult::Draw, false);
			}
		}
	}
	if (anyUnknown) {
		return BitbaseResult::Unknown;
	}
	if (result == BitbaseResult::Draw) {
		state.setValue(index, BitbaseResult::Draw, true);
		return BitbaseResult::Draw;
	}
	// All known, none winning or drawing, so it is a loss for the side to move.
	state.setValue(index, result, true);
	return result;
}

/**
 * Re-evaluates one position during iterative propagation and stores the result if resolved.
 *
 * @param index Bitbase index of the current position.
 * @param position Position reconstructed for this index.
 * @param state Mutable generation state.
 * @returns true if the position reached a definitive result (Win, Loss, or Draw); false if still Unknown.
 */
bool BitbaseGenerator::computePosition(uint64_t index, MoveGenerator &position, GenerationState &state)
{
	auto result = setComputeValue(position, state, false);
	if (index == _debugIndex)
	{
		setComputeValue(position, state, true);
	}
	return result != BitbaseResult::Unknown;
}

/**
 * Prints elapsed wall-clock time for the current generation step.
 *
 * @param clock Clock instance tracking elapsed time.
 */
void BitbaseGenerator::printTimeSpent(ClockManager &clock)
{
	uint64_t timeInMilliseconds = clock.computeTimeSpentInMilliseconds();
	cout << "Time spent: " << (timeInMilliseconds / (60 * 60 * 1000))
		 << ":" << ((timeInMilliseconds / (60 * 1000)) % 60)
		 << ":" << ((timeInMilliseconds / 1000) % 60)
		 << "." << timeInMilliseconds % 1000 << " ";
}

/**
 * Converts one reverse-generated move candidate into a bitbase index.
 *
 * @param wtm Side-to-move flag of the current position.
 * @param list Piece layout used for index mapping.
 * @param move Partially constructed reverse move.
 * @param destination Candidate destination square for the reverse move.
 * @param verbose Enables detailed debug output.
 * @returns Candidate bitbase index to revisit.
 */
uint64_t BitbaseGenerator::computeCandidateIndex(bool wtm, const PieceList &list, Move move,
												 Square destination, bool verbose)
{
	move.setDestination(destination);
	uint64_t index = BoardAccess::getIndex(!wtm, list, move);
	if (DO_DEBUG && _debugLevel > 0 && (verbose || index == _debugIndex))
	{
		cout << "New candidate, index: " << index << " move " << move.getLAN() << endl;
	}
	return index;
}

/**
 * Reverse-generates pawn non-capture moves to possible predecessor squares.
 *
 * @tparam COLOR Pawn color to reverse-generate.
 * @param candidates Output vector receiving candidate indexes.
 * @param position Current position.
 * @param list Piece layout used for index mapping.
 * @param move Partially constructed move with moving piece and departure set.
 * @param verbose Enables detailed debug output.
 */
template <Piece COLOR>
void BitbaseGenerator::reverseGeneratePawnMoves(vector<uint64_t> &candidates,
												const MoveGenerator &position, const PieceList &list, Move move, bool verbose)
{
	const bool wtm = position.isWhiteToMove();
	const Square departure = move.getDeparture();
	const Square testDeparture = switchSide<COLOR>(departure);
	const Square direction = COLOR == WHITE ? SOUTH : NORTH;
	const Square oneRankDestination = departure + direction;
	const bool isMyPawn = move.getMovingPiece() == COLOR + PAWN;
	if (isMyPawn && testDeparture >= A3 && position[oneRankDestination] == NO_PIECE)
	{
		candidates.push_back(
			computeCandidateIndex(wtm, list, move, oneRankDestination, verbose));
		const Square twoRankDestination = oneRankDestination + direction;
		if (getRank(testDeparture) == Rank::R4 && position[twoRankDestination] == NO_PIECE)
		{
			candidates.push_back(
				computeCandidateIndex(wtm, list, move, twoRankDestination, verbose));
		}
	}
}

/**
 * Computes reverse-generated candidate positions for one specific moving piece.
 *
 * @param candidates Output vector receiving candidate indexes.
 * @param position Current position.
 * @param list Piece layout used for index mapping.
 * @param move Partially constructed move (piece and departure are set).
 * @param verbose Enables detailed debug output.
 */
void BitbaseGenerator::computeCandidates(vector<uint64_t> &candidates, const MoveGenerator &position,
										 const PieceList &list, Move move, bool verbose)
{
	bitBoard_t attackBB = position.pieceAttackMask[move.getDeparture()];
	const bool wtm = position.isWhiteToMove();
	if (move.getMovingPiece() == WHITE_KING)
	{
		attackBB &= ~position.pieceAttackMask[position.getKingSquare<BLACK>()];
	}
	if (move.getMovingPiece() == BLACK_KING)
	{
		attackBB &= ~position.pieceAttackMask[position.getKingSquare<WHITE>()];
	}
	reverseGeneratePawnMoves<WHITE>(candidates, position, list, move, verbose);
	reverseGeneratePawnMoves<BLACK>(candidates, position, list, move, verbose);
	if (getPieceType(move.getMovingPiece()) != PAWN)
	{
		for (; attackBB; attackBB &= attackBB - 1)
		{
			const Square destination = lsb(attackBB);
			const bool occupied = position.getAllPiecesBB() & (1ULL << destination);
			if (occupied)
			{
				continue;
			}
			candidates.push_back(computeCandidateIndex(wtm, list, move, destination, verbose));
		}
	}
}

/**
 * Computes all reverse candidates after marking one position as newly won.
 * Candidate positions are derived from attack masks and reverse pseudo-legal moves.
 *
 * @param candidates Output vector receiving candidate indexes.
 * @param position Current position.
 * @param verbose Enables detailed debug output.
 */
void BitbaseGenerator::computeCandidates(vector<uint64_t> &candidates, MoveGenerator &position, bool verbose)
{
	PieceList pieceList(position);
	position.computeAttackMasksForBothColors();
	Piece piece = PAWN + int(position.isWhiteToMove());
	if (verbose)
	{
		position.print();
	}
	for (; piece <= BLACK_KING; piece += 2)
	{
		bitBoard_t pieceBB = position.getPieceBB(piece);
		for (; pieceBB; pieceBB &= pieceBB - 1)
		{
			Move move;
			move.setMovingPiece(piece);
			Square departure = lsb(pieceBB);
			move.setDeparture(departure);
			computeCandidates(candidates, position, pieceList, move, verbose);
		}
	}
}

/**
 * Reconstructs a position from reverse index squares and piece identities.
 *
 * @param position Target position that will be populated.
 * @param reverseIndex Reverse index providing square assignments.
 * @param pieceList Piece identities in fixed order.
 */
void BitbaseGenerator::addPiecesToPosition(
	MoveGenerator &position, const ReverseIndex &reverseIndex, const PieceList &pieceList)
{
	position.unsafeSetPiece(reverseIndex.getSquare(0), WHITE_KING);
	position.unsafeSetPiece(reverseIndex.getSquare(1), BLACK_KING);
	const uint32_t kingAmount = 2;
	for (uint32_t pieceNo = kingAmount; pieceNo < pieceList.getNumberOfPieces(); pieceNo++)
	{
		position.unsafeSetPiece(reverseIndex.getSquare(pieceNo), pieceList.getPiece(pieceNo));
	}
	position.computeAttackMasksForBothColors();
	position.setWhiteToMove(reverseIndex.isWhiteToMove());
}

/**
 * Processes one dynamic work package for iterative bitbase propagation.
 *
 * @param workpackage Shared work provider.
 * @param state Shared generation state.
 */
void BitbaseGenerator::computeWorkpackage(Workpackage &workpackage, GenerationState &state)
{
	MoveGenerator position;
	vector<uint64_t> candidates;

	static const uint64_t packageSize = 50000;
	pair<uint64_t, uint64_t> package = workpackage.getNextPackageToExamine(packageSize);
	while (package.first < package.second)
	{
		for (uint64_t workNo = package.first; workNo < package.second; ++workNo)
		{
			uint64_t index = workpackage.getIndex(workNo);
			ReverseIndex reverseIndex(index, state.getPieceList());

			position.clear();
			addPiecesToPosition(position, reverseIndex, state.getPieceList());
			if (DO_DEBUG && _debugLevel > 0 && index != BoardAccess::getIndex<0>(position))
			{
				cout << "Error, programming bug, index is not correct " << index << endl;
				exit(1);
			}

			const auto success = computePosition(index, position, state);

			if (success)
			{
				computeCandidates(candidates, position, index == _debugIndex);
			}
		}
		if (state.setCandidatesTreadSafe(candidates, false))
		{
			candidates.clear();
		}
		package = workpackage.getNextPackageToExamine(packageSize);
	}
	state.setCandidatesTreadSafe(candidates);
}

/**
 * Runs iterative propagation until no additional candidate positions remain.
 *
 * @param state Current computation state.
 * @param clock Clock tracking total generation time.
 */
void BitbaseGenerator::computeBitbase(GenerationState &state, ClockManager &clock)
{
	for (uint32_t loopCount = 0; loopCount < 1024; loopCount++)
	{
		Workpackage workpackage(state);
		state.clearAllCandidates();
		for (uint32_t threadNo = 0; threadNo < _cores; ++threadNo)
		{
			_threads[threadNo] = thread([this, &workpackage, &state]()
										{ computeWorkpackage(workpackage, state); });
		}

		joinThreads();
		std::cout << "." << std::flush;
		if (!state.hasCandidates())
		{
			break;
		}
	}
}

/**
 * Sets the initial proven value for a position by consulting subordinate bitbases
 * via all capture and promotion moves.
 * Always writes at least the best already-achieved result into state:
 * a forced win or loss is stored as final; a reachable draw is stored as
 * an intermediate lower bound even when non-capture moves remain unresolved.
 * Returns Win or Loss when the result is fully decided by captures/promotions alone,
 * Draw when all evaluated moves are at least draws and no non-captures exist,
 * or Unknown when non-capture moves are present and no forced win was found.
 *
 * @param position Current position to evaluate.
 * @param index Bitbase index of the position.
 * @param moveList Legal moves generated for the side to move.
 * @param state Mutable generation state that receives the result.
 * @returns Final proven result, or Unknown if iterative propagation is still needed.
 */
BitbaseResult BitbaseGenerator::setInitialValueByCapturesAndPromotions(
	MoveGenerator &position, const uint64_t index, MoveList &moveList, QaplaBitbase::GenerationState &state)
{
	// Set default initial value, the value the side to move already "gained" so far.
	// Initial value is a loss for the side to move. We set the white view.
	state.setValue(index, position.isWhiteToMove() ? BitbaseResult::Loss : BitbaseResult::Win, false);

	// Start with Loss — for the side to move (White view always).
	BoardState boardState = position.getBoardState();
	bool anyUnknown = false;
	bool anyDraw = false;

	for (uint32_t moveNo = 0; moveNo < moveList.getTotalMoveAmount(); moveNo++)
	{
		auto move = moveList.getMove(moveNo);
		// Non-capture, non-promotion moves cannot be evaluated yet, because the subordinate
		// bitbases only cover positions reachable via captures or promotions.
		if (!move.isCaptureOrPromote())
		{
			anyUnknown = true;
			continue;
		}
		position.doMove(move);
		Result readerResult = BitbaseReader::getValueFromSingleBitbase(position);
		position.undoMove(move, boardState);
		assert(readerResult != Result::Unknown); // Bitmaps of Reader are complete.

		// Results are stored from white's perspective.
		// A single winning move is enough to declare the position won for the side to move.
		if (readerResult == Result::Win && position.isWhiteToMove())
		{
			state.setValue(index, BitbaseResult::Win, true);
			return BitbaseResult::Win;
		}
		if (readerResult == Result::Loss && !position.isWhiteToMove())
		{
			state.setValue(index, BitbaseResult::Loss, true);
			return BitbaseResult::Loss;
		}
		// The side to move already has a proven draw.
		if (readerResult == Result::Draw)
		{
			anyDraw = true;
		}
	}
	if (anyDraw) {
		// If any is draw, we have at least a draw, if all are at least draw, then it is a final value.
		state.setValue(index, BitbaseResult::Draw, !anyUnknown);
		if (!anyUnknown) {
			return BitbaseResult::Draw;
		}
	}
	return BitbaseResult::Unknown;
}

/**
 * Classifies a no-move situation as checkmate or stalemate.
 *
 * @param position Current position with no legal moves.
 * @param index Bitbase index of this position.
 * @param state Mutable generation state.
 * @returns Classified terminal result.
 */
BitbaseResult BitbaseGenerator::setMateOrStalemate(QaplaMoveGenerator::MoveGenerator &position, const uint64_t index,
											QaplaBitbase::GenerationState &state)
{
	if (!position.isWhiteToMove() && position.isInCheck())
	{
		if (DO_DEBUG && index == _debugIndex)
		{
			cout << _debugIndex << " , Fen: " << position.getFen(0) << " is win by mate (move generator) " << endl;
		}
		state.setWin(index);
		return BitbaseResult::Win;
	}
	if (position.isWhiteToMove() && position.isInCheck())
	{
		if (DO_DEBUG && index == _debugIndex)
		{
			cout << _debugIndex << " , Fen: " << position.getFen(0) << " is loss by mate (move generator) " << endl;
		}
		state.setLoss(index);
		return BitbaseResult::Loss;
	}
	if (DO_DEBUG && index == _debugIndex)
	{
		cout << _debugIndex << " , Fen: " << position.getFen(0) << " is stalemate (move generator) " << endl;
	}
	state.setDraw(index);
	return BitbaseResult::Draw;
}

/**
 * Performs initial classification for one position before iterative propagation.
 *
 * @param index Bitbase index of this position.
 * @param position Reconstructed position.
 * @param state Mutable generation state.
 * @returns Initial classification result.
 */
BitbaseResult BitbaseGenerator::initialComputePosition(
	uint64_t index, MoveGenerator &position, GenerationState &state)
{
	MoveList moveList;

	// Illegal positions can be marked as "searched".
	if (!position.isLegal())
	{
		if (DO_DEBUG && index == _debugIndex)
		{
			cout << _debugIndex << " , Fen: " << position.getFen(0) << " is illegal (move generator) " << endl;
		}
		state.setIllegal(index);
		// Illegal positions are coded as unknown, we might change that later to improve compression. As they are 
		// illegal, they are not relevant.
		return BitbaseResult::Unknown;
	}

	position.genMovesOfMovingColor(moveList);
	if (moveList.getTotalMoveAmount() > 0)
	{
		// Compute all captures and look up the positions in other bitboards
		return setInitialValueByCapturesAndPromotions(position, index, moveList, state);
	}
	return setMateOrStalemate(position, index, state);
}

/**
 * Processes one dynamic work package for initial position classification.
 *
 * @param workpackage Shared work provider.
 * @param state Shared generation state.
 */
void BitbaseGenerator::computeInitialWorkpackage(Workpackage &workpackage, GenerationState &state)
{
	MoveGenerator position;
	vector<uint64_t> candidates;
	[[maybe_unused]] uint64_t entryCount = state.getEntryCount();

	uint64_t packageSize = min(static_cast<uint64_t>(50000), (state.getEntryCount() + 5) / 5);
	pair<uint64_t, uint64_t> package = workpackage.getNextPackageToExamine(packageSize, state.getEntryCount());
	while (package.first < package.second)
	{
		for (uint64_t index = package.first; index < package.second; ++index)
		{
			assert(index < entryCount);
			ReverseIndex reverseIndex(index, state.getPieceList());
			if (!reverseIndex.isLegal())
			{
				state.setIllegal(index);
				continue;
			}
			position.clear();
			addPiecesToPosition(position, reverseIndex, state.getPieceList());
			uint64_t testIndex = BoardAccess::getIndex<0>(position);
			if (index != testIndex)
			{
				state.setIllegal(index);
			}
			else
			{
				BitbaseResult result = initialComputePosition(index, position, state);
				// All positions that might change evaluation after having new informations are candidates.
				// We could be lazy about draws as all positions that are neither win nor loss are draw at the end.
				// ToDo: Check for speed optimization by ignoring draw positions as candidates.
				// This would result in some changes as we need to set all positions initially as "draw" and 
				// Remove the unknown state completely from all generator paths.
				if (result != BitbaseResult::Unknown)
				{
					computeCandidates(candidates, position, index == _debugIndex);
				}
			}
		}
		if (state.setCandidatesTreadSafe(candidates, false))
		{
			for ([[maybe_unused]] uint64_t index : candidates) {
				assert(index < entryCount);
			}
			candidates.clear();
		}
		package = workpackage.getNextPackageToExamine(packageSize, state.getEntryCount());
	}
	state.setCandidatesTreadSafe(candidates);
	for ([[maybe_unused]] uint64_t index : candidates) {
		assert(index < entryCount);
	}
}

/**
 * Computes and persists one concrete bitbase described by a piece list.
 *
 * @param pieceList Piece layout of the bitbase to generate.
 * @param first True if this is the primary requested bitbase.
 * @param compression Compression algorithm used for persisted output.
 * @param generateCpp If true, also emits generated C++ code.
 */
void BitbaseGenerator::computeBitbase(PieceList& pieceList, bool first, QaplaCompress::CompressionType compression, bool generateCpp)
{
	MoveGenerator position;
	string pieceString = pieceList.getPieceString();
	PieceSignature pieceSignature(pieceString.c_str());
	if (pieceString.substr(0, 2) == "KK")
	{
		return;
	}

	cout << pieceString << " using " << _cores << " threads " << std::flush;

	GenerationState state(pieceList, pieceSignature.getPiecesSignature());
	ClockManager clock;
	clock.setStartTime();

	Workpackage workpackage(state);
	state.clearAllCandidates();
	for (uint32_t threadNo = 0; threadNo < _cores; ++threadNo)
	{
		_threads[threadNo] = thread([this, &workpackage, &state]()
			{ computeInitialWorkpackage(workpackage, state); });
	}
	joinThreads();
	cout << "." << std::flush;
	computeBitbase(state, clock);

	string fileName = pieceString + string(".btb");
	cout << "c" << std::endl;
	try {
		state.storeToFile(fileName, pieceString, compression);
		if (generateCpp)
		{
			state.generateCpp(pieceString);
		}
		printTimeSpent(clock);
		printStatistic(state);
		std::cout << std::endl;
		BitbaseReader::setBitbase(pieceString, state.getComputedResults());
	}
	catch (const std::runtime_error& e) {
		std::cerr << "Error: " << e.what() << '\n';
	}
}

/**
 * Recursively computes all dependent bitbases reachable via captures and promotions.
 * For KQKP this includes KQK, KQKQ, KQKR, KQKB, KQKN, and related dependencies.
 *
 * @param pieceList Piece layout of the current bitbase.
 * @param first True if this is the primary requested bitbase.
 * @param compression Compression algorithm used for persisted output.
 * @param generateCpp If true, also emits generated C++ code.
 */
void BitbaseGenerator::computeBitbaseRec(PieceList &pieceList, bool first, QaplaCompress::CompressionType compression, bool generateCpp)
{
	if (pieceList.getNumberOfPieces() <= 2)
		return;
	string pieceString = pieceList.getPieceString();
	if (pieceString.substr(0, 2) == "KK") return;
	if (!first && BitbaseReader::isBitbaseAvailable(pieceString)) return;
	BitbaseReader::loadBitbase(pieceString, false);

	for (uint32_t pieceNo = 2; pieceNo < pieceList.getNumberOfPieces(); pieceNo++)
	{
		PieceList newPieceList(pieceList);
		if (isPawn(newPieceList.getPiece(pieceNo)))
		{
			for (Piece piece = QUEEN; piece >= KNIGHT; piece -= 2)
			{
				newPieceList.promotePawn(pieceNo, piece);
				computeBitbaseRec(newPieceList, false, compression, generateCpp);
				newPieceList = pieceList;
			}
		}
		newPieceList.removePiece(pieceNo);
		computeBitbaseRec(newPieceList, false, compression, generateCpp);
	}

	if (first || !BitbaseReader::isBitbaseAvailable(pieceString))
	{
		computeBitbase(pieceList, first, compression, generateCpp);
	}
}
