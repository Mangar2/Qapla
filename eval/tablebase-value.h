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
 * Turns a tablebase answer into an evaluation value.
 *
 * The answer is added to the current evaluation instead of replacing it, so the
 * gradient of the evaluation survives underneath - king proximity, pawn advance
 * and the rest. That gradient is what actually converts a won ending; a flat
 * constant would tell the engine that it wins but not how to make progress.
 *
 * The tables know five outcomes, not three. A cursed win is won on the board but
 * drawn under the fifty move rule, so it is not worth a winning bonus - it is
 * worth about a pawn, because the opponent still has to defend it correctly.
 * The mirrored case is a blessed loss.
 */

#pragma once

#include "../basics/evalvalue.h"
#include "../basics/materialbalance.h"
#include "../src/syzygy/tbprobe.h"

namespace ChessEval {

	using QaplaBasics::value_t;
	using QaplaBasics::DRAW_VALUE;
	using QaplaBasics::WINNING_BONUS;
	using QaplaBasics::MIN_MATE_VALUE;
	using QaplaBasics::NON_MATE_VALUE_LIMIT;

	/**
	 * Value of a position a cursed win or blessed loss leads to, used by the distance
	 * mapping. The win/draw/loss mapping scores them as plain draws instead.
	 */
	constexpr value_t TB_CURSED_BONUS = QaplaBasics::MaterialBalance::PAWN_VALUE_EG;

	/** Plies of headroom reserved for distance to zero values. */
	constexpr value_t TB_DTZ_RANGE = 1000;

	/** Lowest value a tablebase win can take, so the whole band stays below a real mate. */
	constexpr value_t TB_WIN_VALUE = MIN_MATE_VALUE - TB_DTZ_RANGE;

	/**
	 * Maps a win/draw/loss answer, seen from the side to move, to a search value.
	 *
	 * A win is one constant, not the evaluation plus a bonus. The tables say "won" and
	 * nothing about how to convert it, and a run of 20000 games could not separate a
	 * mapping that keeps the evaluation underneath from this one. So the shape that
	 * cannot go wrong is the one kept: a
	 * constant cannot leave its band, and it cannot pick up NO_VALUE from a position that
	 * was never evaluated because its side to move stood in check.
	 *
	 * The band sits below the distance band, which sits below the mate band, so a table
	 * win always loses to a forced mate and to a shorter measured distance.
	 */
	inline value_t tablebaseWdlToValue(QaplaSyzygy::Wdl wdl) {
		switch (wdl) {
		case QaplaSyzygy::Wdl::Win:         return TB_WIN_VALUE;
		case QaplaSyzygy::Wdl::Loss:        return -TB_WIN_VALUE;
		case QaplaSyzygy::Wdl::Draw:        return DRAW_VALUE;
		case QaplaSyzygy::Wdl::CursedWin:   return DRAW_VALUE;
		case QaplaSyzygy::Wdl::BlessedLoss: return DRAW_VALUE;
		}
		return DRAW_VALUE;
	}

	/** From this distance on the outcome is drawn under the fifty move rule. */
	constexpr int32_t TB_CURSED_FROM = 100;

	static_assert(TB_WIN_VALUE > NON_MATE_VALUE_LIMIT,
		"the distance band has to stay above the ordinary evaluation range");

	/**
	 * Maps a distance to zero answer, seen from the side to move, to a value.
	 *
	 * Unlike the win/draw/loss mapping this one does **not** build on the evaluation.
	 * The evaluation varies by hundreds while the distance contributes at most a
	 * hundred, so adding the two would drown the distance and make it worthless -
	 * and the distance is the only thing here that says how to make progress.
	 *
	 * Distance answers therefore live in a band of their own, one thousand plies wide
	 * and entirely below the mate band, so a shorter distance always beats a longer
	 * one and a real mate always beats both. Beyond a hundred plies the outcome is
	 * drawn under the fifty move rule and drops out of the band: it is worth about a
	 * pawn, because the opponent still has to defend it correctly.
	 *
	 * @param distance plies to the next zeroing move, the sign gives the outcome
	 */
	inline value_t tablebaseDtzToValue(int32_t distance) {

		if (distance == 0) return DRAW_VALUE;

		const int32_t plies = distance > 0 ? distance : -distance;

		if (plies > TB_CURSED_FROM) {
			return distance > 0 ? TB_CURSED_BONUS : -TB_CURSED_BONUS;
		}

		const value_t value = MIN_MATE_VALUE - 1 - value_t(plies);
		return distance > 0 ? value : -value;
	}

}
