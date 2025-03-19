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
 * Implements an interface selector depending on the first command
 */

#include <string>
#include "consoleio.h"
#include "winboard.h"
#include "uci.h"
#include "statistics.h"
#include "uciprintsearchinfo.h"

using namespace std;

namespace QaplaInterface {


	bool startsWith(const std::string& str, const std::vector<std::string>& prefixes) {
		for (const std::string& prefix : prefixes) {
			if (str.rfind(prefix, 0) == 0) { 
				return true;
			}
		}
		return false;
	}

	/**
	 * Processes any input coming from the console
	 */
	static void selectAndStartInterface(IChessBoard* board, IInputOutput* ioHandler) {
		const string firstToken = ioHandler->getNextTokenBlocking();
		const string stats = "stat";
		if (firstToken == "uci") {
			UCI uci;
			UCIPrintSearchInfo sendSearchInfo(ioHandler);
			board->setSendSerchInfo(&sendSearchInfo);
			uci.run(board, ioHandler);
		}
		else if (startsWith(firstToken, { "stat", "epd" })) {
			Statistics statistics;
			statistics.run(board, ioHandler);
		} 
		else {
			Winboard winboard;
			WinboardPrintSearchInfo sendSearchInfo(ioHandler);
			board->setSendSerchInfo(&sendSearchInfo);
			winboard.run(board, ioHandler);
		}
	}

}