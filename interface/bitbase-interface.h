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
 * Implements a Winboard - Interface
 */

#pragma once

//#include "EPDTest.h"
#include <fstream>
#include <vector>
#include "chessinterface.h"
#include "../search/boardadapter.h"

using namespace std;

namespace QaplaInterface {

	class BitbaseInterface: public ChessInterface {
	/**
		* Processes any input coming from the console
		*/
	virtual void runLoop();

	private:

		/**
		 * Reads the amount of cores
		 */
		void readCores() {
			getNextTokenBlocking();
			_maxTheadCount = uint32_t(getCurrentTokenAsUnsignedInt());
		}

		/**
		 * handles a generate EGTB command
		 */
		void generateBitbases();

		/**
		 * handles a verify EGTB command
		 */
		void verifyBitbases();

		/**
		 * Processes any input while computing a move
		 */
		void handleInputWhileGenerating();

		/**
		 * Handles input while in "wait for user action" mode
		 */
		void handleInput();
		volatile Mode _mode;
		bool _xBoardMode;
		bool _computerIsWhite;
		std::vector<std::string> _startPositions;
		ISendSearchInfo* _sendSearchInfo;
	};

}
