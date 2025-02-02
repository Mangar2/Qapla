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
 * Implements a static class to evaluate bishops
 */

#include <array>
#include <cassert>
#include "../basics/types.h"

using namespace QaplaBasics;

class InputLayer {
public:
    InputLayer() {
        initialize();
    }

    void initialize() {
        input.fill(0);
    }

    constexpr void setPiece(Piece piece, Square square) {
		input[computeIndex(piece, square)] = 1;
    }

    constexpr void removePiece(Piece piece, Square square) {
        input[computeIndex(piece, square)] = 0;
    }

    constexpr const std::array<int, 768>& getInput() const {
        return input;
    }

private:
	constexpr uint32_t computeIndex(Piece piece, Square square) {
        assert(piece >= WHITE_PAWN && piece <= BLACK_KING);
        assert(square >= A1 && square < H8);
		return (piece - WHITE_PAWN) * BOARD_SIZE + square;
	}
    std::array<int, 768> input;
};