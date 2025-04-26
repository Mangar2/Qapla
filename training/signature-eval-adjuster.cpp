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

#include <algorithm>
#include "signature-eval-adjuster.h"
#include "game-replay-engine.h"
#include "../basics/piecesignature.h"

namespace QaplaTraining {

    using namespace QaplaBasics;

    SignatureEvalAdjuster::SignatureEvalAdjuster()
        : signatureWin(PieceSignature::SIG_SIZE),
        signatureDraw(PieceSignature::SIG_SIZE),
        signatureLoss(PieceSignature::SIG_SIZE),
        evalSum(PieceSignature::SIG_SIZE),
        valSum(1000), valTotal(1000)
        {
        }

    void SignatureEvalAdjuster::run(const std::vector<std::string>& fenList,
        const QaplaInterface::IChessBoard* engine,
        const std::string& filePath) {
        GameReplayEngine replayEngine(engine, fenList);
        replayEngine.setMoveCallback([this](const MoveInfo& moveInfo) {
            this->onMove(moveInfo);
            });
        replayEngine.setFinishCallback([this]() {
            this->onFinish();
            });
        replayEngine.run(filePath);
    }

    void SignatureEvalAdjuster::onMove(const MoveInfo& moveInfo) {
        // Skip: we only want evaluation after the last move of a capture sequence,
        // to avoid transient eval noise from ongoing exchanges.
        if (!moveInfo.moveBeforeWasCapture || moveInfo.isCapture) return;

        // Ignore positions with search value result largely different to eval.
        // This avoids statistics from situation with compensation for missing material
        int32_t whiteValue = moveInfo.engine->isWhiteToMove() ? moveInfo.value : -moveInfo.value;
		int32_t absValue = std::min(std::abs(whiteValue), 999);
        //if (std::abs(whiteValue - moveInfo.eval) > 50) return;
		
        const auto& indexVector = moveInfo.engine->computeEvalIndexVector();
        if (indexVector[0].name != "pieceSignature") {
            std::cerr << "Error: First index is not pieceSignature" << std::endl;
            return;
        }
		auto pieceIndex = indexVector[0].index;
                
        int64_t currentSum = evalSum[pieceIndex];
        int64_t toAdd = static_cast<int64_t>(moveInfo.eval);
        if ((toAdd > 0 && currentSum > INT64_MAX - toAdd) ||
            (toAdd < 0 && currentSum < INT64_MIN - toAdd)) {
            std::cerr << "Overflow in evalSum for pieceIndex: " << pieceIndex << std::endl;
            return;
        }
		evalSum[pieceIndex] += toAdd;
        valTotal[absValue]++;

        switch (moveInfo.result) {
        case QaplaInterface::GameResult::WHITE_WINS_BY_MATE:
            signatureWin[pieceIndex]++;
            valSum[absValue] += whiteValue >= 0 ? 1 : -1;
            break;
        case QaplaInterface::GameResult::BLACK_WINS_BY_MATE:
            signatureLoss[pieceIndex]++;
			valSum[absValue] += whiteValue < 0 ? 1 : -1;
            break;
        case QaplaInterface::GameResult::NOT_ENDED:
            // Do nothing
            break;
        default:
            // Draw (several draw cases)
            signatureDraw[pieceIndex]++;
            break;
        }

        /*
        PieceSignature debugIndex(pieceIndex / 8);
        cout << debugIndex.toString() << " eval: " << moveInfo.eval << " value: " << moveInfo.value << std::endl;

        debugIndex.set("KQRRBBNPPPKQRRBNNPPP");
        auto index = pieceIndex / 8;
        if (index == debugIndex.getPiecesSignature()) {
            // Debugging 
            uint32_t symIndex = (6 -(pieceIndex & 0x7)) 
                | (((pieceIndex >> 3) & uint32_t(SignatureMask::ALL)) << (PieceSignature::SIG_SHIFT_BLACK + 3))
				| (((pieceIndex >> 3) & (uint32_t(SignatureMask::ALL) << PieceSignature::SIG_SHIFT_BLACK)) >> (PieceSignature::SIG_SHIFT_BLACK - 3));
            const auto [statistic, evalAverage, total] = computeStatistic(pieceIndex, symIndex);
            std::cout << debugIndex.toString() << ": " << moveInfo.eval << " avr: " << evalAverage
				<< " stat: " << statistic
                << " total: " << total << " " << moveInfo.engine->getFen() << std::endl;
        }
        */
    }

    void SignatureEvalAdjuster::onFinish() {
		saveToFile("signature-eval-adjuster.bin");
		std::vector<AdjustResult> resultTable = computeResultTable(1);
		writeResultTableAsCppHeader(resultTable, "EvalCorrection.h");
        std::cout << "Analysis finished." << std::endl;
    }

    void PrintVectorStats(const std::vector<int64_t>& valTotal, const std::vector<int64_t>& valSum) {
        constexpr int perLine = 10;
        constexpr int maxIndex = 999;

        std::cout << std::fixed << std::setprecision(3);

        for (int i = 0; i <= maxIndex; i += perLine) {
            for (int j = 0; j < perLine && i + j <= maxIndex; ++j) {
                int idx = i + j;
                if (valTotal[idx] > 0) {
                    double winRate = static_cast<double>(valSum[idx]) / valTotal[idx];
                    std::cout << "[" << idx << "] " << winRate << "  ";
                }
                else {
                    std::cout << "[" << idx << "] ---   ";
                }
            }
            std::cout << "\n";
        }
    }
    
    std::vector<int> SignatureEvalAdjuster::smoothVector(
        const std::vector<int>& original, int radius, double sigmaSpace) {

        std::vector<int> vec = original;
        const int n = static_cast<int>(vec.size());

        for (int i = 0; i < n; ++i) {
            const int adjustedRadius = std::min(radius, std::min(i, n - 1 - i));

            double sum = 0.0;
            double weightSum = 0.0;

            for (int r = -adjustedRadius; r <= adjustedRadius; ++r) {
                const int j = i + r;
                const double dist2 = r * r;

                const double weight = std::exp(-dist2 / (2.0 * sigmaSpace * sigmaSpace));

                sum += original[j] * weight;
                weightSum += weight;
            }

            vec[i] = static_cast<int>(std::round(sum / weightSum));
        }

        return vec;
    }

    std::vector<int> SignatureEvalAdjuster::ComputeCentipawnByWinProbability()
    {
        constexpr int probabilityBins = 101;
        constexpr int maxCentipawnIndex = 999;

        std::vector<int64_t> weightedSum(probabilityBins, 0);
        std::vector<int64_t> totalWeight(probabilityBins, 0);
        std::vector<int> result(probabilityBins, -1);

        for (int i = 0; i <= maxCentipawnIndex; ++i) {
            if (valTotal[i] == 0) continue;

            double winRate = static_cast<double>(valSum[i]) / valTotal[i];
            double binF = winRate * 100.0;
            int binLow = static_cast<int>(std::floor(binF));
            int binHigh = binLow + 1;

            double fracHigh = binF - binLow;
            double fracLow = 1.0 - fracHigh;

            if (binLow >= 0 && binLow < probabilityBins) {
                weightedSum[binLow] += static_cast<int64_t>(i * valTotal[i] * fracLow);
                totalWeight[binLow] += static_cast<int64_t>(valTotal[i] * fracLow);
            }

            if (binHigh >= 0 && binHigh < probabilityBins) {
                weightedSum[binHigh] += static_cast<int64_t>(i * valTotal[i] * fracHigh);
                totalWeight[binHigh] += static_cast<int64_t>(valTotal[i] * fracHigh);
            }
        }

        for (int bin = 0; bin < probabilityBins; ++bin) {
            if (totalWeight[bin] > 0) {
                result[bin] = static_cast<int>(weightedSum[bin] / totalWeight[bin]);
            }
        }

        for (size_t i = 0; i < result.size(); ++i) {
            std::cout << "Centipawn " << i << ": " << result[i] << std::endl;
        }

		auto smoothed = smoothVector(result, 5, 2.0);

        for (size_t i = 0; i < smoothed.size(); ++i) {
            std::cout << "Centipawn " << i << ": " << smoothed[i] << std::endl;
        }
        return smoothed;
    }

    /**
     * Computes the statistic for a given signature and its symmetric counterpart
     */
    std::vector<SignatureEvalAdjuster::AdjustResult> SignatureEvalAdjuster::computeResultTable(int32_t minAdjust) {
        std::vector<AdjustResult> resultTable(PieceSignature::SIG_SIZE);
        constexpr int MAX_DEVIATION = 30;		 // max. derivation in centipawn to check consistence
        constexpr int TRUST_THRESHOLD = 1000;    // full usage of resultn
        constexpr int MIN_RELIABLE_TOTAL = 100;  // no input, if below
		constexpr int MIN_ADJUSTMENT = 5;	// minimum adjustment in centipawn
		constexpr int MIN_REL_ADJUSTMENT_DIVIDER = 10; // minimum relative adjustment in centipawn
		constexpr int MAX_EVAL_VALUE = 800; // max. eval value to check, larger values indicates endgame eval bonus functions
		// PrintVectorStats(valTotal, valSum);
		std::vector<int> centipawnByWinProbability = ComputeCentipawnByWinProbability();

        for (uint32_t wsig = 0; wsig < (1 << PieceSignature::SIG_SHIFT_BLACK); wsig++) {
            for (uint32_t bsig = 0; bsig < (1 << PieceSignature::SIG_SHIFT_BLACK); bsig++) {
                // We handle symmetric results in the inner calulation together
                if (wsig < bsig) continue;
				// We cannot adjust completely equal signatures
                if (wsig == bsig) continue;
                uint32_t sig = (bsig << PieceSignature::SIG_SHIFT_BLACK | wsig);
                uint32_t sym = (wsig << PieceSignature::SIG_SHIFT_BLACK | bsig);

                const auto [statistic, evalAverage, total] = computeStatistic(sig, sym);
                if (total < MIN_RELIABLE_TOTAL) continue;
                if (std::abs(evalAverage) > MAX_EVAL_VALUE) continue;


                const value_t differenceInCentipawn = propToValue(statistic);
                const value_t valueAdjustment = differenceInCentipawn - evalAverage;

                double weight = std::clamp(
                    static_cast<double>(total - MIN_RELIABLE_TOTAL) / (TRUST_THRESHOLD - MIN_RELIABLE_TOTAL),
                    0.0, 1.0
                );

                // We apply weighting to avoid overfitting on low sample sizes
                value_t weightedAdjustment = static_cast<value_t>(valueAdjustment * weight);
				if (weightedAdjustment <= minAdjust) {
					continue;
				}
                resultTable[sig] = { weightedAdjustment, evalAverage, total };
                resultTable[sym] = { -weightedAdjustment, -evalAverage, total };
            }
        }
        return resultTable;
    }


    void SignatureEvalAdjuster::writeResultTableAsCppHeader(const std::vector<AdjustResult>& resultTable, const std::string& filename) {
        std::ofstream out(filename);
        if (!out) {
            throw std::runtime_error("Unable to open file: " + filename);
        }

        out << "#pragma once" << std::endl;
        out << "// Auto-generated evaluation table" << std::endl;
        out << "#include <array>" << std::endl;
        out << "#include <cstdint>" << std::endl;
        out << "#include \"PieceSignature.h\"" << std::endl << std::endl;

        out << "static inline std::array<int16_t, QaplaBasics::PieceSignature::PIECE_SIGNATURE_SIZE> EVAL_CORRECTION = []() {" << std::endl;
        out << "    std::array<int16_t, QaplaBasics::PieceSignature::PIECE_SIGNATURE_SIZE> result{};" << std::endl;

        for (size_t i = 0; i < resultTable.size(); ++i) {
            if (resultTable[i].adjustment != 0) {
                out << "    result[" << i << "] = " << resultTable[i].adjustment << ";"
                    << " // " << PieceSignature(static_cast<pieceSignature_t>(i)).toString() 
					<< " ( average: " << resultTable[i].evalAverage << " total: " << resultTable[i].total << ")"
                    << std::endl;
            }
        }

        out << "    return result;" << std::endl;
        out << "}();" << std::endl;
        out << std::endl;
    }

    void SignatureEvalAdjuster::saveToFile(const std::string& filename) {
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
		writeMap(evalSum);
        writeMap(valSum);
        out.close();
    }

    void SignatureEvalAdjuster::loadFromFile(const std::string& filename) {
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
		readMap(evalSum);
        in.close();
    }

	void SignatureEvalAdjuster::computeFromFile(const std::string& filename, int32_t minAdjust) {
		loadFromFile(filename);
		auto resultTable = computeResultTable(minAdjust);
		writeResultTableAsCppHeader(resultTable, "EvalCorrection.h");
		std::cout << "Result table generated and saved to EvalCorrection.h" << std::endl;
	}

} // namespace QaplaTraining
