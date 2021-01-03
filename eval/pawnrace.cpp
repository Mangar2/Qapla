#include "pawnrace.h"

using namespace ChessEval;

PawnRace::PawnRace()
{
}

template<Piece COLOR>
bool PawnRace::inFrontOfPawn(Square piecePos, Square pawnPos) {
	bool result = COLOR == WHITE ? piecePos > pawnPos : piecePos < pawnPos;
	result = result && (getFile(piecePos) == getFile(pawnPos));
	return result;
}

template<Piece COLOR>
uint32_t PawnRace::computePawnDistance(Square ownKingPos, Square pawnPos) {
	uint32_t curDistance = COLOR == WHITE ? uint32_t(getOppositRank(pawnPos)) : uint32_t(getRank(pawnPos));
	if (inFrontOfPawn<COLOR>(ownKingPos, pawnPos)) {
		curDistance++;
	}
	return curDistance;
}

template<Piece COLOR>
void PawnRace::computeFastestCandidate(const Board& board) {
	candidatePawnSquare[COLOR] = NO_SQUARE;
	bestRunnerDistanceInHalfmoves[COLOR] = Square(Rank::COUNT);
	hasRunner[COLOR] = false;

	bool atMove = board.isWhiteToMove() == (COLOR == WHITE);
	Square opponentKingPos = board.getKingSquare<OPPONENT[COLOR]>();
	Square ownKingPos = board.getKingSquare<COLOR>();
	uint64_t pawns = passedPawns[COLOR];

	for (; pawns != 0; pawns &= pawns - 1) {
		Square pawnPos = BitBoardMasks::lsb(pawns);
		if (!inFrontOfPawn<COLOR>(opponentKingPos, pawnPos)) {
			uint32_t curDistance = computePawnDistance<COLOR>(ownKingPos, pawnPos);
			if (curDistance == 6) {
				--curDistance;
			}
			if (curDistance < bestRunnerDistanceInHalfmoves[COLOR]) {
				bestRunnerDistanceInHalfmoves[COLOR] = curDistance;
				candidatePawnSquare[COLOR] = pawnPos;
			}
		}
	}
	bestRunnerDistanceInHalfmoves[COLOR] *= 2;
	if (candidatePawnSquare[COLOR] != NO_SQUARE) {
		passedPawns[COLOR] ^= 1ULL << candidatePawnSquare[COLOR];
		if (atMove) {
			bestRunnerDistanceInHalfmoves[COLOR] -= 1;
		}
	}
}

template<Piece COLOR>
inline void PawnRace::computeLegalPositions(const MoveGenerator& board) {
	bitBoard_t opponentKingAttack = board.pieceAttackMask[board.getKingSquare<OPPONENT[COLOR]>()];
	legalPositions[COLOR] = ~board.getPieceBB(PAWN + COLOR) & ~board.pawnAttackMask[OPPONENT[COLOR]] & ~opponentKingAttack;
}

template<Piece COLOR>
inline void PawnRace::initRace(const MoveGenerator& board) {
	computeLegalPositions<WHITE>(board);
	computeLegalPositions<BLACK>(board);
	kingPositions[WHITE] = board.getPieceBB(WHITE_KING);
	kingPositions[BLACK] = board.getPieceBB(BLACK_KING);
	pawnPositions[COLOR] = 1ULL << candidatePawnSquare[COLOR];
}

template<Piece COLOR>
inline void PawnRace::makeKingMove() {
	kingPositions[COLOR] = BitBoardMasks::moveInAllDirections(kingPositions[COLOR]) & legalPositions[COLOR];
}

template<Piece COLOR>
inline bool PawnRace::moveKingAway(Square ownKingPos) {
	bitBoard_t kingBitBoard = kingPositions[COLOR];
	kingPositions[COLOR] = BitBoardMasks::kingMoves[ownKingPos] & legalPositions[COLOR];
	kingPositions[COLOR] &= ~(BitBoardMasks::shift<NORTH>(kingBitBoard) | BitBoardMasks::shift<SOUTH>(kingBitBoard));
	kingPositions[COLOR] &= ~BitBoardMasks::moveInAllDirections(kingPositions[OPPONENT[COLOR]]);
	legalPositions[OPPONENT[COLOR]] &= ~BitBoardMasks::moveInAllDirections(kingPositions[COLOR]);
	return kingPositions[COLOR] != 0;
}

template<Piece COLOR>
inline void PawnRace::makePawnMove() {
	const bitBoard_t THIRD_ROW = COLOR == WHITE ? BitBoardMasks::RANK_3_BITMASK : BitBoardMasks::RANK_6_BITMASK;
	pawnPositions[COLOR] = BitBoardMasks::shiftPawnBitBoard<COLOR, NORTH>(pawnPositions[COLOR]);
	if (pawnPositions[COLOR] & THIRD_ROW) {
		pawnPositions[COLOR] = BitBoardMasks::shiftPawnBitBoard<COLOR, NORTH>(pawnPositions[COLOR]);
	}
}

template<Piece COLOR>
inline bool PawnRace::pawnPromoted() {
	bool result = (pawnPositions[COLOR] & PROMOTE_BIT_MASK[COLOR]) != 0;
	return result;
}

template<Piece COLOR>
void PawnRace::setPawnPromoted() {
	hasRunner[COLOR] = true;
	makeKingMove<OPPONENT[COLOR]>();
	isRunnerTempoSafe[COLOR] = !isPawnCapturedByKing<COLOR>();
	hasTempoCriticalPassedPawn[COLOR] = false;
}

template<Piece COLOR>
bool PawnRace::isPawnCapturedByKing() {
	return (kingPositions[OPPONENT[COLOR]] & pawnPositions[COLOR]) != 0;
}

template<Piece COLOR>
void PawnRace::setPawnCapturedByKing() {
	hasRunner[COLOR] = false;
	isRunnerTempoSafe[COLOR] = false;
	hasTempoCriticalPassedPawn[COLOR] = true;
	if (!pawnPromoted<COLOR>()) {
		makePawnMove<COLOR>();
		if (isPawnCapturedByKing<COLOR>()) {
			hasTempoCriticalPassedPawn[COLOR] = false;
		}
	}
}


template<Piece COLOR>
void PawnRace::checkIfCandidateIsRunner(const MoveGenerator& board) {
	initRace<COLOR>(board);
	Square ownKingPos = board.getKingSquare<COLOR>();
	bool atMove = board.isWhiteToMove() == (COLOR == WHITE);
	bool ownKingBlocksPawn = inFrontOfPawn<COLOR>(ownKingPos, candidatePawnSquare[COLOR]);

	while (1) {
		if (atMove) {
			if (ownKingBlocksPawn) {
				if (!moveKingAway<COLOR>(ownKingPos)) {
					break;
				}
				ownKingBlocksPawn = false;
			}
			else {
				makePawnMove<COLOR>();
			}
		}
		makeKingMove<OPPONENT[COLOR]>();
		if (isPawnCapturedByKing<COLOR>()) {
			setPawnCapturedByKing<COLOR>();
			break;
		}
		if (pawnPromoted<COLOR>()) {
			setPawnPromoted<COLOR>();
			break;
		}
		atMove = true;
	}
}

template<Piece COLOR>
value_t PawnRace::computeBonus() {
	value_t result = 0;
	if (hasRunner[COLOR] && !hasRunner[OPPONENT[COLOR]]) {
		bool notRunnerDueToOpponentPassedPawnForcesTempoLoss = (!isRunnerTempoSafe[COLOR] && hasTempoCriticalPassedPawn[OPPONENT[COLOR]]);
		if (!notRunnerDueToOpponentPassedPawnForcesTempoLoss) {
			result = (20 - bestRunnerDistanceInHalfmoves[COLOR]) * RUNNER_FACTOR;
		}
	}
	return COLOR==WHITE ? result : -result;
}

template<Piece COLOR>
void PawnRace::updateCandidate(const MoveGenerator& board) {
	checkIfCandidateIsRunner<COLOR>(board);
	if (!hasRunner[COLOR]) {
		computeFastestCandidate<COLOR>(board);
	}
}

template<Piece COLOR>
bool PawnRace::hasPromisingCandidate() {
	bool result;
	bool hasCandidate = candidatePawnSquare[COLOR] != NO_SQUARE;
	result = !hasRunner[COLOR] && hasCandidate;
	result &= bestRunnerDistanceInHalfmoves[COLOR] <= bestRunnerDistanceInHalfmoves[OPPONENT[COLOR]] + 1;
	return result;
}

value_t PawnRace::runnerRace(const MoveGenerator& board, bitBoard_t whitePassedPawns, bitBoard_t blackPassedPawns) {
	value_t result = 0;
	bool updated = true;
	
	passedPawns[WHITE] = whitePassedPawns;
	passedPawns[BLACK] = blackPassedPawns;

	computeFastestCandidate<WHITE>(board);
	computeFastestCandidate<BLACK>(board);
	bestPassedPawnDistanceInHalfmoves[WHITE] = bestRunnerDistanceInHalfmoves[WHITE];
	bestPassedPawnDistanceInHalfmoves[BLACK] = bestRunnerDistanceInHalfmoves[BLACK];
	hasTempoCriticalPassedPawn[WHITE] = false;
	hasTempoCriticalPassedPawn[BLACK] = false;

	while (updated) {
		updated = false;
		bool promisingWhiteCandidate = hasPromisingCandidate<WHITE>();
		bool promisingBlackCandidate = hasPromisingCandidate<BLACK>();
		if (promisingWhiteCandidate) {
			updateCandidate<WHITE>(board);
			updated = true;
		}
		if (promisingBlackCandidate) {
			updateCandidate<BLACK>(board);
			updated = true;
		}
	}
	result += computeBonus<WHITE>();
	result += computeBonus<BLACK>();

	return result;
}