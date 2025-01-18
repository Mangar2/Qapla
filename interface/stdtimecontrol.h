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
 * Implements a time recorder to record the runtime
 */

#ifndef __STDTIMECONTROL
#define __STDTIMECONTROL

#include <stdint.h>
#include <iostream>

using namespace std;

namespace QaplaInterface {

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

		/**
		 * prints the time spent
		 */
		void printTimeSpent(uint64_t positions = 0) const {
			double timeSpentInSeconds = double(getTimeSpentInMilliseconds()) / 1000.0;
			if (positions > 0 && timeSpentInSeconds > 0) {
				double pps = double(positions) / timeSpentInSeconds / 1000000;
				cout << "time spent: " << timeSpentInSeconds << " positions: " << positions 
					<< " Mio. positions per second: " << pps << endl;
			}
			else {
				cout << "time spent: " << timeSpentInSeconds << " sec." << endl;
			}
		}

	private:
		int64_t mStartTime;
		int64_t mCPUTime;
	};
}

#endif // __STDTIMECONTROL

