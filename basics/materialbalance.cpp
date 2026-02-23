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
 */

#include "materialbalance.h"

#ifdef PARAM_OPTIMIZE_MATERIALBALANCE
#include <cstdint>
using ChessEval::UciParam;
#endif

using ChessEval::UciParameterProvider;

namespace QaplaBasics {

#ifdef PARAM_OPTIMIZE_MATERIALBALANCE

class MaterialBalanceUciAccess : public UciParameterProvider {
public:
	std::vector<UciParam> getUciParameters() const override {
		return {
			{ .name = "PAWN_VALUE_MG", .defaultValue = MaterialBalance::PAWN_VALUE_MG, .minValue = 1, .maxValue = 4000 },
			{ .name = "PAWN_VALUE_EG", .defaultValue = MaterialBalance::PAWN_VALUE_EG, .minValue = 1, .maxValue = 4000 },
			{ .name = "KNIGHT_VALUE_MG", .defaultValue = MaterialBalance::KNIGHT_VALUE_MG, .minValue = 1, .maxValue = 4000 },
			{ .name = "KNIGHT_VALUE_EG", .defaultValue = MaterialBalance::KNIGHT_VALUE_EG, .minValue = 1, .maxValue = 4000 },
			{ .name = "BISHOP_VALUE_MG", .defaultValue = MaterialBalance::BISHOP_VALUE_MG, .minValue = 1, .maxValue = 4000 },
			{ .name = "BISHOP_VALUE_EG", .defaultValue = MaterialBalance::BISHOP_VALUE_EG, .minValue = 1, .maxValue = 4000 },
			{ .name = "ROOK_VALUE_MG", .defaultValue = MaterialBalance::ROOK_VALUE_MG, .minValue = 1, .maxValue = 4000 },
			{ .name = "ROOK_VALUE_EG", .defaultValue = MaterialBalance::ROOK_VALUE_EG, .minValue = 1, .maxValue = 4000 },
			{ .name = "QUEEN_VALUE_MG", .defaultValue = MaterialBalance::QUEEN_VALUE_MG, .minValue = 1, .maxValue = 4000 },
			{ .name = "QUEEN_VALUE_EG", .defaultValue = MaterialBalance::QUEEN_VALUE_EG, .minValue = 1, .maxValue = 4000 }
		};
	}

	bool setUciParameter(const std::string& name, int32_t value) override {
		if (name == "PAWN_VALUE_MG") {
			MaterialBalance::PAWN_VALUE_MG = value;
		} else if (name == "PAWN_VALUE_EG") {
			MaterialBalance::PAWN_VALUE_EG = value;
		} else if (name == "KNIGHT_VALUE_MG") {
			MaterialBalance::KNIGHT_VALUE_MG = value;
		} else if (name == "KNIGHT_VALUE_EG") {
			MaterialBalance::KNIGHT_VALUE_EG = value;
		} else if (name == "BISHOP_VALUE_MG") {
			MaterialBalance::BISHOP_VALUE_MG = value;
		} else if (name == "BISHOP_VALUE_EG") {
			MaterialBalance::BISHOP_VALUE_EG = value;
		} else if (name == "ROOK_VALUE_MG") {
			MaterialBalance::ROOK_VALUE_MG = value;
		} else if (name == "ROOK_VALUE_EG") {
			MaterialBalance::ROOK_VALUE_EG = value;
		} else if (name == "QUEEN_VALUE_MG") {
			MaterialBalance::QUEEN_VALUE_MG = value;
		} else if (name == "QUEEN_VALUE_EG") {
			MaterialBalance::QUEEN_VALUE_EG = value;
		} else {
			return false;
		}

		MaterialBalance::rebuildPieceValues();
		return true;
	}
};

void MaterialBalance::rebuildPieceValues() {
	pieceValues.fill(0);
	pieceValues[WHITE_PAWN] = EvalValue(PAWN_VALUE_MG, PAWN_VALUE_EG);
	pieceValues[BLACK_PAWN] = EvalValue(-PAWN_VALUE_MG, -PAWN_VALUE_EG);
	pieceValues[WHITE_KNIGHT] = EvalValue(KNIGHT_VALUE_MG, KNIGHT_VALUE_EG);
	pieceValues[BLACK_KNIGHT] = EvalValue(-KNIGHT_VALUE_MG, -KNIGHT_VALUE_EG);
	pieceValues[WHITE_BISHOP] = EvalValue(BISHOP_VALUE_MG, BISHOP_VALUE_EG);
	pieceValues[BLACK_BISHOP] = EvalValue(-BISHOP_VALUE_MG, -BISHOP_VALUE_EG);
	pieceValues[WHITE_ROOK] = EvalValue(ROOK_VALUE_MG, ROOK_VALUE_EG);
	pieceValues[BLACK_ROOK] = EvalValue(-ROOK_VALUE_MG, -ROOK_VALUE_EG);
	pieceValues[WHITE_QUEEN] = EvalValue(QUEEN_VALUE_MG, QUEEN_VALUE_EG);
	pieceValues[BLACK_QUEEN] = EvalValue(-QUEEN_VALUE_MG, -QUEEN_VALUE_EG);
	pieceValues[WHITE_KING] = EvalValue(MAX_VALUE, MAX_VALUE);
	pieceValues[BLACK_KING] = EvalValue(-MAX_VALUE, -MAX_VALUE);
	for (Piece piece = NO_PIECE; piece <= BLACK_KING; ++piece) {
		absolutePieceValues[piece] = abs(pieceValues[piece].midgame());
	}
}

#endif

UciParameterProvider& MaterialBalance::getUciAccess() {
#ifdef PARAM_OPTIMIZE_MATERIALBALANCE
	static MaterialBalanceUciAccess instance;
#else
	static ChessEval::EmptyParameterProvider instance;
#endif
	return instance;
}

}
