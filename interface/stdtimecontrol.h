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
 * Implements a time recorder to record the runtime
 */

#ifndef __STDTIMECONTROL
#define __STDTIMECONTROL

#include <stdint.h>

namespace ChessInterface {

	class StdTimeControl
	{
	public:
		StdTimeControl();
		~StdTimeControl();

		// ---------------------- operators ---------------------------------------

		/**
		 * Stores the current system time
		 */
		void storeStartTime();


		/**
		 * Stores the CPU time
		 */
		void StoreCPUTime();

		// ---------------------- getter ------------------------------------------

		/**
		 * Gets the current System time in milliseconds
		 */
		int64_t getSystemTimeInMilliseconds() const;

		/**
		 * Gets the time difference between "storeStartTime" and now
		 */
		int64_t getTimeSpentInMilliseconds() const;

		/**
		 * Gets the current CPU time
		 */
		int64_t getCPUTimeInMilliseconds() const;

		/**
		 * Gets the difference of current cpu time and start cpu time
		 */
		int64_t getCPUTimeSpentInMilliseconds() const;

	private:
		int64_t mStartTime;
		int64_t mCPUTime;
	};
}

#endif // __STDTIMECONTROL

