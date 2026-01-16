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

#include "pst.h"

using namespace QaplaBasics;
using ChessEval::UciParameterProvider;
using ChessEval::UciParam;

PST::PSTArray PST::_pst;
PST::InitStatics PST::_staticConstructor;

PST::InitStatics::InitStatics() noexcept {
	generatePST(_pst, DEFAULT_PST_PAWN_MG, DEFAULT_PST_PAWN_EG,
				DEFAULT_PST_KNIGHT_MG, DEFAULT_PST_KNIGHT_EG,
				DEFAULT_PST_BISHOP_MG, DEFAULT_PST_BISHOP_EG,
				DEFAULT_PST_ROOK_MG, DEFAULT_PST_ROOK_EG,
				DEFAULT_PST_QUEEN_MG, DEFAULT_PST_QUEEN_EG,
				DEFAULT_PST_KING_MG, DEFAULT_PST_KING_EG);
}

static EvalValue scaleEvalValue(EvalValue base, int32_t scaleMg, int32_t scaleEg) noexcept {
	// Normalize to 500; use std::round for symmetric rounding (handles negative values correctly)
	const double factorMg = double(scaleMg) / 500.0;
	const double factorEg = double(scaleEg) / 500.0;
	const value_t mg = static_cast<value_t>(std::round(base.midgame() * factorMg));
	const value_t eg = static_cast<value_t>(std::round(base.endgame() * factorEg));
	return EvalValue(mg, eg);
}

void PST::generatePST(
	PSTArray& result,
	int32_t pawnMg, int32_t pawnEg,
	int32_t knightMg, int32_t knightEg,
	int32_t bishopMg, int32_t bishopEg,
	int32_t rookMg, int32_t rookEg,
	int32_t queenMg, int32_t queenEg,
	int32_t kingMg, int32_t kingEg
) noexcept {
	for (auto piece : { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING }) {
		const int32_t scaleMgVal =
			(piece == PAWN)   ? pawnMg :
			(piece == KNIGHT) ? knightMg :
			(piece == BISHOP) ? bishopMg :
			(piece == ROOK)   ? rookMg :
			(piece == QUEEN)  ? queenMg :
								 kingMg;
		const int32_t scaleEgVal =
			(piece == PAWN)   ? pawnEg :
			(piece == KNIGHT) ? knightEg :
			(piece == BISHOP) ? bishopEg :
			(piece == ROOK)   ? rookEg :
			(piece == QUEEN)  ? queenEg :
								 kingEg;

		for (Square square = A1; square <= H8; ++square) {
			EvalValue base;
			const uint32_t rank = uint32_t(getRank(square));
			const uint32_t file = uint32_t(getFile(square));
			const uint32_t halfFile = file > uint32_t(File::D) ? uint32_t(File::H) - file : file;

			switch (piece) {
			case PAWN:   base = PAWN_PST[rank][file]; break;
			case KNIGHT: base = KNIGHT_PST[rank][halfFile]; break;
			case BISHOP: base = BISHOP_PST[rank][halfFile]; break;
			case ROOK:   base = ROOK_PST[rank][halfFile]; break;
			case QUEEN:  base = QUEEN_PST[rank][halfFile]; break;
			case KING:   base = KING_PST[rank][halfFile]; break;
			default:     base = 0; break;
			}

			const EvalValue value = scaleEvalValue(base, scaleMgVal, scaleEgVal);

			result[WHITE + piece][square] = value;
			result[BLACK + piece][switchSide(square)] = -value;
		}
	}
}

// Static instance of UCI access
static PST::UciAccess _uciAccessInstance;

UciParameterProvider& PST::getUciAccess() noexcept {
	return _uciAccessInstance;
}

std::vector<UciParam> PST::UciAccess::getUciParameters() const {
	return {
		{"pstPawnMgScale",   DEFAULT_PST_PAWN_MG,   0, 2000},
		{"pstPawnEgScale",   DEFAULT_PST_PAWN_EG,   0, 2000},
		{"pstKnightMgScale", DEFAULT_PST_KNIGHT_MG, 0, 2000},
		{"pstKnightEgScale", DEFAULT_PST_KNIGHT_EG, 0, 2000},
		{"pstBishopMgScale", DEFAULT_PST_BISHOP_MG, 0, 2000},
		{"pstBishopEgScale", DEFAULT_PST_BISHOP_EG, 0, 2000},
		{"pstRookMgScale",   DEFAULT_PST_ROOK_MG,   0, 2000},
		{"pstRookEgScale",   DEFAULT_PST_ROOK_EG,   0, 2000},
		{"pstQueenMgScale",  DEFAULT_PST_QUEEN_MG,  0, 2000},
		{"pstQueenEgScale",  DEFAULT_PST_QUEEN_EG,  0, 2000},
		{"pstKingMgScale",   DEFAULT_PST_KING_MG,   0, 2000},
		{"pstKingEgScale",   DEFAULT_PST_KING_EG,   0, 2000}
	};
}

bool PST::UciAccess::setUciParameter(const std::string& name, int32_t value) {
	if (name == "pstPawnMgScale") {
		_pawnMg = value;
	} else if (name == "pstPawnEgScale") {
		_pawnEg = value;
	} else if (name == "pstKnightMgScale") {
		_knightMg = value;
	} else if (name == "pstKnightEgScale") {
		_knightEg = value;
	} else if (name == "pstBishopMgScale") {
		_bishopMg = value;
	} else if (name == "pstBishopEgScale") {
		_bishopEg = value;
	} else if (name == "pstRookMgScale") {
		_rookMg = value;
	} else if (name == "pstRookEgScale") {
		_rookEg = value;
	} else if (name == "pstQueenMgScale") {
		_queenMg = value;
	} else if (name == "pstQueenEgScale") {
		_queenEg = value;
	} else if (name == "pstKingMgScale") {
		_kingMg = value;
	} else if (name == "pstKingEgScale") {
		_kingEg = value;
	} else {
		return false;
	}

	// Rebuild PST with updated scales
	generatePST(_pst, _pawnMg, _pawnEg, _knightMg, _knightEg, _bishopMg, _bishopEg,
				_rookMg, _rookEg, _queenMg, _queenEg, _kingMg, _kingEg);
	return true;
}


