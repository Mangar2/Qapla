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
 * Implements an interface selector depending on the first command
 */

#include <string>
#include "consoleio.h"
#include "winboard.h"
#include "uci.h"
#include "uciprintsearchinfo.h"

using namespace std;

namespace ChessInterface {

	/**
	 * Processes any input coming from the console
	 */
	static void selectAndStartInterface(IChessBoard* board, IInputOutput* ioHandler) {
		const string firstToken = ioHandler->getNextTokenBlocking();
		if (firstToken == "uci") {
			UCI uci;
			UCIPrintSearchInfo sendSearchInfo(ioHandler);
			board->setSendSerchInfo(&sendSearchInfo);
			uci.run(board, ioHandler);
		} else {
			Winboard winboard;
			WinboardPrintSearchInfo sendSearchInfo(ioHandler);
			board->setSendSerchInfo(&sendSearchInfo);
			winboard.processInput(board, ioHandler);
		}
	}

}