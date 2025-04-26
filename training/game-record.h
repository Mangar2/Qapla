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

//#include "EPDTest.h"
#include <fstream>
#include <sstream>
#include <vector>
#include "../basics/types.h"
#include "../basics/evalvalue.h"
#include "../interface/ichessboard.h"

using namespace std;

namespace QaplaTraining {

   

    class GameRecord {
    public:
        /**
         * Sets the FEN ID of the starting position.
         *
         * @param id 16-bit FEN ID
         */
        void setFENId(uint32_t id) { fenId = id; }

        /**
         * Returns the FEN ID of the starting position.
         *
         * @return FEN ID
         */
        uint32_t getFENId() const { return fenId; }

        /**
         * Sets the result of the game.
         *
         * @param r The game result
         */
        void setResult(QaplaInterface::GameResult r) { result = r; }

        /**
         * Returns the result of the game.
         *
         * @return Game result
         */
        QaplaInterface::GameResult getResult() const { return result; }

        /**
         * Adds a move to the game.
         *
         * @param lan        Move in LAN format (e.g., "e2e4", "e7e8q")
         * @param value      Computed value in range [-1024, 1023]
         * @return           Game result after this move (NOT_ENDED if game continues)
         */
        bool addMove(const std::string& lan, value_t value) {
			if (stopRecordingMoves) {
				return false;
			}
            auto [encoded, storeMove] = encodeMoveEval(lan, value);
            if (storeMove) {
                moves.push_back(encoded);
            }
            stopRecordingMoves = !storeMove;
            return storeMove;
        }

        /**
         * Returns the move at the given index as a LAN string.
         *
         * @param index Index of the move
         * @return      Move as LAN string (e.g., "e2e4", "e7e8q")
         */
        std::string getMove(size_t index) const {
            if (index >= moves.size()) return "";

            uint32_t move = moves[index];
            bool isPromotion = (move & 0x800000) != 0;
            move &= 0x000FFF;

            int from = move >> 6;
            int to = move & 0x3F;

            std::string result = QaplaBasics::squareToString(from) + QaplaBasics::squareToString(to);
            if (isPromotion) {
                result += 'q';
            }

            return result;
        }

        /**
         * Returns all moves in the game.
         *
         * @return Vector of all moves in LAN format
         */
        std::vector<std::string> getMoves() const {
            std::vector<std::string> result;
            result.reserve(moves.size());
            for (size_t i = 0; i < moves.size(); ++i) {
                result.push_back(getMove(i));
            }
            return result;
        }

        /**
         * Returns the static evaluation value of the move at the given index.
         *
         * @param index Index of the move
         * @return      Evaluation value in range [-1024, 1023], or 0 if index is invalid
         */
        value_t getValue(size_t index) const {
            if (index >= moves.size()) return 0;

            uint32_t move = moves[index];
            uint16_t evalBits = (move >> 12) & 0x7FF;
            return static_cast<value_t>(evalBits) - 1024;
        }

        /**
         * Converts the game record to a comma-separated result string.
         * Format: fen, move1, value1, move2, value2, ..., result
         *
         * Uses getMove() and getEval() for consistency.
         *
         * @param fen The initial position in FEN format
         * @return    Result string representation
         */
        std::string toResultString(const std::string& fen) const {
            std::ostringstream oss;
            oss << fen;

            for (size_t i = 0; i < moves.size(); ++i) {
                oss << ", " << getMove(i) << ", " << getValue(i);
            }

            // oss << ", " << static_cast<int>(result);
            return oss.str();
        }

        /**
		 * @brief Returns the number of moves in the game.
         */
		size_t numMoves() const { return moves.size(); }

    private:
        uint32_t fenId;
        std::vector<uint32_t> moves;
        QaplaInterface::GameResult result = QaplaInterface::GameResult::NOT_ENDED;
        bool stopRecordingMoves = false;

        /**
         * Encodes a move and its evaluation into a 24-bit value.
         * Uses bit 23 to flag a promotion to queen.
         *
         * @param lan        The move in LAN format
         * @param staticEval The evaluation value in range [-1024, 1023]
         * @return           Encoded 24-bit value and game result if adjudicated
         */
        std::pair<uint32_t, bool> encodeMoveEval(
            const std::string& lan, value_t staticEval)
        {
			constexpr value_t MAX_EVAL = 1023;
            auto [moveCode, valid] = encodeLAN(lan);
            if (!valid) {
                return { 0, false };
            }

			staticEval = std::clamp(staticEval, -MAX_EVAL, MAX_EVAL);
           
            uint16_t evalBits = static_cast<uint16_t>(staticEval + MAX_EVAL + 1);
            uint32_t encoded = (static_cast<uint32_t>(evalBits) << 12) | moveCode;

            if (lan.length() == 5) {
                encoded |= 0x800000; // Set promotion flag in bit 23
            }

            return { encoded, true };
        }

        /**
         * Encodes a move string into 13 bits:
         * - Bit 12: promotion flag (1 if promotion to queen, 0 otherwise)
         * - Bits 11–6: from-square (0–63)
         * - Bits 5–0:  to-square   (0–63)
         *
         * Only queen promotions are allowed. Underpromotions are rejected.
         *
         * @param move Move in LAN format (e.g., "e2e4", "e7e8q")
         * @return     Encoded 13-bit value and validity flag
         */
        std::pair<uint16_t, bool> encodeLAN(const std::string move) {
            std::string fromStr = move.substr(0, 2);
            std::string toStr = move.substr(2, 2);

            QaplaBasics::square_t from = QaplaBasics::stringToSquare(fromStr);
            QaplaBasics::square_t to = QaplaBasics::stringToSquare(toStr);

            if (from < 0 || from > 63 || to < 0 || to > 63) {
                return { 0, false };
            }

            bool isPromotion = false;
            if (move.length() == 5) {
                char promoChar = move[4];
                int piece = QaplaBasics::charToPiece(promoChar);
                if (piece != QaplaBasics::WHITE_QUEEN && piece != QaplaBasics::BLACK_QUEEN) {
                    return { 0, false };
                }
                isPromotion = true;
            }

            uint16_t encoded = static_cast<uint16_t>((isPromotion ? 1 << 12 : 0) |
                (from << 6) |
                to);
            return { encoded, true };
        }


        friend std::ostream& operator<<(std::ostream& os, const GameRecord& game);
        friend std::istream& operator>>(std::istream& is, GameRecord& game);
		friend bool operator!=(const GameRecord& lhs, const GameRecord& rhs);
    };

    /**
    * Reads GameRecord objects from a binary file.
    * Automatically opens and closes the file.
    */
    class GameRecordReader {
    public:
        /**
         * Constructs a reader. Opens the file if a non-empty filename is provided.
         *
         * @param filename Path to the input file
         */
        GameRecordReader(const std::string& filename) {
            if (!filename.empty()) {
                in.open(filename, std::ios::binary | std::ios::in);
                if (!in) {
                    std::cerr << "Failed to open file for reading: " << filename << std::endl;
                }
            }
        }

        /**
         * Destructor. Closes the file if open.
         */
        ~GameRecordReader() {
            if (in.is_open()) {
                in.close();
            }
        }

        /**
         * Reads the next GameRecord from the file.
         *
         * @param game Reference to GameRecord object to fill
         * @return     True if a record was read successfully, false on failure or EOF
         */
        bool read(GameRecord& game) {
            if (!in.is_open()) return false;
            return static_cast<bool>(in >> game);
        }

        /**
         * Returns true if the end of file has been reached.
         *
         * @return True if EOF
         */
        bool eof() const {
            return in.eof();
        }

    private:
        std::ifstream in;
    };

    /**
     * Writes a GameRecord to an output stream in compact binary format.
     *
     * Format:
     * - 2 bytes: entry size excluding these two bytes
     * - 2 bytes: FEN ID
     * - N x 3 bytes: encoded moves
     * - 1 byte: game result
     *
     * @param os   Output stream
     * @param game GameRecord instance
     * @return     Output stream reference
     */
    inline std::ostream& operator<<(std::ostream& os, const GameRecord& game) {
        uint16_t entrySize = static_cast<uint16_t>(
            sizeof(game.fenId) +              // 2 bytes
            game.moves.size() * 3 +           // 3 bytes per move
            1                                 // 1 byte for result
            );

        os.write(reinterpret_cast<const char*>(&entrySize), sizeof(entrySize));
        os.write(reinterpret_cast<const char*>(&game.fenId), sizeof(game.fenId));

        for (uint32_t move : game.moves) {
            char bytes[3] = {
                static_cast<char>(move & 0xFF),
                static_cast<char>((move >> 8) & 0xFF),
                static_cast<char>((move >> 16) & 0xFF)
            };
            os.write(bytes, 3);
        }

        uint8_t resultByte = static_cast<uint8_t>(game.result);
        os.write(reinterpret_cast<const char*>(&resultByte), 1);

        return os;
    }

    /**
     * Reads a GameRecord from an input stream in compact binary format.
     *
     * @param is   Input stream
     * @param game GameRecord to fill
     * @return     Input stream reference
     */
    inline std::istream& operator>>(std::istream& is, GameRecord& game) {
        game.moves.clear();
        game.result = QaplaInterface::GameResult::NOT_ENDED;

        uint16_t length = 0;
        is.read(reinterpret_cast<char*>(&length), sizeof(length));
        if (!is) return is;

        is.read(reinterpret_cast<char*>(&game.fenId), sizeof(game.fenId));
        if (!is) return is;

        size_t remaining = length - sizeof(game.fenId);
        if (remaining < 1 || remaining % 3 == 0) {
            is.setstate(std::ios::failbit);
            return is;
        }

        size_t moveCount = (remaining - 1) / 3;

        for (size_t i = 0; i < moveCount; ++i) {
            char bytes[3];
            is.read(bytes, 3);
            if (is.gcount() != 3) {
                is.setstate(std::ios::failbit);
                return is;
            }
            uint32_t move = (static_cast<uint8_t>(bytes[2]) << 16) |
                (static_cast<uint8_t>(bytes[1]) << 8) |
                static_cast<uint8_t>(bytes[0]);
            game.moves.push_back(move);
        }

        uint8_t resultByte = 0;
        is.read(reinterpret_cast<char*>(&resultByte), 1);
        if (!is) return is;

        game.result = static_cast<QaplaInterface::GameResult>(resultByte);

        return is;
    }

    /**
     * Compares two GameRecord objects for inequality.
     *
     * @param lhs Left-hand side GameRecord
     * @param rhs Right-hand side GameRecord
     * @return    True if the records differ in FEN ID, result, or moves
     */
    inline bool operator!=(const GameRecord& lhs, const GameRecord& rhs) {
        return lhs.fenId != rhs.fenId ||
            lhs.result != rhs.result ||
            lhs.moves != rhs.moves;
    }


}
