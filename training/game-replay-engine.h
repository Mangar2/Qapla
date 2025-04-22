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

#include <string>
#include <functional>
#include "game-record.h"
#include "../interface/ichessboard.h"
#include "../interface/chessinterface.h"

namespace QaplaTraining {

    struct MoveInfo {
        bool gameStarting;
		std::string move;
        QaplaInterface::IChessBoard* engine;
		value_t value;
		value_t eval;
		bool isCapture;
		bool moveBeforeWasCapture;
        QaplaInterface::GameResult result;
    };

    /**
     * @class GameReplayEngine
     * @brief Reads and replays binary-encoded chess games with customizable callbacks.
     *
     * This class provides a mechanism to read binary-encoded game files and replay them.
     * During replay, three callback hooks can be provided:
     * - On new game start
     * - On move played
     * - On replay finished
     */
    class GameReplayEngine {
    public:
        using MoveCallback = std::function<void(const MoveInfo& move)>;
        using FinishCallback = std::function<void()>;

        /**
         * @brief Constructor
         * @param fenList Vector of all possible FEN strings (indexed by FEN-ID from GameRecord)
         */
        GameReplayEngine(const QaplaInterface::IChessBoard* engine,  const std::vector<std::string>& fenList)
            : fenList_(fenList), chessEngine_(engine->createNew()) {
        }

        void setMoveCallback(MoveCallback callback) {
            moveCallback_ = std::move(callback);
        }

        void setFinishCallback(FinishCallback callback) {
            finishCallback_ = std::move(callback);
        }

        /**
         * @brief Starts reading the file and replaying the games
         */
        void run(const std::string& filePath) {
            GameRecordReader reader(filePath);

            QaplaTraining::GameRecord game;
			uint64_t gameCounter = 0;
            bool error = false;

            while (reader.read(game)) {
                
				gameCounter++;
                const auto result = game.getResult();
                // Reset board to initial position
                const auto fenId = game.getFENId();
                error = !setupBoardFromFEN(fenId);
				if (error) {
					std::cerr << "Error: Invalid FEN ID: " << fenId << std::endl;
					break;
				}

                // Notify start of new game
                if (moveCallback_) {
                    value_t eval = chessEngine_->eval();
                    moveCallback_({ true, "", chessEngine_.get(), 0, eval, false, false});
                }
				moveBeforeWasCapture_ = false;
                for (size_t index = 0; index < game.numMoves(); ++index) {
                    bool isLegal = setMove(game.getMove(index), game.getValue(index), result);
					if (!isLegal) {
						std::cerr << "Error: Invalid move at index " << index << " fen id " << fenId << std::endl;
						break;
					}
                }
				if (gameCounter % 10000 == 0) {
					std::cout << "\rGames replayed: " << gameCounter << std::flush;
				}
            }

            if (finishCallback_) {
                std::cout << "\rGames replayed: " << gameCounter << std::endl;
                finishCallback_();
            }
        }

    private:
        std::vector<std::string> fenList_;
        std::unique_ptr<QaplaInterface::IChessBoard> chessEngine_;
        MoveCallback moveCallback_;
        FinishCallback finishCallback_;
        bool moveBeforeWasCapture_;

        /**
         * @brief Sets up the internal board state from a FEN string
         * @param fen The FEN string representing the initial position
		 * @return true if successful, false otherwise
         */
        bool setupBoardFromFEN(uint16_t fenId) {
            if (fenId < fenList_.size()) {
				QaplaInterface::ChessInterface::setPositionByFen(fenList_[fenId], chessEngine_.get());
				return true;
            }
			else {
				std::cerr << "Error: FEN ID out of range: " << fenId << std::endl;
				return false;
			}
        }

		bool setMove(const std::string& move, int32_t value, QaplaInterface::GameResult result) {
			bool isCapture = QaplaInterface::ChessInterface::isCapture(move, chessEngine_.get());
			value_t eval = chessEngine_->eval();
            if (moveCallback_) {
                moveCallback_({ false, move, chessEngine_.get(), value, eval, isCapture, moveBeforeWasCapture_, result });
            }
			bool isLegalMove = QaplaInterface::ChessInterface::setMove(move, chessEngine_.get());
            if (!isLegalMove) {
                std::cerr << "Error: Illegal move: " << move << std::endl;
                return false;
            }
			moveBeforeWasCapture_ = isCapture;
			return true;
		}

    };


} // namespace QaplaTraining


