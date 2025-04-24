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
 * @Overview
 * Defines a bitmap representing the available pieces at the board
 */

#pragma once

#include <array>
#include <vector>
#include <functional>
#include <tuple>
#include "piecesignature.h"


namespace QaplaBasics {

	std::tuple<pieceSignature_t, pieceSignature_t> PieceSignature::charToSignature(char piece) {
		Signature result = Signature::EMPTY;
		switch (piece) {
		case 0: return { 0, 0 };
		case 'Q': return{ static_cast<pieceSignature_t>(Signature::QUEEN),  static_cast<pieceSignature_t>(SignatureMask::QUEEN) };
		case 'R': return{ static_cast<pieceSignature_t>(Signature::ROOK),  static_cast<pieceSignature_t>(SignatureMask::ROOK) };
		case 'B': return{ static_cast<pieceSignature_t>(Signature::BISHOP),  static_cast<pieceSignature_t>(SignatureMask::BISHOP) };
		case 'N': return{ static_cast<pieceSignature_t>(Signature::KNIGHT),  static_cast<pieceSignature_t>(SignatureMask::KNIGHT) };
		case 'P': return{ static_cast<pieceSignature_t>(Signature::PAWN),  static_cast<pieceSignature_t>(SignatureMask::PAWN) };
		default:
			std::cerr << "Unknown piece: " << piece << std::endl;
			return{ 0, 0 };
		}
	}

	void PieceSignature::generateSignatures(const std::string& pattern, std::vector<pieceSignature_t>& out) {
		using namespace QaplaBasics;

		std::function<void(size_t, pieceSignature_t, char, bool)> recurse;

		recurse = [&](size_t index, pieceSignature_t curSig, char pieceChar, bool isWhite) -> void {

			auto [pieceSignature, pieceMask] = charToSignature(pieceChar);
			if (!isWhite) {
				pieceSignature <<= SIG_SHIFT_BLACK;
				pieceMask <<= SIG_SHIFT_BLACK;
			}
			int32_t remainingPieces = 0;
			if (pieceChar) {
				int32_t maxPieces = std::min(pieceMask / pieceSignature, static_cast<uint32_t>(8));
				remainingPieces = maxPieces - ((curSig & pieceMask) / pieceSignature);
			}

			if (index >= pattern.size()) {
				if (remainingPieces) curSig += pieceSignature;
				out.push_back(curSig);
				return;
			}

			char patternChar = pattern[index];
			switch (patternChar) {
				case 'K': {
					if (remainingPieces) curSig += pieceSignature;
					// 'K' marks the start of black's pieces if not at position 0
					recurse(index + 1, curSig, 0, index == 0);
					break;
				}
				case '*': {
					for (; remainingPieces >= 0; --remainingPieces, curSig += pieceSignature) {
						recurse(index + 1, curSig, 0, isWhite);
					}
					break;
				}
				case '+': {
					for (; remainingPieces > 0; --remainingPieces) {
						curSig += pieceSignature;
						recurse(index + 1, curSig, 0, isWhite);
					}
					break;
				}
				default: {
					if (remainingPieces) curSig += pieceSignature;
					recurse(index + 1, curSig, patternChar, isWhite);
					break;
				}
			}
			
		};


		recurse(0, 0, 0, true);
	}

	void PieceSignature::set(string pieces) {
		_signature = 0;
		pieceSignature_t shift = 0;

		for (int pos = 0; pos < pieces.length(); pos++) {
			auto pieceChar = pieces[pos];
			if (pieceChar == 'K') {
				if (pos > 0) {
					shift = SIG_SHIFT_BLACK;
				}
				continue;
			}
			auto [pieceSignature, pieceMask] = charToSignature(pieceChar);
			if (pieceSignature != 0) {
				pieceSignature <<= shift;
				int32_t remainingPieces = pieceChar ? (pieceMask - (_signature & pieceMask)) / pieceSignature : 0;
				if (remainingPieces > 0) {
					_signature += pieceSignature;
				}
				else {
					std::cerr << " too many pieces of type " << pieceChar << " in signature: " << pieces << std::endl;
				}
			}
		}
	}

};




