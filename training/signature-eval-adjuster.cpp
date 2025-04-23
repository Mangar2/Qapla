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
            : signatureWin(PieceSignature::PIECE_SIGNATURE_SIZE * 8),
            signatureDraw(PieceSignature::PIECE_SIGNATURE_SIZE * 8),
            signatureLoss(PieceSignature::PIECE_SIGNATURE_SIZE * 8),
		    evalSum(PieceSignature::PIECE_SIGNATURE_SIZE * 8)
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
		
        const auto& indexVector = moveInfo.engine->computeEvalIndexVector();
        if (indexVector[0].name != "pieceSignature") {
            std::cerr << "Error: First index is not pieceSignature" << std::endl;
            return;
        }
		auto pieceIndex = indexVector[0].index;
                
        // The piece signature contains a 3-bit packed value represending a pawn difference in the piece signature from -3 to 3
        // The value 7 represents a pawn difference > abs(3). We ignore these cases in our statistic.
        int32_t pieceSignatureValueDifferencePacked = static_cast<int32_t>(pieceIndex % 8);
        if (pieceSignatureValueDifferencePacked == 7) return;

        int64_t currentSum = evalSum[pieceIndex];
        int64_t toAdd = static_cast<int64_t>(moveInfo.eval);
        if ((toAdd > 0 && currentSum > INT64_MAX - toAdd) ||
            (toAdd < 0 && currentSum < INT64_MIN - toAdd)) {
            std::cerr << "Overflow in evalSum for pieceIndex: " << pieceIndex << std::endl;
            return;
        }
		evalSum[pieceIndex] += toAdd;

        switch (moveInfo.result) {
        case QaplaInterface::GameResult::WHITE_WINS_BY_MATE:
            signatureWin[pieceIndex]++;
            break;
        case QaplaInterface::GameResult::BLACK_WINS_BY_MATE:
            signatureLoss[pieceIndex]++;
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
        PieceSignature debugIndex;
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
		std::vector<AdjustResult> resultTable = computeResultTable();
		writeResultTableAsCppHeader(resultTable, "EvalCorrection.h");
        std::cout << "Analysis finished." << std::endl;
    }

    /**
     * Computes the statistic for a given signature and its symmetric counterpart
     */
    std::vector<SignatureEvalAdjuster::AdjustResult> SignatureEvalAdjuster::computeResultTable() {
        std::vector<AdjustResult> resultTable(PieceSignature::PIECE_SIGNATURE_SIZE);
        constexpr int MAX_DEVIATION = 30;		 // max. derivation in centipawn to check consistence
        constexpr int TRUST_THRESHOLD = 1000;    // full usage of resultn
        constexpr int MIN_RELIABLE_TOTAL = 100;  // no input, if below
		constexpr int MIN_ADJUSTMENT = 5;	// minimum adjustment in centipawn
		constexpr int MIN_REL_ADJUSTMENT_DIVIDER = 10; // minimum relative adjustment in centipawn

        for (uint32_t wsig = 0; wsig < (1 << PieceSignature::SIG_SHIFT_BLACK); wsig++) {
            for (uint32_t bsig = 0; bsig < (1 << PieceSignature::SIG_SHIFT_BLACK); bsig++) {
                // We handle symmetric results in the inner calulation together
                if (wsig < bsig) continue;
				// We cannot adjust completely equal signatures
                if (wsig == bsig) continue;
                uint32_t sig = (bsig << PieceSignature::SIG_SHIFT_BLACK | wsig);
                uint32_t sym = (wsig << PieceSignature::SIG_SHIFT_BLACK | bsig);
                bool valueFound = false;

                for (uint32_t valueDeltaInPawn = 0; valueDeltaInPawn <= 3; valueDeltaInPawn++) {
                    uint32_t sigValue = (sig * 8 + valueDeltaInPawn + 3);
                    uint32_t symValue = (sym * 8 + (3 - valueDeltaInPawn));

                    const auto [statistic, evalAverage, total] = computeStatistic(sigValue, symValue);
                    if (total < MIN_RELIABLE_TOTAL) continue;

                    const value_t differenceInCentipawn = propToValue(statistic);
                    const value_t valueAdjustment = differenceInCentipawn - evalAverage;

                    double weight = std::clamp(
                        static_cast<double>(total - MIN_RELIABLE_TOTAL) / (TRUST_THRESHOLD - MIN_RELIABLE_TOTAL),
                        0.0, 1.0
                    );

                    // We apply weighting to avoid overfitting on low sample sizes
                    value_t weightedAdjustment = static_cast<value_t>(valueAdjustment * weight);

					// We select the lowest absolute value of the adjustment to avoid overfitting
                    if (!valueFound ||
                        (std::abs(weightedAdjustment) < std::abs(resultTable[sig].adjustment) && total >= TRUST_THRESHOLD)) {
                        resultTable[sig] = { weightedAdjustment, evalAverage, total };
                        resultTable[sym] = { -weightedAdjustment, -evalAverage, total };
                        valueFound = true;
                    }
                }
                /*
                // Shall we keep small adjustments?
                if (std::abs(resultTable[sig].adjustment) <= MIN_ADJUSTMENT || 
                    std::abs(resultTable[sig].adjustment) < std::abs(resultTable[sig].evalAverage / MIN_REL_ADJUSTMENT_DIVIDER)) {
                    resultTable[sig] = { 0, 0, 0 };
                    resultTable[sym] = { 0, 0, 0 };
                }
                */
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

} // namespace QaplaTraining
