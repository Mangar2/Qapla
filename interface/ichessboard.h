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
 * Implements a generic interface to chess boards to separate dedicated board implementations from
 * the interface management
 */

#pragma once

#include <map>
#include "clocksetting.h"
#include "computinginfoexchange.h"
#include "iwhatIf.h"
#include "isendsearchinfo.h"
#include "../eval/eval-exchange-structures.h"


namespace QaplaInterface {

	enum class GameResult {
		NOT_ENDED,
		DRAW_BY_REPETITION,
		DRAW_BY_50_MOVES_RULE,
		DRAW_BY_STALEMATE,
		DRAW_BY_NOT_ENOUGHT_MATERIAL,
		WHITE_WINS_BY_MATE,
		BLACK_WINS_BY_MATE,
		ILLEGAL_MOVE
	};

    class IChessBoard {
    public:
        virtual ~IChessBoard() = default;

        /** Creates a fresh, empty board instance. */
        virtual IChessBoard* createNew() const = 0;

        /** Sets the search information output handler. */
        virtual void setSendSearchInfo(ISendSearchInfo* sendSearchInfo) = 0;

        /** Retrieves basic engine information (name, author, etc.). */
        virtual std::map<std::string, std::string> getEngineInfo() = 0;

        /** Performs all necessary initializations. */
        virtual void initialize() {}

        /** Executes a move. Partial information is allowed if unambiguous. */
        virtual bool doMove(char movingPiece,
                            uint32_t departureFile, uint32_t departureRank,
                            uint32_t destinationFile, uint32_t destinationRank,
                            char promotePiece) = 0;

        /** Returns true if the move results in a capture. */
        virtual bool isCapture(char movingPiece,
                               uint32_t departureFile, uint32_t departureRank,
                               uint32_t destinationFile, uint32_t destinationRank,
                               char promotePiece) = 0;

        /** Undoes the last move (if history is stored). */
        virtual void undoMove() = 0;

        /** Clears the board to an empty setup. */
        virtual void clearBoard() = 0;

        /** Sets white queenside castling right. */
        virtual void setWhiteQueenSideCastlingRight(bool allow) = 0;

        /** Sets white kingside castling right. */
        virtual void setWhiteKingSideCastlingRight(bool allow) = 0;

        /** Sets black queenside castling right. */
        virtual void setBlackQueenSideCastlingRight(bool allow) = 0;

        /** Sets black kingside castling right. */
        virtual void setBlackKingSideCastlingRight(bool allow) = 0;

        /** Sets en-passant target square. */
        virtual void setEPSquare(uint32_t epFile, uint32_t epRank) = 0;

        /** Sets whether white is to move. */
        virtual void setWhiteToMove(bool whiteToMove) = 0;

        /** Sets the number of halfmoves without pawn move or capture. */
        virtual void setHalfmovesWithoutPawnMoveOrCapture(uint16_t moves) = 0;

        /** Sets the number of halfmoves played in the game. */
        virtual void setPlayedMovesInGame(uint16_t moves) = 0;

        /** Signals that board setup is complete. */
        virtual void finishBoardSetup() = 0;

        /** Returns true if it is white's turn. */
        virtual bool isWhiteToMove() = 0;

        /** Returns true if the side to move is in check. */
        virtual bool isInCheck() = 0;

        /** Places a piece onto the board at the given square. */
        virtual void setPiece(uint32_t file, uint32_t rank, char piece) = 0;

        /** Executes a perft calculation. */
        virtual uint64_t perft(uint16_t depth, bool verbose = true, uint32_t maxThreadCount = 1) = 0;

        /** Returns the current position in FEN format. */
        virtual std::string getFen() = 0;

        /** Immediately prints evaluation information. */
        virtual void printEvalInfo() = 0;

        /** Evaluates the current position numerically. */
        virtual value_t eval() = 0;

        /** Computes evaluation index vector for advanced evaluation. */
        virtual ChessEval::IndexVector computeEvalIndexVector() = 0;

        /** Computes evaluation index lookup map for advanced evaluation. */
        virtual ChessEval::IndexLookupMap computeEvalIndexLookupMap() = 0;

        /** Sets the clock settings for the next move. */
        virtual void setClock(const ClockSetting& clockSetting) = 0;

        /** Starts move computation asynchronously. */
        virtual void computeMove(std::string SearchMoves = "", bool verbose = true) = 0;

        /** Signals that a pondered move was hit. */
        virtual void ponderHit() {}

        /** Requests immediate search information printout. */
        virtual void requestPrintSearchInfo() = 0;

        /** Stops calculation and plays the best move found. */
        virtual void moveNow() = 0;

        /** Retrieves the current game result. */
        virtual GameResult getGameResult() = 0;

        /** Returns current computing result information. */
        virtual ComputingInfoExchange getComputingInfo() = 0;

        /** Signals that a new game has started. */
        virtual void newGame() {}

        /** Sets a configuration option (e.g., from GUI). */
        virtual void setOption(std::string name, std::string value) {}

        /** Provides access to "what-if" evaluation functionality. */
        virtual IWhatIf* getWhatIf() = 0;

        /** Generates bitbases for the given signature and dependencies. */
        virtual void generateBitbases([[maybe_unused]] std::string signature,
                                      [[maybe_unused]] uint32_t cores,
                                      [[maybe_unused]] std::string compression,
			                          [[maybe_unused]] bool generateCpp = false,
                                      [[maybe_unused]] uint32_t traceLevel = 0,
                                      [[maybe_unused]] uint32_t debugLevel = 0,
                                      [[maybe_unused]] uint64_t debugIndex = 64) {}

        /** Verifies bitbases for the given signature. */
        virtual void verifyBitbases([[maybe_unused]] std::string signature,
                                    [[maybe_unused]] uint32_t cores = 1,
                                    [[maybe_unused]] uint32_t traceLevel = 0,
                                    [[maybe_unused]] uint32_t debugLevel = 0) {}

        /** Sets the evaluation version number. */
        virtual void setEvalVersion([[maybe_unused]] uint32_t version) {}

        /** Sets a named evaluation feature. */
        virtual void setEvalFeature([[maybe_unused]] std::string feature,
                                    [[maybe_unused]] value_t value) {}
    };

}


