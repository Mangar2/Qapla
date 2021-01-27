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
 */

#include "movegenerator.h"
#include "bitboardmasks.h"

using namespace ChessMoveGenerator;

MoveGenerator::MoveGenerator(void)
{
	clear();
}


// ----------------------------------------------------------------------------
// -------------------------- General -----------------------------------------
// ----------------------------------------------------------------------------

// -------------------------- genMovesSinglePiece ---------------------------
inline void MoveGenerator::
genMovesSinglePiece(uint32_t movingPiece, Square departure, bitBoard_t destinationBB, MoveList& moveList)
{
	Square destination;
	for (; destinationBB; destinationBB &= destinationBB - 1)
	{
		destination = BitBoardMasks::lsb(destinationBB);
		Piece capture = operator[](destination);
		Move move(departure, destination, movingPiece, capture);
		moveList.addMove(move);
	}
}
	
inline void MoveGenerator::
genMovesMultiplePieces(uint32_t movingPiece, int32_t aStep, bitBoard_t destinationBB, MoveList& moveList)
{
	for (; destinationBB; destinationBB &= destinationBB - 1)
	{
		const Square destination = BitBoardMasks::lsb(destinationBB);
		const Square departure = destination + aStep;
		Piece capture = operator[](destination);
		moveList.addMove(Move(departure, destination, movingPiece, capture));
	}
}

// -------------------------- computePinnedMask -------------------------------
template <Piece COLOR>
void MoveGenerator::computePinnedMask()
{
	const Piece OPPONENT_COLOR = COLOR == WHITE ? BLACK: WHITE;

	bitBoard_t result = 0;
	bitBoard_t ray;
	bitBoard_t allPieceNoPinned;
	// Get a mask of all rays starting from the king position until any piece is found
	ray = Magics::genQueenAttackMask(kingSquares[COLOR], bitBoardAllPieces);
	// Create a mask without all pieces that are possibly pinned
	allPieceNoPinned = bitBoardAllPieces & ~(bitBoardAllPiecesOfOneColor[COLOR] & ray);
	// Look for all pieces that could pin a piece
	ray = Magics::genBishopAttackMask(kingSquares[COLOR], allPieceNoPinned);
	// Check Bishops and Queens for the diagonalys
	ray &= (bitBoardsPiece[BISHOP + OPPONENT_COLOR] | bitBoardsPiece[QUEEN + OPPONENT_COLOR]);
	// Every piece set on Ray is pinning a white piece
	// Set every piece from pinning piece to white king
	for (; ray; ray &= ray - 1)
		result |= BitBoardMasks::mRay[kingSquares[COLOR] + BitBoardMasks::lsb(ray) * 64];
	// Now same thing with rooks
	ray = Magics::genRookAttackMask(kingSquares[COLOR], allPieceNoPinned);
	// Set bits on ray with white rook or queen 
	ray &= (bitBoardsPiece[ROOK + OPPONENT_COLOR] | bitBoardsPiece[QUEEN + OPPONENT_COLOR]);
	// Every piece set on Ray is pinning a black piece
	// Set every piece from pinning piece to own king
	for (; ray; ray &= ray - 1)
		result |= BitBoardMasks::mRay[kingSquares[COLOR] + BitBoardMasks::lsb(ray) * 64];
	
	// Now every bit on result is a position with black pinned piece or a position
	// the black piece may move to without setting the king to check
	pinnedMask[COLOR] =  result;
}

// -------------------------- computeAttackMaskForPiece -----------------------
template<Piece PIECE>
inline bitBoard_t MoveGenerator::computeAttackMaskForPiece(Square square, bitBoard_t allPiecesWithoutKing) {
	assert(square >= A1 && square <= H8);
	bitBoard_t result;
	switch (PIECE) {
	case KNIGHT: result = BitBoardMasks::knightMoves[square]; break;
	case BISHOP: result = Magics::genBishopAttackMask(square, allPiecesWithoutKing); break;
	case ROOK: result = Magics::genRookAttackMask(square, allPiecesWithoutKing); break;
	case QUEEN: result = Magics::genQueenAttackMask(square, allPiecesWithoutKing); break;
	case KING: result = BitBoardMasks::kingMoves[square]; break;
	}
	pieceAttackMask[square] = result;
	return result;
}

// -------------------------- computeAttackMaskForPieces ----------------------
template<Piece PIECE>
bitBoard_t MoveGenerator::computeAttackMaskForPieces(bitBoard_t pieceBB, bitBoard_t allPiecesWithoutKing) {
	bitBoard_t result = 0;
	for (; pieceBB; pieceBB &= pieceBB - 1)
		result |= computeAttackMaskForPiece<PIECE>(BitBoardMasks::lsb(pieceBB), allPiecesWithoutKing);
	return result;
}

// -------------------------- computeAttackMask -------------------------------
template <Piece COLOR>
bitBoard_t MoveGenerator::computeAttackMask()
{
	const Piece OPPONENT_COLOR = COLOR == WHITE ? BLACK : WHITE;

	bitBoard_t allPiecesButNoKing = bitBoardAllPieces & ~bitBoardsPiece[KING + OPPONENT_COLOR];
	pawnAttackMask[COLOR] = BitBoardMasks::computePawnAttackMask<COLOR>(bitBoardsPiece[PAWN + COLOR]);

	bitBoard_t result = pawnAttackMask[COLOR];
	result |= computeAttackMaskForPieces<KNIGHT>(bitBoardsPiece[KNIGHT + COLOR], allPiecesButNoKing);
	result |= computeAttackMaskForPieces<BISHOP>(bitBoardsPiece[BISHOP + COLOR], allPiecesButNoKing);
	result |= computeAttackMaskForPieces<ROOK>(bitBoardsPiece[ROOK + COLOR], allPiecesButNoKing);
	result |= computeAttackMaskForPieces<QUEEN>(bitBoardsPiece[QUEEN + COLOR], allPiecesButNoKing);
	if (kingSquares[COLOR] != NO_SQUARE) {
		result |= computeAttackMaskForPiece<KING>(kingSquares[COLOR], allPiecesButNoKing);
	}

	attackMask[COLOR] = result;
	return result;
}

// -------------------------- computeAttackMasksForBothColors -----------------
void MoveGenerator::computeAttackMasksForBothColors()
{
	attackMask[BLACK] = computeAttackMask<BLACK>();
	attackMask[WHITE] = computeAttackMask<WHITE>();
}

template <Piece COLOR>
void MoveGenerator::computeCastlingMasksForMoveGeneration()
{
	Square square;

	const Square kingSideCastlingTarget = COLOR == WHITE ? G1 : G8;
	const Square queenSideCastlingTarget = COLOR == WHITE ? C1 : C8;

	// every field between current king position to target king position hast to be
	// empty (except for castling rook in Chess960) and not attacked by enemy.
	// The masks are created here
	for (square = getKingSquare<COLOR>(); square <= kingSideCastlingTarget; ++square) {
		castleAttackMaskKingSide[COLOR] |= 1ULL << square;
	}
	for (square = getKingSquare<COLOR>(); square >= queenSideCastlingTarget; --square) {
		castleAttackMaskQueenSide[COLOR] |= 1ULL << square;
	}

	for (square = getKingSquare<COLOR>() + 1; square <= kingSideCastlingTarget; ++square) {
		if (square != getKingRookStartSquare<COLOR>()) castlePieceMaskKingSide[COLOR] |= 1ULL << square;
	}
	for (square = getKingSquare<COLOR>() - 1; 
		 square >= min(queenSideCastlingTarget, getQueenRookStartSquare<COLOR>());
		--square) {
		if (square != getQueenRookStartSquare<COLOR>()) castlePieceMaskQueenSide[COLOR] |= 1ULL << square;
	}
}


/**
 * Initializes all masks for move generation
 */
void MoveGenerator::initCastlingMasksForMoveGeneration() {
	castleAttackMaskKingSide.fill(0);
	castleAttackMaskQueenSide.fill(0);

	castlePieceMaskKingSide.fill(0);
	castlePieceMaskQueenSide.fill(0);
	computeCastlingMasksForMoveGeneration<WHITE>();
	computeCastlingMasksForMoveGeneration<BLACK>();
}

/**
 * Clears/initializes all fields
 */
void MoveGenerator::clear()
{
	Board::clear();
	initCastlingMasksForMoveGeneration();
	attackMask.fill(0);
	pawnAttackMask.fill(0);
	pieceAttackMask.fill(0);
	bitBoardsPiece.fill(0);
}


// ----------------------------------------------------------------------------
// -------------------------- Pawn Moves --------------------------------------
// ----------------------------------------------------------------------------

template <Piece COLOR>
inline void MoveGenerator::
genSilentPawnMoves(MoveList& moveList)
{
	const Piece PIECE = PAWN + COLOR;
	const bitBoard_t LAST_ROW = COLOR == WHITE ? BitBoardMasks::RANK_8_BITMASK : BitBoardMasks::RANK_1_BITMASK;
	const bitBoard_t RANK_4 = COLOR == WHITE ? BitBoardMasks::RANK_4_BITMASK : BitBoardMasks::RANK_5_BITMASK;
	const Square MOVE_UP = COLOR == WHITE ? NORTH : SOUTH;
	const Square MOVE_DOWN = COLOR == WHITE ? SOUTH : NORTH;

	bitBoard_t pawnTarget = (bitBoardsPiece[PIECE] & ~pinnedMask[COLOR]);
	pawnTarget = BitBoardMasks::shiftColor<COLOR, NORTH>(pawnTarget);
	// Do not generate promotions in silent moves
	pawnTarget &= ~bitBoardAllPieces & ~LAST_ROW;
	// Use all pawn bits with no piece in front
	genMovesMultiplePieces(PIECE, MOVE_DOWN, pawnTarget, moveList);
	// Use all pawn bits on row 2 with no piece in next two rows
	pawnTarget = BitBoardMasks::shiftColor<COLOR, NORTH>(pawnTarget);
	genMovesMultiplePieces(PIECE, MOVE_DOWN * 2, 
		pawnTarget & ~bitBoardAllPieces & RANK_4, moveList);
}

template <Piece COLOR>
void MoveGenerator::
genSilentSinglePawnMoves(Square departure, bitBoard_t allowedPositionMask, MoveList& moveList) {
	// use if pawn is pinnen. The pinning piece must be on the same column in front of the pawn.
	// We do not need to bother of promotions and pieces between the attacker and the pawn.
	const Piece PIECE = PAWN + COLOR;
	const bitBoard_t RANK_4 = COLOR == WHITE ? BitBoardMasks::RANK_4_BITMASK : BitBoardMasks::RANK_5_BITMASK;
	const Square MOVE_UP = COLOR == WHITE ? NORTH : -NORTH;

	bitBoard_t destination = BitBoardMasks::shiftColor<COLOR, NORTH>(1ULL << departure) & allowedPositionMask;
	if (destination)
	{
		moveList.addSilentMove(Move(departure, departure + MOVE_UP, PIECE));
		destination = BitBoardMasks::shiftColor<COLOR, NORTH>(destination);
		if (destination & allowedPositionMask & RANK_4) {
			moveList.addSilentMove(Move(departure, departure + 2 * MOVE_UP, PIECE));
		}
	}
}

// -------------------------- genNonSilentWhitePawnMoves ----------------------
template <Piece COLOR>
void MoveGenerator::
genPawnPromotions(bitBoard_t destinationBB, int32_t moveDirection, MoveList& moveList) {
	const bitBoard_t LAST_ROW = COLOR == WHITE ? BitBoardMasks::RANK_8_BITMASK : BitBoardMasks::RANK_1_BITMASK;
	const int32_t promotionDirection = COLOR == WHITE ? -moveDirection : moveDirection;
	
	for (destinationBB &= LAST_ROW; destinationBB; destinationBB &= destinationBB - 1)
	{
		Square destination = BitBoardMasks::lsb(destinationBB);
		Square departure = destination + promotionDirection;
		moveList.addPromote<COLOR>(departure, destination, operator[](destination));
	}
}

template <Piece COLOR>
void MoveGenerator::
genPawnCaptures(bitBoard_t destinationBB, int32_t moveDirection, MoveList& moveList) {
	const bitBoard_t LAST_ROW = COLOR == WHITE ? BitBoardMasks::RANK_8_BITMASK : BitBoardMasks::RANK_1_BITMASK;
	const int32_t captureDirection = COLOR == WHITE ? -moveDirection : moveDirection;

	genMovesMultiplePieces(PAWN + COLOR, captureDirection, destinationBB & ~LAST_ROW, moveList);
	genPawnPromotions<COLOR>(destinationBB, moveDirection, moveList);
}

template<Piece COLOR>
void MoveGenerator::
genEPMove(Square departure, Square epPos, MoveList& moveList)
{
	const Piece OPPONENT_COLOR = COLOR == WHITE ? BLACK: WHITE;
	const Square DIRECTION = COLOR == WHITE ? NORTH : -NORTH;

	bitBoard_t allPiecesAfterEPMove = bitBoardAllPieces;
	bitBoard_t attack;
	// Set the ep move to the All Pieces bitboard to test checks
	// Set destination bit
	allPiecesAfterEPMove |= 1ULL << (epPos + DIRECTION);
	// Remove starting position bit and captured piece position bit
	allPiecesAfterEPMove &= ~ ((1ULL << departure) | (1ULL << epPos));
	// Now test if king is in check, check bishops and queens
	attack = Magics::genBishopAttackMask(kingSquares[COLOR], allPiecesAfterEPMove) & 
		(bitBoardsPiece[BISHOP + OPPONENT_COLOR] | bitBoardsPiece[QUEEN  + OPPONENT_COLOR]);
	// Check rooks and queens
	attack |= Magics::genRookAttackMask(kingSquares[COLOR], allPiecesAfterEPMove) & 
		(bitBoardsPiece[ROOK + OPPONENT_COLOR] | bitBoardsPiece[QUEEN + OPPONENT_COLOR]);
	// if king is not in check after move, generate ep move
	if (!attack)
	{
		moveList.addNonSilentMove(
			Move(departure, epPos + DIRECTION, Move::EP_CODE_UNSHIFTED + PAWN + COLOR, PAWN + OPPONENT_COLOR));
	}
}


template <Piece COLOR>
void MoveGenerator::
genNonSilentPawnMoves(MoveList& moveList, Square epPos)
{
	const Piece OPPONENT_COLOR = COLOR == WHITE ? BLACK: WHITE;
	bitBoard_t pawns = (bitBoardsPiece[PAWN + COLOR] & ~pinnedMask[COLOR]);
	bitBoard_t pawnTarget;
	
	// Test capturing to the left. All pawns on column A are cleared
	pawnTarget = BitBoardMasks::shiftColor<COLOR, NW>(pawns); 
	genPawnCaptures<COLOR>(pawnTarget & bitBoardAllPiecesOfOneColor[OPPONENT_COLOR], NW, moveList);

	// Test capturing to the right. All pawns on column H are cleared
	pawnTarget = BitBoardMasks::shiftColor<COLOR, NE>(pawns);
	genPawnCaptures<COLOR>(pawnTarget & bitBoardAllPiecesOfOneColor[OPPONENT_COLOR], NE, moveList);

	// Generate non capture promotions
	genPawnPromotions<COLOR>(BitBoardMasks::shiftColor<COLOR, NORTH>(pawns) & ~bitBoardAllPieces, 
		NORTH, moveList);

	if (epPos)
	{
		for(bitBoard_t epPawns = BitBoardMasks::mEPMask[epPos] & pawns; epPawns; epPawns &= epPawns - 1)
		{
			genEPMove<COLOR>(BitBoardMasks::lsb(epPawns), epPos, moveList);
		}
	}
}

template <Piece COLOR>
void MoveGenerator::
genPawnCaptureSinglePiece(Square departure, bitBoard_t destinationBB, MoveList& moveList) {
	Square destination;
	for (; destinationBB; destinationBB &= destinationBB - 1)
	{
		destination = BitBoardMasks::lsb(destinationBB);
		if ((COLOR == WHITE && destination >= A8) || (COLOR == BLACK && destination <= H1)) {
			moveList.addPromote<COLOR>(departure, destination, operator[](destination));
		}
		else {
			moveList.addNonSilentMove(Move(departure, destination, PAWN + COLOR, operator[](destination)));
		}
	}
}

// -------------------------- GenSilentMoves ----------------------------------
template<Piece PIECE, MoveGenerator::moveGenType_t TYPE>
inline void MoveGenerator::
genMovesForPiece(MoveList& moveList)
{
	Square departure;
	bitBoard_t attack;
	const Piece COLOR = getPieceColor(PIECE);
	const Piece OPPONENT_COLOR = COLOR == WHITE ? BLACK : WHITE;
	bitBoard_t pieces = bitBoardsPiece[PIECE] & ~pinnedMask[COLOR];

	while (pieces)
	{
		departure = BitBoardMasks::lsb(pieces);
		pieces &= pieces - 1;

		attack = pieceAttackMask[departure];
		if (PIECE == BLACK_KING || PIECE == WHITE_KING) {
			attack &= ~attackMask[OPPONENT_COLOR];
		}

		attack &= TYPE == SILENT ? ~bitBoardAllPieces : bitBoardAllPiecesOfOneColor[OPPONENT_COLOR];

		for (; attack; attack &= attack - 1) {
			Square destination = BitBoardMasks::lsb(attack);
			if (TYPE == SILENT) {
				moveList.addSilentMove(Move(departure, destination, PIECE));
			}
			else {
				moveList.addNonSilentMove(Move(departure, destination, PIECE, operator[](destination)));
			}
		}
	}
}


// -------------------------- genNonPinnedMovesForAllPieces ---------------------------
template<MoveGenerator::moveGenType_t TYPE, Piece COLOR>
void MoveGenerator::genNonPinnedMovesForAllPieces(MoveList& moveList) {

	if (TYPE == NON_SILENT) {
		genNonSilentPawnMoves<COLOR>(moveList, getEP());
	}
	else {
		genSilentPawnMoves<COLOR>(moveList);
	}
	genMovesForPiece<KNIGHT + COLOR, TYPE>(moveList);
	genMovesForPiece<ROOK + COLOR, TYPE>(moveList);
	genMovesForPiece<BISHOP + COLOR, TYPE>(moveList);
	genMovesForPiece<QUEEN + COLOR, TYPE>(moveList);
	genMovesForPiece<KING + COLOR, TYPE>(moveList);
}

template<Piece COLOR>
void MoveGenerator::genPinnedMovesForAllPieces(MoveList& moveList, Square epPos)
{
	bitBoard_t pieces;
	bitBoard_t destination;
	bitBoard_t allowedRayMask;
	Square departure;

	for (pieces = pinnedMask[COLOR] & bitBoardAllPiecesOfOneColor[COLOR]; pieces; pieces &= pieces - 1)
	{
		departure = BitBoardMasks::lsb(pieces);
		// Assure that piece stays in ray
		allowedRayMask = BitBoardMasks::mFullRay[kingSquares[COLOR] + departure * 64] & pinnedMask[COLOR];
		Piece piece = operator[](departure);
		switch (piece) {
		// Pinned KNIGHTS can never move.
		case PAWN + COLOR:
			genSilentSinglePawnMoves<COLOR>(departure, ~bitBoardAllPieces & allowedRayMask, moveList);
			// Captures, not a bug: bitBoardAllPieces is ok to use. A pinned piece will never
			// have a piece of own color in the pinning ray
			destination = BitBoardMasks::pawnCaptures[COLOR][departure];
			destination &= allowedRayMask & bitBoardAllPieces;
			genPawnCaptureSinglePiece<COLOR>(departure, destination, moveList);

			// En passant moves
			if (epPos && (BitBoardMasks::mEPMask[epPos] & (1ULL << departure)))
			{
				genEPMove<COLOR>(departure, epPos, moveList);
			}
			break;
		case BISHOP + COLOR:
		case ROOK + COLOR:
		case QUEEN + COLOR:
			destination = pieceAttackMask[departure] & allowedRayMask;
			genMovesSinglePiece(piece, departure, destination & bitBoardAllPieces, moveList);
			genMovesSinglePiece(piece, departure, destination & ~bitBoardAllPieces, moveList);
			break;
		}
	}
}

template<Piece COLOR>
void MoveGenerator::genPinnedCapturesForAllPieces(MoveList& moveList, Square epPos)
{
	bitBoard_t pieces;
	bitBoard_t destination;
	bitBoard_t allowedRayMask;
	Square departure;

	for (pieces = pinnedMask[COLOR] & bitBoardAllPiecesOfOneColor[COLOR]; pieces; pieces &= pieces - 1)
	{
		departure = BitBoardMasks::lsb(pieces);
		// Assure that piece stays in ray
		allowedRayMask = BitBoardMasks::mFullRay[kingSquares[COLOR] + departure * 64] & pinnedMask[COLOR];
		Piece piece = operator[](departure);
		switch (piece) {
		// Pinned KNIGHTS can never move.
		case PAWN + COLOR:
			// Captures, not a bug: bitBoardAllPieces is ok to use. A pinned piece will never
			// have a piece of own color in the pinning ray
			destination = BitBoardMasks::pawnCaptures[COLOR][departure];
			destination &= allowedRayMask & bitBoardAllPieces;
			genPawnCaptureSinglePiece<COLOR>(departure, destination, moveList);

			// En passant moves
			if (epPos && (BitBoardMasks::mEPMask[epPos] & (1ULL << departure)))
			{
				genEPMove<COLOR>(departure, epPos, moveList);
			}
			break;
		case BISHOP + COLOR:
		case ROOK + COLOR:
		case QUEEN + COLOR:
			destination = pieceAttackMask[departure] & allowedRayMask;
			genMovesSinglePiece(piece, departure, destination & bitBoardAllPieces, moveList);
			break;
		}
	}
}

// ----------------------------------------------------------------------------
// -------------------------- Evades ------------------------------------------
// ----------------------------------------------------------------------------

template <Piece PIECE>
void MoveGenerator::genEvadesByBlocking(MoveList& moveList,
	bitBoard_t removePinnedPiecesMask,
	bitBoard_t blockingPositions) 
{
	Square      departure;
	bitBoard_t destination = bitBoardsPiece[PIECE] & removePinnedPiecesMask;
	for (destination ; destination; destination &= destination - 1)
	{
		departure = BitBoardMasks::lsb(destination);
		genMovesSinglePiece(PIECE, departure, pieceAttackMask[departure] & blockingPositions, moveList);
	}
}

template <Piece COLOR>
void MoveGenerator::genEvades(MoveList& moveList)
{
	const bitBoard_t LAST_ROW = COLOR == WHITE ? BitBoardMasks::RANK_8_BITMASK : BitBoardMasks::RANK_1_BITMASK;
	const bitBoard_t RANK_4 = COLOR == WHITE ? BitBoardMasks::RANK_4_BITMASK : BitBoardMasks::RANK_5_BITMASK;
	const Piece OPPONENT_COLOR = COLOR == WHITE ? BLACK : WHITE;
	const Square MOVE_DOWN = COLOR == WHITE ? SOUTH : NORTH;

	bitBoard_t directAttack;
	bitBoard_t rangeAttack;
	bitBoard_t possibleTargetPositions;
	bitBoard_t destination;
	bitBoard_t pawns;
	bitBoard_t removePinnedPiecesMask = ~pinnedMask[COLOR];
	Square epPos = getEP();
	moveList.clear();

	// Check if a pawn is attacking king
	directAttack = BitBoardMasks::shiftColor<COLOR, NW>(bitBoardsPiece[KING + COLOR]);
	directAttack |= BitBoardMasks::shiftColor<COLOR, NE>(bitBoardsPiece[KING + COLOR]);
	directAttack &= bitBoardsPiece[PAWN + OPPONENT_COLOR];

	// Check if a knight is attacking king
	directAttack |= BitBoardMasks::knightMoves[kingSquares[COLOR]] & bitBoardsPiece[KNIGHT + OPPONENT_COLOR];

	// Now check if a range piece is attacking king
	rangeAttack = Magics::genBishopAttackMask(kingSquares[COLOR], bitBoardAllPieces) & 
		(bitBoardsPiece[BISHOP + OPPONENT_COLOR] | bitBoardsPiece[QUEEN + OPPONENT_COLOR]);
	rangeAttack |= Magics::genRookAttackMask(kingSquares[COLOR], bitBoardAllPieces) &
		(bitBoardsPiece[ROOK + OPPONENT_COLOR] | bitBoardsPiece[QUEEN + OPPONENT_COLOR]);

	// Check if more than one piece is attacking the king. If yes we can´t 
	// do anything else than moving the king
	possibleTargetPositions = directAttack | rangeAttack;
	// Must have any attack, else king would not be in check
	assert(possibleTargetPositions);
	if ((possibleTargetPositions & possibleTargetPositions - 1) == 0)
	{
		// If there is only one range attack, then calculate the "inbetween mask"
		// i.e. the ray between king and piece
		if (rangeAttack)
		{
			possibleTargetPositions = BitBoardMasks::mRay[kingSquares[COLOR] + BitBoardMasks::lsb(rangeAttack) * 64];
		}
		// Now targetBoard contains bits any piece may move to. Thus we now may generate the moves
		// First try pawn
		// Advance one
		// Move the occupied bits to current pawn position
		pawns = bitBoardsPiece[PAWN + COLOR] & removePinnedPiecesMask;
		destination = BitBoardMasks::shiftColor<COLOR, NORTH>(pawns) & ~bitBoardAllPieces;
		// Use all pawn bits with no piece in front
		genMovesMultiplePieces(uint32_t(PAWN + COLOR), MOVE_DOWN, destination & possibleTargetPositions & ~LAST_ROW, moveList);
		genPawnPromotions<COLOR>(destination & possibleTargetPositions, NORTH, moveList);

		// Use all pawn bits on row 2 with no piece in next two rows
		destination = BitBoardMasks::shiftColor<COLOR, NORTH>(destination) & RANK_4 & ~bitBoardAllPieces & possibleTargetPositions;
		genMovesMultiplePieces(PAWN + COLOR, MOVE_DOWN + MOVE_DOWN, destination, moveList);
		
		// Test capturing to the left. All pawns on column A are cleared
		destination = BitBoardMasks::shiftColor<COLOR, NW>(pawns) & possibleTargetPositions & bitBoardAllPiecesOfOneColor[OPPONENT_COLOR];
		genPawnCaptures<COLOR>(destination , NW, moveList);

		// Test capturing to the right. All pawns on column H are cleared
		destination = BitBoardMasks::shiftColor<COLOR, NE>(pawns) & possibleTargetPositions & bitBoardAllPiecesOfOneColor[OPPONENT_COLOR];
		genPawnCaptures<COLOR>(destination, NE, moveList);

		// En passant moves
		if (epPos)
		{
			for(destination = BitBoardMasks::mEPMask[epPos] & pawns; destination; destination &= destination - 1)
			{
				genEPMove<COLOR>(BitBoardMasks::lsb(destination), epPos, moveList);
			}
		}
		
		genEvadesByBlocking<KNIGHT + COLOR>(moveList, removePinnedPiecesMask, possibleTargetPositions);
		genEvadesByBlocking<BISHOP + COLOR>(moveList, removePinnedPiecesMask, possibleTargetPositions);
		genEvadesByBlocking<ROOK + COLOR>(moveList, removePinnedPiecesMask, possibleTargetPositions);
		genEvadesByBlocking<QUEEN + COLOR>(moveList, removePinnedPiecesMask, possibleTargetPositions);
	}
	// Generate king moves
	genMovesSinglePiece(KING + COLOR, kingSquares[COLOR], 
		pieceAttackMask[kingSquares[COLOR]] & ~bitBoardAllPiecesOfOneColor[COLOR] & ~(attackMask[OPPONENT_COLOR]), moveList);
	
}

void MoveGenerator::genEvadesOfMovingColor(MoveList& moveList) {
	if (isWhiteToMove()) {
		computePinnedMask<WHITE>();
		genEvades<WHITE>(moveList);
	}
	else {
		computePinnedMask<BLACK>();
		genEvades<BLACK>(moveList);
	}
}

// -------------------------- GenNonSilentMoves -------------------------------
template <Piece COLOR>
void MoveGenerator::genNonSilentMoves(MoveList& moveList) {
	computePinnedMask<COLOR>();
	if (isInCheck()) {
		genEvades<COLOR>(moveList);
	}
	else {
		moveList.clear();
		genNonPinnedMovesForAllPieces<NON_SILENT, COLOR>(moveList);
		genPinnedCapturesForAllPieces<COLOR>(moveList, getEP());
	}
}

void MoveGenerator::genNonSilentMovesOfMovingColor(MoveList & moveList) {
	if (isWhiteToMove()) {
		genNonSilentMoves<WHITE>(moveList);
	}
	else {
		genNonSilentMoves<BLACK>(moveList);
	}
}

// -------------------------- genMoves ----------------------------------------
template <Piece COLOR>
void MoveGenerator::genMoves(MoveList& moveList)
{
	const Piece OPPONENT_COLOR = COLOR == WHITE ? BLACK: WHITE;
	const Square QUEEN_SIDE_CASTLE = COLOR == WHITE ? C1 : C8;
	const Square KING_SIDE_CASTLE = COLOR == WHITE ? G1 : G8;

	computePinnedMask<COLOR>();

	if (isInCheck())
	{
		genEvades<COLOR>(moveList);
	}
	else
	{
		moveList.clear();
		genNonPinnedMovesForAllPieces<NON_SILENT, COLOR>(moveList);
		genNonPinnedMovesForAllPieces<SILENT, COLOR>(moveList);

		// Castle move, if castle is allowed, positions are not attaced and no piece except rook
		// are standing in the row
		if (isKingSideCastleAllowed<COLOR>() && (attackMask[OPPONENT_COLOR] & castleAttackMaskKingSide[COLOR]) == 0 &&
			(castlePieceMaskKingSide[COLOR] & bitBoardAllPieces) == 0)
			moveList.addSilentMove(Move(kingSquares[COLOR], KING_SIDE_CASTLE, Move::KING_CASTLES_KING_SIDE + COLOR));
		if (isQueenSideCastleAllowed<COLOR>() && (attackMask[OPPONENT_COLOR] & castleAttackMaskQueenSide[COLOR]) == 0 &&
			(castlePieceMaskQueenSide[COLOR] & bitBoardAllPieces) == 0)
			moveList.addSilentMove(Move(kingSquares[COLOR], QUEEN_SIDE_CASTLE, Move::KING_CASTLES_QUEEN_SIDE + COLOR));

		genPinnedMovesForAllPieces<COLOR>(moveList, getEP());
	}
}

void MoveGenerator::genMovesOfMovingColor(MoveList& moveList) {
	if (isWhiteToMove()) {
		genMoves<WHITE>(moveList);
	}
	else {
		genMoves<BLACK>(moveList);
	}
}
