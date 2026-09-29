/**
 * @license
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Volker Boehm
 * @copyright Copyright (c) 2026 Volker Boehm
 * @Overview
 * The figures of searches the reporting search does not own - the extra searches, see
 * extra-search.h. They run on threads of their own and keep their own counters, so the
 * master cannot reach them the way it reaches its helpers. It asks through this interface
 * whenever it updates what it reports, so nodes, nps and tbhits cover every thread the
 * engine runs.
 */

#ifndef __ISEARCH_TOTALS_H
#define __ISEARCH_TOTALS_H

#include <cstdint>

namespace QaplaSearch {

	class ISearchTotals {
	public:
		virtual ~ISearchTotals() = default;

		/**
		 * Nodes searched by every thread of the searches behind this interface. Read while
		 * they search, so it may lag behind by the counts of the running nodes.
		 */
		virtual uint64_t getTotalNodes() const = 0;

		/**
		 * Positions these searches answered from the tablebases
		 */
		virtual uint64_t getTotalTbHits() const = 0;
	};

}

#endif // __ISEARCH_TOTALS_H
