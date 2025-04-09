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
#include "candidate-trainer.h"
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
	private:
		struct GamePairing {
			GamePairing(const IChessBoard* boardTemplate, const ClockSetting& clock)
				: curBoard(boardTemplate->createNew()), newBoard(boardTemplate->createNew()), clock(clock) {
				curBoard->setOption("Hash", "2");
				newBoard->setOption("Hash", "2");
				curBoard->setEvalVersion(0);
				newBoard->setEvalVersion(1);
				curBoard->setClock(clock);
				newBoard->setClock(clock);
			}
			~GamePairing() {
				delete curBoard;
				delete newBoard;
			}
			bool setPositionByFen(const string& fen) const {
				bool err1 = ChessInterface::setPositionByFen(fen, curBoard);
				bool err2 = ChessInterface::setPositionByFen(fen, newBoard);
				return err1 || err2;
			}
			auto getGameResult() const {
				return curBoard->getGameResult();
			}
			void newGame() const {
				curBoard->newGame();
				newBoard->newGame();
			}
			std::tuple<GameResult, std::string, value_t> computeMove(bool curIsWhite) const {
				ComputingInfoExchange computingInfo;
				IChessBoard* sideToPlay = curIsWhite == curBoard->isWhiteToMove()  ? curBoard : newBoard;
				sideToPlay->computeMove();
  			    computingInfo = sideToPlay->getComputingInfo();
				const auto value = computingInfo.valueInCentiPawn;
				const auto move = computingInfo.currentConsideredMove;
				GameResult result;
				if (abs(value) > 10000) {
					result = value > 10000 == sideToPlay->isWhiteToMove() ? GameResult::WHITE_WINS_BY_MATE : GameResult::BLACK_WINS_BY_MATE;
				} 
				else {
					bool illegalMove = !ChessInterface::setMove(move, curBoard);
					ChessInterface::setMove(move, newBoard);
					result = illegalMove ? GameResult::ILLEGAL_MOVE : curBoard->getGameResult();
				}
				return std::tuple(result, move, value);
			}
			IChessBoard* curBoard;
			IChessBoard* newBoard;
			ClockSetting clock;
		private:
			GamePairing(const GamePairing&) = delete;
		};
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
				auto task = std::function<void()>([this, boardTemplate, clock, games]() {
					GamePairing gamePairing = GamePairing(boardTemplate, clock);
					while (!stopped) {
						bool curIsWhite; // This is the default version not the version with changed evaluation
						std::string fen = "";
						{
							std::lock_guard<std::mutex> lock(positionMutex);
							const auto epdNo = statistic ? epdIndex / 2 : epdIndex;
							if (epdNo >= this->startPositions.size() || (games > 0 && epdIndex >= games)) {
								break;
							}
							curIsWhite = (epdIndex % 2) == 0;
							fen = this->startPositions[epdNo];
							epdIndex++;
						}
						GameResult result = playSingleGame(gamePairing, fileWriter, fen, curIsWhite);
						{
							std::lock_guard<std::mutex> lock(statsMutex);
							gameStatistics[result]++;
							int32_t curResult = 0;
							if (result == GameResult::WHITE_WINS_BY_MATE) {
								curResult = curIsWhite ? 1 : - 1;
							}
							else if (result == GameResult::BLACK_WINS_BY_MATE) {
								curResult = curIsWhite ? - 1 : + 1;
							}
							CandidateTrainer::setGameResult(curResult == -1, curResult == 0);
							auto confidence = CandidateTrainer::getConfidenceInterval();
							if (CandidateTrainer::shallTerminate()) break;
							computer1Result += curResult;
							gamesPlayed++;
							const auto positions = statistic ? this->startPositions.size() * 2 : this->startPositions.size();
							double timeSpentInSeconds = double(timeControl.getTimeSpentInMilliseconds()) / 1000.0;
							double estimatedTotalTime = timeSpentInSeconds * double(positions) / double(gamesPlayed);
							if (gamesPlayed % 100 == 0 || gamesPlayed == games) {
								std::cout
									<< "\r" << gamesPlayed << "/" << positions
									<< " time (s): " << timeSpentInSeconds << "/" << estimatedTotalTime
									<< " result: " << std::fixed << std::setprecision(2) << CandidateTrainer::getScore() << "%"
									<< " confidence: " << (confidence.first * 100.0) << "% - " << (confidence.second * 100.0) << "%   ";

								if (gamesPlayed == games) std::cout << std::endl;
							}
						}
					}
				});
				workers[i]->startTask(task);
			}
		}

		void stop() {
			stopped = true;
			waitForEnd();
			stopped = false;
		}

		void waitForEnd() {
			for (auto& worker : workers) {
				worker->waitForTaskCompletion();
			}
		}

	private:

		GameResult playSingleGame(const GamePairing& gamePairing, FileWriter& fileWriter, std::string fen, bool curIsWhite) {
			std::string gameResultString = fen;
			gamePairing.newGame();
			gamePairing.setPositionByFen(fen);
			GameResult gameResult = gamePairing.getGameResult();
			while (gameResult == GameResult::NOT_ENDED && !stopped) {
				const auto [result, move, value] = gamePairing.computeMove(curIsWhite);
				gameResultString += ", " + move + "," + std::to_string(value);
				gameResult = result;
			}

			if (!stopped) {
				fileWriter.writeLine(gameResultString);
			}
			return gameResult;
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
		void trainCandidates(uint32_t numThreads = 1);
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