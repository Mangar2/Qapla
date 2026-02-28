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
 * @Overview
 * Incremental Stockfish-like imbalance term (without bishop pair)
 */

#pragma once

#include <array>

#include "../basics/types.h"
#include "../basics/piecesignature.h"
#include "../interface/uci-parameter-provider.h"

#ifdef PARAM_OPTIMIZE
#define PARAM_OPTIMIZE_IMBALANCE
#endif

using namespace QaplaBasics;

namespace ChessEval {

	class Imbalance {
	public:
		friend class ImbalanceUciAccess;

		static UciParameterProvider& getUciAccess();

		Imbalance() {
			clear();
		}

		inline void clear() {
			_imbalance = 0;
			_whiteSignature = 0;
			_blackSignature = 0;
		}

		inline void addPiece(Piece piece, const PieceSignature& pieceSignature) {
			updateByDelta<true>(piece, pieceSignature);
		}

		inline void removePiece(Piece piece, const PieceSignature& pieceSignature) {
			updateByDelta<false>(piece, pieceSignature);
		}

		inline value_t getValue() const {
			return _imbalance / 16;
		}

	private:
		static constexpr uint32_t PIECE_KIND_COUNT = 5;

		using SignatureLookup = std::array<value_t, PieceSignature::SIG_SIZE_PER_SIDE>;
		using LookupPerSource = std::array<SignatureLookup, PIECE_KIND_COUNT>;
		using LookupMatrix = std::array<LookupPerSource, PIECE_KIND_COUNT>;

		using CoeffRow = std::array<value_t, PIECE_KIND_COUNT>;
		using CoeffMatrix = std::array<CoeffRow, PIECE_KIND_COUNT>;
		using DeltaLookup = std::array<SignatureLookup, PIECE_KIND_COUNT>;

		template<bool ADD>
		inline void updateByDelta(Piece piece, const PieceSignature& pieceSignature) {
			const Piece pieceType = getPieceType(piece);
			if (pieceType == KING || pieceType == NO_PIECE) {
				_whiteSignature = pieceSignature.getSignature<WHITE>();
				_blackSignature = pieceSignature.getSignature<BLACK>();
				return;
			}

			const uint32_t pieceIndex = pieceToIndex(pieceType);
			const pieceSignature_t whiteSignature = pieceSignature.getSignature<WHITE>();
			const pieceSignature_t blackSignature = pieceSignature.getSignature<BLACK>();

			if (whiteSignature == _whiteSignature && blackSignature == _blackSignature) {
				return;
			}

			const Piece color = getPieceColor(piece);
			const pieceSignature_t ownSignature = color == WHITE
				? (ADD ? _whiteSignature : whiteSignature)
				: (ADD ? _blackSignature : blackSignature);
			const pieceSignature_t oppSignature = color == WHITE
				? (ADD ? _blackSignature : blackSignature)
				: (ADD ? _whiteSignature : whiteSignature);

			value_t delta = OWN_ADD_LOOKUP[pieceIndex][ownSignature]
				+ OPP_ADD_LOOKUP[pieceIndex][oppSignature];

			if constexpr (!ADD) {
				delta = -delta;
			}
			if (color == BLACK) {
				delta = -delta;
			}

			_imbalance += delta;

			_whiteSignature = whiteSignature;
			_blackSignature = blackSignature;
		}

		static value_t getPieceCount(pieceSignature_t signature, uint32_t kind);
		static uint32_t pieceToIndex(Piece pieceType);
		static DeltaLookup generateOwnAddLookup(const CoeffMatrix& coeff);
		static DeltaLookup generateOppAddLookup(const CoeffMatrix& coeff);

	#ifdef PARAM_OPTIMIZE_IMBALANCE
		static void rebuildLookups();
	#endif

		static constexpr CoeffMatrix OWN_COEFF_DEFAULT = {{
			{{ 1, 0, 0, 0, 0 }},
			{{ 1, 1, 0, 0, 0 }},
			{{ 1, 1, 1, 0, 0 }},
			{{ 1, 1, 1, 1, 0 }},
			{{ 1, 1, 1, 1, 1 }}
		}};

		// Opponent diagonal entries (PP, NN, BB, RR, QQ) are set to 0 intentionally.
		// In the white-minus-black formulation their contributions cancel out,
		// so exposing/tuning them has no effect.
		static constexpr CoeffMatrix OPP_COEFF_DEFAULT = {{
			{{ 0, 0, 0, 0, 0 }},
			{{ 1, 0, 0, 0, 0 }},
			{{ 1, 1, 0, 0, 0 }},
			{{ 1, 1, 1, 0, 0 }},
			{{ 1, 1, 1, 1, 0 }}
		}};

	#ifdef PARAM_OPTIMIZE_IMBALANCE
		inline static CoeffMatrix OWN_COEFF = OWN_COEFF_DEFAULT;
		inline static CoeffMatrix OPP_COEFF = OPP_COEFF_DEFAULT;
		inline static DeltaLookup OWN_ADD_LOOKUP = generateOwnAddLookup(OWN_COEFF);
		inline static DeltaLookup OPP_ADD_LOOKUP = generateOppAddLookup(OPP_COEFF);
	#else
		inline static const CoeffMatrix OWN_COEFF = OWN_COEFF_DEFAULT;
		inline static const CoeffMatrix OPP_COEFF = OPP_COEFF_DEFAULT;
		inline static const DeltaLookup OWN_ADD_LOOKUP = generateOwnAddLookup(OWN_COEFF_DEFAULT);
		inline static const DeltaLookup OPP_ADD_LOOKUP = generateOppAddLookup(OPP_COEFF_DEFAULT);
	#endif

		value_t _imbalance;
		pieceSignature_t _whiteSignature;
		pieceSignature_t _blackSignature;
	};
}
