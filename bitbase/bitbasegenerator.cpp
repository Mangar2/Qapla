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

#include <bit>
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
#include "syzygy-export.h"
#include "bitbase-profiling.h"
#include "bitbase-repairfile.h"

using namespace std;
using namespace QaplaMoveGenerator;
using namespace QaplaSearch;
using namespace QaplaBitbase;

/**
 * Of two results, both written from white's point of view, the one the given side
 * prefers. Unknown must not be passed in - it would rank as a draw.
 */
static BitbaseResult betterFor(BitbaseResult a, BitbaseResult b, bool forWhite)
{
	const auto rank = [](BitbaseResult result) {
		return result == BitbaseResult::Win ? 1 : result == BitbaseResult::Loss ? -1 : 0;
	};
	if (forWhite) return rank(a) >= rank(b) ? a : b;
	return rank(a) <= rank(b) ? a : b;
}

/**
 * The value of the position a double pawn step leads to, with the en passant capture
 * it hands the opponent taken into account.
 *
 * The index has no room for the en passant right, so the stored entry is the value of
 * the position without it - which is right for every position that is asked about, and
 * wrong for exactly this one child. The capture itself needs no table of its own: it
 * removes a pawn and therefore lands in a subordinate bitbase.
 *
 * Three cases, all of them the opponent's choice:
 *  - the capture is better for him than the entry: it is the value,
 *  - it is worse: he does not play it, the entry stands,
 *  - the position without the right has no move at all: then it is not stalemate as
 *    the entry says, the capture is the only move there is, and it is the value even
 *    when it is worse for him.
 *
 * @param withoutEnPassant the stored entry of the child, from white's point of view
 * @returns the corrected value, or Unknown while the child is not decided yet
 */
BitbaseResult BitbaseGenerator::valueAfterDoubleStep(MoveGenerator& position, Move move,
	BitbaseResult withoutEnPassant)
{
	const PositionSnapshot snapshot = position.getSnapshot();
	position.doMove(move);

	MoveList childMoves;
	position.genMovesOfMovingColor(childMoves);

	const bool opponentIsWhite = position.isWhiteToMove();
	BitbaseResult bestCapture = BitbaseResult::Unknown;
	uint32_t otherMoves = 0;

	for (uint32_t moveNo = 0; moveNo < childMoves.getTotalMoveAmount(); ++moveNo) {
		const Move childMove = childMoves[moveNo];
		if (!childMove.isEPMove()) {
			++otherMoves;
			continue;
		}

		const PositionSnapshot childSnapshot = position.getSnapshot();
		position.doMove(childMove);
		const BitbaseResult value = BitbaseReader::getValueFromSingleBitbase(position);
		const string missingMaterial = value == BitbaseResult::Unknown
			? PieceList(position).getPieceString() : string();
		position.undoMove(childMove, childSnapshot);

		if (value == BitbaseResult::Unknown) {
			if (_missingDependencies++ == 0)
				cerr << endl << "Error: no bitbase for " << missingMaterial
					<< ", reached by an en passant capture - the table being computed"
					" will be wrong" << endl;
			continue;
		}

		bestCapture = bestCapture == BitbaseResult::Unknown
			? value : betterFor(bestCapture, value, opponentIsWhite);
	}

	position.undoMove(move, snapshot);

	// undoMove does not restore the attack masks, and the caller reads them - to
	// generate its candidates backwards, and to generate moves in the next round.
	position.computeAttackMasksForBothColors();

	if (bestCapture == BitbaseResult::Unknown) return withoutEnPassant;

	// Nothing can beat the best there is, so the entry need not even be known
	if (bestCapture == (opponentIsWhite ? BitbaseResult::Win : BitbaseResult::Loss))
		return bestCapture;

	// The only move: what the entry says about stalemate or mate does not hold
	if (otherMoves == 0) return bestCapture;

	if (withoutEnPassant == BitbaseResult::Unknown) return BitbaseResult::Unknown;
	return betterFor(withoutEnPassant, bestCapture, opponentIsWhite);
}

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
	const bool whiteToMove = position.isWhiteToMove();
	PieceList pieceList(position);
	auto& bitbase = state.getComputedResults();

	if (verbose)
	{
		printDebugInfo(position, index);
	}

	const uint8_t current = bitbase.getRawByte(index);
	if (Dtz::isDone(current)) return BitbaseResult::Unknown;

	// A zeroing move that holds the draw was written as a marker during the initial
	// pass: the position can never be forced into a loss, but a quiet move may still
	// win it.
	const bool canStillLose = current == Dtz::UNKNOWN;

	position.genMovesOfMovingColor(moveList);

	// The winner takes the shortest way to the zeroing move, the loser the longest. The
	// minimum over the moves that are answered already is the true minimum, because the
	// distances are found in ascending order - what is still open can only be longer.
	int shortestWin = INT_MAX;
	int longestLoss = 1;
	bool allMovesLose = true;

	for (uint32_t moveNo = 0; moveNo < moveList.getTotalMoveAmount(); moveNo++)
	{
		Move move = moveList[moveNo];

		// Captures, promotions and, in level mode, pawn moves zero the counter and were
		// answered by the initial pass. What lies behind them never enters the distance
		// of what lies before them.
		if (move.isCaptureOrPromote()) continue;
		if (_levelWise && isPawn(move.getMovingPiece())) continue;

		const auto moveIndex = BoardAccess::getIndex(!whiteToMove, pieceList, move);
		const uint8_t child = bitbase.getRawByte(moveIndex);

		if (verbose)
		{
			std::cout << move.getLAN() << ", index: " << moveIndex
				<< ", value: " << int(child) << std::endl;
		}

		if (!Dtz::hasDistance(child))
		{
			// Not answered yet, or a draw: either way this move is an escape.
			allMovesLose = false;
			continue;
		}

		// The position behind the move is seen from the opponent: his loss is our win.
		if (Dtz::isLoss(child)) shortestWin = std::min(shortestWin, Dtz::plies(child) + 1);
		else                    longestLoss = std::max(longestLoss, Dtz::plies(child) + 1);
	}

	if (shortestWin != INT_MAX)
	{
		if (shortestWin > Dtz::MAX_PLIES && _distanceOverflow++ == 0)
			cerr << endl << "Error: a distance of " << shortestWin
				 << " plies does not fit in the byte of the generation state" << endl;

		state.setValue(index, uint8_t(Dtz::WIN_IN_ONE + std::min(shortestWin, Dtz::MAX_PLIES) - 1));
		return whiteToMove ? BitbaseResult::Win : BitbaseResult::Loss;
	}

	// Every move loses, and the captures and pawn moves were established as losing by
	// the initial pass - their zeroing is one ply, which is where longestLoss starts.
	if (canStillLose && allMovesLose)
	{
		if (longestLoss > Dtz::MAX_PLIES && _distanceOverflow++ == 0)
			cerr << endl << "Error: a distance of " << longestLoss
				 << " plies does not fit in the byte of the generation state" << endl;

		state.setValue(index, uint8_t(Dtz::MATE - std::min(longestLoss, Dtz::MAX_PLIES)));
		return whiteToMove ? BitbaseResult::Loss : BitbaseResult::Win;
	}

	return BitbaseResult::Unknown;
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
/**
 * How far the pawns of a position have advanced, summed over all of them.
 *
 * Every pawn move raises it, so a pawn move leads from one level into a higher one and
 * never back. That is what lets the table be computed level by level, the highest first:
 * when a level is reached, everything a pawn move out of it can lead to is finished, and
 * the move is then as answerable as a capture.
 */
static int pawnAdvancement(const PieceList& list, const ReverseIndex& reverseIndex)
{
	int level = 0;
	for (uint32_t pieceNo = 0; pieceNo < list.getNumberOfPieces(); ++pieceNo)
	{
		const Piece piece = list.getPiece(pieceNo);
		if (getPieceType(piece) != PAWN) continue;

		const int rank = int(getRank(Square(reverseIndex.getSquare(pieceNo))));
		level += getPieceColor(piece) == WHITE ? rank : 7 - rank;
	}
	return level;
}

void BitbaseGenerator::addToCandidates(vector<CandidateEntry>& candidates, const CandidateEntry& entry,
	Bitbase& computedResults, GenerationState& state)
{
	const bool stillOpen = _distancePhase
		? Dtz::isOpen(computedResults.getRawByte(entry.index))
		: !GenerationState::isFinal(computedResults.getByte(entry.index));

	if (stillOpen) {
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
			// A pawn of the other colour beside the square this pawn stands on could have
			// answered the double step en passant. The value the candidate would be raised
			// from is the one without that right, so the shortcut does not hold here: the
			// candidate carries no winning mark and the full evaluation decides it, which
			// corrects for the capture. Everywhere else the shortcut stands.
			const Piece opponentPawn = switchColor(COLOR) + PAWN;
			const bool enPassantPossible =
				(getFile(departure) != File::A && position[departure + WEST] == opponentPawn)
				|| (getFile(departure) != File::H && position[departure + EAST] == opponentPawn);

			addToCandidates(candidates,
				{computeCandidateIndex(wtm, list, move, twoRankDestination, verbose),
					enPassantPossible ? false : winningMove},
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
	// Backwards over a pawn move leads out of the level, into a position that is
	// computed later and whose value does not depend on this one: the pawn move zeroes
	// the counter, so what lies behind it never enters its distance. The initial pass of
	// that level answers it.
	if (!_levelWise)
	{
		reverseGeneratePawnMoves<WHITE>(candidates, position, list, move, result, computedResults, verbose, state);
		reverseGeneratePawnMoves<BLACK>(candidates, position, list, move, result, computedResults, verbose, state);
	}
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
		// Eight candidate bits at a time, with the other colour already masked away by
		// the work package. Once the candidates are sparse - and with one ply per round
		// they are - this skips whole bytes instead of asking index by index.
		for (uint64_t base = package.first; base < package.second; base += 8)
		{
			uint8_t bits = workpackage.getCandidateByte(base);

			while (bits)
			{
				const uint64_t index = base + uint64_t(std::countr_zero(bits));
				bits &= uint8_t(bits - 1);

				if (index == _debugIndex)
				{
					cout << "Processing candidate index " << index << endl;
				}

				if (state.isFinal(index)) continue;

				ReverseIndex reverseIndex(index, state.getPieceList());
				position.clear();
				addPiecesToPosition(position, reverseIndex, state.getPieceList());
				if (DO_DEBUG && _debugLevel > 0 && index != BoardAccess::getIndex<0>(position))
				{
					cout << "Error, programming bug, index is not correct " << index << endl;
					exit(1);
				}

				// The distance needs the moves, so there is no shortcut for a position
				// that a candidate already names as won - the shortest of its moves has
				// to be found.
				const auto result = computePosition(index, position, state);
				if (result == BitbaseResult::Unknown) continue;

				computeCandidates(candidates, position, result,
								  state.getComputedResults(), index == _debugIndex, state);
			}
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
/**
 * Runs the rounds until no candidate is left, one side to move per round.
 *
 * @param state Current computation state.
 */
void BitbaseGenerator::propagate(GenerationState& state)
{
	auto& timing = BitbaseProfiling::getStaticInstance();
	int parity = 0;

	for (uint32_t loopCount = 0; loopCount < 8192; loopCount++)
	{
		if (state.candidateCount(0) == 0 && state.candidateCount(1) == 0) break;

		if (state.candidateCount(parity) > 0)
		{
			timing.start("workpackage setup");
			BitWorkpackage workpackage(state, parity);
			state.clearCandidatesOfParity(parity);
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
		}
		parity = 1 - parity;
	}
}

void BitbaseGenerator::computeBitbase(GenerationState &state, ClockManager &clock)
{
	auto& timing = BitbaseProfiling::getStaticInstance();

	// Levels of pawn advancement, the most advanced first. A pawn move raises the level,
	// so when a level is computed everything its pawn moves lead to is finished - and a
	// pawn move is then as answerable as a capture, including its distance. Without pawns
	// there is one level and nothing to order.
	const uint32_t pawnCount = state.getPieceList().getNumberOfPawns();
	_levelWise = pawnCount > 0;
	_distanceOverflow = 0;

	const int maxLevel = _levelWise ? 6 * int(pawnCount) : 0;
	const int minLevel = _levelWise ? int(pawnCount) : 0;

	state.clearAllCandidates();

	for (int level = maxLevel; level >= minLevel; --level)
	{
		timing.start("initial scan parallel");
		InitialWorkpackage workpackage(state.getEntryCount());
		const int levelOrAll = _levelWise ? level : -1;
		const bool firstLevel = level == maxLevel;

		for (uint32_t threadNo = 0; threadNo < _cores; ++threadNo)
		{
			_threads[threadNo] = thread([this, &workpackage, &state, levelOrAll, firstLevel]()
				{ computeInitialWorkpackage(workpackage, state, levelOrAll, firstLevel); });
		}
		joinThreads();
		timing.stop("initial scan parallel");
		cout << "." << std::flush;

		propagate(state);
	}

	_levelWise = false;
	// All positions that remain unresolved after propagation are draws by definition:
	// neither side can force a win or loss from them (cycles, insufficient material, etc.).
	timing.start("finalize draws");
	state.finalizeDraws();
	timing.stop("finalize draws");
	timing.start("mark illegal as unknown");
	markIllegalAsUnknown(state);
	timing.stop("mark illegal as unknown");

	reportDistances(state);
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

	const PositionSnapshot snapshot = position.getSnapshot();
	bool anyUnknown = false;
	bool anyDraw = false;

	const bool pawnMovesAnswered = _levelWise;
	bool drawByPawnMove = false;
	PieceList pieceList(position);

	for (uint32_t moveNo = 0; moveNo < moveList.getTotalMoveAmount(); moveNo++)
	{
		auto move = moveList.getMove(moveNo);
		BitbaseResult readerResult = BitbaseResult::Unknown;
		string missingMaterial;

		if (!move.isCaptureOrPromote())
		{
			// A pawn move zeroes the counter like a capture, and unlike a capture it
			// stays in this table - in a position of a higher level, which is finished
			// by the time this level is computed. Everything else has to wait for the
			// propagation.
			if (!pawnMovesAnswered || !isPawn(move.getMovingPiece()))
			{
				anyUnknown = true;
				continue;
			}

			const uint64_t moveIndex =
				BoardAccess::getIndex(!position.isWhiteToMove(), pieceList, move);

			// The entry is in de Man's codes and seen from its own side to move, which
			// is the opponent here; the tables below answer from white, so this one is
			// turned to white as well. What a finished level did not prove is a draw,
			// which is what its UNKNOWN says.
			const uint8_t childValue = state.getComputedResults().getRawByte(moveIndex);
			const BitbaseResult childMoverView = Dtz::toResult(childValue);

			readerResult = position.isWhiteToMove()
				? (childMoverView == BitbaseResult::Win  ? BitbaseResult::Loss
				 : childMoverView == BitbaseResult::Loss ? BitbaseResult::Win
														 : BitbaseResult::Draw)
				: (childMoverView == BitbaseResult::Win  ? BitbaseResult::Win
				 : childMoverView == BitbaseResult::Loss ? BitbaseResult::Loss
														 : BitbaseResult::Draw);

			// A pawn stepping two squares hands the opponent an en passant capture, and
			// the entry of the child knows nothing about it.
			if (abs(int(move.getDestination()) - int(move.getDeparture())) == 16)
				readerResult = valueAfterDoubleStep(position, move, readerResult);
		}
		else
		{
			position.doMove(move);
			readerResult = BitbaseReader::getValueFromSingleBitbase(position);
			missingMaterial = readerResult == BitbaseResult::Unknown
				? PieceList(position).getPieceString() : string();
			position.undoMove(move, snapshot);
		}

		// No table for the material this capture leads into. Every branch below reacts to
		// Win, Loss or Draw, so an unanswered capture would simply drop out of the
		// reckoning and the position would be decided as if the move did not exist - which
		// is how a missing dependency used to turn into a wrong value instead of an error.
		if (readerResult == BitbaseResult::Unknown) {
			if (_missingDependencies++ == 0) {
				cerr << endl << "Error: no bitbase for " << missingMaterial
					<< ", reached by " << move.getLAN() << " from " << position.getFen(0)
					<< " - the table being computed will be wrong" << endl;
			}
			anyUnknown = true;
			continue;
		}

		// The tables answer from white's point of view, the value written here is seen
		// from the side to move. A move that wins leads to a position the opponent has
		// lost, and the move zeroes the counter - so the distance is one ply, and which
		// kind of move it was is what de Man's two codes say.
		const bool movePawn = !move.isCaptureOrPromote();

		if (readerResult == (position.isWhiteToMove() ? BitbaseResult::Win : BitbaseResult::Loss))
		{
			state.setValue(index, movePawn ? Dtz::PAWN_WIN : Dtz::CAPT_WIN);
			return position.isWhiteToMove() ? BitbaseResult::Win : BitbaseResult::Loss;
		}

		// The side to move already has a proven draw.
		if (readerResult == BitbaseResult::Draw)
		{
			anyDraw = true;
			drawByPawnMove = drawByPawnMove || movePawn;
		}
	}
	if (anyDraw) {
		// A zeroing move holds the draw, so this position can never be forced into a
		// loss - which is what the marker tells the propagation. A win through a quiet
		// move is still open.
		state.setValue(index, drawByPawnMove ? Dtz::PAWN_DRAW : Dtz::CAPT_DRAW);
	}
	if (anyDraw || anyUnknown) {
		// Neither is a result: return Unknown so that no candidates are generated.
		return BitbaseResult::Unknown;
	}

	// Every move zeroes the counter and every one of them loses. The longest way to the
	// zeroing move is therefore one ply.
	state.setValue(index, uint8_t(Dtz::MATE - 1));
	return position.isWhiteToMove() ? BitbaseResult::Loss : BitbaseResult::Win;
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
	if (position.isInCheck())
	{
		if (DO_DEBUG && index == _debugIndex)
		{
			cout << _debugIndex << " , Fen: " << position.getFen(0) << " is mate (move generator) " << endl;
		}

		// Mate is a loss for the side to move in no ply at all: the game ends here, and
		// the fifty move counter never has to be zeroed again.
		state.setValue(index, Dtz::MATE);
		return position.isWhiteToMove() ? BitbaseResult::Loss : BitbaseResult::Win;
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
void BitbaseGenerator::computeInitialWorkpackage(InitialWorkpackage &workpackage, GenerationState &state,
												int level, bool firstLevel)
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

			// An index that is not a position at all has no level to belong to, so it is
			// marked in the first pass and left alone afterwards.
			if (!reverseIndex.isLegal())
			{
				if (firstLevel) state.setIllegal(index);
				continue;
			}

			// Only the level this pass is on. The others are computed in their own pass,
			// the more advanced ones before this one.
			if (level >= 0 && pawnAdvancement(state.getPieceList(), reverseIndex) != level)
				continue;
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
 * Reports what the distances came out at, and that none is missing.
 *
 * A decided position always has a distance: a forced win ends in mate, and mate is
 * zero. One without is a fault of the passes, not a draw.
 */
void BitbaseGenerator::reportDistances(GenerationState& state)
{
	const uint64_t entryCount = state.getEntryCount();
	auto& bitbase = state.getComputedResults();

	uint64_t longest = 0;
	uint64_t beyondFiftyMoves = 0;

	for (uint64_t index = 0; index < entryCount; ++index)
	{
		const uint8_t value = bitbase.getRawByte(index);
		if (!Dtz::hasDistance(value)) continue;

		longest = max<uint64_t>(longest, uint64_t(Dtz::plies(value)));
		if (Dtz::plies(value) > Dtz::DRAW_RULE) ++beyondFiftyMoves;
	}

	cout << "Distances: longest " << longest << " plies, " << beyondFiftyMoves
		 << " beyond the fifty move rule";
	if (_distanceOverflow > 0) cout << ", ERROR: " << _distanceOverflow << " did not fit";
	cout << endl;
}

void BitbaseGenerator::markIllegalAsUnknown(GenerationState& state)
{
	MoveGenerator position;
	for (uint64_t index = 0; index < state.getEntryCount(); ++index) {
		ReverseIndex reverseIndex(index, state.getPieceList());
		if (!reverseIndex.isLegal()) {
			state.getComputedResults().setRawByte(index, Dtz::ILLEGAL);
			continue;
		}
		position.clear();
		addPiecesToPosition(position, reverseIndex, state.getPieceList());
		uint64_t testIndex = BoardAccess::getIndex<0>(position);
		if (index != testIndex || !position.isLegal()) {
			state.getComputedResults().setRawByte(index, Dtz::ILLEGAL);
		}
	}
}

/**
 * Computes and persists one concrete bitbase described by a piece list.
 *
 * @param pieceList Piece layout of the bitbase to generate.
 * @param first True if this is the primary requested bitbase.
 * @param generateCpp If true, also emits generated C++ code.
 */
void BitbaseGenerator::computeBitbase(PieceList& pieceList, bool first, bool generateCpp)
{
	MoveGenerator position;
	string pieceString = pieceList.getPieceString();
	PieceSignature pieceSignature(pieceString.c_str());
	if (pieceString.substr(0, 2) == "KK")
	{
		return;
	}

	cout << pieceString << " using " << _cores << " threads " << std::flush;

	_missingDependencies = 0;
	GenerationState state(pieceList, pieceSignature.getPiecesSignature());
	ClockManager clock;
	clock.setStartTime();

	auto& timing = BitbaseProfiling::getStaticInstance();

	computeBitbase(state, clock);

	cout << "c" << std::endl;
	timing.start("print statistic");
	printTimeSpent(clock);
	printStatistic(state);
	GenerationState::printTotalStatistic();
	timing.stop("print statistic");
	std::cout << std::endl;

	const uint64_t entryCount = state.getEntryCount();
	std::vector<BitbaseResult> repairResults;
	repairResults.reserve(entryCount);
	timing.start("qwdl collect sequence");
	{
		auto& bb = state.getComputedResults();
		for (uint64_t idx = 0; idx < entryCount; ++idx) {
			// Back from the side to move to white, which is what everything outside
			// the generator reads.
			const BitbaseResult mover = Dtz::toResult(bb.getRawByte(idx));
			const bool whiteToMove = (idx & 1) == 0;
			repairResults.push_back(
				whiteToMove || mover == BitbaseResult::Draw || mover == BitbaseResult::Unknown
					? mover
					: mover == BitbaseResult::Win ? BitbaseResult::Loss : BitbaseResult::Win);
		}
	}
	timing.stop("qwdl collect sequence");

	if (generateCpp)
	{
		state.generateCpp(pieceString);
	}

	if (_missingDependencies > 0)
		cerr << pieceString << ": " << _missingDependencies
			<< " captures had no bitbase to answer them - this table is not trustworthy"
			<< endl;

	// Write a Re-Pair + Huffman compressed copy, register it for subordinate lookups,
	// and verify every position.  The .qwdl file replaces the in-memory Bitbase: subsequent
	// bitbases that depend on this one probe it via BitbaseReader::getValueFromSingleBitbase()
	// through the memory-mapped .qwdl file instead of a heap-allocated Bitbase object.
	string qwdlFileName = pieceString + ".qwdl";
	try {
		timing.start("qwdl compress+write");
		cout << "Writing " << qwdlFileName << " ... " << std::flush;
		BitbaseRePairFile::write(qwdlFileName, repairResults);
		cout << "done" << std::endl;
		timing.stop("qwdl compress+write");

		// Register the file so parent bitbases can probe it via getValueFromSingleBitbase().
		BitbaseReader::registerQwdlFile(pieceString, qwdlFileName);

		// And the Syzygy file. It has to be written here, in the recursion, because
		// whether an entry may be stored below its true value is decided by probing
		// the tables one capture down - which exist at this point, and only here.
		timing.start("syzygy write");
		writeSyzygyWdl(pieceString, qwdlFileName, _syzygyPath, std::cout);
		timing.stop("syzygy write");

		// Verify every position against the original WDL sequence.
		BitbaseRePairFile reader;
		if (!reader.open(qwdlFileName)) {
			std::cerr << "Re-Pair: cannot reopen " << qwdlFileName << " for verification\n";
		} else {
			timing.start("qwdl verify");
			cout << "Verifying " << qwdlFileName << " (" << entryCount << " positions) ... " << std::flush;
			uint64_t mismatches = 0;
			for (uint64_t idx = 0; idx < entryCount; ++idx) {
				if (repairResults[idx] == BitbaseResult::Unknown) continue; // illegal position — joker, skip
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
			timing.stop("qwdl verify");
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
 * @param generateCpp If true, also emits generated C++ code.
 */
void BitbaseGenerator::computeBitbaseRec(PieceList &pieceList, bool first, bool generateCpp)
{
	if (pieceList.getNumberOfPieces() <= 2)
		return;
	string pieceString = pieceList.getPieceString();

	// White holds nothing but the king. There is no table of that shape - it is the
	// mirror of a white-strong one, and that is the table a capture into this material
	// is answered from. Asking for the mirror here is what makes it a dependency;
	// without it the parent is computed while the answer does not exist yet.
	if (pieceString.substr(0, 2) == "KK") {
		PieceList mirrored(pieceList);
		mirrored.toSymetric();
		computeBitbaseRec(mirrored, false, generateCpp);
		return;
	}

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
				computeBitbaseRec(newPieceList, false, generateCpp);
				newPieceList = pieceList;
			}
		}
		newPieceList.removePiece(pieceNo);
		computeBitbaseRec(newPieceList, false, generateCpp);
	}

	if (first || !BitbaseReader::isBitbaseAvailable(pieceString))
	{
		computeBitbase(pieceList, first, generateCpp);
	}
}
