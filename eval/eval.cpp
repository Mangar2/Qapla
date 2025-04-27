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
#include "king-attack.h"
#include "king.h"
#include "threat.h"
//#include "eval-correction.h"

using namespace ChessEval;

std::vector<PieceInfo> Eval::fetchDetails(MoveGenerator& position) {
	EvalResults evalResults;
	initEvalResults(position, evalResults);
	evalResults.midgameInPercent = computeMidgameInPercent(position);
	evalResults.midgameInPercentV2 = computeMidgameV2InPercent(position);

	std::vector<PieceInfo> details;
	Pawn::evalWithDetails(position, evalResults, details);
	Rook::evalWithDetails(position, evalResults, details);
	Bishop::evalWithDetails(position, evalResults, details);
	Knight::evalWithDetails(position, evalResults, details);
	Queen::evalWithDetails(position, evalResults, details);
	King::evalWithDetails(position, evalResults, details);
	std::vector<PieceInfo> moreDetails;
	Pawn::evalPassedPawnThreatsWithDetails(position, evalResults, moreDetails);
	KingAttack::evalWithDetails(position, evalResults, moreDetails);
	for (const auto& add : moreDetails) {
		const auto square = add.square;
		for (auto& detail : details) {
			if (detail.square == square) {
				detail.indexVector.insert(detail.indexVector.end(), add.indexVector.begin(), add.indexVector.end());
				detail.totalValue += add.totalValue;
				detail.propertyInfo += add.propertyInfo;
				break;
			}
		}
	}

	return details;
}

IndexVector Eval::computeIndexVector(MoveGenerator& position) {
	IndexVector indexVector;
	uint32_t sig = position.getPiecesSignature();
	indexVector.push_back(IndexInfo{ "pieceSignature", sig, NO_PIECE });
	return indexVector;
	EvalResults evalResults;
	indexVector.push_back(IndexInfo{ "midgame", uint32_t(computeMidgameInPercent(position)), NO_PIECE });
	indexVector.push_back(IndexInfo{ "midgamev2", uint32_t(computeMidgameV2InPercent(position)), NO_PIECE });
	indexVector.push_back(IndexInfo{ "tempo", 0, position.isWhiteToMove() ? WHITE : BLACK });
	indexVector.push_back({ "kingPST", uint32_t(position.getKingSquare<WHITE>()), WHITE });
	indexVector.push_back({ "kingPST", uint32_t(switchSide(position.getKingSquare<BLACK>())), BLACK });
	const std::vector<PieceInfo> details = fetchDetails(position);
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
value_t Eval::lazyEval(MoveGenerator& position,value_t ply, PawnTT* pawnttPtr) {

	value_t result = 0;
	EvalResults evalResults;
	initEvalResults(position, evalResults);

	evalResults.midgameInPercent = computeMidgameInPercent(position);
	evalResults.midgameInPercentV2 = computeMidgameV2InPercent(position);
	
	// Add material to the evaluation
	EvalValue evalValue = position.getMaterialAndPSTValue();

	// Add paw value to the evaluation
	evalValue += Pawn::eval(position, evalResults, pawnttPtr);

	// Try endgame evaluation first. If it returns a modified value, use it.
	// This indicates a known endgame pattern was recognized.
	// Otherwise, continue the standard evaluation


	// Do not change ordering of the following calls. King attack needs result from Mobility
	evalValue += Rook::eval(position, evalResults);
	evalValue += Bishop::eval(position, evalResults);
	evalValue += Knight::eval(position, evalResults);
	evalValue += Queen::eval(position, evalResults);
	evalValue += Threat::eval(position, evalResults);
	evalValue += Pawn::evalPassedPawnThreats(position, evalResults);
	evalValue += King::eval(position, evalResults);

	if (evalResults.midgameInPercent > 0) {
		evalValue += KingAttack::eval(position, evalResults);
	}

	result += evalValue.getValue(evalResults.midgameInPercentV2);

	if constexpr (PRINT) {
		std::vector<PieceInfo> details = fetchDetails(position);
		printEvalBoard(details, evalResults.midgameInPercentV2);
		cout << "Midgame factor:" 
			<< std::right << std::setw(21) << evalResults.midgameInPercentV2 << endl;
		cout << "Piece based eval:" 
			<< std::right << std::setw(19) << result << std::endl;
	}

	const value_t halfmovesWithoutPawnOrCapture = position.getTotalHalfmovesWithoutPawnMoveOrCapture();
	if (halfmovesWithoutPawnOrCapture > 20) {
		result -= result * (halfmovesWithoutPawnOrCapture - 20) / 250;
		if constexpr (PRINT) {
			cout << "No pawn move or capture (" << halfmovesWithoutPawnOrCapture << "):" 
				<< std::right << std::setw(20) << result << std::endl;
		}
	}
	/*
	if (position.getEvalVersion() == 1) {
		result += EVAL_CORRECTION[position.getPiecesSignature()] / 2;
	}
	*/
	if constexpr (PRINT) {
		cout << "Piece Signature Correction:"
			<< std::right << std::setw(9) << result << std::endl;
	}

	value_t endgameCorrection = EvalEndgame::eval(position, result);
	if (endgameCorrection != result) {
		result = endgameCorrection;
		if (result > MIN_MATE_VALUE) {
			result -= ply;
		}
		if (result < -MIN_MATE_VALUE) {
			result += ply;
		}
		if constexpr (PRINT) {
			cout << "Endgame correction:" 
				<< std::right << std::setw(17) << result << std::endl;
		}

	}
	else {
		const value_t randomBonus = position.getRandomBonus();
		if (randomBonus != 0) {
			result += randomBonus;
			result += rand() % (2 * randomBonus + 1) - randomBonus;
			if constexpr (PRINT) {
				cout << "Random bonus:"
					<< std::right << std::setw(20) << result << std::endl;
			}
		}
		
		result += position.isWhiteToMove() ? tempo : -tempo;
		if constexpr (PRINT) {
			cout << "Tempo correction:"
				<< std::right << std::setw(19) << result << std::endl;
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
	
	int padding = (width - static_cast<int>(content.length())) / 2;
	std::cout << std::setw(padding + content.length()) << content 
		<< std::setw(static_cast<std::streamsize>(width) + 1 - padding - static_cast<int>(content.length())) << "|";
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
void Eval::printEval(MoveGenerator& position) {
	value_t evalValue = lazyEval<true>(position, 0);
	cout << "Total:" << std::right << std::setw(30) << evalValue << endl;
}

template value_t Eval::lazyEval<true>(MoveGenerator& position, value_t ply, PawnTT* pawnttPtr);
template value_t Eval::lazyEval<false>(MoveGenerator& position, value_t ply, PawnTT* pawnttPtr);

