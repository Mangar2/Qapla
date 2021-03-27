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
#include <mutex>
#include "bitbase.h"
#include "generationstate.h"

namespace QaplaBitbase {

	class Workpackage
	{
	public:
		Workpackage(const GenerationState& state) {
			state.getWork(_workList);
			_workIndex = 0;
		}

		bool isWorkRemaining() const {
			return _workIndex < _workList.size();
		}

		uint64_t getIndex(uint64_t workIndex) const { return _workList[workIndex]; }

		pair<uint64_t, uint64_t> getNextPackageToExamine(uint64_t count) {
			return getNextPackageToExamine(count, _workList.size());
		}

		pair<uint64_t, uint64_t> getNextPackageToExamine(uint64_t count, uint64_t size) {
			const lock_guard<mutex> lock(_mtxWork);
			auto result = make_pair(_workIndex, min(_workIndex + count, size));
			_workIndex += count;
			return result;
		}


	private:
		std::vector<uint64_t> _workList;
		uint64_t _workIndex;
		mutex _mtxWork;
	};

}

#endif // __WORKPACKAGE_H
