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

#ifndef __ICHESSBOARD_H
#define __ICHESSBOARD_H

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

		virtual IChessBoard* createNew() const = 0;

		/**
		 * Sets the class printing search information in the right format
		 */
		virtual void setSendSerchInfo(ISendSearchInfo* sendSearchInfo) = 0;

		/**
		 * Retrieves the name of the engine
		 */
		virtual map<string, string> getEngineInfo() = 0;

		/**
		 * Initializations are done here
		 */
		virtual void initialize() {}

		/**
		 * Sets a move, the information may be incomplete as long as the information is unambiguous
		 */
		virtual bool doMove(char movingPiece,
			uint32_t depatureFile, uint32_t departureRank,
			uint32_t destinationFile, uint32_t destinationRank,
			char promotePiece) = 0;

		/**
		 * Undoes the last move - possible only, if the engine stores the last moves
		 */
		virtual void undoMove() = 0;

		/**
		 * Clears the board to an empty board
		 */
		virtual void clearBoard() = 0;

		/**
		 * Set castling rights
		 */
		virtual void setWhiteQueenSideCastlingRight(bool allow) = 0;
		virtual void setWhiteKingSideCastlingRight(bool allow) = 0;
		virtual void setBlackQueenSideCastlingRight(bool allow) = 0;
		virtual void setBlackKingSideCastlingRight(bool allow) = 0;

		/**
		 * Sets the destination position for an en-passant move
		 */
		virtual void setEPSquare(uint32_t epFile, uint32_t epRank) = 0;

		/**
		 * Set wether it is whites turn to move
		 */
		virtual void setWhiteToMove(bool whiteToMove) = 0;

		/**
		 * Set the amount of half moves without pawn move or capture played in the start position
		 */
		virtual void setHalfmovesWithouthPawnMoveOrCapture(uint16_t moves) = 0;

		/**
		 * Set the amount of half moves without pawn move or capture played in the start position
		 */
		virtual void setPlayedMovesInGame(uint16_t moves) = 0;

		/**
		 * Returns true, if it is whites turn to move
		 */
		virtual bool isWhiteToMove() = 0;

		/**
		 * Sets a piece to a square
		 */
		virtual void setPiece(uint32_t file, uint32_t rank, char piece) = 0;

		/**
		 * Starts perft calculation
		 */
		virtual uint64_t perft(uint16_t depth, bool verbose = true, uint32_t _maxTheadCount = 1) = 0;

		/**
		 * Promptly print an information string for the current evaluation status
		 */
		virtual void printEvalInfo() = 0;
		virtual value_t eval() = 0;
		virtual ChessEval::IndexVector computeEvalIndexVector() = 0;
		virtual ChessEval::IndexLookupMap computeEvalIndexLookupMap() = 0;

		/**
		 * Sets the clock for the next move.
		 * It must be set by the managing thread and not the computing thread to avoid races!
		 */
		virtual void setClock(const ClockSetting& clockSetting) = 0;

		/**
		 * Starts to compute a move
		 */
		virtual void computeMove(bool verbose = true) = 0;

		/**
		 * Signals a ponder hit
		 */
		virtual void ponderHit() {}

		/**
		 * Print an information string for the current search status
		 */
		virtual void requestPrintSearchInfo() = 0;

		/**
		 * Stop calcuation and play the current move
		 */
		virtual void moveNow() = 0;
		
		/**
		 * Returns the current game status
		 */
		virtual GameResult getGameResult() = 0;

		/**
		 * Returns and information about the current computing result
		 */
		virtual ComputingInfoExchange getComputingInfo() = 0;

		/**
		 * Signals a new game, the engine may ignore this
		 */
		virtual void newGame() {};

		/**
		 * Sets an option
		 */
		virtual void setOption(string name, string value) {};

		/**
		 * Returns a whatif evaluation structure (see IWhatIf for further information)
		 */
		virtual IWhatIf* getWhatIf() = 0;

		/**
		 * Generates bitbases for a signature and all bitbases needed
		 * to compute this bitabase (if they cannot be loaded)
		 */
		virtual void generateBitbases(string signature, uint32_t cores = 1, bool uncompressed = false, 
			uint32_t traceLevel = 0, uint32_t debugLevel = 0, uint64_t debugIndex = 64) 
		{}

		/**
		 * Verify bitbases for a signature 
		 */
		virtual void verifyBitbases(string signature, uint32_t cores = 1, uint32_t traceLevel = 0, uint32_t debugLevel = 0)
		{}


	};
}

#endif // __ICHESSBOARD_H
