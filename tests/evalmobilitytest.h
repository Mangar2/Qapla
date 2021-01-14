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
 * Implements test cases for evalpawn functionality
 */

#pragma once

#include "../eval/evalmobility.h"
#include "../search/boardadapter.h"

using namespace ChessEval;
using namespace ChessSearch;

namespace ChessTest {

	struct EvalMobilityTest {
		EvalMobilityTest() : ok(0), fail(0) {}
		~EvalMobilityTest() { printResult(); }

		value_t getEval(string fen) {
			scanner.setBoard(fen, &adapter);
			EvalMobility eval;
			return eval.eval(adapter.getBoard());
		}

		void test(MoveGenerator& board, string message, value_t expected) {
			EvalMobility eval;
			value_t found = eval.eval(board);
			if (found != expected) {
				cout << message << " found: " << found << " expected: " << expected << endl;
				found = eval.eval(board);
				fail++;
			}
			else {
				cout << message << " ok " << endl;
				ok++;
			}
		}

		void test(string fen, string message, value_t expected) {
			scanner.setBoard(fen, &adapter);
			MoveGenerator& board = adapter.getBoard();
			test(board, "WHITE " + message, expected);
			MoveGenerator boardSym;
			boardSym.setToSymetricBoard(board);
			test(boardSym, "BLACK " + message, -expected);
		}

		void printResult() {
			cout << "ok: " << ok << " fail: " << fail << endl;
		}

		void measureRuntime() {


#ifdef _DEBUG
			const uint64_t LOOPS = 10000000;
#else
			const uint64_t LOOPS = 1000000000;
#endif
			scanner.setBoard("4k3/3p1p2/6p1/2p1p2p/P3P3/2P3P1/2P2P2/4K3 w - - 0 1", &adapter);
			StdTimeControl timeControl;
			timeControl.storeStartTime();

			for (uint64_t i = 0; i < LOOPS; i++) {
				EvalMobility eval;
				eval.eval(adapter.getBoard());
			}
			timeControl.printTimeSpent(LOOPS);
		}

		BoardAdapter adapter;
		FenScanner scanner;
		uint32_t ok;
		uint32_t fail;
	};


	static void runEvalMobilityTests() {
		EvalMobilityTest test;
		test.test("N3k3/8/8/8/8/8/8/4K3", "Knight in corner", EvalMobilityValues::KNIGHT_MOBILITY_MAP[2]);
		test.test("N3k3/p7/8/8/8/8/8/4K3", "Knight in corner one pawn attack", EvalMobilityValues::KNIGHT_MOBILITY_MAP[1]);
		test.test("4k3/8/N7/8/8/8/8/4K3", "Knight at edge", EvalMobilityValues::KNIGHT_MOBILITY_MAP[4]);
		test.test("4k3/8/1N6/8/8/8/8/4K3", "Knight near edge", EvalMobilityValues::KNIGHT_MOBILITY_MAP[6]);
		test.test("4k3/8/8/3N4/8/8/8/4K3", "Knight in center", EvalMobilityValues::KNIGHT_MOBILITY_MAP[6]);
		test.test("4k3/8/8/8/8/8/PPPPPPPP/2B1K3", "Bishop behind pawns", EvalMobilityValues::BISHOP_MOBILITY_MAP[0]);
		test.test("4k3/8/8/8/8/4P3/PPP1PPPP/2B1K3", "Bishop 1 move", EvalMobilityValues::BISHOP_MOBILITY_MAP[1]);
		test.test("4k3/8/8/8/5p2/8/PPP1PPPP/2B1K3", "Bishop 2 moves", EvalMobilityValues::BISHOP_MOBILITY_MAP[2]);
		test.test("4k3/8/8/8/5p2/8/P1P1PPPP/2B1K3", "Bishop stopped by black pawn", EvalMobilityValues::BISHOP_MOBILITY_MAP[3]);
		test.test("4k3/8/8/6p1/8/8/P1P1PPPP/2B1K3", "Bishop 4 moves", EvalMobilityValues::BISHOP_MOBILITY_MAP[4]);
		test.test("4k3/8/8/6P1/8/8/P1PBPPPP/4K3", "Bishop 6 moves", EvalMobilityValues::BISHOP_MOBILITY_MAP[6]);
		test.test("4k3/p3p3/1p3p2/8/3B4/8/PP2PP2/4K3", "Pawn attacks", EvalMobilityValues::BISHOP_MOBILITY_MAP[2]);
		test.test("4k3/8/8/8/4B3/8/P3PP2/4K3", "Bishop 13 moves", EvalMobilityValues::BISHOP_MOBILITY_MAP[13]);
		test.test("4k3/8/8/5P2/4PrP1/5P2/PP6/2B1K3", "Bishop opponent rook path through", 
			EvalMobilityValues::BISHOP_MOBILITY_MAP[5] - EvalMobilityValues::ROOK_MOBILITY_MAP[0]);
		test.test("4k3/7P/6PR/7P/8/8/PP6/2B1K3", "Bishop own rook block",
			EvalMobilityValues::BISHOP_MOBILITY_MAP[4] + EvalMobilityValues::ROOK_MOBILITY_MAP[0]);
		test.test("4k3/8/8/5P2/4PQP1/5P2/PP6/2B1K3", "Bishop own queen path through",
			EvalMobilityValues::BISHOP_MOBILITY_MAP[5] + EvalMobilityValues::QUEEN_MOBILITY_MAP[11]);
		test.test("4k3/8/8/8/8/8/P7/RK6", "Rook blocked", EvalMobilityValues::ROOK_MOBILITY_MAP[0]);
		test.test("4k3/8/8/8/P7/8/8/R4K2", "Rook 6 moves", EvalMobilityValues::ROOK_MOBILITY_MAP[6]);
		test.test("4k3/8/8/p7/8/8/8/R4K2", "Rook 7 moves", EvalMobilityValues::ROOK_MOBILITY_MAP[7]);
		test.test("4k3/1p6/p7/1p6/8/8/8/R2K4", "Not on pawn attacked fields", EvalMobilityValues::ROOK_MOBILITY_MAP[5]);
		test.test("4k3/P7/8/8/8/8/8/RK6", "Rook 5 moves", EvalMobilityValues::ROOK_MOBILITY_MAP[5]);
		test.test("4k3/8/8/RP6/RP6/8/8/1K6", "Two connected rook on open file", 2 * EvalMobilityValues::ROOK_MOBILITY_MAP[7]);
		test.test("4k3/8/1P6/QP6/RP6/8/8/1K6", "Queen/Rook path through", 
			EvalMobilityValues::ROOK_MOBILITY_MAP[7] + EvalMobilityValues::QUEEN_MOBILITY_MAP[7]);
		test.test("4k3/8/1P6/QR6/RB6/8/8/1K6", "Queen/Rook/Bishop path through",
			EvalMobilityValues::BISHOP_MOBILITY_MAP[9] + 2 * EvalMobilityValues::ROOK_MOBILITY_MAP[7] + 
			EvalMobilityValues::QUEEN_MOBILITY_MAP[18]);
		test.test("4k3/8/8/8/8/8/PP6/QK6", "Queen cornered", EvalMobilityValues::QUEEN_MOBILITY_MAP[0]);


		// test.measureRuntime();
	}

}

