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
 * Implements piece square table for static evaluation for the piece placement
 */

#pragma once

#include <array>
#include "../basics/types.h"

namespace QaplaBasics {

    /**
     * @brief A template class that maps chessboard squares to values for both white and black perspectives.
     *
     * This class allows mapping a chess square (from 0 to 63) to a value of type T.
     * It supports color-dependent access using a template parameter for the color (WHITE or BLACK).
     *
     * The mapping is initialized from a human-readable array in board order:
     * a8, b8, c8, d8, e8, f8, g8, h8
     * a7, b7, c7, d7, e7, f7, g7, h7
     * a6, b6, c6, d6, e6, f6, g6, h6
     * a5, b5, c5, d5, e5, f5, g5, h5
     * a4, b4, c4, d4, e4, f4, g4, h4
     * a3, b3, c3, d3, e3, f3, g3, h3
     * a2, b2, c2, d2, e2, f2, g2, h2
     * a1, b1, c1, d1, e1, f1, g1, h1
     * Internally, the board is remapped to a standard layout (a1=0, h8=63).
     * For black, the square is mirrored vertically (e.g. a2 -> a7).
     *
     * @tparam T The type of the value associated with each square.
     */
    template<typename T>
    class SquareTable {
    public:
        /**
         * @brief Constructs the SquareTable using a human-readable 8x8 board layout.
         *
         * The layout must be ordered row by row from top-left (a8) to bottom-right (h1).
         * Both white and black mappings are generated. Black values are mirrored vertically.
         *
         * @param humanReadable A 64-element array representing the board in human-readable format.
         */
        constexpr SquareTable(const std::array<T, 64>& humanReadable) : whiteTable{}, blackTable{} {
            for (int i = 0; i < 64; ++i) {
                Square sqWhite = humanToSquare(i);
                whiteTable[sqWhite] = humanReadable[i];
                blackTable[switchSide(sqWhite)] = humanReadable[i];
            }
        }

        /**
         * @brief Constructs the SquareTable using a human-readable 8x4 board layout.
         *
         * The layout must be ordered row by row from top-left (a8) to bottom-right (e1).
         * Both white and black mappings are generated. Black values are mirrored vertically.
         *
         * @param humanReadable A 64-element array representing the board in human-readable format.
         */
        constexpr SquareTable(const std::array<T, 32>& symmetricHalf) {
            for (Square sq = A1; sq <= H8; ++sq) {
                int halfIdx = toHalfIndex(sq);
                whiteTable[sq] = symmetricHalf[halfIdx];
                blackTable[switchSide(sq)] = symmetricHalf[halfIdx];
            }
        }

        /**
         * @brief Returns the value mapped to a square from the perspective of the given color.
         *
         * @tparam COLOR The color perspective (WHITE or BLACK).
         * @param sq The square index (0�63, where a1 = 0, h8 = 63).
         * @return The mapped value for the given square and color.
         */
        template<Piece COLOR>
        constexpr T map(Square sq) const {
            if constexpr (COLOR == WHITE) {
                return whiteTable[sq];
            }
            else {
                return blackTable[sq];
            }
        }

    private:
        std::array<T, 64> whiteTable;
        std::array<T, 64> blackTable;

        static constexpr Square humanToSquare(int idx) {
            return static_cast<Square>((7 - idx / 8) * 8 + idx % 8);
        }

        static constexpr int toHalfIndex(Square sq) {
            File file = File(sq);
            int fileIndex = static_cast<int>(file < File::E ? file: file - File::E);
            return static_cast<int>(Rank(sq)) * 4 + fileIndex;
        }
    };

}


