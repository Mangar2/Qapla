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
#include "evalpawn.h"
#include "evalmobility.h"
#include "rook.h"
#include "kingattack.h"

using namespace ChessEval;

 /**
  * Calculates an evaluation for the current board position
 */
template <bool PRINT>
value_t Eval::lazyEval(MoveGenerator& board, EvalResults& evalResults) {

	value_t result = 0;
	value_t endGameResult;

	evalResults.midgameInPercent = computeMidgameInPercent(board);
	evalResults.midgameInPercentV2 = computeMidgameV2InPercent(board);
	if (PRINT) printf("Midgame factor      : %ld%%\n", evalResults.midgameInPercentV2);
	
	// Add material to the evaluation
	value_t material = board.getMaterialValue().getValue(evalResults.midgameInPercentV2);
	result += material;
	if (PRINT) printf("Marerial            : %ld\n", material);

	// Add paw value to the evaluation
	result += EvalPawn::eval(board, evalResults);
	endGameResult = EvalEndgame::eval(board, result);

	if (endGameResult != result) {
		result = endGameResult;
	}
	else {
		// Do not change ordering of the following calls. King attack needs result from Mobility
		// EvalValue evalValue = Rook::eval<false>(board, evalResults);
		// value_t rookValue = evalValue.getValue(evalResults.midgameInPercentV2);
		// result += rookValue;

		result += EvalMobility::eval(board, evalResults);
		if (evalResults.midgameInPercent > 0) {
			result += KingAttack::eval(board, evalResults);
		}
	}
	return result;
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
	auto mobilityFactors = EvalMobility::factors<COLOR>(board, evalResults);
	result.insert(mobilityFactors.begin(), mobilityFactors.end());
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
	EvalPawn::print(board, evalResults);

	const value_t endGameResult = EvalEndgame::print(board, evalValue);

	if (endGameResult != evalValue) {
		evalValue = endGameResult;
	}
	else {
		EvalMobility::print(board, evalResults);
		if (evalResults.midgameInPercent > 0) {
			KingAttack::print(board, evalResults);
		}
	}
	printf("Total               : %ld\n", evalValue);
}




