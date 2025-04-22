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
 * Implements a Winboard - Interface
 */

#pragma once

//#include "EPDTest.h"
#include <fstream>
#include <vector>
#include "chessinterface.h"
#include "candidate-trainer.h"
#include "../search/boardadapter.h"
#include "self-play-manager.h"

using namespace std;

namespace QaplaInterface {

	class Statistics : public ChessInterface {
	public:
		Statistics();


		/**
		 * Prints a game result information
		 */
		void printGameResult(GameResult result);

	private:
		class ChessGame {
		public:
			std::string fen;
			std::vector<std::pair<std::string, int>> moves;

			ChessGame(const std::string& fen) : fen(fen) {}
			void addMove(const std::string& move, int eval) {
				moves.emplace_back(move, eval);
			}
		};

		/**
		 * Reads the amount of cores
		 */
		void readCores() {
			getNextTokenBlocking();
			_maxTheadCount = uint32_t(getCurrentTokenAsUnsignedInt());
		}

		/**
		 * Reads the amount of memory
		 */
		void readMemory() {
			getNextTokenBlocking();
			_maxMemory = uint32_t(getCurrentTokenAsUnsignedInt());
		}

		/**
		 * Manages a move
		 */
		bool handleMove(string move = "");


		/**
		 * Processes any input coming from the console
		 */
		virtual void runLoop();

		/**
		 * Sets xBoard mode 
		 */
		void handleXBoard() {
			_xBoardMode = true;
		}

		/*
		 * Removes the last two moves, if a human person is at move
		 */
		void handleRemove();
		void handleWhatIf(std::string whatif);
		
		/**
		 * Sets the computer to analyze mode
		 */
		void analyzeMove();

		/**
		 * Starts a test of a list of EPD strings
		 */
		void WMTest();

		/**
		 * Sets the board from fen
		 */
		void setBoard();

		/**
		 * Handles a play level command
		 */
		void readLevelCommand();

		/**
		 * check command to modify the clock
		 */
		bool checkClockCommands();

		/**
		 * Processes any input while computing a move
		 */
		void handleInputWhileComputingMove();

		void train();
		void trainCandidates(uint32_t numThreads = 1);
		void playEpdGames(uint32_t numThreads = 1);
		void playStatistic(uint32_t numThreads = 1);
		void loadEPD();
		void loadEPD(const std::string& filename);
		void loadGamesFromFile(const std::string& filename);
		std::tuple<EvalValue, value_t>  computeEval(
			ChessEval::IndexLookupMap& lookupMap, std::map<std::string, std::vector<uint64_t>>& lookupCount, bool verbose = false);
		void trainPosition(ChessEval::IndexLookupMap& lookupMap, int32_t evalDiff);
		
		void computeMaterialDifference();

		/**
		 * Handles input while in "wait for user action" mode
		 */
		void handleInput();
		volatile Mode _mode;
		bool _xBoardMode;
		bool _computerIsWhite;
		std::vector<std::string> _startPositions;
		std::vector<ChessGame> _games;
		ISendSearchInfo* _sendSearchInfo;
		SelfPlayManager epdTasks;
	};

}
