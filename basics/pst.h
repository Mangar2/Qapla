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
 * Implements piece square table for static evaluation for the piece placement
 */

#pragma once

#include <vector>
#include "../basics/types.h"
#include "../basics/evalvalue.h"
#include "../interface/uci-parameter-provider.h"


namespace QaplaBasics {

	class PST
	{
	public:
		// Forward declaration of UCI parameter handler
		class UciAccess;

		/**
		 * Get UCI parameter access interface
		 * @return Reference to UCI parameter provider
		 */
		static ChessEval::UciParameterProvider& getUciAccess() noexcept;

#ifdef PARAM_OPTIMIZE
		// Default values for UCI parameters (normalized: 500 keeps values unchanged)
		// Midgame scales
		static constexpr int32_t DEFAULT_PST_PAWN_MG   = 500;
		static constexpr int32_t DEFAULT_PST_KNIGHT_MG = 500;
		static constexpr int32_t DEFAULT_PST_BISHOP_MG = 500;
		static constexpr int32_t DEFAULT_PST_ROOK_MG   = 500;
		static constexpr int32_t DEFAULT_PST_QUEEN_MG  = 500;
		static constexpr int32_t DEFAULT_PST_KING_MG   = 500;
		// Endgame scales
		static constexpr int32_t DEFAULT_PST_PAWN_EG   = 500;
		static constexpr int32_t DEFAULT_PST_KNIGHT_EG = 500;
		static constexpr int32_t DEFAULT_PST_BISHOP_EG = 500;
		static constexpr int32_t DEFAULT_PST_ROOK_EG   = 500;
		static constexpr int32_t DEFAULT_PST_QUEEN_EG  = 500;
		static constexpr int32_t DEFAULT_PST_KING_EG   = 500;

		// Current scales (UCI-configurable) - Midgame
		inline static int32_t _pawnMg   = DEFAULT_PST_PAWN_MG;
		inline static int32_t _knightMg = DEFAULT_PST_KNIGHT_MG;
		inline static int32_t _bishopMg = DEFAULT_PST_BISHOP_MG;
		inline static int32_t _rookMg   = DEFAULT_PST_ROOK_MG;
		inline static int32_t _queenMg  = DEFAULT_PST_QUEEN_MG;
		inline static int32_t _kingMg   = DEFAULT_PST_KING_MG;
		// Current scales (UCI-configurable) - Endgame
		inline static int32_t _pawnEg   = DEFAULT_PST_PAWN_EG;
		inline static int32_t _knightEg = DEFAULT_PST_KNIGHT_EG;
		inline static int32_t _bishopEg = DEFAULT_PST_BISHOP_EG;
		inline static int32_t _rookEg   = DEFAULT_PST_ROOK_EG;
		inline static int32_t _queenEg  = DEFAULT_PST_QUEEN_EG;
		inline static int32_t _kingEg   = DEFAULT_PST_KING_EG;
#endif

		/**
		 * Gets a value from the piece square tables
		 */
		[[nodiscard]] static EvalValue getValue(Square square, Piece piece) noexcept { return _pst[piece][square]; }
		
		[[nodiscard]] static std::vector<EvalValue> getPSTLookup(Piece piece) noexcept { return std::vector<EvalValue>(_pst[piece], _pst[piece] + BOARD_SIZE); }



	private:

		// Type definition for PST array
		using PSTArray = EvalValue[PIECE_AMOUNT][BOARD_SIZE];

		/**
		 * Generate PST array
		 */
		static void generatePST() noexcept;

		/**
		 * Initializes the piece square table
		 */
		static struct InitStatics {
			InitStatics() noexcept;
		} _staticConstructor;

		static PSTArray _pst;



		// 0 => 48,9%; EG700=>49,74%; PMG700=>49,2%
		constexpr static value_t PAWN_PST[][int(File::COUNT)][2] = {
			{ {   0,  0 }, {  0,  0 }, {  0,  0 }, {  0,  0 }, {  0,  0 }, {  0,  0 }, {   0,  0 }, {   0,  0 } },
			{ {   0,  0 }, {  0,  0 }, {  5,  0 }, {  5,  0 }, {  5,  0 }, {  5,  0 }, {   0,  0 }, {   0,  0 } },
			{ {  -5,  0 }, { -5,  0 }, {  5,  0 }, { 10,  0 }, { 10,  0 }, {  5,  0 }, {  -5,  0 }, {  -5,  0 } },
			{ { -10,  0 }, { -5,  0 }, { 10,  0 }, { 20,  0 }, { 20,  0 }, { 10,  0 }, { -10,  0 }, { -10,  0 } },
			{ { -10,  0 }, { -5,  0 }, {  0,  0 }, { 10,  0 }, { 10,  0 }, {  0,  0 }, { -10,  0 }, { -10,  0 } },
			{ { -10,  0 }, { -5,  0 }, {  0,  0 }, {  0,  0 }, {  0,  0 }, {  0,  0 }, { -10,  0 }, { -10,  0 } },
			{ {   0,  0 }, {  0,  0 }, {  0,  0 }, {  0,  0 }, {  0,  0 }, {  0,  0 }, {   0,  0 }, {   0,  0 } },
			{ {   0,  0 }, {  0,  0 }, {  0,  0 }, {  0,  0 }, {  0,  0 }, {  0,  0 }, {   0,  0 }, {   0,  0 } }
		};

		// 0 => 48,9%; 700EG=>49,7%; 700MG=>47,7%; 
		constexpr static value_t KNIGHT_PST[][int(File::COUNT) / 2][2] = {
			{ { -60, -50 }, { -30, -30 }, { -24, -20 }, { -24, -10 } },
			{ { -18, -40 }, { -12, -25 }, {  -6, -10 }, {  -3,   2 } },
			{ { -12, -30 }, {  -3, -10 }, {   1,  -2 }, {   3,  10 } },
			{ {  -6, -20 }, {   1,   0 }, {   9,   5 }, {  15,  15 } },
			{ {  -6, -20 }, {   3,  -5 }, {  12,   5 }, {  15,  15 } },
			{ {  -1, -30 }, {   6, -15 }, {  15,  -5 }, {  15,  10 } },
			{ { -18, -40 }, {  -6, -25 }, {   0, -25 }, {   6,   2 } },
			{ { -60, -50 }, { -24, -40 }, { -15, -25 }, {  -6, -10 } }
		};

		// 0 => 47,6%; 700EG => 49,9%; 700MG => 49,8%
		constexpr static value_t BISHOP_PST[][int(File::COUNT) / 2][2] = {
			{ { -20, -20 }, {  0, -15 }, { -5, -15 }, { -10, -10 } },
			{ { -10, -15 }, {  5,  -5 }, { 10,  -5 }, {   0,   0 } },
			{ {  -5, -10 }, { 10,   0 }, {  0,   0 }, {  10,   5 } },
			{ {  -5,  -5 }, {  5,   0 }, { 10,   0 }, {  15,   5 } },
			{ {  -5,  -5 }, { 10,   0 }, { 10,   0 }, {  15,   5 } },
			{ {  -5, -10 }, {  0,   0 }, {  0,   0 }, {   5,   2 } },
			{ { -10, -15 }, { -5,  -5 }, {  0,  -5 }, {   0,   0 } },
			{ { -20, -20 }, { -5, -15 }, { -5, -15 }, { -10, -10 } }
		};

		// 0 => 47,5%; REG700 =>50%
		constexpr static value_t ROOK_PST[][int(File::COUNT) / 2][2] = {
			{ { -15, -5 }, { -10, -5 }, {  -5, -5 }, { -2, -5 } },
			{ { -10, -5 }, {  -5, -5 }, {  -2,  0 }, {  2,  0 } },
			{ { -10, -2 }, {  -5, -2 }, {   0,  0 }, {  2,  0 } },
			{ { -10, -2 }, {  -5,  0 }, {   0,  0 }, {  2,  0 } },
			{ { -10, -2 }, {  -5,  0 }, {   0,  0 }, {  2,  0 } },
			{ { -10,  0 }, {   0,  0 }, {   2,  0 }, {  5,  2 } },
			{ {   5,  2 }, {   5,  2 }, {   5,  5 }, {  5,  2 } },
			{ { -10,  5 }, { -10,  5 }, {   0,  5 }, {  0,  5 } },
		};

		// Set to zero => 49,5% (MG, EG) MG700=>49,6%; EG700=>49,25%
		constexpr static value_t QUEEN_PST[][int(File::COUNT) / 2][2] = {
			{ { 0, -20 }, { 0, -20 }, { 0, -15 }, { 0, -10 } },
			{ { 0, -20 }, { 2, -15 }, { 5, -10 }, { 5,   0 } },
			{ { 0, -15 }, { 5, -10 }, { 5,  -5 }, { 5,   0 } },
			{ { 0, -10 }, { 5,   0 }, { 5,   5 }, { 5,  10 } },
			{ { 0, -10 }, { 5,   0 }, { 5,   5 }, { 5,  10 } },
			{ { 0, -15 }, { 5, -10 }, { 5,  -5 }, { 5,   0 } },
			{ { 0, -20 }, { 2, -15 }, { 5, -10 }, { 5,  -5 } },
			{ { 0, -20 }, { 0, -20 }, { 0, -15 }, { 0, -15 } },
		};

		// 0 => 42,1%, EG 0: 46,7%, MG 300: 49,7%, EG 300: 48,0%; EG700 = 51,5%; MG700=>49,9%
		constexpr static value_t KING_PST[][int(File::COUNT) / 2][2] = {
            { {  50,   0 }, {  60,   3 }, {  40,   8 }, {  30,   8 } },
            { {  55,   8 }, {  55,  16 }, {  30,  40 }, {  25,  40 } },
            { {  40,  16 }, {  45,  40 }, {  25,  56 }, {  15,  56 } },
            { {  30,  32 }, {  35,  56 }, {  20,  64 }, {  10,  64 } },
            { {  20,  32 }, {  25,  56 }, {  15,  64 }, {   5,  64 } },
            { {  10,  16 }, {  15,  40 }, {  10,  56 }, {   2,  56 } },
            { {   5,   8 }, {  10,  16 }, {   5,  32 }, {   0,  32 } },
            { {   0,   0 }, {   0,   3 }, {   0,   8 }, {   0,   8 } }
		};

	};

	/**
	 * UCI parameter access implementation for PST scales
	 */
	class PST::UciAccess : public ChessEval::UciParameterProvider {
	public:
		std::vector<ChessEval::UciParam> getUciParameters() const override;
		bool setUciParameter(const std::string& name, int32_t value) override;
	};
}


