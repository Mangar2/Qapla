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

#include <vector>
#include <string>
#include "../interface/ichessboard.h"

namespace QaplaTraining {

    struct MoveInfo;

    class SignatureEvalAdjuster {
    public:

        SignatureEvalAdjuster();

        /**
         * @brief Executes the analysis and builds the correction table
         * @param fenList List of all possible FEN strings
         * @param engine  Prototype chess engine
         * @param filePath Path to the binary game file
         */
        void run(const std::vector<std::string>& fenList,
            const QaplaInterface::IChessBoard* engine,
            const std::string& filePath);

        void computeFromFile(const std::string& filename, int32_t minAdjust);

        void saveToFile(const std::string& filename);

        void loadFromFile(const std::string& filename);

    private:
        struct AdjustResult {
            int32_t adjustment;
            int32_t evalAverage;
            int64_t total;
        };

        std::vector<int> ComputeCentipawnByWinProbability();
        /**
         * @brief Smooths the vector using a 1D bilateral filter
         * @param vec The vector to smooth
         * @param radius The radius of the smoothing window
         * @param sigmaSpace The spatial standard deviation for the bilateral filter
         */
        std::vector<int> smoothVector(
            const std::vector<int>& original, int radius, double sigmaSpace);

		uint64_t computeTotal(uint32_t sig) {
			if (sig >= signatureWin.size()) {
				std::cout << "Error: Signature out of range: " << sig << std::endl;
				return 0;
			}
			return signatureWin[sig] + signatureDraw[sig] + signatureLoss[sig];
		}
		std::tuple<int32_t, int32_t, int64_t> computeStatistic(uint32_t sig, uint32_t sym) {
			int64_t total = computeTotal(sig);
			int64_t evalSum = this->evalSum[sig];
			int32_t statistic = static_cast<int32_t>(signatureWin[sig] - signatureLoss[sig]);
			if (sig != sym) {
				evalSum -= this->evalSum[sym];
				statistic -= static_cast<int32_t>(signatureWin[sym] - signatureLoss[sym]);
				total += computeTotal(sym);
			}
			if (total == 0) {
                return { 0, 0, 0 };
			}
            int32_t evalAverage = static_cast<int32_t>(evalSum / total);
            return { static_cast<int32_t>(statistic * 100 / total), evalAverage, total };
		}

        std::vector<AdjustResult> computeResultTable(int32_t minAdjust);
        void writeResultTableAsCppHeader(const std::vector<AdjustResult>& resultTable, const std::string& filename);

        std::vector<int64_t> signatureWin;
        std::vector<int64_t> signatureDraw;
        std::vector<int64_t> signatureLoss;
        std::vector<int64_t> evalSum;
        std::vector<int64_t> valSum;
        std::vector<int64_t> valTotal;
        void onMove(const MoveInfo& moveInfo);
        void onFinish();
    };

} // namespace QaplaTraining