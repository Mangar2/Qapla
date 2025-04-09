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

#include <string>
#include <iomanip>
#include "eval.h"
#include "evalendgame.h"
#include "pawn.h"
#include "queen.h"
#include "rook.h"
#include "bishop.h"
#include "knight.h"
#include "kingattack.h"
#include "king.h"
#include "threat.h"
#include "print-eval.h"

using namespace ChessEval;

std::vector<PieceInfo> Eval::fetchDetails(MoveGenerator& board, EvalResults& evalResults) {
	std::vector<PieceInfo> details;
	Pawn::evalWithDetails(board, evalResults, details);
	Rook::evalWithDetails(board, evalResults, details);
	Bishop::evalWithDetails(board, evalResults, details);
	Knight::evalWithDetails(board, evalResults, details);
	Queen::evalWithDetails(board, evalResults, details);
	King::evalWithDetails(board, evalResults, details);
	std::vector<PieceInfo> ppDetails;
	Pawn::evalPassedPawnThreatsWithDetails(board, evalResults, ppDetails);
	for (const auto& pp : ppDetails) {
		const auto square = pp.square;
		for (auto& detail : details) {
			if (detail.square == square) {
				detail.indexVector.insert(detail.indexVector.end(), pp.indexVector.begin(), pp.indexVector.end());
				detail.totalValue += pp.totalValue;
				break;
			}
		}
	}

	return details;
}

IndexVector Eval::computeIndexVector(MoveGenerator& position) {
	IndexVector indexVector;
	EvalResults evalResults;
	indexVector.push_back(IndexInfo{ "midgame", uint32_t(computeMidgameInPercent(position)), NO_PIECE });
	indexVector.push_back(IndexInfo{ "midgamev2", uint32_t(computeMidgameV2InPercent(position)), NO_PIECE });
	indexVector.push_back(IndexInfo{ "tempo", 0, position.isWhiteToMove() ? WHITE : BLACK });
	indexVector.push_back({ "kingPST", uint32_t(position.getKingSquare<WHITE>()), WHITE });
	indexVector.push_back({ "kingPST", uint32_t(switchSide(position.getKingSquare<BLACK>())), BLACK });
	initEvalResults(position, evalResults);
	const std::vector<PieceInfo> details = fetchDetails(position, evalResults);
	for (const auto& piece : details) {
		indexVector.insert(indexVector.end(), piece.indexVector.begin(), piece.indexVector.end());
	}
	KingAttack::eval(position, evalResults);
	KingAttack::addToIndexVector(evalResults, indexVector);
	Threat::addToIndexVector(position, evalResults, indexVector);
	return indexVector;
}

IndexLookupMap Eval::computeIndexLookupMap(MoveGenerator& position) {
	IndexLookupMap indexLookup = Pawn::getIndexLookup();
	indexLookup.merge(Knight::getIndexLookup());
	indexLookup.merge(Bishop::getIndexLookup());
	indexLookup.merge(Rook::getIndexLookup());
	indexLookup.merge(Queen::getIndexLookup());
	indexLookup.merge(KingAttack::getIndexLookup());
	indexLookup.merge(Threat::getIndexLookup());
	const auto& pieceValues = position.getPieceValues();
	indexLookup["material"] = std::vector<EvalValue>{ pieceValues.begin(), pieceValues.end() };
	indexLookup["kingPST"] = PST::getPSTLookup(KING);
	indexLookup["tempo"] = std::vector<EvalValue>{ EvalValue(tempo) };
	return indexLookup;
}

 /**
  * Calculates an evaluation for the current board position
  * The result is a value from the view of white, thus positive values are better for white
  * negative values better for black.
 */
template <bool PRINT>
value_t Eval::lazyEval(MoveGenerator& position, EvalResults& evalResults, value_t ply, PawnTT* pawnttPtr) {

	value_t result = 0;
	value_t endGameResult;
	initEvalResults(position, evalResults);

	evalResults.midgameInPercent = computeMidgameInPercent(position);
	evalResults.midgameInPercentV2 = computeMidgameV2InPercent(position);
	if (PRINT) cout << "Midgame factor:" << std::right << std::setw(20) << evalResults.midgameInPercentV2 << endl;
	
	// Add material to the evaluation
	value_t material = position.getMaterialAndPSTValue().getValue(evalResults.midgameInPercentV2);
	result += material;

	// Add paw value to the evaluation
	const auto pawnEval = Pawn::eval(position, evalResults, pawnttPtr);
	result += pawnEval;
	endGameResult = EvalEndgame::eval(position, result);

	if (endGameResult != result) {
		result = endGameResult;
		if (result > MIN_MATE_VALUE) {
			result -= ply;
		}
		if (result < MIN_MATE_VALUE) {
			result += ply;
		}
	}
	else {
		if (abs(result) < WINNING_BONUS) {
			result += position.isWhiteToMove() ? tempo : -tempo;
		}

		// Do not change ordering of the following calls. King attack needs result from Mobility
		EvalValue evalValue = Rook::eval(position, evalResults);
		evalValue += Bishop::eval(position, evalResults);
		evalValue += Knight::eval(position, evalResults);
		evalValue += Queen::eval(position, evalResults);
		evalValue += Threat::eval(position, evalResults);
		evalValue += Pawn::evalPassedPawnThreats(position, evalResults);
		if (evalResults.midgameInPercentV2 < 100) {
			evalValue += King::eval(position, evalResults);
		}
		evalValue += King::eval(position, evalResults);

		if (evalResults.midgameInPercent > 0) {
			result += KingAttack::eval(position, evalResults);
		}
		result += evalValue.getValue(evalResults.midgameInPercentV2);

		if constexpr (PRINT) {
			std::vector<PieceInfo> details = fetchDetails(position, evalResults);
			printEvalBoard(details, evalResults.midgameInPercentV2);
		}
	}
	// If a value == 0, the position will not be stored in hash tables
	// Value == 0 indicates a forced draw situation like repetetive moves 
	// or move count without pawn move or capture == 50
	if (result == 0) result = 1;
	return result;
}

void Eval::printEvalBoard(const std::vector<PieceInfo>& details, value_t midgameInPercent) {
	// Überschrift
	constexpr auto WIDTH = 11;
	constexpr auto ROWS = 5;
	std::cout 
		<< "        A           B           C           D           E           F           G           H" 
		<< std::endl
		<< "  +-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+" 
		<< std::endl;

	for (Rank rank = Rank::R8; rank >= Rank::R1; --rank) {
		for (int row = 0; row < ROWS; ++row) {
			if (row == 3) {
				std::cout << int(rank) + 1 << " |"; 
			}
			else {
				std::cout << "  |";
			}

			for (File file = File::A; file <= File::H; ++file) {
				const PieceInfo* pieceInfo = getPiece(details, computeSquare(file, rank));
				if (pieceInfo) {
					printPieceRow(row, *pieceInfo, midgameInPercent);
				}
				else {
					std::cout << std::setw(WIDTH) << " " << "|";
				}
			}
			std::cout << std::endl;
		}
		std::cout 
			<< "  +-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+"
			<< std::endl;

	}
}

const PieceInfo* Eval::getPiece(const std::vector<PieceInfo>& details, Square square) {
	for (const auto& piece : details) {
		if (piece.square == square) {
			return &piece;
		}
	}
	return nullptr;
}

void  Eval::printPieceRow(int row, const PieceInfo& pieceInfo, value_t midgameInPercent) {
	std::string content;
	constexpr auto width = 11;
	switch (row) {
	case 1: 
		content = (getPieceColor(pieceInfo.piece) == WHITE ? std::string("W") : std::string("B")) + pieceToChar(pieceInfo.piece);
		break;
	case 2: // Eval mid- and endgame
		content = pieceInfo.totalValue.toString();
		break;
	case 3: // Eval 
		content = std::to_string(pieceInfo.totalValue.getValue(midgameInPercent));
		break;
	case 4: // Properties-Zeile
		content = pieceInfo.propertyInfo;
		break;
	default: 
		content = "";
		break;
	}
	int padding = (width - int(content.length())) / 2;
	std::cout << std::setw(padding + content.length()) << content << std::setw(width + 1 - padding - content.length()) << "|";
}

void Eval::initEvalResults(MoveGenerator& position, EvalResults& evalResults) {
	evalResults.queensBB = position.getPieceBB(WHITE_QUEEN) | position.getPieceBB(BLACK_QUEEN);
	evalResults.pawnsBB = position.getPieceBB(WHITE_PAWN) | position.getPieceBB(BLACK_PAWN);
	evalResults.piecesAttack[WHITE] = evalResults.piecesAttack[BLACK] = 0;
	evalResults.piecesDoubleAttack[WHITE] = evalResults.piecesDoubleAttack[BLACK] = 0;
	position.computePinnedMask<WHITE>();
	position.computePinnedMask<BLACK>();
}

/**
 * Prints the evaluation results
 */
void Eval::printEval(MoveGenerator& board) {
	EvalResults evalResults;
	board.print();

	value_t evalValue = lazyEval<true>(board, evalResults, 0);

	const value_t endGameResult = EvalEndgame::print(board, evalValue);

	if (endGameResult == evalValue) {
		if (evalResults.midgameInPercent > 0) {
			KingAttack::print(board, evalResults);
		}
	}
	cout << "Total:" << std::right << std::setw(30) << evalValue << endl;
}

template value_t Eval::lazyEval<true>(MoveGenerator& board, EvalResults& evalResults, value_t ply, PawnTT* pawnttPtr);
template value_t Eval::lazyEval<false>(MoveGenerator& board, EvalResults& evalResults, value_t ply, PawnTT* pawnttPtr);

