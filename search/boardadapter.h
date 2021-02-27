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
 * Implements the IChessBoard interface to connect a frontend with the chess engine
 */

#ifndef __BOARDADAPTER_H
#define __BOARDADAPTER_H

#include <thread>
#include "../interface/isendsearchinfo.h"
#include "../interface/ichessboard.h"
#include "../interface/iinputoutput.h"
#include "../basics/move.h"
#include "../basics/movelist.h"
#include "../movegenerator/movegenerator.h"
#include "../search/perft.h"
#include "../search/search.h"
#include "../eval/eval.h"
#include "../search/iterativedeepening.h"
#include "movehistory.h"
#include "../bitbase/bitbasegenerator.h"

using namespace ChessMoveGenerator;
using namespace ChessInterface;
using namespace ChessEval;

namespace ChessSearch {

	class BoardAdapter : public IChessBoard {
	public:
		BoardAdapter() : positionModified(true) {}

		/**
		 * Sets the class printing search information in the right format
		 */
		virtual void setSendSerchInfo(ISendSearchInfo* sendSearchInfo) {
			iterativeDeepening.setSendSearchInfoInterface(sendSearchInfo);
		};

		/**
		 * Retrieves the engine name
		 */
		virtual map<string, string> getEngineInfo() { 
			return map<string, string>{
				{ "name", "Qapla 0.1.01" },
				{ "author", "Volker Böhm"}
			};
		}

		/**
		 * Generates bitbases for a signature and all bitbases needed
		 * to compute this bitabase (if they cannot be loaded)
		 */
		virtual void generateBitbases(string signature) {
			ChessBitbase::BitbaseGenerator generator;
			generator.computeBitbaseRec(signature);
		}

		/**
		 * Load databases like tablebases or bitbases
		 */
		virtual void initialize() {
			ChessBitbase::BitbaseReader::loadBitbase();
		}

		/**
		 * Retrieves the what if object
		 */
		virtual IWhatIf* getWhatIf() {
			WhatIf::whatIf.setBoard(position);
			return &WhatIf::whatIf;
		}

		/**
		 * Sets the 
		 */
		virtual void newGame() {
			iterativeDeepening.clearHash();
		}

		/**
		 * Playes a move. Only the destination square must be provided, other information must be provided
		 * only to solve anbiguity. 
		 * @param movingPiece piece to move, may be EMPTY_PIECE (= 0) if the piece is unknown
		 * @param departureFile file of the departure square (0-7) or -1 for unknown
		 * @param departureRank rank of the departure square (0-7) or -1 for unknown
		 * @param destinationFile file of the destination square (0-7) 
		 * @param destinationRank rank of the destination square (0-7)
		 * @param char promotePiece (mandatory only for a promotion move else EMPTY_PIECE)
		 */
		virtual bool doMove(char movingPiece,
			uint32_t departureFile, uint32_t departureRank,
			uint32_t destinationFile, uint32_t destinationRank,
			char promotePiece)
		{
			Move move = findMove(position, movingPiece, departureFile, departureRank,
				destinationFile, destinationRank, promotePiece);

			if (!move.isEmpty()) {
				if (positionModified) {
					moveHistory.setStartPosition(position);
					positionModified = false;
				}
				position.doMove(move);
				moveHistory.addMove(move);
				playedMovesInGame++;
			}

			return !move.isEmpty();
		};

		/**
		 * Undoes the last move
		 */
		virtual void undoMove() {
			position = moveHistory.undoMove();
			if (playedMovesInGame > 0) {
				playedMovesInGame--;
			}
			iterativeDeepening.clearHash();
		}

		/**
		 * Clears the position, setting everything to empty.
		 */
		virtual void clearBoard() {
			position.clear();
			positionModified = true;
			playedMovesInGame = 0;
		}

		/**
		 * Set side to move
		 */
		virtual void setWhiteToMove(bool whiteToMove) {
			position.setWhiteToMove(whiteToMove);
		}

		/**
		 * Gets side to move
		 */
		virtual bool isWhiteToMove() {
			return position.isWhiteToMove();
		}

		/**
		 * Sets a piece to a square
		 */
		virtual void setPiece(uint32_t file, uint32_t rank, char piece) {
			const File castFile = static_cast<File>(file);
			const Rank castRank = static_cast<Rank>(rank);
			if (isFileInBoard(castFile) && isRankInBoard(castRank)) {
				const Square square = computeSquare(castFile, castRank);
				position.setPiece(square, charToPiece(piece));
			}
		}

		/**
		 * Allow or deny white queen side castling
		 */
		virtual void setWhiteQueenSideCastlingRight(bool allow) {
			position.setCastlingRight(WHITE, false, allow);
		}

		/**
		 * Allow or deny white king side castling
		 */
		virtual void setWhiteKingSideCastlingRight(bool allow) {
			position.setCastlingRight(WHITE, true, allow);
		}

		/**
		 * Allow or deny black queen side castling
		 */
		virtual void setBlackQueenSideCastlingRight(bool allow) {
			position.setCastlingRight(BLACK, false, allow);
		}

		/**
		 * Allow or deny black king side castling
		 */
		virtual void setBlackKingSideCastlingRight(bool allow) {
			position.setCastlingRight(BLACK, true, allow);
		}

		/**
		 * Sets the EP destination square
		 */
		virtual void setEPSquare(uint32_t epFile, uint32_t epRank) {
			// Adjust ep, beause it is stored as postion of the pawn to capture
			epRank = epRank == 3 ? 4 : 5;
			position.setEP(computeSquare(File(epFile), Rank(epRank)));
		}

		/**
		 * Sets the number of half moves without pawn move or capture
		 */
		virtual void setHalfmovesWithouthPawnMoveOrCapture(uint16_t number) {
			position.setHalfmovesWithoutPawnMoveOrCapture(number);
		}

		/**
		 * Sets the amount of moves already played in the game
		 */
		virtual void setPlayedMovesInGame(uint16_t moves) {
			playedMovesInGame = moves;
		}

		/**
		 * Starts perft
		 */
		virtual uint64_t perft(uint16_t depth, bool verbose = true, uint32_t maxTheadCount = 1) {
			uint32_t additionalWorkerCount = maxTheadCount == 0 ? 0 : maxTheadCount - 1;
			uint64_t res = ChessSearch::doPerftRec(position, depth, additionalWorkerCount, true, verbose);
			return res;
		}

		/**
		 * Provides the result of the game
		 */
		virtual GameResult getGameResult() {
			GameResult result = isMate(position);
			if (result == GameResult::NOT_ENDED) {
				if (moveHistory.isDrawByRepetition(position)) {
					result = GameResult::DRAW_BY_REPETITION;
				}
				else if (position.getHalfmovesWithoutPawnMoveOrCapture() > 100) {
					result = GameResult::DRAW_BY_50_MOVES_RULE;
				}
			}

			return result;
		}

		/**
		 * Force the engine to play the move now
		 */
		virtual void moveNow() {
			iterativeDeepening.stopSearch();
		}

		/**
		 * Sets the clock for the next move.
		 * It must be set by the managing thread and not the computing thread to avoid races!
		 */
		virtual void setClock(const ClockSetting& clockSetting) {
			curClock = clockSetting;
			curClock.setPlayedMovesInGame(playedMovesInGame);
			iterativeDeepening.setClockForNextSearch(curClock);
		}

		/**
		 * Compute a move
		 */
		virtual void computeMove(bool verbose = true) {
			_computingInfo = iterativeDeepening.searchByIterativeDeepening(position, moveHistory);
		}

		/**
		 * Signals a ponder hit
		 */
		virtual void ponderHit() {
			iterativeDeepening.ponderHit();
		}


		/**
		 * Creates an exchange version of the computing info structure to send to winposition
		 */
		virtual ComputingInfoExchange getComputingInfo() {
			return _computingInfo.getExchangeStructure();
		}

		/**
		 * Prints the actual Search status information
		 */
		virtual void requestPrintSearchInfo() {
			iterativeDeepening.requestPrintSearchInfo();
		}

		/**
		 * Not standard - request to print the details of eval for the current position to stdout
		 */
		virtual void printEvalInfo() {
			Eval eval;
			eval.printEval(position);
		}

		/**
		 * Gets internal information from eval
		 */
		template <Piece COLOR>
		auto getEvalFactors() {
			Eval eval;
			return eval.getEvalFactors<COLOR>(position);
		}

		/**
		 * Sets the amount of worker threads working in parallel to the main thread
		 */
		void setWorkerAmount(uint32_t workerCount) {
			_workerCount = workerCount;
		}

		/**
		 * Provides the position object with the current position
		 */
		MoveGenerator& getBoard() {
			return position;
		}

		/**
		 * Find the correct move providing a partial move information
		 */
		static Move findMove(MoveGenerator& position, char movingPieceChar,
			uint32_t departureFile, uint32_t departureRank,
			uint32_t destinationFile, uint32_t destinationRank, char promotePieceChar)
		{
			MoveList moveList;
			Move foundMove;

			uint16_t moveNoFound = 0;
			position.genMovesOfMovingColor(moveList);
			const bool whiteToMove = position.isWhiteToMove();
			Piece promotePiece = charToPiece(whiteToMove ? toupper(promotePieceChar) : tolower(promotePieceChar));
			Piece movingPiece = charToPiece(whiteToMove ? toupper(movingPieceChar) : tolower(movingPieceChar));

			for (uint16_t moveNo = 0; moveNo < moveList.getTotalMoveAmount(); moveNo++) {
				const Move move = moveList[moveNo];

				if ((movingPiece == NO_PIECE || move.getMovingPiece() == movingPiece) &&
					(departureFile == -1 || getFile(move.getDeparture()) == File(departureFile)) &&
					(departureRank == -1 || getRank(move.getDeparture()) == Rank(departureRank)) &&
					(getFile(move.getDestination()) == File(destinationFile)) &&
					(destinationRank == -1 || getRank(move.getDestination()) == Rank(destinationRank)) &&
					(move.getPromotion() == promotePiece))
				{
					if (!foundMove.isEmpty()) {
						// Error: more than one possible move found
						foundMove = Move();
						break;
					}
					foundMove = move;
				}
			}
			return foundMove;
		}

	private:


		/**
		 * Checks, if we have a mate situation
		 */
		GameResult isMate(MoveGenerator& position) {
			GameResult result = GameResult::NOT_ENDED;
			MoveList moveList;
			position.genMovesOfMovingColor(moveList);

			if (moveList.getTotalMoveAmount() == 0) {
				if (position.isWhiteToMove()) {
					if (position.isInCheck()) {
						result = GameResult::BLACK_WINS_BY_MATE;
					}
					else {
						result = GameResult::DRAW_BY_STALEMATE;
					}
				}
				else {
					if (position.isInCheck()) {
						result = GameResult::WHITE_WINS_BY_MATE;
					}
					else {
						result = GameResult::DRAW_BY_STALEMATE;
					}
				}
			}

			return result;
		}

		bool positionModified;
		MoveGenerator position;
		MoveHistory moveHistory;
		uint32_t playedMovesInGame;
		ComputingInfo _computingInfo;
		ClockSetting curClock;
		IterativeDeepening iterativeDeepening;
		// WhatIf whatIf;
		uint32_t _workerCount;

	};
}


#endif //__BOARDADAPTER_H