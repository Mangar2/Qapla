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

#include "piecesignature.h"

using namespace QaplaBasics;

array<pieceSignature_t, PIECE_AMOUNT> PieceSignature::mapPieceToSignature;
array<pieceSignature_t, size_t(SignatureMask::ALL)> PieceSignature::futilityOnCaptureMap;
array<value_t, size_t(SignatureMask::ALL)> PieceSignature::staticPiecesValue;
PieceSignature::InitStatics PieceSignature::_staticConstructur;

PieceSignature::InitStatics::InitStatics() {
	for (uint32_t index = 0; index < uint32_t(SignatureMask::ALL); index++) {
		futilityOnCaptureMap[index] = true;
		if (getPieceAmount(index) <= 2) {
			futilityOnCaptureMap[index] = false;
		}
	}
	for (uint32_t index = 0; index < uint32_t(SignatureMask::ALL); index++) {
		staticPiecesValue[index] =
			getPieceAmount<QUEEN>(index) * 9 +
			getPieceAmount<ROOK>(index)  * 5 +
			getPieceAmount<BISHOP>(index) * 3 +
			getPieceAmount<KNIGHT>(index) * 3 + 
			getPieceAmount<PAWN>(index) / 3;
	}

	mapPieceToSignature.fill(pieceSignature_t(Signature::EMPTY));
	mapPieceToSignature[WHITE_PAWN] = pieceSignature_t(Signature::PAWN);
	mapPieceToSignature[WHITE_KNIGHT] = pieceSignature_t(Signature::KNIGHT);
	mapPieceToSignature[WHITE_BISHOP] = pieceSignature_t(Signature::BISHOP);
	mapPieceToSignature[WHITE_ROOK] = pieceSignature_t(Signature::ROOK);
	mapPieceToSignature[WHITE_QUEEN] = pieceSignature_t(Signature::QUEEN);
	mapPieceToSignature[BLACK_PAWN] = pieceSignature_t(Signature::PAWN) << SIG_SHIFT_BLACK;
	mapPieceToSignature[BLACK_KNIGHT] = pieceSignature_t(Signature::KNIGHT) << SIG_SHIFT_BLACK;
	mapPieceToSignature[BLACK_BISHOP] = pieceSignature_t(Signature::BISHOP) << SIG_SHIFT_BLACK;
	mapPieceToSignature[BLACK_ROOK] = pieceSignature_t(Signature::ROOK) << SIG_SHIFT_BLACK;
	mapPieceToSignature[BLACK_QUEEN] = pieceSignature_t(Signature::QUEEN) << SIG_SHIFT_BLACK;
}

