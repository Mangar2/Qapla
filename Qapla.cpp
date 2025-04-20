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
#include <cstdlib>
#include <ctime>
#include "interface/selectinterface.h"

#ifdef USE_STOCKFISH_EVAL
#include "nnue/engine.h"
#endif

using namespace QaplaInterface;

namespace QaplaSearch {
	value_t SearchParameter::cmdLineParam[10];

	class ChessEnvironment {
	public:
	
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


*/

int main(int argc, char* argv[])
{

	srand(static_cast<unsigned>(time(nullptr)));
	/*
	 * This engine can be run testing itself with stockfish evaluation by the nnue network.
	 * The nnue branch contains the neccesary files. Still the required stockfish integration is part of
	 * the main branch and requires to define USE_STOCKFISH_EVAL to use it.
	 */
#ifdef USE_STOCKFISH_EVAL
	//Stockfish::Engine::load_network("./nnue/nn-1111cefa1111.nnue", "./nnue/nn-37f18f62d772.nnue");
	Stockfish::Engine::initialize();
	Stockfish::Engine::load_network("NNUE1", "NNUE2");
#endif

	std::cout << "Qapla 0.2.058 (C) 2025 Volker Boehm" << std::endl;
	// This enables setting search parameters to a static object. The search parameters are set as name, value pairs
	// Currently this is used for testing only
	SearchParameter::parseCommandLine(argc, argv);
	QaplaSearch::ChessEnvironment environment;
	// The environment collates the interface with the chess engine. Both are separated by an adapter interface to be
	// reusable for other engines. 
	// Qapla supports winboard, uci and a "statistic" interface providing additional functionality. The interface
	// is selected by the first command - e.g. "uci" for the uci interface. Winboard is default as it is best for debugging.
	environment.run();
}

