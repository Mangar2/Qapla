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

#include "eval.h"
#include "evalendgame.h"
#include "pawn.h"
#include "queen.h"
#include "rook.h"
#include "bishop.h"
#include "knight.h"
#include "kingattack.h"
#include "threat.h"
#include "print-eval.h"

using namespace ChessEval;

 /**
  * Calculates an evaluation for the current board position
 */
template <bool PRINT>
value_t Eval::lazyEval(MoveGenerator& board, EvalResults& evalResults, value_t ply) {

	value_t result = 0;
	value_t endGameResult;
	initEvalResults(board, evalResults);

	evalResults.midgameInPercent = computeMidgameInPercent(board);
	evalResults.midgameInPercentV2 = computeMidgameV2InPercent(board);
	if (PRINT) cout << "Midgame factor:" << std::right << std::setw(20) << evalResults.midgameInPercentV2 << endl;
	
	// Add material to the evaluation
	value_t material = board.getMaterialAndPSTValue().getValue(evalResults.midgameInPercentV2);
	result += material;
	if (PRINT) printEvalStep("Material", board.getMaterialValue().getValue(evalResults.midgameInPercentV2), 
		board.getMaterialValue(), evalResults.midgameInPercentV2);

	if (PRINT) board.printPst();
	if (PRINT) printEvalStep("PST", result, board.getPstBonus(), evalResults.midgameInPercentV2);

	// Add paw value to the evaluation
	const auto pawnEval = Pawn::eval<PRINT>(board, evalResults);
	if (PRINT) printValue("Pawns", result, pawnEval);
	result += pawnEval;
	
	endGameResult = EvalEndgame::eval(board, result);

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
		// Do not change ordering of the following calls. King attack needs result from Mobility
		EvalValue evalValue = Rook::eval<PRINT>(board, evalResults);
		evalValue += Bishop::eval<PRINT>(board, evalResults);
		evalValue += Knight::eval<PRINT>(board, evalResults);
		evalValue += Queen::eval<PRINT>(board, evalResults);
		evalValue += Threat::eval<PRINT>(board, evalResults);
		evalValue += Pawn::evalPassedPawnThreats<PRINT>(board, evalResults);
		result += evalValue.getValue(evalResults.midgameInPercentV2);
		if (PRINT) printEvalStep("Pieces", result, evalValue, evalResults.midgameInPercentV2);

		if (evalResults.midgameInPercent > 0) {
			result += KingAttack::eval(board, evalResults);
		}
	}
	return result;
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
 * Gets a map of relevant factors to examine eval
 */
template <Piece COLOR>
map<string, value_t> Eval::getEvalFactors(MoveGenerator& board) {
	EvalResults evalResults;
	lazyEval<false>(board, evalResults, 0);
	map<string, value_t> result;
	return result;
}

map<string, value_t> Eval::getEvalFactors(MoveGenerator& board) {
	map<string, value_t> result;
	auto factors = getEvalFactors<WHITE>(board);
	result.insert(factors.begin(), factors.end());
	factors = getEvalFactors<BLACK>(board);
	result.insert(factors.begin(), factors.end());
	return result;
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

template map<string, value_t> Eval::getEvalFactors<WHITE>(MoveGenerator& board);
template map<string, value_t> Eval::getEvalFactors<BLACK>(MoveGenerator& board);



