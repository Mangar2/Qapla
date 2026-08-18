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
 * The engine side of the tablebase interface: the settings, and the one guard
 * every caller asks before probing.
 *
 * The format layer in tbprobe.h finds and maps the files and knows nothing about
 * the engine. What is kept here is what it must not know - the option values and
 * a question that needs a board.
 */

#pragma once

#include <iostream>

#include "tbprobe.h"
#include "../../interface/uci-option-provider.h"
#include "../../movegenerator/movegenerator.h"

namespace QaplaSyzygy {

	class Tablebase : public QaplaInterface::UciOptionProvider {
	public:
		static Tablebase& getUciAccess() {
			static Tablebase instance;
			return instance;
		}

		std::vector<QaplaInterface::UciOption> getUciOptions() const override {
			using QaplaInterface::UciOption;
			return {
				UciOption::string("SyzygyPath"),
				UciOption::spin("SyzygyProbeDepth", DEFAULT_PROBE_DEPTH, 1, 100),
				UciOption::check("Syzygy50MoveRule", DEFAULT_USE_RULE_50),
				UciOption::spin("SyzygyProbeLimit", TB_MAX_PIECES, 0, TB_MAX_PIECES)
			};
		}

		bool setUciOption(const std::string& name, const std::string& value) override {

			if (name == "SyzygyPath") {
				_path = value;
				const LoadResult loaded = QaplaSyzygy::setPath(_path);
				std::cout << "info string tablebases: " << loaded.wdlFiles << " win/draw/loss and "
					<< loaded.dtzFiles << " distance files, up to " << loaded.maxCardinality
					<< " pieces" << std::endl;
				return true;
			}

			if (name == "SyzygyProbeDepth") {
				_probeDepth = QaplaInterface::uciValueToInt(value, DEFAULT_PROBE_DEPTH);
				return true;
			}

			if (name == "Syzygy50MoveRule") {
				_useRule50 = QaplaInterface::uciValueToBool(value);
				return true;
			}

			if (name == "SyzygyProbeLimit") {
				_probeLimit = QaplaInterface::uciValueToInt(value, TB_MAX_PIECES);
				return true;
			}

			return false;
		}

		/**
		 * Frees the mapped files between games and registers the same path again,
		 * quietly. Dropping the path here instead would leave the next game without
		 * tables until the GUI happens to set the option once more.
		 */
		static void newGame() {
			const Tablebase& self = getUciAccess();
			if (self._path.empty()) return;
			QaplaSyzygy::setPath(self._path);
		}

		/** Frees everything, including the path. */
		static void release() {
			getUciAccess()._path.clear();
			QaplaSyzygy::release();
		}

		/**
		 * Highest piece count that may be probed: what was loaded, capped by the
		 * configured limit. Zero when no tables are available, which lets every
		 * caller drop out on a single comparison.
		 */
		static uint32_t cardinality() {
			const Tablebase& self = getUciAccess();
			const uint32_t loaded = QaplaSyzygy::maxCardinality();
			return std::min(loaded, uint32_t(std::max(self._probeLimit, 0)));
		}

		static int32_t probeDepth() { return getUciAccess()._probeDepth; }
		static bool useRule50() { return getUciAccess()._useRule50; }

		/**
		 * Whether the tables may be asked about this position at all.
		 *
		 * It does not answer whether the table for this exact material exists - a
		 * lookup for a missing one simply reports no table. Castling rights rule a
		 * position out because the format does not model them, and they only ever
		 * decrease, so no descendant can bring one back.
		 */
		static bool isProbeable(const QaplaMoveGenerator::MoveGenerator& position) {
			const uint32_t limit = cardinality();
			if (limit == 0) return false;
			if (uint32_t(QaplaBasics::popCount(position.getAllPiecesBB())) > limit) return false;
			return position.getBoardState().getCastlingRightsMask() == 0;
		}

	private:
		static constexpr int32_t DEFAULT_PROBE_DEPTH = 1;
		static constexpr bool    DEFAULT_USE_RULE_50 = true;

		std::string _path;
		int32_t     _probeDepth = DEFAULT_PROBE_DEPTH;
		bool        _useRule50 = DEFAULT_USE_RULE_50;
		int32_t     _probeLimit = TB_MAX_PIECES;
	};

}
