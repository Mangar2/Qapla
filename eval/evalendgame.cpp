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
#include "eval-helper.h"
#include "evalendgame.h"
#include "kingpawnattack.h"
#include "../bitbase/bitbase.h"
#include "../bitbase/bitbaseindex.h"
#include "../bitbase/bitbase-reader.h"
#include "pawn.h"
#include "king.h"


using namespace ChessEval;
using namespace QaplaBitbase;


vector<EvalEndgame::evalFunction_t*> EvalEndgame::functionMap;
array<uint8_t, PieceSignature::PIECE_SIGNATURE_SIZE> EvalEndgame::mapPieceSignatureToFunctionNo;
EvalEndgame::InitStatics EvalEndgame::_staticConstructor;

#define REGISTER(index, function) registerFunction(index, function<WHITE>, false); registerFunction(index, function<BLACK>, true);


EvalEndgame::InitStatics::InitStatics() {
	mapPieceSignatureToFunctionNo.fill(255);

	// Queen
	REGISTER("KQ+R*B*N*P*K", forceToAnyCornerToMate);
	REGISTER("KQR*B*N*P*KB", ForceToCornerWithBonus);
	REGISTER("KQR*B*N*P*KN", ForceToCornerWithBonus);
	REGISTER("KQKR", KQKR);
	REGISTER("KQNKQ", forceToAnyCornerButDraw);
	REGISTER("KQP+KRP+", KQPsKRPs);
	REGISTER("KQNKRR", forceToAnyCornerButDraw);

	// Rook
	REGISTER("KR+B*N*P*K", forceToAnyCornerToMate);
	REGISTER("KRRP*KN", winningValue);
	REGISTER("KRRP*KB", winningValue);
	REGISTER("KRBKR", forceToAnyCornerButDraw);
	REGISTER("KRNKR", forceToAnyCornerButDraw);
	REGISTER("KRNKBB", forceToAnyCornerButDraw);
	REGISTER("KRKB", forceToAnyCornerButDraw);
	REGISTER("KRKN", forceToAnyCornerButDraw);
	REGISTER("KP+KR", KPsKR);

	// Bishop
	REGISTER("KBB+K", KBBK);
	REGISTER("KB+P+K", KBsPsK);
	REGISTER("KB+N+K", KBNK);
	REGISTER("KBB+KN", winningValue);
	REGISTER("KBBKR", forceToAnyCornerButDraw);
	REGISTER("KBNKR", forceToAnyCornerButDraw);
	REGISTER("KBKP", minusKnightPlusPawn);
	REGISTER("KBK", drawValue);

	// Knight
	REGISTER("KNP+K", KNPsK);
	REGISTER("KNNK", drawValue);
	REGISTER("KNNN+K", forceToAnyCornerToMate);
	REGISTER("KNNPK", winningValue);
	REGISTER("KNNKR", forceToAnyCornerButDraw);
	REGISTER("KNKP", minusKnightPlusPawn);
	REGISTER("KNK", drawValue);

	// Pawn
	REGISTER("KP+K", KPsK);
	registerFunction("KP+KP+", KPsKPs);

	// Draw situations
	REGISTER("KK", drawValue);

}

void EvalEndgame::registerFunction(string pieces, evalFunction_t function, bool changeSide) {
	const int16_t NOT_FOUND = -1;
	int16_t functionNo = NOT_FOUND;
	PieceSignature signature;
	uint32_t iteration = 0;
	for (uint8_t index = 0; index < functionMap.size(); index++) {
		if (functionMap[index] == function) {
			functionNo = index;
			break;
		}
	}
	if (functionNo == NOT_FOUND) {
		functionNo = int16_t(functionMap.size());
		functionMap.push_back(function);
	}
	if (functionMap.size() > 255) {
		cout << "Too many functions for endgame eval. Please change code" << endl;
		return;
	}
	while (signature.set(pieces, iteration)) {
		if (changeSide) {
			signature.changeSide();
		}
		mapPieceSignatureToFunctionNo[signature.getPiecesSignature()] = uint8_t(functionNo);
		iteration++;
	}
}

value_t EvalEndgame::getFromBitbase(MoveGenerator& position, value_t value) {
	if (value >= WINNING_BONUS || value <= -WINNING_BONUS) {
		return value;
	}
	const value_t bitbaseValue = position.isWhiteToMove() ? 
			BitbaseReader::getValueFromBitbase(position, value) : 
			- BitbaseReader::getValueFromBitbase(position, -value);
	return bitbaseValue;
}

template<Piece COLOR>
value_t EvalEndgame::KQPsKRPs(MoveGenerator& position, value_t value) {
	static const value_t QUEEN_BONUS_PER_PAWN = 10;
	
	auto result = value;
	auto pawnAmount = popCount(position.getPieceBB(WHITE_PAWN) | position.getPieceBB(BLACK_PAWN));
	result += whiteValue<COLOR>(QUEEN_BONUS_PER_PAWN) * pawnAmount;
	return result;
}

template <Piece COLOR>
value_t EvalEndgame::KPsK(MoveGenerator& position, value_t value) {
	value_t result = value;
	Square opponentKingSquare = position.getKingSquare<switchColor(COLOR)>();
	Square myKingSquare = position.getKingSquare<COLOR>();
	bitBoard_t pawns = position.getPieceBB(PAWN + COLOR);

	if ((pawns & (pawns - 1)) == 0) {
		return value;
	}

	bitBoard_t opponentKingInfluence = BitBoardMasks::kingMoves[opponentKingSquare] | (1ULL << opponentKingSquare);
	bitBoard_t myKingInfluence = BitBoardMasks::kingMoves[myKingSquare] | (1ULL << myKingSquare);

	if ((pawns & ~BitBoardMasks::FILE_A_BITMASK) == 0 && isSquareInBitMask<COLOR>(A8, opponentKingInfluence)) {
		result = DRAW_VALUE;
	}
	else if ((pawns & ~BitBoardMasks::FILE_H_BITMASK) == 0 && isSquareInBitMask<COLOR>(H8, opponentKingInfluence)) {
		result = DRAW_VALUE;
	}
	else if ((BitBoardMasks::shiftColor<COLOR, NORTH>(pawns) & myKingInfluence) != 0) {
		result += whiteValue<COLOR>(WINNING_BONUS);
	}
	return result;
}

value_t EvalEndgame::KPsKPs(MoveGenerator& position, value_t value) {
	
	value_t result = 0;
	EvalResults evalResults;

	Pawn evalPawn;
	result += evalPawn.computePawnValueNoPiece(position, evalResults);

	//evalResults.midgameInPercentV2 = 0;
	//result += King::eval(position, evalResults).endgame();
	
	/*
	value_t result = value;
	EvalResults evalResults;
	Pawn::computePassedPawns(position, evalResults);
	PawnRace pawnRace;
	value_t runnerValue = pawnRace.runnerRace(position,
		evalResults.passedPawns[WHITE], evalResults.passedPawns[BLACK]);
	if (runnerValue != 0) {
		result /= 4;
		result += runnerValue;
	}
	*/
	if (evalResults.passedPawns[WHITE] == 0 &&  evalResults.passedPawns[BLACK] == 0) {
		KingPawnAttack kingPawnAttack;
		result += kingPawnAttack.computeKingRace(position) * KING_RACED_PAWN_BONUS;
	}
	return result;
}

template <Piece COLOR>
value_t EvalEndgame::drawValue(MoveGenerator& position, value_t value) {
	return DRAW_VALUE;
}

template <Piece COLOR>
value_t EvalEndgame::nearDrawValue(MoveGenerator& position, value_t value) {
	return NEAR_DRAW[COLOR];
}

template <Piece COLOR>
value_t EvalEndgame::winningValue(MoveGenerator& position, value_t value) {
	return value + whiteValue<COLOR>(WINNING_BONUS);
}

template <Piece COLOR>
value_t EvalEndgame::KPsKR(MoveGenerator& position, value_t value) {
	if (position.getEvalVersion() == 0) return value;
	auto const OPPONENT = opponentColor<COLOR>();
	auto kingSquare = position.getKingSquare<COLOR>();
	auto opponentKingSquare = position.getKingSquare<OPPONENT>();
	auto opponentKingRank = getRank<COLOR>(opponentKingSquare);
	bitBoard_t pawns = position.getPieceBB(PAWN + COLOR);
	value_t runningPawns = 0;
	bool onlyOnePawn = (pawns & (pawns - 1)) == 0;
	while (pawns) {
		Square pawnSquare = popLSB(pawns);
		const auto pawnRank = getRank<COLOR>(pawnSquare);
		// checks, if king and pawn can force the rook to sacrifice
		if (pawnRank >= Rank::R5 && EvalHelper::computeDistance(pawnSquare, kingSquare) <= 2 && isRunner<COLOR>(position, pawnSquare)) {
			runningPawns++;
		}
		else if (onlyOnePawn) {
			auto opponentFileDistance = std::abs(static_cast<value_t>(getFile(opponentKingSquare) - getFile(pawnSquare)));
			if (opponentFileDistance <= 1 && opponentKingRank > pawnRank) value -= whiteValue<COLOR>(WINNING_BONUS);
		}
	}
	value += runningPawns * whiteValue<COLOR>(100);
	return value;
}

template <Piece COLOR>
value_t EvalEndgame::KBNK(MoveGenerator& position, value_t value) {
	Square themKingSquare = position.getKingSquare<switchColor(COLOR)>();
	bitBoard_t usBishops = position.getPieceBB(BISHOP + COLOR);
	Square usKnightSquare = lsb(position.getPieceBB(KNIGHT + COLOR));
	value_t cornerValue = forceToCorrectCorner<COLOR>(position, 0, usBishops & WHITE_FIELDS) * 5;
	value_t knightDistance = computeDistance(themKingSquare, usKnightSquare) * 2;
	value = whiteValue<COLOR>(MAX_BONUS + cornerValue - knightDistance);
	return value;
}

template <Piece COLOR>
value_t EvalEndgame::KBBK(MoveGenerator& position, value_t value) {
	value = DRAW_VALUE; 
	bitBoard_t bishops = position.getPieceBB(BISHOP + COLOR);
	if ((bishops & WHITE_FIELDS) && (bishops & BLACK_FIELDS)) {
		value = forceToAnyCornerToMate<COLOR>(position, 0);
	}
	return value;
}

template <Piece COLOR>
value_t EvalEndgame::KBsPsK(MoveGenerator& position, value_t value) {
	value_t result = value;
	Square opponentKingSquare = position.getKingSquare<switchColor(COLOR)>();
	bitBoard_t kingInfluence = BitBoardMasks::kingMoves[opponentKingSquare] | (1ULL << opponentKingSquare);

	bitBoard_t bishops = position.getPieceBB(BISHOP + COLOR);
	bitBoard_t pawns = position.getPieceBB(PAWN + COLOR);

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
		result += whiteValue<COLOR>(WINNING_BONUS);
	}
	return result;
}

template <Piece COLOR>
value_t EvalEndgame::KNPsK(MoveGenerator& position, value_t value) {
	value_t result = value;
	Square kingPos = position.getKingSquare<COLOR>();
	bitBoard_t pawns = position.getPieceBB(PAWN + COLOR);
	bitBoard_t pawnMoves = BitBoardMasks::shiftColor<COLOR, NORTH>(pawns);
	pawnMoves &= ~(position.getPieceBB(PAWN + COLOR) + position.getPieceBB(KING + COLOR));
	bitBoard_t kingMoves = position.pieceAttackMask[kingPos] & ~position.getAllPiecesBB() & ~position.attackMask[switchColor(COLOR)];

	if (pawnMoves == 0 && kingMoves == 0) {
		bool atMove = COLOR == WHITE ? position.isWhiteToMove() : !position.isWhiteToMove();
		bool allPawnsInOneEdge = (pawns & ~BitBoardMasks::FILE_A_BITMASK) == 0 || (pawns & ~BitBoardMasks::FILE_H_BITMASK) == 0;
		
		bitBoard_t opponentKingAndOwnKnight = position.getPieceBB(KNIGHT + COLOR) + position.getPieceBB(KING + switchColor(COLOR));
		bool opponentKingAndKnightOnSameColor = (opponentKingAndOwnKnight & WHITE_FIELDS) == 0 || (opponentKingAndOwnKnight & BLACK_FIELDS) == 0;
		bool knightPushesKingAway = (opponentKingAndKnightOnSameColor && !atMove) || (!opponentKingAndKnightOnSameColor && atMove);
		
		if (allPawnsInOneEdge && !knightPushesKingAway) {
			result = DRAW_VALUE;
		}
	}
	else {
		result += whiteValue<COLOR>(WINNING_BONUS);
	}
	return result;
}


template <Piece COLOR>
value_t EvalEndgame::KQKR(MoveGenerator& position, value_t value) {
	value_t result = value;
	const Piece OPPONENT_COLOR = switchColor(COLOR);
	Square opponentKingSquare = position.getKingSquare<OPPONENT_COLOR>();
	Square opponentRookPos = lsb(position.getPieceBB(ROOK + OPPONENT_COLOR));
	bitBoard_t kingAttackRay = Magics::genRookAttackMask(position.getKingSquare<COLOR>(), position.getPieceBB(KING + OPPONENT_COLOR));
	bool rookMayPutKingInCheckDefendedByOwnKing = (kingAttackRay & position.pieceAttackMask[opponentRookPos] & position.pieceAttackMask[opponentKingSquare]) != 0;

	if (!rookMayPutKingInCheckDefendedByOwnKing) {
		result += whiteValue<COLOR>(WINNING_BONUS);
	}

	result = forceToAnyCorner<COLOR>(position, result);
	return result;
}

template <Piece COLOR>
value_t EvalEndgame::forceToAnyCornerButDraw(MoveGenerator& position, value_t value) {
	return forceToAnyCorner<COLOR>(position, whiteValue<COLOR>(30));
}

template <Piece COLOR>
value_t EvalEndgame::forceToAnyCornerToMate(MoveGenerator& position, value_t value) {
	Square themKingSquare = position.getKingSquare<switchColor(COLOR)>();
	Square usKingSquare = position.getKingSquare<COLOR>();
	value_t themKingDistanceToCorner = computeDistanceToAnyCorner(themKingSquare);
	value_t usKingDinstanceToCorner = computeDistanceToAnyCorner(usKingSquare);
	value_t distanceValue = -computeKingDistance(position) * 8;
	distanceValue -= themKingDistanceToCorner * 4;
	if (usKingDinstanceToCorner < themKingDistanceToCorner) {
		distanceValue -= (themKingDistanceToCorner - usKingDinstanceToCorner) * 16;
	}

	value = whiteValue<COLOR>(MAX_BONUS + distanceValue);
	return value;
}

template <Piece COLOR>
value_t EvalEndgame::forceToAnyCorner(MoveGenerator& position, value_t value) {
	Square opponentKingSquare = position.getKingSquare<switchColor(COLOR)>();
	Square kingSquare = position.getKingSquare<COLOR>();
	value_t opponentKingDistanceToCorner = computeDistanceToAnyCorner(opponentKingSquare);
	value_t ownKingDinstanceToCorner = computeDistanceToAnyCorner(kingSquare);
	value_t distanceValue = -computeKingDistance(position) * 2;
	distanceValue -= opponentKingDistanceToCorner * 1;
	if (ownKingDinstanceToCorner < opponentKingDistanceToCorner) {
		distanceValue -= (opponentKingDistanceToCorner - ownKingDinstanceToCorner) * 4;
	}

	value += whiteValue<COLOR>(distanceValue);
	return value;
}

template <Piece COLOR>
value_t EvalEndgame::forceToCorrectCorner(MoveGenerator& position, value_t value, bool whiteCorner) {
	Square opponentKingSquare = position.getKingSquare<switchColor(COLOR)>();
	value_t distanceValue = -computeKingDistance(position) * 1 - computeDistanceToCorrectCorner(opponentKingSquare, whiteCorner) * 2;
	value += whiteValue<COLOR>(distanceValue);
	return value;
}

template <Piece COLOR>
value_t EvalEndgame::ForceToCornerWithBonus(MoveGenerator& position, value_t value) {
	value = forceToAnyCorner<COLOR>(position, value);
	value += whiteValue<COLOR>(WINNING_BONUS);
	return value;
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

template <Piece COLOR>
bool EvalEndgame::isRunner(MoveGenerator& board, Square pawnSquare) {
	Square ownKingSquare = switchSideToWhite<COLOR>(board.getKingSquare<COLOR>());
	Square opponentKingSquare = switchSideToWhite<COLOR>(board.getKingSquare<opponentColor<COLOR>()>());
	value_t opponentToMove = (COLOR == WHITE) == !board.isWhiteToMove();
	pawnSquare = switchSideToWhite<COLOR>(pawnSquare);
	auto pawnFile = getFile(pawnSquare);
	Square queeningSquare = computeSquare(pawnFile, Rank::R8);
	if (pawnSquare <= H2) pawnSquare += NORTH;
	value_t movesToQueen = static_cast<value_t>(Rank::R8 - getRank(pawnSquare)) + opponentToMove;
	if (EvalHelper::computeDistance(queeningSquare, opponentKingSquare) > movesToQueen) return true;
	bool ownKingIsFasterWest = pawnFile == File::A || 
		EvalHelper::computeDistance(queeningSquare + WEST, ownKingSquare) + opponentToMove <=
		EvalHelper::computeDistance(queeningSquare + WEST, opponentKingSquare);
	bool ownKingIsFasterEast = pawnFile == File::H ||
		EvalHelper::computeDistance(queeningSquare + EAST, ownKingSquare) + opponentToMove <=
		EvalHelper::computeDistance(queeningSquare + EAST, opponentKingSquare);
	if (ownKingIsFasterEast && ownKingIsFasterWest)
		return true;
	return false;
}

/**
 * Computes the "distance" = file + rank distance between two squares 
 */
value_t EvalEndgame::computeKingDistance(MoveGenerator& position) {
	Square square1 = position.getKingSquare<WHITE>();
	Square square2 = position.getKingSquare<BLACK>();
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
