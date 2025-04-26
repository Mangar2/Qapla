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

#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <iostream>
#include "../interface/ichessboard.h"
#include "../interface/chessinterface.h"
#include "game-replay-engine.h"

namespace QaplaTraining {

    /**
     * @class PositionFilter
     * @brief Filters positions where the absolute evaluation exceeds a threshold, but the game was not won.
     */
    class PositionFilter {
    public:
        /**
         * @brief Constructor
         * @param threshold Evaluation threshold (absolute value) to consider a position "winning"
         */
        explicit PositionFilter(value_t threshold) : threshold_(threshold) {}

        /**
         * @brief Connects to the replay engine and collects all suspicious positions
         * @param engine GameReplayEngine instance
         * @param filePath Path to the game file
         */
        void analyzeGames(GameReplayEngine& engine, const std::string& filePath) {
            std::string moveList = "";
            std::string startFen = "";
            engine.setMoveCallback([this, &moveList, &startFen](const MoveInfo& moveInfo) {
				if (moveInfo.gameStarting) {
					newGame_ = true;
					if (suspiciousPosition_ != "") {
						cout << "start" << startFen << std::endl;
						cout << "moves: " << moveList << std::endl;
						cout << suspiciousPosition_ << std::endl << std::endl;
						suspiciousPositions_.push_back(suspiciousPosition_);
					}
                    suspiciousPosition_ = "";
                    startFen = "(" + to_string(moveInfo.fenId) + "): " + moveInfo.startFen;
                    moveList = "";
                    return;
				}

                int32_t whiteValue = moveInfo.engine->isWhiteToMove() ? moveInfo.value : -moveInfo.value;

                if (std::abs(whiteValue) >= threshold_ && !isWinFor(whiteValue, moveInfo.result) && newGame_) {
					suspiciousPosition_ = moveInfo.engine->getFen() + " ";
                    newGame_ = false;
                }
				if (moveInfo.engine->isWhiteToMove()) {
                    moveList += to_string(moveInfo.moveNo) + ". ";
				}
                moveList += moveInfo.move + "(" + to_string(whiteValue) + "), ";
                if (suspiciousPosition_ != "") {
					suspiciousPosition_ += moveInfo.move + "(" + to_string(whiteValue) + "), ";
                }
                });

            engine.setFinishCallback([this]() {
                std::cout << "\nFiltered " << suspiciousPositions_.size() << " suspicious positions.\n";
                });

            engine.run(filePath);
        }

        /**
         * @brief Returns the list of filtered FENs
         */
        const std::vector<std::string>& getSuspiciousPositions() const {
            return suspiciousPositions_;
        }

    private:
        value_t threshold_;
        QaplaInterface::GameResult currentResult_;
        std::vector<std::string> suspiciousPositions_;
        bool newGame_ = false;
		std::string suspiciousPosition_;

        /**
         * @brief Determines if the given evaluation is consistent with a win
         */
        bool isWinFor(value_t value, QaplaInterface::GameResult result) const {

            if (value > 0 && result == QaplaInterface::GameResult::WHITE_WINS_BY_MATE) {
                return true;
            }
            if (value < 0 && result == QaplaInterface::GameResult::BLACK_WINS_BY_MATE) {
                return true;
            }
            return false;
        }
    };

} // namespace QaplaTraining


