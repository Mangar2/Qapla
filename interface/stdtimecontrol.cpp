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
 */

#include <time.h>
#include <sys/timeb.h>
#include "stdtimecontrol.h"

using namespace QaplaInterface;

StdTimeControl::StdTimeControl()
{
	mStartTime = 0;
	mCPUTime   = 0;
}

StdTimeControl::~StdTimeControl()
{
}

/**
 * Gets the current CPU time in milliseconds
 */
int64_t StdTimeControl::getCPUTimeInMilliseconds() const
{
   return ((int64_t)(clock()) * 1000) / (int64_t)(CLOCKS_PER_SEC); 
}

int64_t StdTimeControl::getSystemTimeInMilliseconds()
{
	#pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    timeb aCurrentTime;
    ftime(&aCurrentTime); 

    #pragma GCC diagnostic pop
	return ((int64_t)(aCurrentTime.time) * 1000 + 
		(int64_t)(aCurrentTime.millitm));
}

int64_t StdTimeControl::getTimeSpentInMilliseconds() const
{
	return getSystemTimeInMilliseconds() - mStartTime;
}

void StdTimeControl::storeStartTime()
{
	mStartTime = getSystemTimeInMilliseconds();
}

void StdTimeControl::StoreCPUTime() 
{
	mCPUTime = getCPUTimeInMilliseconds();
}

int64_t StdTimeControl::getCPUTimeSpentInMilliseconds() const
{
	return getCPUTimeInMilliseconds() - mCPUTime;
}


