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
#include "bitbase-profiling.h"
#include "bitbase-repairfile.h"

using namespace std;
using namespace QaplaMoveGenerator;
using namespace QaplaSearch;
using namespace QaplaBitbase;

/**
 * Checks whether all quiet moves for the side to move lead to a forced loss.
 *
 * This function is called during iterative propagation only when tryDirectEntry()
 * could not resolve the position directly. It checks ONLY quiet moves (non-capture,
 * non-promotion) in the current bitbase. Captures and promotions were already fully
 * evaluated during initialization by setInitialValueByCapturesAndPromotions().
 *
 * Only Unknown positions are processed. Draw positions (draw-capture marker) are skipped
 * because the drawing capture makes a forced Loss logically impossible — the side to move
 * can always escape via that capture. Draw→Win upgrades are handled by tryDirectEntry().
 *
 * Design principles:
 * 1) We NEVER search for winning moves here. Win detection is handled exclusively by
 *    tryDirectEntry(), which uses results from previous, fully completed propagation
 *    rounds. This prevents a correctness bug where a loss resolved in the current round
 *    could immediately trigger a win in the same round, violating the invariant that
 *    each propagation round adds exactly one depth of proven knowledge.
 * 2) We do NOT detect or store draws. Unknown and Draw are both non-final; they are
 *    resolved by finalizeDraws() after all propagation completes. By skipping draws
 *    early, we improve performance without sacrificing correctness.
 *
 * @param index Bitbase index of the current position.
 * @param position Current position to evaluate.
 * @param state Mutable generation state providing the bitbase and storing the result.
 * @param verbose Enables detailed debug output.
 * @returns The forced-loss result if ALL quiet moves lose, or Unknown otherwise.
 */
BitbaseResult BitbaseGenerator::setComputeValue(
	uint64_t index, MoveGenerator& position, QaplaBitbase::GenerationState &state, bool verbose)
{
	MoveList moveList;
	bool whiteToMove = position.isWhiteToMove();
	PieceList pieceList(position);
	auto& bitbase = state.getComputedResults();

	if (verbose)
	{
		printDebugInfo(position, index);
	}

	// Guard: this function searches only for forced losses, so only Unknown positions are candidates.
	// Draw (intermediate marker): a drawing capture was found during initialization. The drawing
	// capture prevents the side to move from ever being forced into a Loss, regardless of quiet
	// moves. Only Win and Loss are final; Draw is not — but it also cannot become a Loss, so
	// there is nothing for this function to do. Upgrade Draw→Win is handled by tryDirectEntry.
	BitbaseResult currentResult = bitbase.get2Bits(index);
	if (currentResult != BitbaseResult::Unknown) {
		return BitbaseResult::Unknown;
	}

	// The loss value from white's perspective for the side to move:
	// white to move and loses → Loss; black to move and loses → Win (stored from white's view).
	const BitbaseResult lossForSideToMove = whiteToMove ? BitbaseResult::Loss : BitbaseResult::Win;

	position.genMovesOfMovingColor(moveList);

	for (uint32_t moveNo = 0; moveNo < moveList.getTotalMoveAmount(); moveNo++)
	{
		Move move = moveList[moveNo];
		if (!move.isCaptureOrPromote())
		{
			const auto moveIndex = BoardAccess::getIndex(!whiteToMove, pieceList, move);
			auto moveResult = bitbase.get2Bits(moveIndex);
			if (moveResult == BitbaseResult::Unknown) {
				return BitbaseResult::Unknown;
			}
			if (verbose)
			{
				std::cout << move.getLAN() << ", index: " << moveIndex
						  << ", value: " << to_string(moveResult)
						  << std::endl;
			}
			// Any quiet move that does NOT lose means the position has an escape.
			if (moveResult != lossForSideToMove) {
				return BitbaseResult::Unknown;
			}
		}
	}
	// All quiet moves lead to a loss for the side to move, and captures were already
	// evaluated during initialization (all-losing or none). This is a final forced loss.
	state.setValue(index, lossForSideToMove);
	return lossForSideToMove;
}

/**
 * Re-evaluates one position during iterative propagation and stores the result if resolved.
 *
 * @param index Bitbase index of the current position.
 * @param position Position reconstructed for this index.
 * @param state Mutable generation state.
 * @returns true if the position reached a definitive result (Win, Loss, or Draw); false if still Unknown.
 */
BitbaseResult BitbaseGenerator::computePosition(uint64_t index, MoveGenerator &position, GenerationState &state)
{
	auto result = setComputeValue(index, position, state, false);
	if (index == _debugIndex)
	{
		setComputeValue(index, position, state, true);
		std::cout << "Final result for index " << index << ": " << to_string(result) << std::endl;
	}
	return result;
}

/**
 * Attempts to directly set the result for a candidate position without full move evaluation.
 * A Win candidate with white to move can be stored immediately as Win.
 * A Loss candidate with black to move can be stored immediately as Loss.
 *
 * @param index Bitbase index of the candidate position.
 * @param winningMove True if the candidate move is a winning move for the side to move in this position.
 * @param whiteToMove True if white is to move in the candidate position.
 * @param state Mutable generation state.
 * @returns true if the position was directly resolved, false if full evaluation is needed.
 */
bool BitbaseGenerator::tryDirectEntry(uint64_t index, bool winningMove,
									  bool whiteToMove, GenerationState &state)
{
	if (!winningMove) {
		return false;
	}
	if (whiteToMove) {
		if (index == _debugIndex)
		{
			std::cout << "Directly setting index " << index << " to Win based on candidate result." << std::endl;
		}
		state.setWin(index);
		return true;
	}
	if (index == _debugIndex)
	{
		std::cout << "Directly setting index " << index << " to Loss based on candidate result." << std::endl;
	}
	state.setLoss(index);
	return true;
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
	if ((DO_DEBUG && _debugLevel > 0 && verbose) || index == _debugIndex)
	{
		cout << "New candidate, index: " << index << " move " << move.getLAN() << endl;
	}
	return index;
}

/**
 * Adds a candidate to the vector only if the position is not already final.
 *
 * @param candidates Output vector receiving candidate indexes.
 * @param entry Candidate entry to add.
 * @param computedResults Bitbase holding current results for early filtering.
 */
void BitbaseGenerator::addToCandidates(vector<CandidateEntry>& candidates, const CandidateEntry& entry,
	Bitbase& computedResults, GenerationState& state)
{
	if (!GenerationState::isFinal(computedResults.get2Bits(entry.index))) {
		// Skip push if the shared state already contains this candidate with sufficient priority.
		// Atomic relaxed read — thread-safe on all architectures, near-zero cost.
		if (!state.isCandidateSet(entry.index, entry.winningMove)) {
			candidates.push_back(entry);
		}
	}
}

/**
 * Reverse-generates pawn non-capture moves to possible predecessor squares.
 *
 * @tparam COLOR Pawn color to reverse-generate.
 * @param candidates Output vector receiving candidate indexes.
 * @param position Current position.
 * @param list Piece layout used for index mapping.
 * @param move Partially constructed move with moving piece and departure set.
 * @param computedResults Bitbase holding current results for early filtering.
 * @param verbose Enables detailed debug output.
 */
template <Piece COLOR>
void BitbaseGenerator::reverseGeneratePawnMoves(vector<CandidateEntry> &candidates,
												const MoveGenerator &position, const PieceList &list, Move move, BitbaseResult result,
												Bitbase& computedResults, bool verbose, GenerationState& state)
{
	// BitbaseResult is from perspective of the current position before moving. 
	const bool wtm = position.isWhiteToMove();
	const Square departure = move.getDeparture();
	const Square testDeparture = switchSide<COLOR>(departure);
	const Square direction = COLOR == WHITE ? SOUTH : NORTH;
	const Square oneRankDestination = departure + direction;
	const bool isMyPawn = move.getMovingPiece() == COLOR + PAWN;
	if (isMyPawn && testDeparture >= A3 && position[oneRankDestination] == NO_PIECE)
	{
		bool wtmAfterMove = !wtm;
		bool winningMove = (result == BitbaseResult::Win && wtmAfterMove) || (result == BitbaseResult::Loss && !wtmAfterMove);
		addToCandidates(candidates,
			{computeCandidateIndex(wtm, list, move, oneRankDestination, verbose), winningMove},
			computedResults, state);
		const Square twoRankDestination = oneRankDestination + direction;
		if (getRank(testDeparture) == Rank::R4 && position[twoRankDestination] == NO_PIECE)
		{
			addToCandidates(candidates,
				{computeCandidateIndex(wtm, list, move, twoRankDestination, verbose), winningMove},
				computedResults, state);
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
void BitbaseGenerator::computeCandidates(vector<CandidateEntry> &candidates, const MoveGenerator &position,
										 const PieceList &list, Move move, BitbaseResult result,
										 Bitbase& computedResults, bool verbose, GenerationState& state)
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
	reverseGeneratePawnMoves<WHITE>(candidates, position, list, move, result, computedResults, verbose, state);
	reverseGeneratePawnMoves<BLACK>(candidates, position, list, move, result, computedResults, verbose, state);
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
			bool wtmAfterMove = !wtm;
			bool winningMove = (result == BitbaseResult::Win && wtmAfterMove) || (result == BitbaseResult::Loss && !wtmAfterMove);
			addToCandidates(candidates,
				{computeCandidateIndex(wtm, list, move, destination, verbose), winningMove},
				computedResults, state);
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
void BitbaseGenerator::computeCandidates(vector<CandidateEntry> &candidates, MoveGenerator &position, BitbaseResult result,
										 Bitbase& computedResults, bool verbose, GenerationState& state)
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
			computeCandidates(candidates, position, pieceList, move, result, computedResults, verbose, state);
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
 * Propagation worker using BitWorkpackage.
 * Iterates directly over the index range; getCandidate() returns -1/0/1 by reading
 * the copied bitmaps (no CandidateEntry vector needed).
 *
 * @param workpackage Shared bit-based work provider.
 * @param state Shared generation state.
 */
void BitbaseGenerator::computeWorkpackage(BitWorkpackage &workpackage, GenerationState &state)
{
	MoveGenerator position;
	vector<CandidateEntry> candidates;
	candidates.reserve(_packageSize * 2);

	pair<uint64_t, uint64_t> package = workpackage.getNextPackageToExamine(_packageSize);
	while (package.first < package.second)
	{
		for (uint64_t index = package.first; index < package.second; ++index)
		{
			int candidateValue = workpackage.getCandidate(index);
			if (candidateValue < 0) continue;  // not a candidate

			bool winningMove = (candidateValue == 1);
			auto computedResult = state.getComputedResults().get2Bits(index);

			if (index == _debugIndex)
			{
				cout << "Processing candidate index " << index << " with candidate result ";
				cout << (winningMove ? "Winning" : "Losing") << endl;
			}

			if (GenerationState::isFinal(computedResult)) {
				continue;
			}
			ReverseIndex reverseIndex(index, state.getPieceList());

			bool directEntry = tryDirectEntry(index, winningMove,
											  reverseIndex.isWhiteToMove(), state);

			position.clear();
			addPiecesToPosition(position, reverseIndex, state.getPieceList());
			if (DO_DEBUG && _debugLevel > 0 && index != BoardAccess::getIndex<0>(position))
			{
				cout << "Error, programming bug, index is not correct " << index << endl;
				exit(1);
			}

			if (!directEntry) {
				const auto result = computePosition(index, position, state);
				if (result == BitbaseResult::Unknown) {
					continue;
				}
			}

			auto resolvedResult = state.getComputedResults().get2Bits(index);
			computeCandidates(candidates, position, resolvedResult, state.getComputedResults(), index == _debugIndex, state);
		}
		state.setCandidatesTreadSafe(candidates);
		candidates.clear();
		package = workpackage.getNextPackageToExamine(_packageSize);
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
	auto& timing = BitbaseProfiling::getStaticInstance();
	for (uint32_t loopCount = 0; loopCount < 1024; loopCount++)
	{
		timing.start("workpackage setup");
		BitWorkpackage workpackage(state);
		state.clearAllCandidates();
		timing.stop("workpackage setup");

		timing.start("propagation parallel");
		for (uint32_t threadNo = 0; threadNo < _cores; ++threadNo)
		{
			_threads[threadNo] = thread([this, &workpackage, &state]()
										{ computeWorkpackage(workpackage, state); });
		}

		joinThreads();
		timing.stop("propagation parallel");
		std::cout << "." << std::flush;
		if (!state.hasCandidates())
		{
			break;
		}
	}
	// All positions that remain unresolved after propagation are draws by definition:
	// neither side can force a win or loss from them (cycles, insufficient material, etc.).
	timing.start("finalize draws");
	state.finalizeDraws();
	timing.stop("finalize draws");
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
	// Note: drawDueToMissingMaterial() must NOT be used here, because even "theoretically drawn"
	// material configurations (e.g. KNKN) can contain specific checkmate positions that are
	// genuine wins/losses. finalizeDraws() correctly handles all unresolved positions at the end.

	// Fundamental model: Unknown and Draw are BOTH non-final. Win and Loss are the ONLY final states.
	//
	// Encoding for positions with capture/promotion moves (stored from white's perspective):
	//
	// Case 1: A winning capture exists (Win for white-to-move, or Loss for black-to-move)
	//   → write final Win/Loss immediately. Return Win/Loss.
	//
	// Case 2: A drawing capture exists (regardless of whether quiet moves remain)
	//   → write Draw marker; return Unknown.
	//   The drawing capture guarantees the side to move can always escape to at least a draw,
	//   so this position can NEVER become a Loss. setComputeValue therefore skips it.
	//   A win via a quiet move is still possible: tryDirectEntry handles the Draw→Win upgrade
	//   in a later propagation round. Any Draw that remains after all rounds is final.
	//
	// Case 3: Only losing captures exist, but quiet moves remain
	//   → leave as Unknown (fillAll default); return Unknown.
	//   setComputeValue checks each quiet move during propagation:
	//     - All quiet moves lose → write final Loss
	//     - Any quiet move draws → stays Unknown → finalizeDraws() writes final Draw
	//     - Any quiet move wins  → tryDirectEntry writes final Win
	//
	// Case 4: Only losing captures and no quiet moves
	//   → write final Loss/Win. Return Loss/Win.
	//
	// Key insight: there are no intermediate Losses. A Loss is only written once every
	// quiet move is proven to also lose. This makes _computedPositions unnecessary.

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
		BitbaseResult readerResult = BitbaseReader::getValueFromSingleBitbase(position);
		position.undoMove(move, boardState);
		assert(readerResult != BitbaseResult::Unknown); // Bitmaps of Reader are complete.

		// Results are stored from white's perspective.
		// A single winning move is enough to declare the position won for the side to move.
		if (readerResult == BitbaseResult::Win && position.isWhiteToMove())
		{
			state.setValue(index, BitbaseResult::Win);
			return BitbaseResult::Win;
		}
		if (readerResult == BitbaseResult::Loss && !position.isWhiteToMove())
		{
			state.setValue(index, BitbaseResult::Loss);
			return BitbaseResult::Loss;
		}
		// The side to move already has a proven draw.
		if (readerResult == BitbaseResult::Draw)
		{
			anyDraw = true;
		}
	}
	if (anyDraw) {
		// Case 2: a drawing capture exists — the side to move can always escape to at least
		// a draw, so this position can never be forced into a Loss. Write Draw as a marker
		// so setComputeValue skips it. Win is still possible via tryDirectEntry.
		state.setValue(index, BitbaseResult::Draw);
	}
	if (anyDraw || anyUnknown) {
		// Both Draw and Unknown are non-final: return Unknown so neither branch
		// generates candidates (only Win/Loss do).
		return BitbaseResult::Unknown;
	}
	// Case 4: all moves were captures/promotions and all lose for the side to move — final.
	const BitbaseResult worstCase = position.isWhiteToMove() ? BitbaseResult::Loss : BitbaseResult::Win;
	state.setValue(index, worstCase);
	return worstCase;
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
void BitbaseGenerator::computeInitialWorkpackage(InitialWorkpackage &workpackage, GenerationState &state)
{
	MoveGenerator position;
	vector<CandidateEntry> candidates;
	candidates.reserve(_packageSize * 2);
	[[maybe_unused]] uint64_t entryCount = state.getEntryCount();

	uint64_t packageSize = min(_packageSize, (state.getEntryCount() + 5) / 5);
	pair<uint64_t, uint64_t> package = workpackage.getNextPackageToExamine(packageSize);
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
				if (GenerationState::isFinal(result))
				{
					computeCandidates(candidates, position, result, state.getComputedResults(), index == _debugIndex, state);
				}
			}
		}
		for ([[maybe_unused]] const auto& entry : candidates) {
			assert(entry.index < entryCount);
		}
		state.setCandidatesTreadSafe(candidates);
		candidates.clear();
		package = workpackage.getNextPackageToExamine(packageSize);
	}
	state.setCandidatesTreadSafe(candidates);
	for ([[maybe_unused]] const auto& entry : candidates) {
		assert(entry.index < entryCount);
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

	auto& timing = BitbaseProfiling::getStaticInstance();

	timing.start("initial scan parallel");
	InitialWorkpackage workpackage(state.getEntryCount());
	state.clearAllCandidates();
	for (uint32_t threadNo = 0; threadNo < _cores; ++threadNo)
	{
		_threads[threadNo] = thread([this, &workpackage, &state]()
			{ computeInitialWorkpackage(workpackage, state); });
	}
	joinThreads();
	timing.stop("initial scan parallel");
	cout << "." << std::flush;
	computeBitbase(state, clock);

	string fileName = pieceString + string(".btb");
	cout << "c" << std::endl;
	// Print statistics BEFORE storeToFile, which may compact 2-bit data to 1-bit in place.
	timing.start("print statistic");
	printTimeSpent(clock);
	printStatistic(state);
	GenerationState::printTotalStatistic();
	timing.stop("print statistic");
	std::cout << std::endl;

	// Collect the full WDL sequence while the bitbase is still in 2-bit form.
	// storeToFile() may compact the data to 1-bit in place, so this must happen first.
	const uint64_t entryCount = state.getEntryCount();
	std::vector<BitbaseResult> repairResults;
	repairResults.reserve(entryCount);
	{
		auto& bb = state.getComputedResults();
		for (uint64_t idx = 0; idx < entryCount; ++idx) {
			repairResults.push_back(bb.get2Bits(idx));
		}
	}

	try {
		state.storeToFile(fileName, pieceString, compression);
		if (generateCpp)
		{
			state.generateCpp(pieceString);
		}
	}
	catch (const std::runtime_error& e) {
		std::cerr << "Error: " << e.what() << '\n';
	}

	// Write a Re-Pair + Huffman compressed copy, register it for subordinate lookups,
	// and verify every position.  The .qwdl file replaces the in-memory Bitbase: subsequent
	// bitbases that depend on this one probe it via BitbaseReader::getValueFromSingleBitbase()
	// through the memory-mapped .qwdl file instead of a heap-allocated Bitbase object.
	string qwdlFileName = pieceString + ".qwdl";
	try {
		cout << "Writing " << qwdlFileName << " ... " << std::flush;
		BitbaseRePairFile::write(qwdlFileName, repairResults);
		cout << "done" << std::endl;

		// Register the file so parent bitbases can probe it via getValueFromSingleBitbase().
		BitbaseReader::registerQwdlFile(pieceString, qwdlFileName);

		// Verify every position against the original WDL sequence.
		BitbaseRePairFile reader;
		if (!reader.open(qwdlFileName)) {
			std::cerr << "Re-Pair: cannot reopen " << qwdlFileName << " for verification\n";
		} else {
			cout << "Verifying " << qwdlFileName << " (" << entryCount << " positions) ... " << std::flush;
			uint64_t mismatches = 0;
			for (uint64_t idx = 0; idx < entryCount; ++idx) {
				BitbaseResult fromFile = reader.probe(idx);
				if (repairResults[idx] != fromFile) {
					++mismatches;
					if (mismatches <= 5) {
						std::cerr << "\n  mismatch at index " << idx
								  << ": expected " << to_string(repairResults[idx])
								  << ", got " << to_string(fromFile);
					}
				}
			}
			if (mismatches == 0) {
				cout << "OK" << std::endl;
			} else {
				std::cerr << "\n" << qwdlFileName << " verification FAILED: " << mismatches << " mismatches\n";
			}
		}
	}
	catch (const std::exception& e) {
		std::cerr << "Re-Pair error: " << e.what() << '\n';
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
