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

#ifndef __STATISTICS_H
#define __STATISTICS_H

//#include "EPDTest.h"
#include <fstream>
#include <vector>
#include "chessinterface.h"
#include "../search/boardadapter.h"

using namespace std;

namespace QaplaInterface {

	
	class FileWriter {
	private:
		std::ofstream file;

	public:
		explicit FileWriter(const std::string& filename = "") {
			if (!filename.empty()) {
				open(filename);
			}
		}

		void open(const std::string& filename) {
			if (filename.empty()) {
				throw std::runtime_error("Error: Filename is empty");
			}
			if (file.is_open()) {
				file.close();
			}
			file.open(filename, std::ios::out | std::ios::trunc);
			if (!file) {
				throw std::runtime_error("Error: Cannot open file " + filename);
			}
		}

		~FileWriter() {
			if (file.is_open()) {
				file.close();
			}
		}

		void writeLine(const std::string& line) {
			if (file) {
				file << line << std::endl;
			}
		}
	};

	class PlayEpdGamesTask {
	public:
		PlayEpdGamesTask() : epdIndex(0) {}

		void setOutputFile(const std::string& filename) {
			fileWriter.open(filename);
		}

		void start(uint32_t numThreads, const ClockSetting& clock
			, const std::vector<std::string>& startPositions
			, const IChessBoard* boardTemplate
			, uint32_t games = 0) {
			stop();
			timeControl.storeStartTime();
			this->startPositions = startPositions;
			gameStatistics.clear();
			computer1Result = 0;
			gamesPlayed = 0;
			epdIndex = 0;

			for (size_t i = 0; i < numThreads; ++i) {
				if (i >= workers.size()) {
					workers.emplace_back(std::make_unique<WorkerThread>());
				}
				workers[i]->startTask([this, board = boardTemplate->createNew(), clock, games]() {
					board->setOption("Hash", "2");
					while (!stopped) {
						bool computer1IsWhite;
						std::string fen = "";
						{
							std::lock_guard<std::mutex> lock(positionMutex);
							const auto epdNo = statistic ? epdIndex / 2 : epdIndex;
							if (epdNo >= this->startPositions.size() || (games > 0 && epdIndex >= games)) {
								break;
							}
							computer1IsWhite = (epdIndex % 2) == 0;
							fen = this->startPositions[epdNo];
							epdIndex++;
						}
						GameResult result = playSingleGame(board, clock, fileWriter, fen, computer1IsWhite);
						{
							std::lock_guard<std::mutex> lock(statsMutex);
							gameStatistics[result]++;
							int32_t curResult = 0;
							if (result == GameResult::WHITE_WINS_BY_MATE) {
								curResult = computer1IsWhite ? 1 : - 1;
							}
							else if (result == GameResult::BLACK_WINS_BY_MATE) {
								curResult = computer1IsWhite ? - 1 : + 1;
							}
							computer1Result += curResult;
							gamesPlayed++;
							const auto positions = statistic ? this->startPositions.size() * 2 : this->startPositions.size();
							double timeSpentInSeconds = double(timeControl.getTimeSpentInMilliseconds()) / 1000.0;
							double estimatedTotalTime = timeSpentInSeconds * double(positions) / double(gamesPlayed);
							if (gamesPlayed % 100 == 0 || gamesPlayed == games) {
								std::cout
									<< "\r" << gamesPlayed << "/" << positions
									<< " time (s): " << timeSpentInSeconds << "/" << estimatedTotalTime
									<< " result: " << std::fixed << std::setprecision(2) << (computer1Result + gamesPlayed) * 50.0 / gamesPlayed;
								if (gamesPlayed == games) std::cout << std::endl;
							}
						}
					}
					delete board;
				});
			}
		}

		void stop() {
			stopped = true;
			for (auto& worker : workers) {
				worker->waitForTaskCompletion();
			}
			stopped = false;
		}

	private:

		GameResult playSingleGame(IChessBoard* board, const ClockSetting& clock, FileWriter& fileWriter, std::string fen, bool computer1IsWhite) {
			std::string gameResultString = fen;
			if (!ChessInterface::setPositionByFen(fen, board)) {
				return GameResult::ILLEGAL_MOVE;
			}
			GameResult result = board->getGameResult();
			while (result == GameResult::NOT_ENDED && !stopped) {
				board->setClock(clock);
				const auto evalVersion = computer1IsWhite == board->isWhiteToMove() ? 0 : 1;
				board->setEvalVersion(evalVersion);
				board->computeMove();
				ComputingInfoExchange computingInfo = board->getComputingInfo();
				const auto move = computingInfo.currentConsideredMove;
				const auto value = computingInfo.valueInCentiPawn;

				// std::cout << move << " " << value << " evalVersion " << evalVersion << std::endl;
				gameResultString += ", " + move + "," + std::to_string(value);

				// Adjudication: Stops game, if winning position reached.
				if (abs(value) > 10000) {
					result = value > 10000 == board->isWhiteToMove() ? GameResult::WHITE_WINS_BY_MATE : GameResult::BLACK_WINS_BY_MATE;
					break;
				}
				if (!ChessInterface::setMove(move, board)) {
					result = GameResult::ILLEGAL_MOVE;
					break;
				}
				result = board->getGameResult();
			}

			if (!stopped) {
				fileWriter.writeLine(gameResultString);
			}
			return result;
		}

		uint64_t computer1Result;
		uint64_t gamesPlayed;
		std::vector<std::string> startPositions;
		std::vector<std::unique_ptr<WorkerThread>> workers;
		std::array<int32_t, 1024> gameResults;
		FileWriter fileWriter;
		std::map<GameResult, uint32_t> gameStatistics;
		std::mutex statsMutex;
		std::mutex positionMutex;
		uint64_t epdIndex;
		bool stopped;
		bool statistic = true;
		StdTimeControl timeControl;
	};


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
		 * handles a generate EGTB command
		 */
		void generateEGTB();

		/**
		 * handles a verify EGTB command
		 */
		void verifyEGTB();

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
		 * Starts computing a move
		 */
		void computeMove();

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
		 * Undoes the last move
		 */
		void undoMove();

		/**
		 * Processes any input while computing a move
		 */
		void handleInputWhileComputingMove();

		void train();
		void playEpdGames(uint32_t numThreads = 1);
		void playStatistic(uint32_t numThreads = 1);
		void loadEPD();
		void loadGamesFromFile(const std::string& filename);
		std::tuple<EvalValue, value_t>  computeEval(
			ChessEval::IndexLookupMap& lookupMap, std::map<std::string, std::vector<uint64_t>>& lookupCount, bool verbose = false);
		void trainPosition(ChessEval::IndexLookupMap& lookupMap, int32_t evalDiff);

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
		PlayEpdGamesTask epdTasks;
	};

}

#endif // __STATISTICS_H