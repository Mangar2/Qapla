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
 * Implements a structure containing various results from eval calculations
 */

#ifndef __EVAL_EXCHANGE_STRUCTURES_H
#define __EVAL_EXCHANGE_STRUCTURES_H

 // Idee 1: Evaluierung in der Ruhesuche ohne positionelle Details
 // Idee 2: Zugsortierung nach lookup Tabelle aus reduziertem Board-Hash
 // Idee 3: Beweglichkeit einer Figur aus der Suche evaluieren. Speichern, wie oft eine Figur von einem Startpunkt erfolgreich bewegt wurde.

#include <vector>
#include <map>
#include "../basics/types.h"
#include "../basics/evalvalue.h"

using namespace QaplaBasics;

namespace ChessEval {

	struct IndexInfo {
		std::string name;
		uint32_t index;
		Piece color;
	};
	using IndexVector = std::vector<IndexInfo>;
	using IndexLookupMap = std::map<std::string, std::vector<EvalValue>>;

	struct PieceInfo {
		Piece piece;
		Square square;
		IndexVector indexVector;
		std::string propertyInfo;
		EvalValue totalValue;
	};
}

#endif // __EVAL_EXCHANGE_STRUCTURES_H