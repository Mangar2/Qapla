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

#include <fstream>
#include <unordered_map>
#include "self-play-manager.h"
#include "chessinterface.h"
#include "../basics/piecesignature.h"
#include "../training/game-record.h"

namespace QaplaInterface {

	
	void ResultPerPieceIndex::saveToFile(const std::string& filename) {
		std::ofstream out(filename, std::ios::binary);
		if (!out) return;

		auto writeMap = [&](const std::vector<int64_t>& vec) {
			for (size_t i = 0; i < vec.size(); ++i) {
				if (vec[i] != 0) {
					out.write(reinterpret_cast<const char*>(&i), sizeof(uint32_t));
					out.write(reinterpret_cast<const char*>(&vec[i]), sizeof(int64_t));
				}
			}
			uint32_t endMarker = UINT32_MAX;
			out.write(reinterpret_cast<const char*>(&endMarker), sizeof(uint32_t));
			};

		writeMap(signatureWin);
		writeMap(signatureDraw);
		writeMap(signatureLoss);
		out.close();
	}

	void ResultPerPieceIndex::loadFromFile(const std::string& filename) {
		std::ifstream in(filename, std::ios::binary);
		if (!in) return;

		auto readMap = [&](std::vector<int64_t>& vec) {
			while (true) {
				uint32_t index;
				in.read(reinterpret_cast<char*>(&index), sizeof(uint32_t));
				if (index == UINT32_MAX || in.eof()) break;

				int64_t value;
				in.read(reinterpret_cast<char*>(&value), sizeof(int64_t));
				if (index < vec.size()) {
					vec[index] = value;
				}
			}
			};

		readMap(signatureWin);
		readMap(signatureDraw);
		readMap(signatureLoss);
		in.close();
	}

	int64_t ResultPerPieceIndex::computeWinAllSignatures(uint32_t value) {
		int64_t total = 0;
		int64_t win = 0;
		for (uint32_t sig = 0; sig < PieceSignature::PIECE_SIGNATURE_SIZE; sig++) {
			uint32_t vSig = sig * 8 + value + 3;
			uint32_t vSigSym = sig * 8 + (3 - value);
			total += computeTotal(vSig) + computeTotal(vSigSym);
			win += signatureWin[vSig] - signatureWin[vSigSym] - signatureLoss[vSig] + signatureLoss[vSigSym];
		}
		return (win) * 100 / total;
	}

	int64_t ResultPerPieceIndex::computeTotalForPieceOnlySignature(uint32_t wsig, uint32_t bsig) {
		int64_t total = 0;
		for (uint32_t value = 0; value <= 3; value++) {
			for (uint32_t wpawn = 0; wpawn < 4; wpawn++) {
				for (uint32_t bpawn = 0; bpawn < 4; bpawn++) {
					uint32_t sig = (((bpawn + bsig) << PieceSignature::SIG_SHIFT_BLACK) + wsig + wpawn) * 8 + value + 3;
					uint32_t symSig = (((wpawn + wsig) << PieceSignature::SIG_SHIFT_BLACK) + bsig + bpawn) * 8 + 3 - value;
					total += computeTotal(sig) + computeTotal(symSig);
				}
			}
		}
		return total;
	}

	void ResultPerPieceIndex::setResult(std::vector<uint32_t>& indexes, GameResult gameResult) {

		for (auto index : indexes) {
			int32_t val = static_cast<int32_t>(index % 8);
			if (val == 7) continue;
			switch (gameResult) {
			case GameResult::WHITE_WINS_BY_MATE:
				signatureWin[index]++;
				break;
			case GameResult::BLACK_WINS_BY_MATE:
				signatureLoss[index]++;
				break;
			case GameResult::DRAW_BY_REPETITION:
			case GameResult::DRAW_BY_STALEMATE:
			case GameResult::DRAW_BY_50_MOVES_RULE:
			case GameResult::DRAW_BY_NOT_ENOUGHT_MATERIAL:
				signatureDraw[index]++;
				break;
			default:
				break;
			}
		}
	}
	void ResultPerPieceIndex::printSigResult(uint32_t sig, uint32_t sym) {
		const int64_t total = computeTotal(sig) + computeTotal(sym);
		const int64_t statistic = computeStatistic(sig, sym);
		std::cout << statistic << "% (" << total << ") ";
	}
	void ResultPerPieceIndex::printResult() {
		std::cout << std::endl;
		std::string codeInput = "";
		for (uint32_t value = 0; value <= 3; value++) {
			cout << "Win ratio (" << value << "): " << computeWinAllSignatures(value) << " % " << std::endl;
		}
		for (uint32_t wsig = 0; wsig < 256 * 4; wsig+= 4) {
			for (uint32_t bsig = 0; bsig < 256 * 4; bsig+= 4) {
				// removed due to symmetry
				if (wsig < bsig) {
					continue;
				}
				bool any = false;
				const pieceSignature_t sig = (bsig << PieceSignature::SIG_SHIFT_BLACK) + wsig;
				if (computeTotalForPieceOnlySignature(wsig, bsig) < 5000) {
					continue;
				}
				const PieceSignature pieceSignature(sig);
				std::cout << pieceSignature.toString() + " ";
				std::string line = std::string("constexpr PieceSignatureLookup " + pieceSignature.toString() + " = PieceSignatureLookup{ ");
				value_t maxRelevance = 0;
				std::string valueSpacer = "";
				for (uint32_t value = 0; value <= 3; value++) {
					bool print = true;
					std::string spacer = "";
					for (uint32_t wpawn = 0; wpawn < 4; wpawn++) {
						for (uint32_t bpawn = 0; bpawn < 4; bpawn++) {
							const pieceSignature_t sigWithPawn = sig + (bpawn << PieceSignature::SIG_SHIFT_BLACK) + wpawn;
							const pieceSignature_t symSigWithPawn = ((wsig + wpawn) << PieceSignature::SIG_SHIFT_BLACK) + (bsig + bpawn);
							const uint32_t sigWithValue = sigWithPawn * 8 + (value + 3);
							const uint32_t symSigWithValue = symSigWithPawn * 8 + (3 - value);
							if (computeTotal(sigWithValue) + computeTotal(symSigWithValue) < 100) {
								continue;
							}
							if (print) {
								std::cout << "[" << value << "] ";
								line += valueSpacer + "{" + to_string(value) + ", {";
							}
							print = false;
							PieceSignature pieceSignatureWithPawn(sigWithPawn);
							const int32_t pawnDiff = value - pieceSignatureWithPawn.toValueNP();
							const int32_t wpc = (pawnDiff > 0 ? bpawn + pawnDiff : wpawn);
							const int32_t bpc = (pawnDiff < 0 ? wpawn - pawnDiff : bpawn);
							bool possiblyMore = wpc >= 3 && bpc >= 3;
							const auto statistic = computeStatistic(sigWithValue, symSigWithValue);
							maxRelevance = std::max(maxRelevance, std::abs(statistic - std::array<value_t, 4>{ 0, 31, 61, 76 }[value]));
							std::cout << "P[" << wpc << (possiblyMore ? "+" : "") << "," << bpc << (possiblyMore ? "+" : "") << "] ";
							line += spacer + "{" + to_string(wpc) + ", " + to_string(bpc) + ", " + to_string(statistic) + "}";

							spacer = ", ";
							valueSpacer = ", ";
							printSigResult(sigWithValue, symSigWithValue);
						}
					}
					if (!print) {
						line += "}}";
					}
				}
				line += "};";
				if (maxRelevance >= 10) {
					codeInput += std::string("/*") + to_string(maxRelevance) + "*/ " + line + "\n";
				}
				std::cout << std::endl;
			}
		}
		std::cout << std::endl;
		std::cout << codeInput << std::endl;
	}

	class GamePairing {
	public:
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
		auto eval() const {
			return curBoard->eval();
		}
		void newGame() const {
			curBoard->newGame();
			newBoard->newGame();
		}
		IChessBoard* getCurBoard() const {
			return curBoard;
		}
		bool isCapture(std::string move) const {
			return ChessInterface::isCapture(move, curBoard);
		}
		std::tuple<GameResult, std::string, value_t, bool> computeMove(bool curIsWhite) const {
			ComputingInfoExchange computingInfo;
			IChessBoard* sideToPlay = curIsWhite == curBoard->isWhiteToMove() ? curBoard : newBoard;
			sideToPlay->computeMove();
			computingInfo = sideToPlay->getComputingInfo();
			const auto value = computingInfo.valueInCentiPawn;
			const auto move = computingInfo.currentConsideredMove;
			GameResult result;
			bool capture = isCapture(move);
			if (abs(value) > 1000) {
				result = value > 1000 == sideToPlay->isWhiteToMove() ? GameResult::WHITE_WINS_BY_MATE : GameResult::BLACK_WINS_BY_MATE;
			}
			else {
				bool illegalMove = !ChessInterface::setMove(move, curBoard);
				ChessInterface::setMove(move, newBoard);
				result = illegalMove ? GameResult::ILLEGAL_MOVE : curBoard->getGameResult();
			}
			return std::tuple(result, move, value, capture);
		}
		IChessBoard* curBoard;
		IChessBoard* newBoard;
		ClockSetting clock;
	private:
		GamePairing(const GamePairing&) = delete;
	};

	void SelfPlayManager::start(uint32_t numThreads, const ClockSetting& clock
		, const std::vector<std::string>& startPositions
		, const IChessBoard* boardTemplate
		, uint32_t games) {
		const uint64_t gamesPerEpd = 16;
		stop();
		timeControl.storeStartTime();
		this->startPositions = startPositions;
		gameStatistics.clear();
		computer1Result = 0;
		gamesPlayed = 0;
		fiftyMovesRule = 0;
		epdIndex = 0;

		for (size_t i = 0; i < numThreads; ++i) {
			if (i >= workers.size()) {
				workers.emplace_back(std::make_unique<WorkerThread>());
			}
			auto task = std::function<void()>([this, i, boardTemplate, clock, games]() {
				srand(static_cast<unsigned>(time(nullptr)) ^ static_cast<unsigned>(i));
				GamePairing gamePairing = GamePairing(boardTemplate, clock);
				while (!stopped) {
					bool curIsWhite; // This is the default version not the version with changed evaluation
					uint64_t epdNo = 0;
					std::string fen = "";
					{
						std::lock_guard<std::mutex> lock(positionMutex);
						epdNo = statistic ? epdIndex / gamesPerEpd : epdIndex;
						if (epdNo >= this->startPositions.size() || (games > 0 && epdIndex >= games)) {
							break;
						}
						curIsWhite = (epdIndex % gamesPerEpd) == 0;
						fen = this->startPositions[epdNo];
						epdIndex++;
					}
					QaplaTraining::GameRecord game = playSingleGame(gamePairing, fen, static_cast<uint32_t>(epdNo), curIsWhite);
					{
						std::lock_guard<std::mutex> lock(statsMutex);
						const auto result = game.getResult();
						gameStatistics[result]++;
						int32_t curResult = 0;
						if (result == GameResult::WHITE_WINS_BY_MATE) {
							curResult = curIsWhite ? 1 : -1;
						}
						else if (result == GameResult::BLACK_WINS_BY_MATE) {
							curResult = curIsWhite ? -1 : +1;
						}
						else if (result == GameResult::DRAW_BY_REPETITION) fiftyMovesRule++;
						writer.write(game);
						/*
						CandidateTrainer::setGameResult(curResult == -1, curResult == 0);
						auto confidence = CandidateTrainer::getConfidenceInterval();
						if (CandidateTrainer::shallTerminate()) break;
						*/
						computer1Result += curResult;
						gamesPlayed++;
						const auto positions = statistic ? this->startPositions.size() * gamesPerEpd : this->startPositions.size();
						double timeSpentInSeconds = double(timeControl.getTimeSpentInMilliseconds()) / 1000.0;
						double estimatedTotalTime = timeSpentInSeconds * double(positions) / double(gamesPlayed);
						if (gamesPlayed % 100 == 0 || gamesPlayed == games) {
							std::cout
								<< "\r" << gamesPlayed << "/" << positions
								<< " time (s): " << timeSpentInSeconds << "/" << estimatedTotalTime
								// << " result: " << std::fixed << std::setprecision(2) << CandidateTrainer::getScore() << "%"
								// << " confidence: " << (confidence.first * 100.0) << "% - " << (confidence.second * 100.0) << "%   "
								;
							if (gamesPlayed == games) std::cout << std::endl;
						}
					}
				}
			});
			workers[i]->startTask(task);
		}
	}

	QaplaTraining::GameRecord SelfPlayManager::playSingleGame(const GamePairing& gamePairing, std::string fen, uint32_t fenIndex, bool curIsWhite) {

		std::string gameResultString = fen;
		QaplaTraining::GameRecord gameRecord;
		gameRecord.setFENId(fenIndex);

		gamePairing.newGame();
		gamePairing.setPositionByFen(fen);
		GameResult gameResult = gamePairing.getGameResult();
		bool captureBefore = false;
		vector<uint32_t> detectedIndices;
		while (gameResult == GameResult::NOT_ENDED && !stopped) {
			const auto [result, move, value, capture] = gamePairing.computeMove(curIsWhite);
			if (capture) {
				captureBefore = true;
			} else if (captureBefore) {
				const auto& indexVector = gamePairing.getCurBoard()->computeEvalIndexVector();
				if (indexVector[0].name == "pieceSignature") {
					detectedIndices.push_back(indexVector[0].index);
				}
				const auto& index = indexVector[0];
				captureBefore = false;
			}
			gameResultString += ", " + move + ", " + std::to_string(value);
			const auto eval = gamePairing.eval();
			gameRecord.addMove(move, value);
			gameResult = result;
		}
		if (!stopped) {
			gameRecord.setResult(gameResult);
		}
		return gameRecord;
	}
}