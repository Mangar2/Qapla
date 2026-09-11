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
 * @copyright Copyright (c) 2025 Volker Böhm
 * @Overview
 * The bridge from a generated Qapla bitbase to a Syzygy win/draw/loss file.
 *
 * Two directions meet here and nowhere else: the generator's index, value set and
 * point of view on one side, the format's on the other. src/syzygy stays free of
 * the engine, bitbase/ stays free of the format - this file knows both.
 */

#pragma once

#include <cstdint>
#include <ostream>
#include <string>

namespace QaplaBitbase {

	/**
	 * Writes the generated bitbase of one material as a Syzygy .rtbw file.
	 *
	 * Reads the values from the Re-Pair file the generator wrote, walks the whole
	 * Qapla index space, and puts every value in the slot the format's index gives
	 * it. Slots no legal position reaches stay unset and are filled by the writer.
	 *
	 * @param pieceString material, in the generator's spelling ("KRK")
	 * @param qwdlFile    the .qwdl file of that material
	 * @param outDir      directory the .rtbw file is written to
	 * @returns false when a step failed; the reason is written to log
	 */
	bool writeSyzygyWdl(const std::string& pieceString, const std::string& qwdlFile,
		const std::string& outDir, std::ostream& log);

	/**
	 * Compares two sets of Syzygy files over every legal position of one material.
	 *
	 * Both answers are resolved values, not raw entries: an entry is a lower bound
	 * wherever a capture reaches the true value, so the captures have to be played
	 * out on both sides before anything is compared.
	 *
	 * The 50 move rule is folded away - a cursed win counts as a win and a blessed
	 * loss as a loss - because the generator does not know the rule yet. What is
	 * compared is therefore the sign, and that has to match exactly.
	 *
	 * @param pieceString material, in the generator's spelling ("KRK")
	 * @param ourDir      directory holding the file written here
	 * @param refDir      directory holding the reference files
	 * @returns true when no position differs
	 */
	bool compareSyzygyWdl(const std::string& pieceString, const std::string& ourDir,
		const std::string& refDir, const std::string& qwdlFile, std::ostream& log);


	/**
	 * Measures the cost of a probe against our files and against a reference set.
	 *
	 * The same random positions in the same order for both, drawn from a fixed seed,
	 * and the stored entry alone - the capture resolution above it is engine work and
	 * would only add the same constant to both sides.
	 *
	 * @param amount number of positions to probe
	 * @returns false when our files are measurably slower than the reference
	 */
	bool measureSyzygySpeed(const std::string& pieceString, const std::string& ourDir,
		const std::string& refDir, uint64_t amount, std::ostream& log);

}
