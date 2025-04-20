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
 * @Overview
 * Implements chess board evaluation for end games. 
 * Returns +100, if white is one pawn up
 */
#pragma once

#include <string>
#include <vector>
#include <array>
#include "../basics/evalvalue.h"
#include "../basics/bits.h"
#include "../movegenerator/movegenerator.h"

using namespace QaplaBasics;

namespace ChessEval {



    class PieceSignatureLookup {
    public:

        struct Entry {
            int wPawns;
            int bPawns;
            int winProbPercent;

            constexpr bool operator==(const Entry&) const = default; // erlaubt Vergleich
        };

        struct EntryGroup {
            int evalPawnOffset;
            std::initializer_list<Entry> entries;
        };

        /**
		 * Initializes the signature lookup table with the given entries.
         */
        constexpr PieceSignatureLookup(std::initializer_list<EntryGroup> groups) {
            for (auto g = groups.begin(); g != groups.end(); ++g) {
                applyGroup(*g);

                if (g == groups.end() - 1) {
                    applyExtendedGroup(*g);
                }
            }
        }

        constexpr PieceSignatureLookup() = default;

        /**
		 * Lookup a value based on the number of pawns for both colors.
		 * @param board The current board state.
		 * @return The value associated with the given pawn counts.
         */
        value_t lookup(const QaplaMoveGenerator::MoveGenerator& board) const {
            const value_t wPawn = popCount(board.getPieceBB(PAWN + WHITE));
            const value_t bPawn = popCount(board.getPieceBB(PAWN + BLACK));
            const size_t index = static_cast<size_t>(bPawn) * 256 + static_cast<size_t>(wPawn);
            return table[index];
        }

    protected:
        /**
         * Applies all entries in a single group to the lookup table.
         * The last entry is interpreted as a symmetric range seed.
         * @param group The entry group containing pawn probabilities for a fixed material offset.
         */
        constexpr void applyGroup(const EntryGroup& group) {
            const auto& list = group.entries;
            const Entry* begin = list.begin();
            const Entry* end = list.end();

            for (const Entry* it = begin; it != end - 1; ++it) {
                setSymmetricEntry(it->wPawns, it->bPawns, group.evalPawnOffset, it->winProbPercent);
            }

            const Entry& range = *(end - 1);
            setSymmetricRange(range.wPawns, range.bPawns, range.winProbPercent, group.evalPawnOffset);
        }

        /**
         * Applies an extended version of the last group (same logic as applyGroup).
         * This is the entry point for future group-specific logic.
         * @param group The entry group to apply.
         */
        constexpr void applyExtendedGroup(const EntryGroup& group) {

            const auto& list = group.entries;
            const Entry* begin = list.begin();
            const Entry* end = list.end();

            for (int deltaBlack = -1; deltaBlack >= -8; --deltaBlack) {
                bool anyInserted = false;

                for (const Entry* it = begin; it != end - 1; ++it) {
                    const int b = it->bPawns + deltaBlack;
                    if (b >= 0) {
                        setSymmetricEntry(it->wPawns, b, group.evalPawnOffset, it->winProbPercent);
                        anyInserted = true;
                    }
                }

                const Entry& range = *(end - 1);
                const int b = range.bPawns + deltaBlack;
                if (b >= 0) {
                    setSymmetricRange(range.wPawns, b, range.winProbPercent, group.evalPawnOffset);
                    anyInserted = true;
                }

                if (!anyInserted) {
                    break; // nichts mehr gültig – abbrechen
                }
            }
        }

        /**
         * Sets an evaluation entry in the table for black and white view
		 * @param wPawns Number of white pawns
		 * @param bPawns Number of black pawns
		 * @param evalPawnOffset the piece weight difference of the position (based on pawn = 1, bishop&knight = 3, rook = 5, queen = 9)
		 * @param winProbPercent The computed winning probability in percent (0-100)
		 * @return The table is updated with the given values.
         */
        constexpr void setSymmetricEntry(int wPawns, int bPawns, int evalPawnOffset, int winProbPercent) {
            const value_t rawEval = propToValue(winProbPercent);
            // The pawn offset represents the average winning probability and must be subtracted, because
            // this is only to correct existing evaluation. We need the delta between material based winning
            // propability and computed winning propability
            const value_t correctedEval = rawEval - evalPawnOffset * 100;

            const size_t idxWhite = static_cast<size_t>(bPawns) * 256 + static_cast<size_t>(wPawns);
            const size_t idxBlack = static_cast<size_t>(wPawns) * 256 + static_cast<size_t>(bPawns);

            table[idxWhite] = correctedEval;
            table[idxBlack] = -correctedEval;
        }

        /**
         * Sets multiple symmetric evaluation entries for all (wPawns + offset, bPawns + offset) combinations
         * as long as both remain in [0,8]. The value difference remains constant across all.
         * @param wStart Minimum number of white pawns.
         * @param bStart Minimum number of black pawns.
         * @param winProbPercent The winning probability for the starting combination (in percent, -100 to 100).
         * @param evalPawnOffset Material imbalance in pawn units (same for all affected entries).
         */
        constexpr void setSymmetricRange(int wStart, int bStart, int winProbPercent, int evalPawnOffset) {
            int w = wStart;
            int b = bStart;
            while (w <= 8 && b <= 8) {
                setSymmetricEntry(w, b, evalPawnOffset, winProbPercent);
                ++w;
                ++b;
            }
        }
    private:
        static constexpr size_t TableSize = 256 * 256;
        std::array<value_t, TableSize> table{};
    };

    struct NamedSignatureLookup {
        const char* name;
        pieceSignature_t id;
        PieceSignatureLookup lookup;
    };
}
