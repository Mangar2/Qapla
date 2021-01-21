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
 */


#include <iostream>
#include "movegenerator/bitboardmasks.h"
#include "search/boardadapter.h"
#include "interface/fenscanner.h"
#include "interface/movescanner.h"
#include "interface/stdtimecontrol.h"
#include "interface/winboard.h"
#include "interface/winboardprintsearchinfo.h"
#include "interface/selectinterface.h"

#include "search/search.h"
#include "tests/evalpawntest.h"
#include "tests/evalmobilitytest.h"

#include "pgn/pgnfiletokenizer.h"
#include "pgn/pgngame.h"

using namespace ChessInterface;

ChessSearch::BoardAdapter adapter;

namespace ChessSearch {
	class ChessEnvironment {
	public:
		ChessEnvironment() {
			setFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
		}

		bool setFen(const char* fen) {
			FenScanner scanner;
			return scanner.setBoard(fen, &adapter);
		}

		bool setMove(string move) {
			MoveScanner scanner(move);
			bool res = false;
			if (scanner.isLegal()) {
				res = adapter.doMove(scanner.piece, scanner.departureFile,
					scanner.departureRank, scanner.destinationFile, scanner.destinationRank, scanner.promote);
			}
			assert(res == true);
			return res;
		}


		MoveGenerator& getBoard() { return adapter.getBoard(); }

		void run() {
			selectAndStartInterface(&adapter, &ioHandler);
		}

		BoardAdapter adapter;
	private:
		ConsoleIO ioHandler;
		
	};
}

/*
struct FenTest {
	string fen;
	vector<uint64_t> nodes;
};

typedef vector<FenTest> FenTests;

FenTests fenTests = {
	{
		"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
		{ 20, 400, 8902, 197281, 4865609, 119060324, 3195901860, 
		  84998978956, 2439530234167, 69352859712417, 2097651003696806,
		  62854969236701747, 1981066775000396239 
		}
	},
	{
		"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -",
		{ 48, 2039, 97862, 4085603, 193690690, 8031647685 }
	},
	{
		"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -",
		{ 14, 191, 2812, 43238, 674624, 11030083, 178633661, 3009794393 }
	},
	{
		"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
		{ 46, 2079, 89890, 3894594, 164075551, 6923051137,  287188994746, 11923589843526, 490154852788714 }
	},
	{
		"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
		{ 44, 1486, 62379, 2103487, 89941194 }
	},
	{
		"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
		{ 6, 264, 9467, 422333, 15833292, 706045033 }
	},
	{
		"r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1",
		{ 6, 264, 9467, 422333, 15833292, 706045033 }
	}
};

void runPerftTests(const FenTests& tests, uint64_t maxNodes = 1000000000) {
	FenScanner scanner;
	StdTimeControl timeControl;
	bool testResult = true;
	timeControl.storeStartTime();
	uint64_t totalNodes = 0;
	for (auto const& test : tests) {
		auto depth = 1;
		cout << "Fen: " << test.fen << endl;
		scanner.setBoard(test.fen, &adapter);
		for (auto const& expectedNodes : test.nodes) {
			if (expectedNodes > maxNodes) {
				continue;
			}
			const uint64_t calculatedNodes = adapter.perft(depth, 0);
			totalNodes += calculatedNodes;
			bool resultOK = expectedNodes == calculatedNodes;
			if (!resultOK) {
				cout << "**************************************************************" << endl;
				cout << "ERROR Expected: " << expectedNodes << " found: " << calculatedNodes << endl;
				testResult = false;
			}
			cout << depth << ": " << calculatedNodes << " end positions found " << (resultOK? "OK" : "ERROR") << endl;
			depth++;
		}
		cout << endl;
	}
	uint64_t timeSpent = timeControl.getTimeSpentInMilliseconds();
	double nodesPerSecond = double(totalNodes) / 1000 / timeSpent;
	if (testResult) {
		cout << "All tests finished without error. Million nodes per second:  " << nodesPerSecond << endl;
	}
	else {
		cout << "Test finished with errors" << endl;
	}
}

#include "search/threadpool.h"

void checkThreadPoolSpeed() {
	StdTimeControl timeControl;
	atomic<int> i = 0;
	atomic<int> j = 0;
	static const auto WORKER_AMOUNT = 3;
	ChessSearch::ThreadPool<2 * WORKER_AMOUNT> pool;
	ChessSearch::WorkPackage worki;
	worki.setFunction([&i]() { i++; });
	ChessSearch::WorkPackage workj;
	workj.setFunction([&j]() { j++; });
	pool.startWorker(2 * WORKER_AMOUNT);
	this_thread::sleep_for(chrono::milliseconds(10));
	timeControl.storeStartTime();
	for (int count = 0; count < 100000; count++) {
		pool.assignWork(&worki, WORKER_AMOUNT);
		pool.assignWork(&workj, WORKER_AMOUNT);
		worki.waitUntilFinished();
		workj.waitUntilFinished();
		i -= WORKER_AMOUNT;
	}
	uint64_t timeSpent = timeControl.getTimeSpentInMilliseconds();
	cout << "i: " << i << " j: " << j << ": " << timeSpent << endl;
}

void checkThreadSpeed() {
	StdTimeControl timeControl;
	int i = 0;
	timeControl.storeStartTime();
	for (int count = 0; count < 10000; count++) {
		thread th = thread([&i]() { i++; });
		th.join();
	}
	uint64_t timeSpent = timeControl.getTimeSpentInMilliseconds();
	cout << i << ": " << timeSpent << endl;
}
*/

void runTests() {
	ChessTest::runEvalMobilityTests();
}

void createStatistic() {
	ChessSearch::ChessEnvironment environment;
	ChessPGN::PGNFileTokenizer fileTokenizer("quabla_all.pgn");
	ChessPGN::PGNGame game;
	array<value_t, 30> win;
	array<value_t, 30> loss;
	array<value_t, 30> draw;
	win.fill(0);
	loss.fill(0);
	draw.fill(0);
	while (game.setGame(fileTokenizer)) {
		cout << '.';
		const string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
		FenScanner scanner;
		scanner.setBoard(fen, &environment.adapter);
		for (auto& move : game.getMoves()) {
			const MoveGenerator& board = environment.adapter.getBoard();
			environment.setMove(move);
			auto white = environment.adapter.getEvalFactors<WHITE>();
			auto black = environment.adapter.getEvalFactors<BLACK>();
			const string tag = "Knight attack";
			if (white.find(tag) != white.end()) {
				// if (white[tag] == 0) { environment.adapter.getBoard().print(); }
				if (game.getTag("Result") == "1-0") {
					win[white[tag]] ++; 
					loss[black[tag]] ++;
				}
				else if (game.getTag("Result") == "0-1") {
					loss[white[tag]] ++;
					win[black[tag]] ++;
				}
				else if (game.getTag("Result") == "1/2-1/2") {
					draw[white[tag]] ++;
					draw[black[tag]] ++;
				}
			}
		}
	};
	cout << endl;
	for (uint32_t i = 0; i < 30; i++) {
		uint32_t total = win[i] + loss[i] + draw[i];
		if (total > 0) {
			cout << i
				<< " score: " << ((win[i] * 100 + draw[i] * 50) / total) << "% (" << win[i] << ")"
				<< " win: " << (win[i] * 100 / total) << "% (" << win[i] << ")"
				<< " loss: " << (loss[i] * 100 / total) << "% (" << loss[i] << ")"
				<< " draw: " << (draw[i] * 100 / total) << "% (" << draw[i] << ")"
				<< endl;
		}
	}
}

int main()
{
	
	// checkThreadPoolSpeed();
	// checkThreadSpeed();
	/*
	StdTimeControl timeControl;
	for (uint32_t workerAmount = 1; workerAmount <= 1; workerAmount++) {
		adapter.setWorkerAmount(workerAmount);
		timeControl.storeStartTime();
		uint64_t totalNodes = adapter.perft(7);
		uint64_t timeSpent = timeControl.getTimeSpentInMilliseconds();
		double nodesPerSecond = double(totalNodes) / 1000 / timeSpent;
		cout << totalNodes << " end positions found, NPS: " << nodesPerSecond 
			<< " threads: " << workerAmount + 1 << endl;
	}
	*/
	// ChessEval::Eval::initStatics();
	// adapter.printEvalInfo();

	// adapter.setWorkerAmount(1);
	// runPerftTests(fenTests, 10000000000);
	// std::this_thread::sleep_for(std::chrono::seconds(20));
	ChessSearch::ChessEnvironment environment;
	environment.run();
	// createStatistic();
	// runTests();
}

