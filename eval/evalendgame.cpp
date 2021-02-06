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

#include "evalresults.h"
#include "evalendgame.h"
#include "kingpawnattack.h"
#include "../bitbase/bitbaseindex.h"
#include "../bitbase/bitbasereader.h"


using namespace ChessEval;

EvalEndgame::evalFunction_t* EvalEndgame::functionMap[EvalEndgame::MAX_EVAL_FUNCTIONS];
uint8_t EvalEndgame::functionAmount;
uint8_t EvalEndgame::mapPieceSignatureToFunctionNo[PieceSignature::PIECE_SIGNATURE_SIZE];
EvalEndgame::InitStatics EvalEndgame::_staticConstructor;

#define REGISTER(index, function) registerFunction(index, function<WHITE>, false); registerFunction(index, function<BLACK>, true);


EvalEndgame::InitStatics::InitStatics() {
	for (uint32_t index = 0; index < PieceSignature::PIECE_SIGNATURE_SIZE; index++) {
		mapPieceSignatureToFunctionNo[index] = 255;
	}
	// Queen
	REGISTER("KQKR", KQKR);
	REGISTER("KQR*B*N*P*KB", ForceToCornerWithBonus);
	REGISTER("KQR*B*N*P*KN", ForceToCornerWithBonus);
	REGISTER("KQ+R*B*N*K", ForceToCornerWithBonus);
	REGISTER("KQP+KRP+", KQPsKRPs);
	
	// Rook
	REGISTER("KR+B*N*K", ForceToCornerWithBonus);
	
	// Bishop
	REGISTER("KBBK", KBBK);
	REGISTER("KB+P+K", KBsPsK);
	REGISTER("KBNK", KBNK);

	// Knight
	REGISTER("KNP+K", KNPsK);
	
	// Pawn
	REGISTER("KP+K", KPsK)

	registerFunction("KP+KP+", KPsKPs);
	
	// Winning situations
	REGISTER("KQ+R*B*N*P+K", winningValue);
	REGISTER("KR+B*N*P+K", winningValue);
	REGISTER("KNNPK", winningValue);

	// Near Draw situation
	REGISTER("KRBKR", forceToAnyCornerButDraw);
	REGISTER("KRNKR", forceToAnyCornerButDraw);
	REGISTER("KRNKBB", forceToAnyCornerButDraw);
	REGISTER("KBBKR", forceToAnyCornerButDraw);
	REGISTER("KBNKR", forceToAnyCornerButDraw);
	REGISTER("KNNKR", forceToAnyCornerButDraw);
	REGISTER("KRKB", forceToAnyCornerButDraw);
	REGISTER("KRKN", forceToAnyCornerButDraw);
	REGISTER("KBKP", minusKnightPlusPawn);
	REGISTER("KNKP", minusKnightPlusPawn);


	// Draw situations
	REGISTER("KBK", drawValue);
	REGISTER("KN+K", drawValue);
	REGISTER("KK", drawValue);

}


void EvalEndgame::registerFunction(string pieces, evalFunction_t function, bool changeSide) {
	uint8_t functionNo = 0;
	PieceSignature signature;
	uint32_t iteration = 0;
	for (uint8_t index = 0; index < functionAmount; index++) {
		if (functionMap[index] == function) {
			functionNo = index;
			break;
		}
	}
	if (functionNo == 0) {
		functionMap[functionAmount] = function;
		functionNo = functionAmount;
		functionAmount++;
	}
	while (signature.set(pieces, iteration)) {
		if (changeSide) {
			signature.changeSide();
		}
		mapPieceSignatureToFunctionNo[signature.getPiecesSignature()] = functionNo;
		iteration++;
	}
}

value_t EvalEndgame::materialAndPawnStructure(MoveGenerator& board) {
	EvalPawn evalPawn;
	EvalResults mobility;
	value_t result = board.getMaterialValue().endgame();
	result += evalPawn.eval(board, mobility);
	return result;
}

template<Piece COLOR>
value_t EvalEndgame::KQPsKRPs(MoveGenerator& board, value_t currentValue) {
	static const value_t QUEEN_BONUS_PER_PAWN = 10;
	
	auto result = materialAndPawnStructure(board);
	auto pawnAmount = BitBoardMasks::popCount(board.getPieceBB(WHITE_PAWN) | board.getPieceBB(BLACK_PAWN));
	result += QUEEN_BONUS_PER_PAWN * pawnAmount * COLOR_VALUE[COLOR];
	return result;
}

value_t EvalEndgame::KPsKPs(MoveGenerator& board, value_t currentValue) {
	value_t result = 0;
	EvalPawn evalPawn;
	EvalResults mobility;
	result += evalPawn.computePawnValueNoPiece(board, mobility);
	
	if (mobility.passedPawns[WHITE] == 0 && mobility.passedPawns[BLACK] == 0) {
		KingPawnAttack kingPawnAttack;
		result += kingPawnAttack.computeKingRace(board) * KING_RACED_PAWN_BONUS;
	}
	return result;
}

template <Piece COLOR>
value_t EvalEndgame::drawValue(MoveGenerator& board, value_t currentValue) {
	return DRAW_VALUE;
}

template <Piece COLOR>
value_t EvalEndgame::nearDrawValue(MoveGenerator& board, value_t currentValue) {
	return NEAR_DRAW[COLOR];
}

template <Piece COLOR>
value_t EvalEndgame::winningValue(MoveGenerator& board, value_t currentValue) {
	value_t result = materialAndPawnStructure(board);
	result += BONUS[COLOR];
	return result;
}

template <Piece COLOR>
value_t EvalEndgame::KPsK(MoveGenerator& board, value_t currentValue) {
	value_t result = materialAndPawnStructure(board);
	Square opponentKingSquare = board.getKingSquare<OPPONENT[COLOR]>();

	bitBoard_t pawns = board.getPieceBB(PAWN + COLOR);

	if ((pawns & (pawns - 1)) == 0) {
		result = BitbaseReader::getValueFromBitbase(board, currentValue);
	}
	else {

		bitBoard_t kingInfluence = BitBoardMasks::kingMoves[opponentKingSquare] | (1ULL << opponentKingSquare);

		if ((pawns & ~BitBoardMasks::FILE_A_BITMASK) == 0 && isSquareInBitMask<COLOR>(A8, kingInfluence)) {
			result = DRAW_VALUE;
		}
		else if ((pawns & ~BitBoardMasks::FILE_H_BITMASK) == 0 && isSquareInBitMask<COLOR>(H8, kingInfluence)) {
			result = DRAW_VALUE;
		}
		else if (BitBoardMasks::shiftColor<COLOR, NORTH>(pawns) && kingInfluence != 0) {
			result += BONUS[COLOR];
		}
	}
	return result;
}

template <Piece COLOR>
value_t EvalEndgame::KBNK(MoveGenerator& board, value_t currentValue) {
	value_t result = board.getMaterialValue().endgame();
	Square opponentKingSquare = board.getKingSquare<OPPONENT[COLOR]>();
	bitBoard_t bishops = board.getPieceBB(BISHOP + COLOR);

	result += BONUS[COLOR];
	Square knightPos = BitBoardMasks::lsb(board.getPieceBB(KNIGHT + COLOR));
	result += computeDistance(opponentKingSquare, knightPos) * COLOR == WHITE ? -2 : 2;
	result = forceToCorrectCorner<COLOR>(board, result, bishops & WHITE_FIELDS);
	return result;
}

template <Piece COLOR>
value_t EvalEndgame::KBBK(MoveGenerator& board, value_t currentValue) {
	value_t result = DRAW_VALUE; 
	bitBoard_t bishops = board.getPieceBB(BISHOP + COLOR);
	if ((bishops & WHITE_FIELDS) && (bishops & BLACK_FIELDS)) {
		result = board.getMaterialValue().endgame();
		result += BONUS[COLOR];
		result = forceToAnyCorner<COLOR>(board, result);
	}
	return result;
}

template <Piece COLOR>
value_t EvalEndgame::KBsPsK(MoveGenerator& board, value_t currentValue) {
	value_t result = materialAndPawnStructure(board);
	Square opponentKingSquare = board.getKingSquare<OPPONENT[COLOR]>();
	bitBoard_t kingInfluence = BitBoardMasks::kingMoves[opponentKingSquare] | (1ULL << opponentKingSquare);

	bitBoard_t bishops = board.getPieceBB(BISHOP + COLOR);
	bitBoard_t pawns = board.getPieceBB(PAWN + COLOR);

	if ((pawns & ~BitBoardMasks::FILE_A_BITMASK) == 0 &&
		isSquareInBB<COLOR>(A8, kingInfluence) &&
		bishopIsAbleToAttacksPromotionField<COLOR>(A1, bishops))
	{
		result = DRAW_VALUE;
	}
	else if ((pawns & ~BitBoardMasks::FILE_H_BITMASK) == 0 &&
		isSquareInBB<COLOR>(H8, kingInfluence) &&
		bishopIsAbleToAttacksPromotionField<COLOR>(H1, bishops))
	{
		result = DRAW_VALUE;
	}
	else {
		result += BONUS[COLOR];
	}
	return result;
}

template <Piece COLOR>
value_t EvalEndgame::KNPsK(MoveGenerator& board, value_t currentValue) {
	value_t result = materialAndPawnStructure(board);
	Square kingPos = board.getKingSquare<COLOR>();
	bitBoard_t pawns = board.getPieceBB(PAWN + COLOR);
	bitBoard_t pawnMoves = BitBoardMasks::shiftColor<COLOR, NORTH>(pawns);
	pawnMoves &= ~(board.getPieceBB(PAWN + COLOR) + board.getPieceBB(KING + COLOR));
	bitBoard_t kingMoves = board.pieceAttackMask[kingPos] & ~board.getAllPiecesBB() & ~board.attackMask[OPPONENT[COLOR]];

	if (pawnMoves == 0 && kingMoves == 0) {
		bool atMove = COLOR == WHITE ? board.isWhiteToMove() : !board.isWhiteToMove();
		bool allPawnsInOneEdge = (pawns & ~BitBoardMasks::FILE_A_BITMASK) == 0 || (pawns & ~BitBoardMasks::FILE_H_BITMASK) == 0;
		
		bitBoard_t opponentKingAndOwnKnight = board.getPieceBB(KNIGHT + COLOR) + board.getPieceBB(KING + OPPONENT[COLOR]);
		bool opponentKingAndKnightOnSameColor = (opponentKingAndOwnKnight & WHITE_FIELDS) == 0 || (opponentKingAndOwnKnight & BLACK_FIELDS) == 0;
		bool knightPushesKingAway = (opponentKingAndKnightOnSameColor && !atMove) || (!opponentKingAndKnightOnSameColor && atMove);
		
		if (allPawnsInOneEdge && !knightPushesKingAway) {
			result = DRAW_VALUE;
		}
	}
	else {
		result += BONUS[COLOR];
	}
	return result;
}


template <Piece COLOR>
value_t EvalEndgame::KQKR(MoveGenerator& board, value_t currentValue) {
	value_t result = board.getMaterialValue().endgame();
	const Piece OPPONENT_COLOR = OPPONENT[COLOR];
	Square opponentKingSquare = board.getKingSquare<OPPONENT_COLOR>();
	Square opponentRookPos = BitBoardMasks::lsb(board.getPieceBB(ROOK + OPPONENT_COLOR));
	bitBoard_t kingAttackRay = Magics::genRookAttackMask(board.getKingSquare<COLOR>(), board.getPieceBB(KING + OPPONENT_COLOR));
	bool rookMayPutKingInCheckDefendedByOwnKing = (kingAttackRay & board.pieceAttackMask[opponentRookPos] & board.pieceAttackMask[opponentKingSquare]) != 0;

	if (!rookMayPutKingInCheckDefendedByOwnKing) {
		result += BONUS[COLOR];
	}

	result = forceToAnyCorner<COLOR>(board, result);
	return result;
}

template <Piece COLOR>
value_t EvalEndgame::forceToAnyCornerButDraw(MoveGenerator& board, value_t currentValue) {
	return forceToAnyCorner<COLOR>(board, COLOR == WHITE ? 30 : -30);
}

template <Piece COLOR>
value_t EvalEndgame::forceToAnyCorner(MoveGenerator& board, value_t currentValue) {
	Square opponentKingSquare = board.getKingSquare<OPPONENT[COLOR]>();
	Square kingSquare = board.getKingSquare<COLOR>();
	value_t opponentKingDistanceToCorner = computeDistanceToAnyCorner(opponentKingSquare);
	value_t ownKingDinstanceToCorner = computeDistanceToAnyCorner(kingSquare);
	value_t distanceValue = -computeKingDistance(board) * 2;
	distanceValue -= opponentKingDistanceToCorner * 1;
	if (ownKingDinstanceToCorner < opponentKingDistanceToCorner) {
		distanceValue -= (opponentKingDistanceToCorner - ownKingDinstanceToCorner) * 4;
	}

	currentValue += COLOR == WHITE ? distanceValue : -distanceValue;
	return currentValue;
}

template <Piece COLOR>
value_t EvalEndgame::forceToCorrectCorner(MoveGenerator& board, value_t currentValue, bool whiteCorner) {
	Square opponentKingSquare = board.getKingSquare<OPPONENT[COLOR]>();
	value_t distanceValue = -computeKingDistance(board) * 1 - computeDistanceToCorrectCorner(opponentKingSquare, whiteCorner) * 2;
	currentValue += COLOR == WHITE ? distanceValue : -distanceValue;
	return currentValue;
}

template <Piece COLOR>
value_t EvalEndgame::ForceToCornerWithBonus(MoveGenerator& board, value_t currentValue) {
	currentValue = forceToAnyCorner<COLOR>(board, currentValue);
	currentValue += BONUS[COLOR];
	return currentValue;
}

template <Piece COLOR>
bool EvalEndgame::bishopIsAbleToAttacksPromotionField(Square file, bitBoard_t bishops) {
	bool result;
	if ((file & 1) == 0) {
		result = COLOR == WHITE ? (bishops & BLACK_FIELDS) : (bishops & WHITE_FIELDS);
	}
	else {
		result = COLOR == WHITE ? (bishops & WHITE_FIELDS) : (bishops & BLACK_FIELDS);
	}
	return result;
}

template <Piece COLOR>
inline bool EvalEndgame::isSquareInBB(Square square, bitBoard_t mask) {
	uint32_t shift = square;
	if (COLOR == BLACK) {
		// mapping the colum of a square keeping the rank (example B6 will become B2)
		shift ^= 0x38;
	}
	bitBoard_t positionMask = 1ULL << shift;
	return (mask & positionMask) != 0;
}

value_t EvalEndgame::computeDistance(Square square1, Square square2) {
	value_t fileDistance = abs(value_t(getFile(square1)) - value_t(getFile(square2)));
	value_t rankDistance = abs(value_t(getRank(square1)) - value_t(getRank(square2)));
	return fileDistance + rankDistance;
}

value_t EvalEndgame::computeKingDistance(MoveGenerator& board) {
	Square square1 = board.getKingSquare<WHITE>();
	Square square2 = board.getKingSquare<BLACK>();
	value_t result = computeDistance(square1, square2);
	return result;
}

value_t EvalEndgame::computeDistanceToBorder(Square kingPos) {
	value_t kingFile = value_t(getFile(kingPos));
	value_t kingRank = value_t(getRank(kingPos));
	value_t fileDistance = min(kingFile, 7 - kingFile);
	value_t rankDistance = min(kingRank, 7 - kingRank);
	value_t result = min(fileDistance, rankDistance) * 4 + max(fileDistance, rankDistance);
	return result;
}

value_t EvalEndgame::computeDistanceToAnyCorner(Square kingPos) {
	value_t kingFile = value_t(getFile(kingPos));
	value_t kingRank = value_t(getRank(kingPos));
	value_t fileDistance = min(kingFile, value_t(File::H) - kingFile);
	value_t rankDistance = min(kingRank, value_t(Rank::R8) - kingRank);
	return max(fileDistance, rankDistance) * 2 + min(fileDistance, rankDistance);
}

value_t EvalEndgame::computeDistanceToCorrectCorner(Square kingPos, bool whiteCorner) {
	value_t result;
	value_t kingFile = value_t(getFile(kingPos));
	value_t kingRank = value_t(getRank(kingPos));
	if (whiteCorner) {
		value_t distanceA8 = max(value_t(Rank::R8) - kingRank, kingFile);
		value_t distanceH1 = max(kingRank, value_t(File::H) - kingFile);
		result = min(distanceA8, distanceH1);
	}
	else {
		value_t distanceA1 = max(kingRank, kingFile);
		value_t distanceH8 = max(value_t(Rank::R8) - kingRank, value_t(File::H) - kingFile);
		result = min(distanceA1, distanceH8);
	}
	return result;
}
