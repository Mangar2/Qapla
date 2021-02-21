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

using namespace ChessEval;

 /**
  * Calculates an evaluation for the current board position
 */
template <bool PRINT>
value_t Eval::lazyEval(MoveGenerator& board, EvalResults& evalResults) {

	value_t result = 0;
	value_t endGameResult;
	initEvalResults(board, evalResults);

	evalResults.midgameInPercent = computeMidgameInPercent(board);
	evalResults.midgameInPercentV2 = computeMidgameV2InPercent(board);
	if (PRINT) printf("Midgame factor      : %ld%%\n", evalResults.midgameInPercentV2);
	
	// Add material to the evaluation
	value_t material = board.getMaterialValue().getValue(evalResults.midgameInPercentV2);
	result += material;
	if (PRINT) printf("Marerial            : %ld\n", material);

	// Add paw value to the evaluation
	result += Pawn::eval<PRINT>(board, evalResults);
	endGameResult = EvalEndgame::eval(board, result);

	if (endGameResult != result) {
		result = endGameResult;
	}
	else {
		// Do not change ordering of the following calls. King attack needs result from Mobility
		EvalValue evalValue = Rook::eval<PRINT>(board, evalResults);
		evalValue += Bishop::eval<PRINT>(board, evalResults);
		evalValue += Knight::eval<PRINT>(board, evalResults);
		evalValue += Queen::eval<PRINT>(board, evalResults);
		evalValue += Threat::eval<PRINT>(board, evalResults);
		result += evalValue.getValue(evalResults.midgameInPercentV2);

		if (evalResults.midgameInPercent > 0) {
			result += KingAttack::eval(board, evalResults);
		}
	}
	return result;
}

void Eval::initEvalResults(MoveGenerator& position, EvalResults& evalResults) {
	evalResults.queensBB = position.getPieceBB(WHITE_QUEEN) | position.getPieceBB(BLACK_QUEEN);
	evalResults.pawnsBB = position.getPieceBB(WHITE_PAWN) | position.getPieceBB(BLACK_PAWN);
	position.computePinnedMask<WHITE>();
	position.computePinnedMask<BLACK>();
}

/**
 * Gets a map of relevant factors to examine eval
 */
template <Piece COLOR>
map<string, value_t> Eval::getEvalFactors(MoveGenerator& board) {
	EvalResults evalResults;
	lazyEval<false>(board, evalResults);
	map<string, value_t> result;
	auto factors = KingAttack::factors<COLOR>(board, evalResults);
	result.insert(factors.begin(), factors.end());
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

	value_t evalValue = lazyEval<true>(board, evalResults);

	const value_t endGameResult = EvalEndgame::print(board, evalValue);

	if (endGameResult != evalValue) {
		evalValue = endGameResult;
	}
	else {
		if (evalResults.midgameInPercent > 0) {
			KingAttack::print(board, evalResults);
		}
	}
	printf("Total               : %ld\n", evalValue);
}




