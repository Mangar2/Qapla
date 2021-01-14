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

#include "../eval/evalpawn.h"
#include "../search/boardadapter.h"

using namespace ChessEval;
using namespace ChessSearch;

namespace ChessTest {

	struct EvalPawnTest {
		EvalPawnTest() : ok(0), fail(0), adapter() {}
		~EvalPawnTest() { printResult(); }

		value_t getEval(string fen) {
			scanner.setBoard(fen, &adapter);
			return eval.eval(adapter.getBoard());
		}

		void test(MoveGenerator& board, string message, value_t expected) {
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
				eval.eval(adapter.getBoard());
			}
			timeControl.printTimeSpent(LOOPS);
		}

		EvalPawn eval;
		BoardAdapter adapter;
		FenScanner scanner;
		uint32_t ok;
		uint32_t fail;
	};


	static void runPawnEvalTests() {
		EvalPawnTest test;

		test.test("4k3/pppppppp/8/8/8/3P4/PPP1PPPP/4K3", "pawn rank 3", EvalPawnValues::ADVANCED_PAWN_VALUE[2]);
		test.test("4k3/pppppppp/8/8/3P4/8/PPP1PPPP/4K3", "pawn rank 4", EvalPawnValues::ADVANCED_PAWN_VALUE[3]);
		test.test("4k3/pppppppp/8/3P4/8/8/PPP1PPPP/4K3", "pawn rank 5", EvalPawnValues::ADVANCED_PAWN_VALUE[4]);
		test.test("4k3/pppppppp/3P4/8/8/8/PPP1PPPP/4K3", "pawn rank 6", EvalPawnValues::ADVANCED_PAWN_VALUE[5]);
		test.test("4k3/pppppppp/3P4/2P5/1P6/P7/4PPPP/4K3", "ranks 3-6",
			EvalPawnValues::ADVANCED_PAWN_VALUE[3] + EvalPawnValues::ADVANCED_PAWN_VALUE[4] + EvalPawnValues::ADVANCED_PAWN_VALUE[5]);
		test.test("4k3/pppppppp/8/8/8/P7/PPPPPPPP/4K3", "double pawn", EvalPawnValues::DOUBLE_PAWN_PENALTY);
		test.test("4k3/pppppppp/8/8/8/8/PP1P1PPP/4K3", "isolated pawn", EvalPawnValues::ISOLATED_PAWN_PENALTY);
		test.test("4k3/pppppppp/8/8/8/2P5/P1P1PPPP/4K3 -", "isolated double", EvalPawnValues::DOUBLE_PAWN_PENALTY + 2 * EvalPawnValues::ISOLATED_PAWN_PENALTY);
		test.test("4k3/pppppppp/8/8/P7/8/PPPPPPPP/4K3", "double rank 4", EvalPawnValues::DOUBLE_PAWN_PENALTY + EvalPawnValues::ADVANCED_PAWN_VALUE[3]);
		test.test("4k3/pppppppp/8/P7/8/8/PPPPPPPP/4K3", "double rank 5", EvalPawnValues::DOUBLE_PAWN_PENALTY + EvalPawnValues::ADVANCED_PAWN_VALUE[4]);
		test.test("4k3/pppppppp/P7/8/8/8/PPPPPPPP/4K3", "double rank 6", EvalPawnValues::DOUBLE_PAWN_PENALTY + EvalPawnValues::ADVANCED_PAWN_VALUE[5]);
		test.test("4k3/pppppppp/8/8/8/P3P2P/PPPPPPPP/4K3", "three double pawn", 3 * EvalPawnValues::DOUBLE_PAWN_PENALTY);
		test.test("4k3/pppppppp/8/1P6/8/1P6/PPPPPPPP/4K3", "triple pawn", 2 * EvalPawnValues::DOUBLE_PAWN_PENALTY + EvalPawnValues::ADVANCED_PAWN_VALUE[4]);
		test.test("4k3/1ppppppp/P7/p7/8/8/PPPPPPPP/4K3", "double rank 6, black pawn inbetween",
			EvalPawnValues::DOUBLE_PAWN_PENALTY + EvalPawnValues::ADVANCED_PAWN_VALUE[5] - EvalPawnValues::ADVANCED_PAWN_VALUE[3]);

		test.test("4k3/2pppppp/8/8/8/8/P1PPPPPP/4K3", "PP rank 2",
			EvalPawnValues::PASSED_PAWN_VALUE[1] + EvalPawnValues::ISOLATED_PAWN_PENALTY + EvalPawnValues::ADVANCED_PAWN_VALUE[1]);
		test.test("4k3/2pppppp/8/8/8/P7/2PPPPPP/4K3", "PP rank 3",
			EvalPawnValues::PASSED_PAWN_VALUE[2] + EvalPawnValues::ISOLATED_PAWN_PENALTY + EvalPawnValues::ADVANCED_PAWN_VALUE[2]);
		test.test("4k3/2pppppp/8/8/P7/8/2PPPPPP/4K3", "PP rank 4",
			EvalPawnValues::PASSED_PAWN_VALUE[3] + EvalPawnValues::ISOLATED_PAWN_PENALTY + EvalPawnValues::ADVANCED_PAWN_VALUE[3]);
		test.test("4k3/2pppppp/8/P7/8/8/2PPPPPP/4K3", "PP rank 5",
			EvalPawnValues::PASSED_PAWN_VALUE[4] + EvalPawnValues::ISOLATED_PAWN_PENALTY + EvalPawnValues::ADVANCED_PAWN_VALUE[4]);
		test.test("4k3/2pppppp/1p6/P7/8/8/2PPPPPP/4K3", "not passed rank 5",
			EvalPawnValues::ISOLATED_PAWN_PENALTY + EvalPawnValues::ADVANCED_PAWN_VALUE[4]);
		test.test("4k3/2pppppp/8/Pp6/8/8/2PPPPPP/4K3", "PP rank 5, adjacent pawn",
			EvalPawnValues::PASSED_PAWN_VALUE[4] + EvalPawnValues::ISOLATED_PAWN_PENALTY + EvalPawnValues::ADVANCED_PAWN_VALUE[4] -
			EvalPawnValues::ADVANCED_PAWN_VALUE[3]);
		test.test("4k3/2pppppp/8/P7/1p6/8/2PPPPPP/4K3", "PP rank 5, advanced pawn on adjacent file",
			EvalPawnValues::PASSED_PAWN_VALUE[4] + EvalPawnValues::ISOLATED_PAWN_PENALTY + EvalPawnValues::ADVANCED_PAWN_VALUE[4] -
			EvalPawnValues::ADVANCED_PAWN_VALUE[4]);
		test.test("4k3/2pppppp/8/P7/p7/8/P1PPPPPP/4K3 -", "PP rank 5, opponent on same file but behind",
			EvalPawnValues::PASSED_PAWN_VALUE[4] + EvalPawnValues::ISOLATED_PAWN_PENALTY + EvalPawnValues::ADVANCED_PAWN_VALUE[4] +
			EvalPawnValues::DOUBLE_PAWN_PENALTY - EvalPawnValues::ADVANCED_PAWN_VALUE[4] - EvalPawnValues::ISOLATED_PAWN_PENALTY);
		test.test("4k3/2pppppp/P7/8/8/8/2PPPPPP/4K3", "PP rank 6",
			EvalPawnValues::PASSED_PAWN_VALUE[5] + EvalPawnValues::ISOLATED_PAWN_PENALTY + EvalPawnValues::ADVANCED_PAWN_VALUE[5]);
		test.test("4k3/P1pppppp/8/8/8/8/2PPPPPP/4K3", "PP rank 7",
			EvalPawnValues::PASSED_PAWN_VALUE[6] + EvalPawnValues::ADVANCED_PAWN_VALUE[6]);

		test.test("4k3/2pppppp/8/8/8/8/PPPPPPPP/4K3", "Protected PP rank 2",
			EvalPawnValues::PASSED_PAWN_VALUE[1] + EvalPawnValues::ADVANCED_PAWN_VALUE[1]);
		test.test("4k3/2pppppp/8/8/8/P7/1PPPPPPP/4K3", "Protected PP rank 3",
			EvalPawnValues::PROTECTED_PASSED_PAWN_VALUE[2] + EvalPawnValues::ADVANCED_PAWN_VALUE[2]);
		test.test("4k3/2pppppp/8/8/P7/1P6/11PPPPPP/4K3", "Protected PP rank 4",
			EvalPawnValues::PROTECTED_PASSED_PAWN_VALUE[3] + EvalPawnValues::ADVANCED_PAWN_VALUE[3]);
		test.test("4k3/2pppppp/8/P7/1P6/8/11PPPPPP/4K3", "Protected PP rank 5",
			EvalPawnValues::PROTECTED_PASSED_PAWN_VALUE[4] + EvalPawnValues::ADVANCED_PAWN_VALUE[4] + EvalPawnValues::ADVANCED_PAWN_VALUE[3]);
		test.test("4k3/2pppppp/P7/1P6/8/8/11PPPPPP/4K3", "Protected PP rank 6",
			EvalPawnValues::PROTECTED_PASSED_PAWN_VALUE[5] + EvalPawnValues::ADVANCED_PAWN_VALUE[5] + EvalPawnValues::ADVANCED_PAWN_VALUE[4]);
		test.test("4k3/P1pppppp/1P6/8/8/8/11PPPPPP/4K3", "Protected PP rank 7",
			EvalPawnValues::PROTECTED_PASSED_PAWN_VALUE[6] + EvalPawnValues::ADVANCED_PAWN_VALUE[6] + EvalPawnValues::ADVANCED_PAWN_VALUE[5]);

		test.test("4k3/3ppppp/8/8/8/8/PPPPPPPP/4K3", "Connected PP rank 2",
			2 * EvalPawnValues::CONNECTED_PASSED_PAWN_VALUE[1] + EvalPawnValues::ADVANCED_PAWN_VALUE[1]);
		test.test("4k3/3ppppp/8/8/8/P7/1PPPPPPP/4K3", "Connected PP rank 3",
			EvalPawnValues::CONNECTED_PASSED_PAWN_VALUE[1] + EvalPawnValues::CONNECTED_PASSED_PAWN_VALUE[2] + EvalPawnValues::ADVANCED_PAWN_VALUE[2]);
		test.test("4k3/3ppppp/8/8/P7/1P6/11PPPPPP/4K3", "Connected PP rank 4",
			EvalPawnValues::CONNECTED_PASSED_PAWN_VALUE[2] + EvalPawnValues::CONNECTED_PASSED_PAWN_VALUE[3] + EvalPawnValues::ADVANCED_PAWN_VALUE[3]);
		test.test("4k3/3ppppp/8/P7/8/1P6/11PPPPPP/4K3", "Connected PP rank 5",
			EvalPawnValues::CONNECTED_PASSED_PAWN_VALUE[2] + EvalPawnValues::CONNECTED_PASSED_PAWN_VALUE[4] + EvalPawnValues::ADVANCED_PAWN_VALUE[4] + EvalPawnValues::ADVANCED_PAWN_VALUE[2]);
		test.test("4k3/3ppppp/P7/8/8/1P6/11PPPPPP/4K3", "Connected PP rank 6",
			EvalPawnValues::CONNECTED_PASSED_PAWN_VALUE[2] + EvalPawnValues::CONNECTED_PASSED_PAWN_VALUE[5] + EvalPawnValues::ADVANCED_PAWN_VALUE[5] + EvalPawnValues::ADVANCED_PAWN_VALUE[2]);
		test.test("4k3/PPpppppp/8/8/8/8/11PPPPPP/4K3", "Connected PP rank 7",
			2 * EvalPawnValues::CONNECTED_PASSED_PAWN_VALUE[6] + 2 * EvalPawnValues::ADVANCED_PAWN_VALUE[6]);
		test.measureRuntime();
	}

}

