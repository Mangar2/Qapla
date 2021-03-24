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
 * Workpackage for a thread in bitbase generation
 */

#ifndef __WORKPACKAGE_H
#define __WORKPACKAGE_H

#include <vector>
#include "bitbase.h"

namespace QaplaBitbase {

	enum class WorkType {
		initial, search
	};

	enum class WorkResult {
		unknown, notWon, won, illegalPosition
	};

	class Workpackage
	{

		Workpackage(std::vector<uint64_t>& indexList, WorkType workType) {
			_workList = indexList;
			_workType = workType;
			_workIndex = 0;
		}

		bool isInital() {
			return _workType == WorkType::initial;
		}

		bool isWorkRemaining() {
			return _workIndex < _workList.size();
		}

		uint64_t getNextIndexToLookAt() {
			return _workList[_workIndex];
			_workIndex++;
		}

		void setWorkResult(WorkResult result) {
			_result.push_back(result);
		}

	private:
		std::vector<uint64_t> _workList;
		std::vector<WorkResult> _result;
		int32_t _workIndex;
		WorkType _workType;
	};

}

#endif // __WORKPACKAGE_H
