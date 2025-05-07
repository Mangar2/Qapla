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
#include <fstream>
#include <unordered_map>
#include "../basics/piecesignature.h"
#include "piece-signature-statistic.h"

using namespace QaplaBasics;

namespace QaplaTraining {

	void PieceSignatureStatistic::saveToFile(const std::string& filename) {
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

	void PieceSignatureStatistic::loadFromFile(const std::string& filename) {
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

	void PieceSignatureStatistic::generateResultTableOnce() {
		PieceSignatureStatistic stat;
		stat.loadFromFile("result.bin");
		auto table = stat.computeResultTable();
		applyFullBoardDamping(table, 0.5, 0.6);
		writeResultTableAsCppHeader(table, "ResultTable.h");
		std::cout << "Result table generated and saved to ResultTable.h" << std::endl;
	}

	int64_t PieceSignatureStatistic::computeWinAllSignatures(uint32_t value) {
		int64_t total = 0;
		int64_t win = 0;
		for (uint32_t sig = 0; sig < PieceSignature::SIG_SIZE; sig++) {
			uint32_t vSig = sig * 8 + value + 3;
			uint32_t vSigSym = sig * 8 + (3 - value);
			total += computeTotal(vSig) + computeTotal(vSigSym);
			win += signatureWin[vSig] - signatureWin[vSigSym] - signatureLoss[vSig] + signatureLoss[vSigSym];
		}
		return (win) * 100 / total;
	}

	int64_t PieceSignatureStatistic::computeTotalForPieceOnlySignature(uint32_t wsig, uint32_t bsig) {
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

	/**
	 * Computes the statistic for a given signature and its symmetric counterpart
	 */
	std::vector<int32_t> PieceSignatureStatistic::computeResultTable() {
		std::vector<int32_t> resultTable(PieceSignature::SIG_SIZE);
		constexpr int MAX_DEVIATION = 30;		 // max. derivation in centipawn to check consistence
		constexpr int TRUST_THRESHOLD = 1000;    // full usage of resultn
		constexpr int MIN_RELIABLE_TOTAL = 100;  // no input, if below

		for (uint32_t wsig = 0; wsig < (1 << PieceSignature::SIG_SHIFT_BLACK); wsig++) {
			for (uint32_t bsig = 0; bsig < (1 << PieceSignature::SIG_SHIFT_BLACK); bsig++) {
				if (wsig < bsig) {
					// We handle symmetric results in the inner calulation together
					continue;
				}
				uint32_t sig = (bsig << PieceSignature::SIG_SHIFT_BLACK | wsig);
				uint32_t sym = (wsig << PieceSignature::SIG_SHIFT_BLACK | bsig);
				bool valueFound = false;
				for (uint32_t value = 0; value <= 3; value++) {
					uint32_t sigValue = (sig * 8 + value + 3);
					uint32_t symValue = (sym * 8 + (3 - value));
					const auto [statistic, total] = computeStatistic(sigValue, symValue);
					if (total < MIN_RELIABLE_TOTAL) {
						continue;
					}
					const value_t differenceInCentipawn = propToValue(statistic);
					const value_t valueAdjustment = differenceInCentipawn - value * 100;

					double weight = std::clamp(
						static_cast<double>(total - MIN_RELIABLE_TOTAL) / (TRUST_THRESHOLD - MIN_RELIABLE_TOTAL),
						0.0, 1.0
					);
					value_t weightedAdjustment = static_cast<value_t>(valueAdjustment * weight);

					if (!valueFound || 
						(std::abs(weightedAdjustment) < std::abs(resultTable[sig]) && total >= TRUST_THRESHOLD)) {
						resultTable[sig] = weightedAdjustment;
						resultTable[sym] = -weightedAdjustment;
						valueFound = true;
					}
				}
			}
		}
		return resultTable;
	}

	void PieceSignatureStatistic::applyFullBoardDamping(
		std::vector<int32_t>& resultTable,
		double fullBoardWeight,         
		double materialOfTrust          
	) {
		constexpr value_t MAX_MATERIAL = 64; // 32 per side
		const value_t trustThreshold = static_cast<value_t>(materialOfTrust * MAX_MATERIAL);

		for (uint32_t sig = 0; sig < PieceSignature::SIG_SIZE; ++sig) {
			PieceSignature ps(sig);
			value_t materialWhite = ps.getStaticPiecesValue<WHITE>();
			value_t materialBlack = ps.getStaticPiecesValue<BLACK>();
			value_t totalMaterial = materialWhite + materialBlack;

			if (totalMaterial <= trustThreshold)
				continue;

			double invertedReductionRatio = static_cast<double>(totalMaterial - trustThreshold) / (MAX_MATERIAL - trustThreshold);
			double reduction = invertedReductionRatio * (1.0 - fullBoardWeight);
			double weight = 1.0 - reduction;

			resultTable[sig] = static_cast<int32_t>(resultTable[sig] * weight);
		}
	}

	void PieceSignatureStatistic::writeResultTableAsCppHeader(const std::vector<int32_t>& resultTable, const std::string& filename) {
		std::ofstream out(filename);
		if (!out) {
			throw std::runtime_error("Unable to open file: " + filename);
		}

		out << "#pragma once" << std::endl;
		out << "// Auto-generated evaluation table" << std::endl;
		out << "#include <array>" << std::endl;
		out << "#include <cstdint>" << std::endl;
		out << "#include \"PieceSignature.h\"" << std::endl << std::endl;

		out << "inline std::array<int32_t, " << resultTable.size() << "> createResultTable() {" << std::endl;
		out << "    std::array<int32_t, " << resultTable.size() << "> result{};" << std::endl;

		for (size_t i = 0; i < resultTable.size(); ++i) {
			if (resultTable[i] != 0) {
				out << "    result[" << i << "] = " << resultTable[i] << ";"
					<< " // " << PieceSignature(static_cast<pieceSignature_t>(i)).toString() << std::endl;
			}
		}

		out << "    return result;" << std::endl;
		out << "}" << std::endl;
		out << std::endl;
		out << "static inline std::array<int32_t, " << resultTable.size() << "> RESULT_TABLE = createResultTable();" << std::endl;
	}


	void PieceSignatureStatistic::printSigResult(uint32_t sig, uint32_t sym) {
		const auto [statistic, total] = computeStatistic(sig, sym);
		std::cout << statistic << "% (" << total << ") ";
	}
	void PieceSignatureStatistic::printResult() {
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
							const auto [statistic, total] = computeStatistic(sigWithValue, symSigWithValue);
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

	
}