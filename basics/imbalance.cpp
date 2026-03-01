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

#include "imbalance.h"

namespace ChessEval {

value_t Imbalance::computeValueFromBoard(const std::array<Piece, BOARD_SIZE>& board) {
	PieceSignature pieceSignature;
	value_t imbalance = 0;

	for (Square square = A1; square <= H8; ++square) {
		const Piece piece = board[square];
		const Piece pieceType = getPieceType(piece);

		if (pieceType == KING || pieceType == NO_PIECE) {
			continue;
		}

		const uint32_t pieceIndex = pieceToIndex(pieceType);
		const pieceSignature_t whiteSignature = pieceSignature.getSignature<WHITE>();
		const pieceSignature_t blackSignature = pieceSignature.getSignature<BLACK>();
		const Piece color = getPieceColor(piece);
		const pieceSignature_t ownSignature = color == WHITE ? whiteSignature : blackSignature;
		const pieceSignature_t oppSignature = color == WHITE ? blackSignature : whiteSignature;

		value_t delta = OWN_ADD_LOOKUP[pieceIndex][ownSignature]
			+ OPP_ADD_LOOKUP[pieceIndex][oppSignature];
		if (color == BLACK) {
			delta = -delta;
		}

		imbalance += delta;
		pieceSignature.addPiece(piece);
	}

	return imbalance;
}

value_t Imbalance::getPieceCount(pieceSignature_t signature, uint32_t kind) {
	switch (kind) {
	case 0: return (signature & SignatureMask::PAWN) / Signature::PAWN;
	case 1: return (signature & SignatureMask::KNIGHT) / Signature::KNIGHT;
	case 2: return (signature & SignatureMask::BISHOP) / Signature::BISHOP;
	case 3: return (signature & SignatureMask::ROOK) / Signature::ROOK;
	case 4: return (signature & SignatureMask::QUEEN) / Signature::QUEEN;
	default: return 0;
	}
}

uint32_t Imbalance::pieceToIndex(Piece pieceType) {
	switch (pieceType) {
	case PAWN: return 0;
	case KNIGHT: return 1;
	case BISHOP: return 2;
	case ROOK: return 3;
	case QUEEN: return 4;
	default: return 0;
	}
}

Imbalance::DeltaLookup Imbalance::generateOwnAddLookup(const CoeffMatrix& coeff) {
	DeltaLookup result{};

	for (uint32_t piece = 0; piece < PIECE_KIND_COUNT; ++piece) {
		for (uint32_t signature = 0; signature < PieceSignature::SIG_SIZE_PER_SIDE; ++signature) {
			value_t delta = 0;

			for (uint32_t source = 0; source <= piece; ++source) {
				delta += coeff[piece][source] * getPieceCount(signature, source);
			}
			for (uint32_t target = piece; target < PIECE_KIND_COUNT; ++target) {
				delta += coeff[target][piece] * getPieceCount(signature, target);
			}
			delta += coeff[piece][piece];

			result[piece][signature] = delta;
		}
	}

	return result;
}

Imbalance::DeltaLookup Imbalance::generateOppAddLookup(const CoeffMatrix& coeff) {
	DeltaLookup result{};

	for (uint32_t piece = 0; piece < PIECE_KIND_COUNT; ++piece) {
		for (uint32_t signature = 0; signature < PieceSignature::SIG_SIZE_PER_SIDE; ++signature) {
			value_t delta = 0;

			for (uint32_t source = 0; source <= piece; ++source) {
				delta += coeff[piece][source] * getPieceCount(signature, source);
			}
			for (uint32_t target = piece; target < PIECE_KIND_COUNT; ++target) {
				delta -= coeff[target][piece] * getPieceCount(signature, target);
			}

			result[piece][signature] = delta;
		}
	}

	return result;
}

#ifdef PARAM_OPTIMIZE_IMBALANCE

void Imbalance::rebuildLookups() {
	OWN_ADD_LOOKUP = generateOwnAddLookup(OWN_COEFF);
	OPP_ADD_LOOKUP = generateOppAddLookup(OPP_COEFF);
}

class ImbalanceUciAccess : public UciParameterProvider {
public:
	std::vector<UciParam> getUciParameters() const override {
		std::vector<UciParam> params;
		params.reserve(20);

		for (uint32_t target = 0; target < Imbalance::PIECE_KIND_COUNT; ++target) {
			for (uint32_t source = 0; source <= target; ++source) {
				params.push_back({
					.name = makeName("imbalanceOwn", target, source),
					.defaultValue = Imbalance::OWN_COEFF[target][source],
					.minValue = -1000,
					.maxValue = 1000
					});
				if (target != source) {
					params.push_back({
						.name = makeName("imbalanceOpp", target, source),
						.defaultValue = Imbalance::OPP_COEFF[target][source],
						.minValue = -1000,
						.maxValue = 1000
						});
				}
			}
		}

		return params;
	}

	bool setUciParameter(const std::string& name, int32_t value) override {
		for (uint32_t target = 0; target < Imbalance::PIECE_KIND_COUNT; ++target) {
			for (uint32_t source = 0; source <= target; ++source) {
				if (name == makeName("imbalanceOwn", target, source)) {
					Imbalance::OWN_COEFF[target][source] = value;
					Imbalance::rebuildLookups();
					return true;
				}
				if (target != source) {
					if (name == makeName("imbalanceOpp", target, source)) {
						Imbalance::OPP_COEFF[target][source] = value;
						Imbalance::rebuildLookups();
						return true;
					}
				}
			}
		}

		return false;
	}

private:
	static std::string makeName(const std::string& prefix, uint32_t target, uint32_t source) {
		return prefix + pieceName(target) + pieceName(source);
	}

	static std::string pieceName(uint32_t pieceIndex) {
		switch (pieceIndex) {
		case 0: return "P";
		case 1: return "N";
		case 2: return "B";
		case 3: return "R";
		case 4: return "Q";
		default: return "X";
		}
	}
};

#endif

UciParameterProvider& Imbalance::getUciAccess() {
#ifdef PARAM_OPTIMIZE_IMBALANCE
	static ImbalanceUciAccess instance;
#else
	static EmptyParameterProvider instance;
#endif
	return instance;
}

}
