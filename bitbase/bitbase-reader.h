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
 * Reads all bitbases to memory
 *
 */

#pragma once

#include <map>
#include <filesystem>
#include "piecelist.h"
#include "bitbase.h"


namespace QaplaBitbase {
    /**
     * Represents the result of a bitbase query.
     * This enum may not be changed as it represents the actual bit-coding of the file. 
     */
    enum class Result {
        Unknown = 0,       ///< Result is unknown or bitbase unavailable
        Win = 1,
        Loss = 2,
        Draw = 3,
        DrawOrLoss = 3   ///< Used for legacy 1-bit bitbases where draw and loss are not distinguished
    };

    static constexpr array<const char*, 4> ResultMap{ "Unknown", "Win", "Loss", "Draw" };

    /**
     * Class for loading, managing and querying endgame bitbases.
     */
    class BitbaseReader
    {
    public:

		/**
		 * @brief Sets the path to the bitbase directory.
		 * @param path Path to the bitbase directory.
		 * @return True if the path is valid and set successfully.
		 */
        static bool setBitbasePath(const std::string& path);

        /** Loads all relevant bitbases. */
        static std::vector<std::string> loadBitbase();
        static void registerBitbaseFromHeader();

        /**
         * Registers a bitbase using embedded header data.
         * @param pieceString A string describing the piece configuration.
         * @param data Array of bitbase data.
         * @param sizeInBytes Size of data in bytes.
         */
        static void registerBitbaseFromHeader(std::string pieceString, const uint32_t data[], uint32_t sizeInBytes);

        /**
         * Recursively loads bitbases based on wildcard piece string.
         * @param name Piece string with optional '*' wildcards.
         * @param force If true, reloads even if bitbase is already present.
         */
        static vector<std::string> loadBitbaseRec(std::string name, bool force = false);

        /**
         * Queries a single subordinate bitbase for a position reachable via a capture or promotion.
         * Returns Win, Loss, or Draw when the bitbase uses 2-bit encoding; falls back to Win/DrawOrLoss
         * for legacy 1-bit bitbases. Returns Unknown when no matching bitbase is available.
         * Results are expressed from white's perspective.
         * @param position Position to query.
         * @return Bitbase result from white's perspective.
         */
        static Result getValueFromSingleBitbase(const MoveGenerator& position);

        /**
         * Queries a bitbase from both white and black perspectives.
         * @param position Position to query.
         * @return Bitbase result depending on side to move.
         */
        static Result getValueFromBitbase(const MoveGenerator& position);

        /**
         * Queries bitbase and applies score adjustment based on result.
         * @param position Position to query.
         * @param currentValue Current evaluation score.
         * @return Adjusted evaluation score.
         */
        static value_t getValueFromBitbase(const MoveGenerator& position, value_t currentValue);

        /**
         * Loads a bitbase from file.
         * @param pieceString Piece configuration string.
		 * @param onlyHeader If true, only loads header information.
         */
        static void loadBitbase(std::string pieceString, bool onlyHeader);

        /**
         * Checks if a bitbase is available.
         * @param pieceString Piece configuration string.
         * @return True if bitbase is loaded.
         */
        static bool isBitbaseAvailable(std::string pieceString);

        /**
         * Manually sets a bitbase.
         * @param pieceString Piece configuration string.
         * @param bitBase Bitbase instance to set.
         */
        static void setBitbase(std::string pieceString, const Bitbase& bitBase);

    private:

        /**
         * Returns a loaded bitbase for a piece signature.
         * @param signature Signature of piece configuration.
         * @return Pointer to Bitbase or nullptr if unavailable.
         */
        static Bitbase* getBitbase(PieceSignature signature);

        BitbaseReader();
        static inline std::map<pieceSignature_t, Bitbase> _bitbases;
        static inline std::filesystem::path bitbasePath;
    };

}

