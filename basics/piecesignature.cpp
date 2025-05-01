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
 * @author Volker B�hm
 * @copyright Copyright (c) 2021 Volker B�hm
 * @Overview
 * Defines a bitmap representing the available pieces at the board
 */

#include <array>
#include <vector>
#include <functional>
#include <tuple>
#include "piecesignature.h"


namespace QaplaBasics {

	std::tuple<pieceSignature_t, pieceSignature_t> PieceSignature::charToSignature(char piece) const {
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

	/**
	 * Parses a sequence of a given piece character in a pattern string.
	 *
	 * @param pieceChar The piece character to match ('Q', 'R', 'B', 'N', 'P').
	 * @param part The pattern substring for the current color.
	 * @param pos Current parsing position, updated internally.
	 * @return A tuple (minCount, allowMore, valid).
	 */
	static std::tuple<int, bool, bool> parsePieceInPattern(char pieceChar, std::string_view part) {
		int minCount = 0;
		bool allowMore = false;
		bool valid = true;
		size_t pos = 0;

		for (; pos < part.size() && part[pos] != pieceChar; ++pos);
		for (; pos < part.size() && part[pos] == pieceChar; ++pos, ++minCount);

		if (pos < part.size()) {
			allowMore = (part[pos] == '+' || part[pos] == '*');
			valid = !((minCount == 0) && allowMore);
			if (part[pos] == '*') {
				valid = minCount == 1;
				minCount = 0;
			}
		}

		return { minCount, allowMore, valid };
	}

	bool PieceSignature::matchesPattern(const std::string & pattern) const {
		std::array<std::string, 2> parts{ pattern, "" };
		for (uint32_t pos = 1; pos < pattern.size(); ++pos) {
			if (pattern[pos] == 'K') {
				parts[0] = pattern.substr(1, pos);
				parts[1] = pattern.substr(pos + 1);
				break;
			}
		}

		for (int color = 0; color < 2; ++color) {
			const std::string& part = parts[color];
			for (char pieceChar : {'Q', 'R', 'B', 'N', 'P'}) {
				auto [minCount, allowMore, valid] = parsePieceInPattern(pieceChar, part);
				if (!valid)
					return false;

				auto [value, mask] = charToSignature(pieceChar);
				if (color == 1) {
					value <<= SIG_SHIFT_BLACK;
					mask <<= SIG_SHIFT_BLACK;
				}

				uint32_t count = (_signature & mask) / value;

				if (count < static_cast<uint32_t>(minCount))
					return false;
				if (count > static_cast<uint32_t>(minCount) && !allowMore)
					return false;
			}
		}

		return true;
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
				pieceMask <<= shift;
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




