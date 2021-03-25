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
 * Class holding the result of a bitbase generator work package
 * List of indexes won
 * List of indexes lost or dra
 * List of indexes with illegal positions/situations
 */

#ifndef __WORKPACKAGE_H
#define __WORKPACKAGE_H

#include <vector>
#include <mutex>
#include "bitbase.h"

namespace QaplaBitbase {

	class Result
	{
	public:
		Result() {
		}

		void addWin(uint64_t index) { _win.push_back(index); }
		void addLossOrDraw(uint64_t index) { _drawOrLoss.push_back(index); }
		void addIllegal(uint64_t index) { _illegal.push_back(index); }

		const std::vector<uint64_t>& getWins() const { return _win; }
		const std::vector<uint64_t>& getLossOrDraw() const { return _drawOrLoss; }
		const std::vector<uint64_t>& getIllegal() const { return _illegal; }


	private:
		std::vector<uint64_t> _win;
		std::vector<uint64_t> _drawOrLoss;
		std::vector<uint64_t> _illegal;
	};

}

#endif // __WORKPACKAGE_H
