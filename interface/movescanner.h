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
 * Scans a move string
 */

#pragma once

#include <string>
#include <string_view>
#include <cstdint>
#include <algorithm>

namespace QaplaInterface {

    constexpr std::string_view kPieceChars = "NnBbRrQqKk";

    constexpr bool isPieceChar(char pieceChar) noexcept {
        return kPieceChars.find(pieceChar) != std::string_view::npos;
    }

    constexpr bool isCastleNotationChar(char ch) noexcept {
        return ch == '0' || ch == 'O';
    }

    constexpr bool isCheckSign(char ch) noexcept {
        return ch == '+';
    }

    constexpr bool isMateSign(char ch) noexcept {
        return ch == '#';
    }

    constexpr bool isPromoteChar(char ch) noexcept {
        return ch == '=';
    }

    constexpr bool isRankChar(char rank) noexcept {
        return rank >= '1' && rank <= '8';
    }

    constexpr bool isFileChar(char file) noexcept {
        return file >= 'a' && file <= 'h';
    }

    constexpr bool isCaptureChar(char capture) noexcept {
        return capture == 'x' || capture == ':';
    }

    constexpr uint32_t charToRank(char rank) noexcept {
        return static_cast<uint32_t>(rank - '1');
    }

    constexpr uint32_t charToFile(char file) noexcept {
        return static_cast<uint32_t>(file - 'a');
    }

    class MoveScanner {
    public:
        /**
         * Constructs a MoveScanner and attempts to parse the given move string.
         *
         * @param move the move string in algebraic notation (e.g. "Nxe5", "e8=Q", "O-O")
         */
        explicit MoveScanner(const std::string& move) {
            scanMove(move);
        }

        /**
         * Checks whether the move is considered legal by the scanner.
         *
         * @return true if the move was fully parsed and valid; false otherwise
         */
        bool isLegal() {
            return legal;
        }

        /**
         * The piece type involved in the move (e.g. 'N', 'B', 'Q', 'K', or 'P' for pawn).
         * Uppercase is used for white, lowercase for black.
         */
        char piece = 0;

        /**
         * The piece the pawn is promoted to, if applicable (e.g. 'Q' in "e8=Q").
         */
        char promote = 0;

        /**
         * The file (0-based index) from which the piece moves, or NO_POS if unspecified.
         */
        int32_t departureFile = NO_POS;

        /**
         * The rank (0-based index) from which the piece moves, or NO_POS if unspecified.
         */
        int32_t departureRank = NO_POS;

        /**
         * The file (0-based index) to which the piece moves.
         */
        int32_t destinationFile = NO_POS;

        /**
         * The rank (0-based index) to which the piece moves.
         */
        int32_t destinationRank = NO_POS;

        /**
         * True if the move string was successfully and fully parsed.
         */
        bool legal = false;

        /**
         * Constant representing an unspecified rank or file.
         */
        static constexpr int32_t NO_POS = -1;

    private:
        void scanMove(const std::string& move) {
            int32_t curIndex = static_cast<int32_t>(move.size()) - 1;

            while (curIndex >= 0 && move[static_cast<size_t>(curIndex)] == ' ') {
                --curIndex;
            }

            if (!handleCastleNotation(move)) {
                skipCheckAndMateSigns(move, curIndex);
                promote = getPiece(move, curIndex);
                skipEPInfo(move, curIndex);
                skipCaptureChar(move, curIndex);
                destinationRank = getRank(move, curIndex);
                destinationFile = getFile(move, curIndex);
                skipCaptureChar(move, curIndex);
                departureRank = getRank(move, curIndex);
                departureFile = getFile(move, curIndex);
                piece = getPiece(move, curIndex);
                legal = (curIndex == -1);
            }

            if (piece == 0 && (departureFile == NO_POS || departureRank == NO_POS)) {
                piece = 'P';
            }
        }

        bool handleCastleNotation(const std::string& move) {
            if (move.size() < 3 || !isCastleNotationChar(move[0])) {
                return false;
            }

            departureFile = 4;
            departureRank = NO_POS;
            destinationFile = NO_POS;
            destinationRank = NO_POS;
            piece = 'K';

            if (move.size() >= 5 && move[1] == '-' && isCastleNotationChar(move[2])) {
                destinationFile = (move[3] == '-' && isCastleNotationChar(move[4])) ? 2 : 6;
            }
            return true;
        }

        void skipCheckAndMateSigns(const std::string& move, int32_t& curIndex) {
            while (curIndex >= 0 && (isCheckSign(move[static_cast<size_t>(curIndex)]) ||
                isMateSign(move[static_cast<size_t>(curIndex)]))) {
                --curIndex;
            }
        }

        char getPiece(const std::string& move, int32_t& curIndex) {
            if (curIndex >= 0 && isPieceChar(move[static_cast<size_t>(curIndex)])) {
                char p = move[static_cast<size_t>(curIndex--)];
                if (curIndex >= 0 && isPromoteChar(move[static_cast<size_t>(curIndex)])) {
                    --curIndex;
                }
                return p;
            }
            return 0;
        }

        void skipEPInfo(const std::string& move, int32_t& curIndex) {
            if (curIndex >= 3 &&
                move[static_cast<size_t>(curIndex - 3)] == 'e' &&
                move[static_cast<size_t>(curIndex - 2)] == '.' &&
                move[static_cast<size_t>(curIndex - 1)] == 'p' &&
                move[static_cast<size_t>(curIndex)] == '.') {
                curIndex -= 4;
            }
        }

        void skipCaptureChar(const std::string& move, int32_t& curIndex) {
            if (curIndex >= 0 && isCaptureChar(move[static_cast<size_t>(curIndex)])) {
                --curIndex;
            }
        }

        int32_t getRank(const std::string& move, int32_t& curIndex) {
            if (curIndex >= 0 && isRankChar(move[static_cast<size_t>(curIndex)])) {
                return static_cast<int32_t>(charToRank(move[static_cast<size_t>(curIndex--)]));
            }
            return NO_POS;
        }

        int32_t getFile(const std::string& move, int32_t& curIndex) {
            if (curIndex >= 0 && isFileChar(move[static_cast<size_t>(curIndex)])) {
                return static_cast<int32_t>(charToFile(move[static_cast<size_t>(curIndex--)]));
            }
            return NO_POS;
        }
    };

} // namespace QaplaInterface

