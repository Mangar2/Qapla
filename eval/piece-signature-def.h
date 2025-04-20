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
 * Implements chess board evaluation. Returns +100, if white is one pawn up
 * Used to evaluate in quiescense search
 */

#pragma once

#include "piece-signature-lookup.h"

namespace ChessEval
{
	/*38*/ constexpr PieceSignatureLookup KK = PieceSignatureLookup{ {0, {{0, 0, 0}, {1, 1, 0}, {2, 2, 0}, {3, 3, 0}}}, {1, {{1, 0, 51}, {2, 1, 69}, {3, 2, 69}, {4, 3, 65}}}, {2, {{2, 0, 97}, {3, 1, 93}, {4, 2, 93}, {5, 3, 90}}}, {3, {{3, 0, 99}, {4, 1, 98}, {5, 2, 97}, {6, 3, 96}}} };
	/*76*/ constexpr PieceSignatureLookup KNK = PieceSignatureLookup{ {0, {{0, 3, -66}, {1, 4, -39}, {2, 5, -20}, {3, 6, 0}}}, {1, {{0, 2, -32}, {1, 3, -3}, {2, 4, 18}, {3, 5, 39}}}, {2, {{0, 1, -7}, {1, 2, 35}, {2, 3, 56}, {3, 4, 70}}}, {3, {{0, 0, 0}, {1, 1, 75}, {2, 2, 82}, {3, 3, 87}}} };
	/*20*/ constexpr PieceSignatureLookup KNKN = PieceSignatureLookup{ {0, {{0, 0, 0}, {1, 1, 0}, {2, 2, 0}, {3, 3, 0}}}, {1, {{1, 0, 24}, {2, 1, 39}, {3, 2, 43}, {4, 3, 45}}}, {2, {{2, 0, 74}, {3, 1, 76}, {4, 2, 76}, {5, 3, 76}}}, {3, {{3, 0, 96}, {4, 1, 93}, {5, 2, 91}, {6, 3, 90}}} };
	/*76*/ constexpr PieceSignatureLookup KBK = PieceSignatureLookup{ {0, {{0, 3, -56}, {1, 4, -27}, {2, 5, -2}, {3, 6, 14}}}, {1, {{0, 2, -19}, {1, 3, 17}, {2, 4, 41}, {3, 5, 55}}}, {2, {{0, 1, 11}, {1, 2, 61}, {2, 3, 75}, {3, 4, 82}}}, {3, {{0, 0, 0}, {1, 1, 87}, {2, 2, 92}, {3, 3, 94}}} };
	/*27*/ constexpr PieceSignatureLookup KBKN = PieceSignatureLookup{ {0, {{0, 0, 0}, {1, 1, 11}, {2, 2, 11}, {3, 3, 2}}}, {1, {{1, 0, 30}, {2, 1, 57}, {3, 2, 58}, {4, 3, 50}}}, {2, {{2, 0, 81}, {3, 1, 86}, {4, 2, 85}, {5, 3, 80}}}, {3, {{3, 0, 97}, {4, 1, 96}, {5, 2, 95}, {6, 3, 93}}} };
	/*17*/ constexpr PieceSignatureLookup KBKB = PieceSignatureLookup{ {0, {{0, 0, 0}, {1, 1, 0}, {2, 2, 0}, {3, 3, 0}}}, {1, {{1, 0, 15}, {2, 1, 42}, {3, 2, 44}, {4, 3, 42}}}, {2, {{2, 0, 69}, {3, 1, 77}, {4, 2, 76}, {5, 3, 73}}}, {3, {{3, 0, 93}, {4, 1, 93}, {5, 2, 91}, {6, 3, 89}}} };
	/*71*/ constexpr PieceSignatureLookup KBNKN = PieceSignatureLookup{ {0, {{1, 4, -15}, {2, 5, -2}, {3, 6, 8}}}, {1, {{0, 2, -3}, {1, 3, 21}, {2, 4, 35}, {3, 5, 47}}}, {2, {{0, 1, 2}, {1, 2, 47}, {2, 3, 64}, {3, 4, 73}}}, {3, {{0, 0, 5}, {1, 1, 64}, {2, 2, 82}, {3, 3, 87}}} };
	/*74*/ constexpr PieceSignatureLookup KBNKB = PieceSignatureLookup{ {0, {{0, 3, -25}, {1, 4, -15}, {2, 5, 0}, {3, 6, 12}}}, {1, {{0, 2, -7}, {1, 3, 19}, {2, 4, 38}, {3, 5, 51}}}, {2, {{0, 1, 1}, {1, 2, 42}, {2, 3, 63}, {3, 4, 75}}}, {3, {{0, 0, 2}, {1, 1, 58}, {2, 2, 78}, {3, 3, 88}}} };
	/*42*/ constexpr PieceSignatureLookup KBBKN = PieceSignatureLookup{ {1, {{1, 3, 62}, {2, 4, 73}, {3, 5, 70}}}, {2, {{1, 2, 82}, {2, 3, 87}, {3, 4, 86}}}, {3, {{1, 1, 93}, {2, 2, 94}, {3, 3, 94}}} };
	/*72*/ constexpr PieceSignatureLookup KBBKB = PieceSignatureLookup{ {0, {{1, 4, 20}, {2, 5, 33}, {3, 6, 38}}}, {1, {{0, 2, 4}, {1, 3, 46}, {2, 4, 64}, {3, 5, 70}}}, {2, {{0, 1, 8}, {1, 2, 69}, {2, 3, 81}, {3, 4, 87}}}, {3, {{0, 0, 4}, {1, 1, 85}, {2, 2, 93}, {3, 3, 94}}} };
	/*28*/ constexpr PieceSignatureLookup KBBKBN = PieceSignatureLookup{ {0, {{1, 1, 15}, {2, 2, 20}, {3, 3, 16}}}, {1, {{2, 1, 55}, {3, 2, 59}, {4, 3, 55}}}, {2, {{2, 0, 76}, {3, 1, 83}, {4, 2, 84}, {5, 3, 80}}}, {3, {{4, 1, 95}, {5, 2, 93}, {6, 3, 92}}} };
	/*69*/ constexpr PieceSignatureLookup KRK = PieceSignatureLookup{ {1, {{0, 4, -23}, {1, 5, 21}, {2, 6, 41}}}, {2, {{0, 3, -8}, {1, 4, 46}, {2, 5, 65}, {3, 6, 72}}}, {3, {{0, 2, 16}, {1, 3, 69}, {2, 4, 81}, {3, 5, 85}}} };
	/*25*/ constexpr PieceSignatureLookup KRKN = PieceSignatureLookup{ {0, {{0, 2, -23}, {1, 3, 1}, {2, 4, 10}, {3, 5, 15}}}, {1, {{0, 1, 17}, {1, 2, 41}, {2, 3, 50}, {3, 4, 56}}}, {2, {{0, 0, 38}, {1, 1, 75}, {2, 2, 79}, {3, 3, 81}}}, {3, {{1, 0, 95}, {2, 1, 94}, {3, 2, 92}, {4, 3, 92}}} };
	/*32*/ constexpr PieceSignatureLookup KRKNN = PieceSignatureLookup{ {0, {{2, 1, 26}, {3, 2, 21}, {4, 3, 14}}}, {1, {{3, 1, 63}, {4, 2, 58}, {5, 3, 52}}} };
	/*23*/ constexpr PieceSignatureLookup KRKB = PieceSignatureLookup{ {0, {{0, 2, -23}, {1, 3, -13}, {2, 4, -2}, {3, 5, 7}}}, {1, {{0, 1, 24}, {1, 2, 30}, {2, 3, 41}, {3, 4, 50}}}, {2, {{0, 0, 55}, {1, 1, 71}, {2, 2, 74}, {3, 3, 78}}}, {3, {{1, 0, 96}, {2, 1, 94}, {3, 2, 91}, {4, 3, 92}}} };
	/*50*/ constexpr PieceSignatureLookup KRNKR = PieceSignatureLookup{ {0, {{0, 3, -24}, {1, 4, -9}, {2, 5, 0}, {3, 6, 8}}}, {1, {{0, 2, 0}, {1, 3, 22}, {2, 4, 32}, {3, 5, 42}}}, {2, {{0, 1, 15}, {1, 2, 50}, {2, 3, 61}, {3, 4, 69}}}, {3, {{0, 0, 26}, {1, 1, 78}, {2, 2, 83}, {3, 3, 86}}} };
	/*14*/ constexpr PieceSignatureLookup KRBKN = PieceSignatureLookup{ {2, {{0, 3, 47}, {1, 4, 64}, {2, 5, 64}, {3, 6, 65}}}, {3, {{0, 2, 75}, {1, 3, 79}, {2, 4, 81}, {3, 5, 79}}} };
	/*18*/ constexpr PieceSignatureLookup KRBKBB = PieceSignatureLookup{ {0, {{0, 2, -17}, {1, 3, -18}, {2, 4, -16}, {3, 5, -8}}}, {1, {{0, 1, 22}, {1, 2, 22}, {2, 3, 26}, {3, 4, 32}}}, {2, {{1, 1, 59}, {2, 2, 61}, {3, 3, 64}}}, {3, {{1, 0, 82}, {2, 1, 87}, {3, 2, 84}, {4, 3, 84}}} };
	/*40*/ constexpr PieceSignatureLookup KRBKR = PieceSignatureLookup{ {0, {{0, 3, -19}, {1, 4, 0}, {2, 5, 10}, {3, 6, 16}}}, {1, {{0, 2, 7}, {1, 3, 34}, {2, 4, 45}, {3, 5, 52}}}, {2, {{0, 1, 26}, {1, 2, 62}, {2, 3, 72}, {3, 4, 76}}}, {3, {{0, 0, 36}, {1, 1, 85}, {2, 2, 89}, {3, 3, 90}}} };
	/*33*/ constexpr PieceSignatureLookup KRBNKRB = PieceSignatureLookup{ {0, {{0, 3, -9}, {1, 4, 2}, {2, 5, 8}, {3, 6, 12}}}, {1, {{0, 2, 13}, {1, 3, 33}, {2, 4, 40}, {3, 5, 46}}}, {2, {{0, 1, 28}, {1, 2, 56}, {2, 3, 64}, {3, 4, 68}}}, {3, {{1, 1, 73}, {2, 2, 81}, {3, 3, 83}}} };
	/*32*/ constexpr PieceSignatureLookup KRBBKRN = PieceSignatureLookup{ {0, {{2, 5, 27}, {3, 6, 32}}}, {1, {{1, 3, 58}, {2, 4, 60}, {3, 5, 59}}}, {2, {{1, 2, 75}, {2, 3, 80}, {3, 4, 77}}}, {3, {{2, 2, 91}, {3, 3, 88}}} };
	/*33*/ constexpr PieceSignatureLookup KRBBKRB = PieceSignatureLookup{ {0, {{1, 4, 24}, {2, 5, 30}, {3, 6, 33}}}, {1, {{1, 3, 53}, {2, 4, 58}, {3, 5, 60}}}, {2, {{1, 2, 72}, {2, 3, 78}, {3, 4, 77}}}, {3, {{1, 1, 86}, {2, 2, 90}, {3, 3, 88}}} };
	/*17*/ constexpr PieceSignatureLookup KRRKRB = PieceSignatureLookup{ {0, {{0, 2, -11}, {1, 3, -5}, {2, 4, -4}, {3, 5, -2}}}, {1, {{0, 1, 27}, {1, 2, 33}, {2, 3, 35}, {3, 4, 36}}}, {2, {{0, 0, 44}, {1, 1, 68}, {2, 2, 66}, {3, 3, 66}}}, {3, {{1, 0, 90}, {2, 1, 88}, {3, 2, 85}, {4, 3, 84}}} };
	/*13*/ constexpr PieceSignatureLookup KRRKRBN = PieceSignatureLookup{ {0, {{2, 1, 0}, {3, 2, -6}, {4, 3, -13}}}, {1, {{3, 1, 33}, {4, 2, 28}, {5, 3, 23}}}, {2, {{4, 1, 58}, {5, 2, 57}, {6, 3, 54}}}, {3, {{5, 1, 79}, {6, 2, 80}, {7, 3, 74}}} };
	/*42*/ constexpr PieceSignatureLookup KRRKRBB = PieceSignatureLookup{ {0, {{2, 1, -28}, {3, 2, -38}, {4, 3, -42}}}, {1, {{3, 1, 1}, {4, 2, -4}, {5, 3, -9}}}, {2, {{4, 1, 37}, {5, 2, 27}, {6, 3, 26}}} };
	/*30*/ constexpr PieceSignatureLookup KRRNKRBB = PieceSignatureLookup{ {0, {{2, 4, -30}, {3, 5, -23}}}, {1, {{2, 3, 11}, {3, 4, 8}}}, {2, {{3, 3, 42}}}, {3, {{3, 2, 66}, {4, 3, 65}}} };
	/*32*/ constexpr PieceSignatureLookup KRRNKRBBN = PieceSignatureLookup{ {0, {{4, 3, -32}}}, {1, {{5, 3, 0}}}, {2, {{6, 3, 31}}} };
	/*15*/ constexpr PieceSignatureLookup KRRNKRR = PieceSignatureLookup{ {0, {{1, 4, 4}, {2, 5, 10}, {3, 6, 15}}}, {1, {{1, 3, 29}, {2, 4, 37}, {3, 5, 46}}}, {2, {{1, 2, 55}, {2, 3, 62}, {3, 4, 69}}}, {3, {{1, 1, 77}, {2, 2, 80}, {3, 3, 82}}} };
	/*28*/ constexpr PieceSignatureLookup KRRNNKRBBN = PieceSignatureLookup{ {0, {{3, 5, -26}}}, {1, {{3, 4, 3}}}, {2, {{3, 3, 33}}}, {3, {{4, 3, 50}}} };
	/*15*/ constexpr PieceSignatureLookup KRRBKRBNN = PieceSignatureLookup{ {0, {{4, 3, -15}}}, {1, {{5, 3, 18}}}, {2, {{6, 3, 48}}} };
	/*33*/ constexpr PieceSignatureLookup KRRBKRBBN = PieceSignatureLookup{ {0, {{3, 2, -30}, {4, 3, -33}}}, {1, {{4, 2, 2}, {5, 3, -2}}}, {2, {{5, 2, 29}, {6, 3, 28}}}, {3, {{7, 3, 59}}} };
	/*23*/ constexpr PieceSignatureLookup KRRBKRR = PieceSignatureLookup{ {0, {{0, 3, 4}, {1, 4, 15}, {2, 5, 19}, {3, 6, 22}}}, {1, {{0, 2, 27}, {1, 3, 42}, {2, 4, 49}, {3, 5, 54}}}, {2, {{0, 1, 48}, {1, 2, 66}, {2, 3, 71}, {3, 4, 74}}}, {3, {{1, 1, 84}, {2, 2, 86}, {3, 3, 86}}} };
	/*25*/ constexpr PieceSignatureLookup KRRBNKRBBN = PieceSignatureLookup{ {0, {{2, 4, -12}, {3, 5, -21}}}, {1, {{3, 4, 8}}}, {2, {{3, 3, 36}}}, {3, {{4, 3, 57}}} };
	/*39*/ constexpr PieceSignatureLookup KRRBNKRBBNN = PieceSignatureLookup{ {0, {{4, 3, -30}}}, {1, {{5, 3, -3}}}, {2, {{6, 3, 22}}} };
	/*44*/ constexpr PieceSignatureLookup KRRBNNKRBBNN = PieceSignatureLookup{ {0, {{3, 5, -23}}}, {1, {{3, 4, 2}}}, {2, {{3, 3, 17}}}, {3, {{4, 3, 35}}} };
	/*27*/ constexpr PieceSignatureLookup KRRBBKRBBNN = PieceSignatureLookup{ {0, {{4, 3, -23}}}, {1, {{5, 3, 9}}}, {2, {{6, 3, 34}}} };
	/*34*/ constexpr PieceSignatureLookup KRRBBKRRN = PieceSignatureLookup{ {0, {{2, 5, 34}, {3, 6, 34}}}, {1, {{2, 4, 65}, {3, 5, 58}}}, {2, {{3, 4, 73}}}, {3, {{3, 3, 83}}} };
	/*35*/ constexpr PieceSignatureLookup KRRBBKRRB = PieceSignatureLookup{ {0, {{2, 5, 34}, {3, 6, 35}}}, {1, {{2, 4, 58}, {3, 5, 58}}}, {2, {{2, 3, 73}, {3, 4, 73}}}, {3, {{3, 3, 82}}} };
	/*34*/ constexpr PieceSignatureLookup KRRBBNKRBBNN = PieceSignatureLookup{ {0, {{3, 5, -11}}}, {1, {{3, 4, 11}}}, {2, {{3, 3, 28}}}, {3, {{4, 3, 42}}} };

	/*39*/ constexpr PieceSignatureLookup KQKR = PieceSignatureLookup{ {1, {{0, 3, 14}, {1, 4, 48}, {2, 5, 61}, {3, 6, 70}}}, {2, {{0, 2, 30}, {1, 3, 60}, {2, 4, 76}, {3, 5, 83}}}, {3, {{0, 1, 48}, {1, 2, 73}, {2, 3, 86}, {3, 4, 91}}} };
	/*42*/ constexpr PieceSignatureLookup KQKRN = PieceSignatureLookup{ {0, {{0, 1, -4}, {1, 2, 16}, {2, 3, 32}, {3, 4, 42}}}, {1, {{0, 0, 15}, {1, 1, 43}, {2, 2, 58}, {3, 3, 69}}}, {2, {{1, 0, 65}, {2, 1, 78}, {3, 2, 84}, {4, 3, 86}}}, {3, {{2, 0, 93}, {3, 1, 94}, {4, 2, 94}, {5, 3, 94}}} };
	/*40*/ constexpr PieceSignatureLookup KQKRB = PieceSignatureLookup{ {0, {{0, 1, 3}, {1, 2, 20}, {2, 3, 31}, {3, 4, 40}}}, {1, {{0, 0, 19}, {1, 1, 47}, {2, 2, 61}, {3, 3, 69}}}, {2, {{1, 0, 70}, {2, 1, 83}, {3, 2, 86}, {4, 3, 87}}}, {3, {{2, 0, 95}, {3, 1, 95}, {4, 2, 95}, {5, 3, 94}}} };
	/*38*/ constexpr PieceSignatureLookup KQKRBN = PieceSignatureLookup{ {0, {{3, 1, 24}, {4, 2, 30}, {5, 3, 38}}}, {1, {{5, 2, 57}, {6, 3, 62}}} };
	/*34*/ constexpr PieceSignatureLookup KQKRR = PieceSignatureLookup{ {0, {{1, 0, 17}, {2, 1, 27}, {3, 2, 28}, {4, 3, 34}}}, {1, {{2, 0, 55}, {3, 1, 58}, {4, 2, 58}, {5, 3, 61}}}, {2, {{3, 0, 78}, {4, 1, 77}, {5, 2, 76}, {6, 3, 78}}}, {3, {{4, 0, 84}, {5, 1, 86}, {6, 2, 85}}} };
	/*20*/ constexpr PieceSignatureLookup KQNKRB = PieceSignatureLookup{ {2, {{2, 4, 77}, {3, 5, 81}}}, {3, {{2, 3, 87}, {3, 4, 90}}} };
	/*44*/ constexpr PieceSignatureLookup KQNKRBN = PieceSignatureLookup{ {0, {{3, 4, 44}}}, {1, {{3, 3, 68}}}, {2, {{4, 3, 85}}}, {3, {{4, 2, 93}, {5, 3, 92}}} };
	/*29*/ constexpr PieceSignatureLookup KQNKRBB = PieceSignatureLookup{ {0, {{3, 4, 29}}}, {1, {{3, 3, 57}}}, {2, {{4, 3, 79}}}, {3, {{5, 3, 89}}} };
	/*52*/ constexpr PieceSignatureLookup KQNKRR = PieceSignatureLookup{ {0, {{3, 5, 52}}}, {1, {{3, 4, 70}}}, {2, {{3, 3, 84}}}, {3, {{3, 2, 91}, {4, 3, 91}}} };
	/*42*/ constexpr PieceSignatureLookup KQNKRRB = PieceSignatureLookup{ {0, {{4, 3, 42}}}, {1, {{4, 2, 60}, {5, 3, 64}}}, {2, {{6, 3, 80}}} };
	/*65*/ constexpr PieceSignatureLookup KQNKQ = PieceSignatureLookup{ {0, {{1, 4, -2}, {2, 5, 0}, {3, 6, 6}}}, {1, {{0, 2, -2}, {1, 3, 25}, {2, 4, 36}, {3, 5, 37}}}, {2, {{0, 1, 7}, {1, 2, 52}, {2, 3, 63}, {3, 4, 63}}}, {3, {{0, 0, 11}, {1, 1, 72}, {2, 2, 80}, {3, 3, 79}}} };
	/*16*/ constexpr PieceSignatureLookup KQBKRN = PieceSignatureLookup{ {2, {{3, 5, 77}}}, {3, {{1, 2, 83}, {2, 3, 88}, {3, 4, 86}}} };
	/*36*/ constexpr PieceSignatureLookup KQBKRB = PieceSignatureLookup{ {1, {{3, 6, 67}}}, {2, {{1, 3, 75}, {2, 4, 79}, {3, 5, 79}}}, {3, {{0, 1, 62}, {1, 2, 80}, {2, 3, 88}, {3, 4, 90}}} };
	/*46*/ constexpr PieceSignatureLookup KQBKRBN = PieceSignatureLookup{ {0, {{2, 3, 31}, {3, 4, 46}}}, {1, {{2, 2, 65}, {3, 3, 69}}}, {2, {{3, 2, 85}, {4, 3, 85}}}, {3, {{3, 1, 93}, {4, 2, 92}, {5, 3, 93}}} };
	/*31*/ constexpr PieceSignatureLookup KQBKRBB = PieceSignatureLookup{ {0, {{3, 4, 31}}}, {1, {{3, 3, 57}}}, {2, {{3, 2, 76}, {4, 3, 77}}}, {3, {{4, 2, 89}, {5, 3, 89}}} };
	/*52*/ constexpr PieceSignatureLookup KQBKRR = PieceSignatureLookup{ {0, {{3, 5, 52}}}, {1, {{2, 3, 68}, {3, 4, 71}}}, {2, {{2, 2, 83}, {3, 3, 85}}}, {3, {{2, 1, 90}, {3, 2, 93}, {4, 3, 92}}} };
	/*38*/ constexpr PieceSignatureLookup KQBKRRN = PieceSignatureLookup{ {0, {{4, 3, 38}}}, {1, {{5, 3, 65}}}, {2, {{6, 3, 83}}} };
	/*39*/ constexpr PieceSignatureLookup KQBKRRB = PieceSignatureLookup{ {0, {{3, 2, 31}, {4, 3, 39}}}, {1, {{4, 2, 61}, {5, 3, 63}}}, {2, {{4, 1, 76}, {5, 2, 81}, {6, 3, 80}}} };
	/*64*/ constexpr PieceSignatureLookup KQBKQ = PieceSignatureLookup{ {0, {{0, 3, -9}, {1, 4, 4}, {2, 5, 5}, {3, 6, 14}}}, {1, {{0, 2, 4}, {1, 3, 34}, {2, 4, 41}, {3, 5, 41}}}, {2, {{0, 1, 14}, {1, 2, 57}, {2, 3, 66}, {3, 4, 65}}}, {3, {{0, 0, 12}, {1, 1, 74}, {2, 2, 82}, {3, 3, 80}}} };
	/*33*/ constexpr PieceSignatureLookup KQBNKRBBN = PieceSignatureLookup{ {0, {{3, 4, 33}}}, {1, {{3, 3, 56}}}, {2, {{4, 3, 76}}}, {3, {{5, 3, 87}}} };
	/*63*/ constexpr PieceSignatureLookup KQBNKRRB = PieceSignatureLookup{ {0, {{3, 5, 63}}}, {1, {{3, 4, 74}}}, {2, {{3, 3, 83}}}, {3, {{4, 3, 90}}} };
	/*30*/ constexpr PieceSignatureLookup KQBBKQB = PieceSignatureLookup{ {0, {{3, 6, 30}}}, {1, {{3, 5, 55}}}, {2, {{3, 4, 68}}}, {3, {{3, 3, 80}}} };
	/*23*/ constexpr PieceSignatureLookup KQRKRBN = PieceSignatureLookup{ {1, {{3, 5, 54}}}, {2, {{2, 3, 64}, {3, 4, 74}}}, {3, {{2, 2, 80}, {3, 3, 86}}} };
	/*23*/ constexpr PieceSignatureLookup KQRKRR = PieceSignatureLookup{ {1, {{3, 6, 54}}}, {2, {{1, 3, 50}, {2, 4, 64}, {3, 5, 73}}}, {3, {{1, 2, 54}, {2, 3, 76}, {3, 4, 84}}} };
	/*27*/ constexpr PieceSignatureLookup KQRKRRN = PieceSignatureLookup{ {0, {{3, 4, 27}}}, {1, {{3, 3, 54}}}, {2, {{3, 2, 72}, {4, 3, 74}}}, {3, {{4, 2, 86}, {5, 3, 87}}} };
	/*29*/ constexpr PieceSignatureLookup KQRKRRB = PieceSignatureLookup{ {0, {{2, 3, 13}, {3, 4, 29}}}, {1, {{2, 2, 43}, {3, 3, 56}}}, {2, {{3, 2, 71}, {4, 3, 76}}}, {3, {{3, 1, 85}, {4, 2, 84}, {5, 3, 87}}} };
	/*28*/ constexpr PieceSignatureLookup KQRKRRBN = PieceSignatureLookup{ {0, {{5, 3, 28}}}, {1, {{6, 3, 51}}} };
	/*22*/ constexpr PieceSignatureLookup KQRKQ = PieceSignatureLookup{ {1, {{1, 5, 31}, {2, 6, 29}}}, {2, {{1, 4, 51}, {2, 5, 52}, {3, 6, 48}}}, {3, {{0, 2, 54}, {1, 3, 67}, {2, 4, 69}, {3, 5, 66}}} };
	/*12*/ constexpr PieceSignatureLookup KQRKQBN = PieceSignatureLookup{ {0, {{3, 2, -4}, {4, 3, -6}}}, {1, {{4, 2, 27}, {5, 3, 25}}}, {2, {{5, 2, 56}, {6, 3, 49}}} };
	/*13*/ constexpr PieceSignatureLookup KQRNKRRB = PieceSignatureLookup{ {2, {{3, 5, 74}}}, {3, {{3, 4, 83}}} };
	/*34*/ constexpr PieceSignatureLookup KQRNKRRBN = PieceSignatureLookup{ {0, {{3, 4, 34}}}, {1, {{3, 3, 55}}}, {2, {{4, 3, 75}}}, {3, {{5, 3, 85}}} };
	/*26*/ constexpr PieceSignatureLookup KQRNKRRBB = PieceSignatureLookup{ {0, {{3, 4, 26}}}, {1, {{3, 3, 48}}}, {2, {{4, 3, 68}}}, {3, {{5, 3, 81}}} };
	/*34*/ constexpr PieceSignatureLookup KQRBKRRB = PieceSignatureLookup{ {1, {{3, 6, 65}}}, {2, {{3, 5, 74}}}, {3, {{2, 3, 80}, {3, 4, 82}}} };
	/*32*/ constexpr PieceSignatureLookup KQRBKRRBN = PieceSignatureLookup{ {0, {{3, 4, 32}}}, {1, {{3, 3, 54}}}, {2, {{4, 3, 73}}}, {3, {{4, 2, 84}, {5, 3, 84}}} };
	/*22*/ constexpr PieceSignatureLookup KQRBKRRBB = PieceSignatureLookup{ {0, {{3, 4, 22}}}, {1, {{3, 3, 44}}}, {2, {{4, 3, 64}}}, {3, {{5, 3, 79}}} };
	/*19*/ constexpr PieceSignatureLookup KQRBKQBB = PieceSignatureLookup{ {0, {{2, 4, -19}, {3, 5, -14}}}, {1, {{3, 4, 17}}}, {2, {{3, 3, 45}}}, {3, {{4, 3, 66}}} };
	/*31*/ constexpr PieceSignatureLookup KQRBNKRRBN = PieceSignatureLookup{ {1, {{3, 6, 62}}}, {2, {{3, 5, 69}}}, {3, {{3, 4, 78}}} };
	/*33*/ constexpr PieceSignatureLookup KQRBNKRRBNN = PieceSignatureLookup{ {0, {{3, 4, 33}}}, {1, {{3, 3, 53}}}, {2, {{4, 3, 68}}}, {3, {{5, 3, 81}}} };
	/*29*/ constexpr PieceSignatureLookup KQRBNKRRBBN = PieceSignatureLookup{ {0, {{3, 4, 29}}}, {1, {{3, 3, 47}}}, {2, {{4, 3, 64}}}, {3, {{5, 3, 76}}} };
	/*33*/ constexpr PieceSignatureLookup KQRBNNKRRBBNN = PieceSignatureLookup{ {0, {{3, 4, 33}}}, {1, {{3, 3, 45}}}, {2, {{4, 3, 59}}}, {3, {{5, 3, 72}}} };

	/*16*/ constexpr PieceSignatureLookup KQRBNNKQRBN = PieceSignatureLookup{ {0, {{3, 6, -1}}}, {1, {{3, 5, 24}}}, {2, {{3, 4, 45}}}, {3, {{3, 3, 62}}} };
	/*13*/ constexpr PieceSignatureLookup KQRBNNKQRBNN = PieceSignatureLookup{ {0, {{3, 3, 0}}}, {1, {{4, 3, 25}}}, {2, {{5, 3, 48}}} };
	/*34*/ constexpr PieceSignatureLookup KQRBBKRRBBN = PieceSignatureLookup{ {0, {{3, 4, 34}}}, {1, {{3, 3, 53}}}, {2, {{4, 3, 69}}}, {3, {{5, 3, 78}}} };
	/*18*/ constexpr PieceSignatureLookup KQRBBKQRB = PieceSignatureLookup{ {0, {{2, 5, 15}, {3, 6, 18}}}, {1, {{2, 4, 43}, {3, 5, 38}}}, {2, {{2, 3, 61}, {3, 4, 57}}}, {3, {{3, 3, 72}}} };
	/*12*/ constexpr PieceSignatureLookup KQRBBKQRBN = PieceSignatureLookup{ {0, {{3, 3, 9}}}, {1, {{3, 2, 43}, {4, 3, 35}}}, {2, {{4, 2, 65}, {5, 3, 57}}}, {3, {{5, 2, 74}, {6, 3, 72}}} };
	/*14*/ constexpr PieceSignatureLookup KQRBBKQRBB = PieceSignatureLookup{ {0, {{3, 3, 0}}}, {1, {{4, 3, 26}}}, {2, {{4, 2, 47}, {5, 3, 48}}}, {3, {{6, 3, 65}}} };
	/*39*/ constexpr PieceSignatureLookup KQRBBNKRRBBNN = PieceSignatureLookup{ {0, {{3, 4, 39}}}, {1, {{3, 3, 52}}}, {2, {{4, 3, 63}}}, {3, {{5, 3, 70}}} };
	/*16*/ constexpr PieceSignatureLookup KQRBBNKQRBN = PieceSignatureLookup{ {0, {{3, 6, 8}}}, {1, {{3, 5, 27}}}, {2, {{3, 4, 45}}}, {3, {{3, 3, 60}}} };
	/*15*/ constexpr PieceSignatureLookup KQRBBNKQRBB = PieceSignatureLookup{ {0, {{3, 6, 1}}}, {1, {{3, 5, 26}}}, {2, {{3, 4, 46}}}, {3, {{3, 3, 61}}} };
	/*14*/ constexpr PieceSignatureLookup KQRBBNKQRBBN = PieceSignatureLookup{ {0, {{3, 3, 0}}}, {1, {{4, 3, 24}}}, {2, {{5, 3, 47}}}, {3, {{6, 3, 64}}} };
	/*23*/ constexpr PieceSignatureLookup KQRBBNNKQRBBN = PieceSignatureLookup{ {0, {{3, 6, 5}}}, {1, {{3, 5, 21}}}, {2, {{3, 4, 41}}}, {3, {{3, 3, 53}}} };
	/*21*/ constexpr PieceSignatureLookup KQRBBNNKQRBBNN = PieceSignatureLookup{ {0, {{3, 3, 0}}}, {1, {{4, 3, 22}}}, {2, {{5, 3, 40}}} };
	/*12*/ constexpr PieceSignatureLookup KQRRKRRBN = PieceSignatureLookup{ {1, {{3, 5, 43}}}, {2, {{3, 4, 64}}}, {3, {{3, 3, 78}}} };
	/*26*/ constexpr PieceSignatureLookup KQRRKQR = PieceSignatureLookup{ {1, {{2, 6, 25}}}, {2, {{2, 5, 35}, {3, 6, 35}}}, {3, {{2, 4, 51}, {3, 5, 51}}} };
	/*10*/ constexpr PieceSignatureLookup KQRRKQRN = PieceSignatureLookup{ {0, {{2, 4, -10}, {3, 5, -8}}}, {1, {{2, 3, 28}, {3, 4, 23}}}, {2, {{3, 3, 52}}}, {3, {{3, 2, 68}, {4, 3, 71}}} };
	/*14*/ constexpr PieceSignatureLookup KQRRKQRB = PieceSignatureLookup{ {0, {{2, 4, -10}, {3, 5, -4}}}, {1, {{2, 3, 20}, {3, 4, 26}}}, {2, {{2, 2, 47}, {3, 3, 54}}}, {3, {{3, 2, 68}, {4, 3, 72}}} };
	/*19*/ constexpr PieceSignatureLookup KQRRKQRBN = PieceSignatureLookup{ {0, {{4, 3, -11}}}, {1, {{4, 2, 26}, {5, 3, 16}}}, {2, {{5, 2, 49}, {6, 3, 42}}}, {3, {{7, 3, 64}}} };
	/*38*/ constexpr PieceSignatureLookup KQRRKQRBB = PieceSignatureLookup{ {0, {{4, 3, -31}}}, {1, {{5, 3, -7}}}, {2, {{6, 3, 24}}} };
	/*21*/ constexpr PieceSignatureLookup KQRRNKQRNN = PieceSignatureLookup{ {0, {{3, 5, -21}}}, {1, {{3, 4, 18}}}, {2, {{3, 3, 42}}}, {3, {{4, 3, 62}}} };
	/*29*/ constexpr PieceSignatureLookup KQRRNKQRB = PieceSignatureLookup{ {2, {{3, 6, 32}}}, {3, {{3, 5, 48}}} };
	/*16*/ constexpr PieceSignatureLookup KQRRNKQRBN = PieceSignatureLookup{ {0, {{3, 5, -11}}}, {1, {{3, 4, 18}}}, {2, {{3, 3, 45}}}, {3, {{4, 3, 65}}} };
	/*20*/ constexpr PieceSignatureLookup KQRRNKQRBNN = PieceSignatureLookup{ {0, {{4, 3, -14}}}, {1, {{5, 3, 13}}}, {2, {{6, 3, 41}}} };
	/*25*/ constexpr PieceSignatureLookup KQRRNKQRBB = PieceSignatureLookup{ {0, {{3, 5, -19}}}, {1, {{3, 4, 7}}}, {2, {{3, 3, 36}}}, {3, {{4, 3, 56}}} };
	/*33*/ constexpr PieceSignatureLookup KQRRNKQRBBN = PieceSignatureLookup{ {0, {{4, 3, -21}}}, {1, {{5, 3, 2}}}, {2, {{6, 3, 28}}} };
	/*12*/ constexpr PieceSignatureLookup KQRRNKQRR = PieceSignatureLookup{ {0, {{2, 5, -1}, {3, 6, 8}}}, {1, {{2, 4, 19}, {3, 5, 34}}}, {2, {{3, 4, 56}}}, {3, {{3, 3, 71}}} };
	/*13*/ constexpr PieceSignatureLookup KQRRNKQRRN = PieceSignatureLookup{ {0, {{3, 3, 0}}}, {1, {{4, 3, 27}}}, {2, {{4, 2, 48}, {5, 3, 53}}}, {3, {{5, 2, 73}, {6, 3, 69}}} };
	/*29*/ constexpr PieceSignatureLookup KQRRNNKQRBBN = PieceSignatureLookup{ {0, {{3, 5, -16}}}, {1, {{3, 4, 6}}}, {2, {{3, 3, 32}}}, {3, {{4, 3, 50}}} };
	/*15*/ constexpr PieceSignatureLookup KQRRNNKQRRN = PieceSignatureLookup{ {0, {{3, 6, 1}}}, {1, {{3, 5, 27}}}, {2, {{3, 4, 46}}}, {3, {{3, 3, 61}}} };
	/*11*/ constexpr PieceSignatureLookup KQRRNNKQRRNN = PieceSignatureLookup{ {0, {{3, 3, 0}}}, {1, {{4, 3, 25}}}, {2, {{5, 3, 50}}} };
	/*38*/ constexpr PieceSignatureLookup KQRRBKQRN = PieceSignatureLookup{ {2, {{3, 6, 24}}}, {3, {{3, 5, 38}}} };
	/*21*/ constexpr PieceSignatureLookup KQRRBKQRNN = PieceSignatureLookup{ {0, {{3, 5, -20}}}, {1, {{3, 4, 13}}}, {2, {{3, 3, 40}}}, {3, {{4, 3, 58}}} };
	/*32*/ constexpr PieceSignatureLookup KQRRBKQRB = PieceSignatureLookup{ {1, {{2, 6, 22}, {3, 7, 10}}}, {2, {{2, 5, 32}, {3, 6, 29}}}, {3, {{2, 4, 51}, {3, 5, 46}}} };
	/*16*/ constexpr PieceSignatureLookup KQRRBKQRBN = PieceSignatureLookup{ {0, {{2, 4, -10}, {3, 5, -8}}}, {1, {{2, 3, 21}, {3, 4, 20}}}, {2, {{3, 3, 45}}}, {3, {{4, 3, 63}}} };
	/*28*/ constexpr PieceSignatureLookup KQRRBKQRBNN = PieceSignatureLookup{ {0, {{4, 3, -16}}}, {1, {{5, 3, 6}}}, {2, {{6, 3, 33}}} };
	/*27*/ constexpr PieceSignatureLookup KQRRBKQRBB = PieceSignatureLookup{ {0, {{3, 5, -20}}}, {1, {{3, 4, 8}}}, {2, {{3, 3, 34}}}, {3, {{4, 3, 54}}} };
	/*41*/ constexpr PieceSignatureLookup KQRRBKQRBBN = PieceSignatureLookup{ {0, {{4, 3, -26}}}, {1, {{5, 3, -1}}}, {2, {{6, 3, 20}}}, {3, {{7, 3, 43}}} };
	/*14*/ constexpr PieceSignatureLookup KQRRBKQRRN = PieceSignatureLookup{ {0, {{3, 3, -3}}}, {1, {{3, 2, 31}, {4, 3, 23}}}, {2, {{4, 2, 52}, {5, 3, 47}}}, {3, {{5, 2, 70}, {6, 3, 65}}} };
	/*13*/ constexpr PieceSignatureLookup KQRRBKQRRB = PieceSignatureLookup{ {0, {{2, 2, 0}, {3, 3, 0}}}, {1, {{3, 2, 27}, {4, 3, 25}}}, {2, {{4, 2, 52}, {5, 3, 48}}}, {3, {{5, 2, 67}, {6, 3, 66}}} };
	/*12*/ constexpr PieceSignatureLookup KQRRBNKRRBBNN = PieceSignatureLookup{ {1, {{3, 5, 30}}}, {2, {{3, 4, 51}}}, {3, {{3, 3, 64}}} };
	/*43*/ constexpr PieceSignatureLookup KQRRBNKQRBN = PieceSignatureLookup{ {2, {{3, 6, 19}}}, {3, {{3, 5, 33}}} };
	/*26*/ constexpr PieceSignatureLookup KQRRBNKQRBNN = PieceSignatureLookup{ {0, {{3, 5, -15}}}, {1, {{3, 4, 11}}}, {2, {{3, 3, 35}}}, {3, {{4, 3, 51}}} };
	/*42*/ constexpr PieceSignatureLookup KQRRBNKQRBB = PieceSignatureLookup{ {2, {{3, 6, 19}}}, {3, {{3, 5, 34}}} };
	/*34*/ constexpr PieceSignatureLookup KQRRBNKQRBBN = PieceSignatureLookup{ {0, {{3, 5, -22}}}, {1, {{3, 4, 3}}}, {2, {{3, 3, 27}}}, {3, {{4, 3, 44}}} };
	/*46*/ constexpr PieceSignatureLookup KQRRBNKQRBBNN = PieceSignatureLookup{ {0, {{4, 3, -24}}}, {1, {{5, 3, 0}}}, {2, {{6, 3, 15}}} };
	/*21*/ constexpr PieceSignatureLookup KQRRBNKQRRN = PieceSignatureLookup{ {0, {{2, 5, -1}, {3, 6, 0}}}, {1, {{3, 5, 23}}}, {2, {{3, 4, 42}}}, {3, {{3, 3, 55}}} };
	/*19*/ constexpr PieceSignatureLookup KQRRBNKQRRNN = PieceSignatureLookup{ {0, {{3, 3, -4}}}, {1, {{4, 3, 20}}}, {2, {{5, 3, 42}}}, {3, {{6, 3, 57}}} };
	/*15*/ constexpr PieceSignatureLookup KQRRBNKQRRB = PieceSignatureLookup{ {0, {{2, 5, 0}, {3, 6, 7}}}, {1, {{2, 4, 17}, {3, 5, 28}}}, {2, {{3, 4, 47}}}, {3, {{3, 3, 61}}} };
	/*16*/ constexpr PieceSignatureLookup KQRRBNKQRRBN = PieceSignatureLookup{ {0, {{3, 3, 0}}}, {1, {{3, 2, 30}, {4, 3, 23}}}, {2, {{4, 2, 49}, {5, 3, 45}}}, {3, {{5, 2, 70}, {6, 3, 62}}} };
	/*51*/ constexpr PieceSignatureLookup KQRRBNNKQRBBNN = PieceSignatureLookup{ {0, {{3, 5, -24}}}, {1, {{3, 4, -1}}}, {2, {{3, 3, 16}}}, {3, {{4, 3, 25}}} };
	/*32*/ constexpr PieceSignatureLookup KQRRBNNKQRRNN = PieceSignatureLookup{ {0, {{3, 6, -4}}}, {1, {{3, 5, 18}}}, {2, {{3, 4, 36}}}, {3, {{3, 3, 44}}} };
	/*26*/ constexpr PieceSignatureLookup KQRRBNNKQRRBN = PieceSignatureLookup{ {0, {{3, 6, 2}}}, {1, {{3, 5, 24}}}, {2, {{3, 4, 41}}}, {3, {{3, 3, 50}}} };
	/*20*/ constexpr PieceSignatureLookup KQRRBNNKQRRBNN = PieceSignatureLookup{ {0, {{3, 3, 0}}}, {1, {{4, 3, 20}}}, {2, {{5, 3, 41}}}, {3, {{6, 3, 59}}} };
	/*22*/ constexpr PieceSignatureLookup KQRRBBKQRBNN = PieceSignatureLookup{ {0, {{3, 5, -7}}}, {1, {{3, 4, 17}}}, {2, {{3, 3, 39}}}, {3, {{4, 3, 59}}} };
	/*23*/ constexpr PieceSignatureLookup KQRRBBKQRBBN = PieceSignatureLookup{ {0, {{3, 5, -10}}}, {1, {{3, 4, 14}}}, {2, {{3, 3, 38}}}, {3, {{4, 3, 55}}} };
	/*28*/ constexpr PieceSignatureLookup KQRRBBKQRBBNN = PieceSignatureLookup{ {0, {{4, 3, -20}}}, {1, {{5, 3, 3}}}, {2, {{6, 3, 36}}} };
	/*18*/ constexpr PieceSignatureLookup KQRRBBKQRRN = PieceSignatureLookup{ {0, {{3, 6, 12}}}, {1, {{3, 5, 32}}}, {2, {{3, 4, 46}}}, {3, {{3, 3, 58}}} };
	/*15*/ constexpr PieceSignatureLookup KQRRBBKQRRNN = PieceSignatureLookup{ {0, {{3, 3, 3}}}, {1, {{4, 3, 26}}}, {2, {{5, 3, 46}}}, {3, {{6, 3, 61}}} };
	/*17*/ constexpr PieceSignatureLookup KQRRBBKQRRB = PieceSignatureLookup{ {0, {{2, 5, 11}, {3, 6, 17}}}, {1, {{3, 5, 33}}}, {2, {{3, 4, 48}}}, {3, {{3, 3, 59}}} };
	/*12*/ constexpr PieceSignatureLookup KQRRBBKQRRBN = PieceSignatureLookup{ {0, {{3, 3, 6}}}, {1, {{4, 3, 28}}}, {2, {{4, 2, 58}, {5, 3, 49}}}, {3, {{5, 2, 76}, {6, 3, 65}}} };
	/*20*/ constexpr PieceSignatureLookup KQRRBBKQRRBB = PieceSignatureLookup{ {0, {{3, 3, 0}}}, {1, {{4, 3, 21}}}, {2, {{5, 3, 41}}}, {3, {{6, 3, 58}}} };
}

