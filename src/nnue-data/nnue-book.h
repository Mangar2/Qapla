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
 * @copyright Copyright (c) 2026 Volker Böhm
 * @Overview
 * The book that holds the library of start positions for the NNUE training
 * data.
 *
 * It carries no payload, so a node is the two bytes of the plain book format,
 * see src/book. The book states which positions the training games start at and
 * nothing else: the labels of the training data come from those games, and the
 * values the search had while the position was selected - its evaluation and the
 * value it returned - say nothing about the game that is played from there.
 * Keeping them would triple the file for two numbers nobody reads.
 */

#pragma once

#include "../book/book.h"

namespace QaplaNnueData {

	using PositionBook = QaplaBook::Book<>;
}
