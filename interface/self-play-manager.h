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
#include "../training/game-record.h"

using namespace std;

namespace QaplaInterface {

	class GamePairing;


	/**
	 * Handles writing GameRecord objects to a binary file.
	 * Automatically opens and closes the file. Flushes after each write.
	 */
	class GameRecordWriter {
	public:
		GameRecordWriter() {}

		/**
		 * Opens the file if a non-empty filename is provided.
		 *
		 * @param filename Path to the output file
		 */
		void open(const std::string& filename) {
			this->filename = filename;
			if (out.is_open()) {
				out.close();
			}
			if (!filename.empty()) {
				out.open(filename, std::ios::binary | std::ios::out | std::ios::trunc);
				if (!out) {
					std::cerr << "Failed to open file for writing: " << filename << std::endl;
				}
			}
		}

		/**
		 * Opens the file in append mode if a non-empty filename is provided.
		 *
		 * @param filename Path to the output file
		 */
		void append(const std::string& filename) {
			this->filename = filename;
			if (out.is_open()) {
				out.close();
			}
			if (!filename.empty()) {
				out.open(filename, std::ios::binary | std::ios::out | std::ios::app);
				if (!out) {
					std::cerr << "Failed to open file for appending: " << filename << std::endl;
				}
			}
		}

		void close() {
			if (out.is_open()) {
				out.close();
			}
		}

		~GameRecordWriter() {
			close();
		}

		/**
		 * Writes a GameRecord to the file. Does nothing if file is not open.
		 *
		 * @param game GameRecord to write
		 */
		void write(const QaplaTraining::GameRecord& game) {
			if (!out.is_open()) return;
			out << game;
			out.flush();
		}

		std::string getFilename() const {
			return filename;
		}

	private:
		std::ofstream out;
		std::string filename;
	};


	struct ResultPerPieceIndex {
		ResultPerPieceIndex()
			: signatureWin(PieceSignature::PIECE_SIGNATURE_SIZE * 8),
			signatureDraw(PieceSignature::PIECE_SIGNATURE_SIZE * 8),
			signatureLoss(PieceSignature::PIECE_SIGNATURE_SIZE * 8)
		{}

		void setResult(std::vector<uint32_t>& indexes, GameResult gameResult);
		void printResult();

		void saveToFile(const std::string& filename);

		void loadFromFile(const std::string& filename);

	private:
		uint64_t computeTotal(uint32_t sig) {
			if (sig >= signatureWin.size()) {
				std::cout << "Error: Signature out of range: " << sig << std::endl;
				return 0;
			}
			return signatureWin[sig] + signatureDraw[sig] + signatureLoss[sig];
		}
		int32_t computeStatistic(uint32_t sig, uint32_t sym) {
			const int64_t total = computeTotal(sig) + computeTotal(sym);
			if (total == 0) {
				return 0;
			}
			return static_cast<int32_t>((signatureWin[sig] - signatureWin[sym] - signatureLoss[sig] + signatureLoss[sym]) * 100 / total);
		}
		int64_t computeWinAllSignatures(uint32_t value);
		int64_t computeTotalForPieceOnlySignature(uint32_t wsig, uint32_t bsig);
		void printSigResult(uint32_t sig, uint32_t sym);
		std::vector<int64_t> signatureWin;
		std::vector<int64_t> signatureDraw;
		std::vector<int64_t> signatureLoss;

		
	};

	class SelfPlayManager {
	private:
		
	public:
		SelfPlayManager() : epdIndex(0) {}

		void setOutputFile(const std::string& filename) {
			writer.append(filename);
		}

		void start(uint32_t numThreads, const ClockSetting& clock
			, const std::vector<std::string>& startPositions
			, const IChessBoard* boardTemplate
			, uint32_t games = 0);

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

		QaplaTraining::GameRecord playSingleGame(const GamePairing& gamePairing, std::string fen, uint32_t fenIndex, bool curIsWhite);

		uint64_t computer1Result;
		uint64_t gamesPlayed;
		uint64_t fiftyMovesRule = 0;
		std::vector<std::string> startPositions;
		std::vector<std::unique_ptr<WorkerThread>> workers;
		std::array<int32_t, 1024> gameResults;
		std::map<GameResult, uint32_t> gameStatistics;
		std::mutex statsMutex;
		std::mutex positionMutex;
		uint64_t epdIndex;
		bool stopped;
		bool statistic = true;
		StdTimeControl timeControl;
		ResultPerPieceIndex resultPerPieceIndex;
		GameRecordWriter writer;
	};


	

}
