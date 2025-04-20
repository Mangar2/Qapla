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
#include "../basics/piecesignature.h"

using namespace std;

namespace QaplaTraining {

	class GamePairing;

	struct PieceSignatureStatistic {
		PieceSignatureStatistic()
			: signatureWin(QaplaBasics::PieceSignature::PIECE_SIGNATURE_SIZE * 8),
			signatureDraw(QaplaBasics::PieceSignature::PIECE_SIGNATURE_SIZE * 8),
			signatureLoss(QaplaBasics::PieceSignature::PIECE_SIGNATURE_SIZE * 8)
		{}

		void printResult();

		void saveToFile(const std::string& filename);

		void loadFromFile(const std::string& filename);

		void generateResultTableOnce();

	private:
		void applyFullBoardDamping(
			std::vector<int32_t>& resultTable,
			double fullBoardWeight,         // e.g. 0.5
			double materialOfTrust          // e.g. 0.6
		);
		uint64_t computeTotal(uint32_t sig) {
			if (sig >= signatureWin.size()) {
				std::cout << "Error: Signature out of range: " << sig << std::endl;
				return 0;
			}
			return signatureWin[sig] + signatureDraw[sig] + signatureLoss[sig];
		}
		void writeResultTableAsCppHeader(const std::vector<int32_t>& resultTable, const std::string& filename);
		std::vector<int32_t> computeResultTable();
		std::tuple<int32_t, int64_t> computeStatistic(uint32_t sig, uint32_t sym) {
			int64_t total = computeTotal(sig);
			int32_t statistic = static_cast<int32_t>(signatureWin[sig] - signatureLoss[sig]);
			if (sig != sym) {
				statistic -= static_cast<int32_t>(signatureWin[sym] - signatureLoss[sym]);
				total += computeTotal(sym);
			}
			if (total == 0) {
				return std::make_tuple(0, 0);
			}
			return std::make_tuple(static_cast<int32_t>(statistic * 100 / total), total);
		}
		int64_t computeWinAllSignatures(uint32_t value);
		int64_t computeTotalForPieceOnlySignature(uint32_t wsig, uint32_t bsig);
		void printSigResult(uint32_t sig, uint32_t sym);
		std::vector<int64_t> signatureWin;
		std::vector<int64_t> signatureDraw;
		std::vector<int64_t> signatureLoss;

		
	};

}
