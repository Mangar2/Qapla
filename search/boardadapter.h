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

 /*

#include "WhatIf.h"
*/


using namespace ChessMoveGenerator;
using namespace ChessInterface;
using namespace ChessEval;
using namespace ChessSearch;

class BoardAdapter: public IChessBoard {
public:
	BoardAdapter(ISendSearchInfo* sendInfo) 
		: boardModified(true), computingInfo(sendInfo) 
	{
	}

	virtual IWhatIf* getWhatIf() { 
		WhatIf::whatIf.setBoard(board);
		return &WhatIf::whatIf;
	}
		
    virtual bool doMove(char movingPiece,
			uint32_t departureFile, uint32_t departureRank,
			uint32_t destinationFile, uint32_t destinationRank,
			char promotePiece)
	{
		Move move = findMove(board, movingPiece, departureFile, departureRank, 
			destinationFile, destinationRank, promotePiece);
		
		if (!move.isEmpty()) {
			if (boardModified) {
				moveHistory.setStartPosition(board);
				boardModified = false;
			}
			board.doMove(move);
			moveHistory.addMove(move);
			playedMovesInGame++;
		}

		board.print();

		return !move.isEmpty();
	};

	virtual void undoMove() {
		board = moveHistory.undoMove();
		if (playedMovesInGame > 0) {
			playedMovesInGame--;
		}
	}
	
	/**
	 * Clears the board, setting everything to empty.
	 */
	virtual void clearBoard() {
		board.clear();
		boardModified = true;
		playedMovesInGame = 0;
	}

	/**
	 * Set side to move
	 */
	virtual void setWhiteToMove(bool whiteToMove) {
		board.setWhiteToMove(whiteToMove);
	}

	/**
	 * Gets side to move
	 */
	virtual bool isWhiteToMove() {
		return board.isWhiteToMove();
	}

	/**
	 * Sets a piece to a square
	 */
	virtual void setPiece(uint32_t file, uint32_t rank, char piece) {
		const File castFile = static_cast<File>(file);
		const Rank castRank = static_cast<Rank>(rank);
		if (isFileInBoard(castFile) && isRankInBoard(castRank)) {
			const Square square = computeSquare(castFile, castRank);
			board.setPiece(square, charToPiece(piece));
		}
	}

	/**
	 * Allow or deny white queen side castling
	 */
	virtual void setWhiteQueenSideCastlingRight(bool allow) {
		board.setCastlingRight(WHITE, false, allow);
	}

	/**
	 * Allow or deny white king side castling
	 */
	virtual void setWhiteKingSideCastlingRight(bool allow) {
		board.setCastlingRight(WHITE, true, allow);
	}
	
	/**
	 * Allow or deny black queen side castling
	 */
	virtual void setBlackQueenSideCastlingRight(bool allow) {
		board.setCastlingRight(BLACK, false, allow);
	}
	
	/**
	 * Allow or deny black king side castling
	 */
	virtual void setBlackKingSideCastlingRight(bool allow) {
		board.setCastlingRight(BLACK, true, allow);
	}

	/**
	 * Sets the number of half moves without pawn move or capture
	 */
	virtual void setHalfmovesWithouthPawnMoveOrCapture(uint16_t number) { 
		board.setHalfmovesWithoutPawnMoveOrCapture(number);
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
	virtual uint64_t perft(uint16_t depth, uint32_t verbose = 1) { 
		uint64_t res = ChessSearch::doPerftRec(board, depth, _workerCount, true);
		return res;
	}

	/**
	 * Provides the result of the game
	 */
	virtual GameResult getGameResult() {
		GameResult result = isMate(board);
		if (result == GameResult::NOT_ENDED) {
			if (moveHistory.isDrawByRepetition(board)) {
				result = GameResult::DRAW_BY_REPETITION;
			} else if (board.getHalfmovesWithoutPawnMoveOrCapture() > 100) {
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
	 * Compute a move
	 */
	virtual void computeMove(const ClockSetting& clockSetting, bool verbose = true) {
		curClock = clockSetting;
		curClock.setPlayedMovesInGame(playedMovesInGame);
		computingInfo.setVerbose(verbose);
		iterativeDeepening.searchByIterativeDeepening(board, curClock, computingInfo /*, moveHistory */);
	}

	/**
	 * Creates an exchange version of the computing info structure to send to winboard
	 */
	virtual ComputingInfoExchange getComputingInfo() {
		ComputingInfoExchange exchange;
		exchange.currentConsideredMove = computingInfo.pvMovesStore.getMove(0).getLAN();
		exchange.nodesSearched = computingInfo.nodesSearched;
		exchange.searchDepth = computingInfo.searchDepth;
		exchange.elapsedTimeInMilliseconds = computingInfo.timeControl.getTimeSpentInMilliseconds();
		exchange.totalAmountOfMovesToConcider = computingInfo.totalAmountOfMovesToConcider;
		exchange.movesLeftToConcider = 
			computingInfo.totalAmountOfMovesToConcider - computingInfo.currentMoveNoSearched;
		return exchange;
	}

	/**
	 * Prints the actual Search status information
	 */
	virtual void requestPrintSearchInfo() {
		computingInfo.requestPrintSearchInfo();
	}

	/**
	 * Not standard - request to print the details of eval for the current position to stdout
	 */
	virtual void printEvalInfo() {
		Eval eval;
		eval.printEval(board);
	}

	void callSearch() {
		Search search;
		ComputingInfoExchange exchange;
		ComputingInfo computingInfo(0);
		ClockManager clockManager;

		//search.searchRec(board, computingInfo, clockManager);
	}

	/**
	 * Sets the amount of worker threads working in parallel to the main thread
	 */
	void setWorkerAmount(uint32_t workerCount) {
		_workerCount = workerCount;
	}

	/**
	 * Provides the board object with the current position
	 */
	MoveGenerator& getBoard() {
		return board;
	}

	/**
	 * Find the correct move providing a partial move information
	 */
	static Move findMove(MoveGenerator& board, char movingPieceChar,
		uint32_t departureFile, uint32_t departureRank,
		uint32_t destinationFile, uint32_t destinationRank, char promotePieceChar)
	{
		MoveList moveList;
		Move foundMove;

		uint16_t moveNoFound = 0;
		board.genMovesOfMovingColor(moveList);
		const bool whiteToMove = board.isWhiteToMove();
		Piece promotePiece = charToPiece(whiteToMove ? toupper(promotePieceChar) : tolower(promotePieceChar));
		Piece movingPiece = charToPiece(whiteToMove ? toupper(movingPieceChar) : tolower(movingPieceChar));

		for (uint16_t moveNo = 0; moveNo < moveList.getTotalMoveAmount(); moveNo++) {
			const Move move = moveList[moveNo];

			if ((movingPiece == NO_PIECE || move.getMovingPiece() == movingPiece) &&
				(departureFile == -1 || getFile(move.getDeparture()) == File(departureFile)) &&
				(departureRank == -1 || getRank(move.getDeparture()) == Rank(departureRank)) &&
				(destinationFile == -1 || getFile(move.getDestination()) == File(destinationFile)) &&
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
	GameResult isMate(MoveGenerator& board) {
		GameResult result = GameResult::NOT_ENDED;
		MoveList moveList;
		board.genMovesOfMovingColor(moveList);
		
		if (moveList.getTotalMoveAmount() == 0) {
			if (board.isWhiteToMove()) {
				if (board.isInCheck()) {
					result = GameResult::BLACK_WINS_BY_MATE;
				} else {
					result = GameResult::DRAW_BY_STALEMATE;
				}
			} else {
				if (board.isInCheck()) {
					result = GameResult::WHITE_WINS_BY_MATE;
				} else {
					result = GameResult::DRAW_BY_STALEMATE;
				}
			}
		}
		
		return result;
	}
	
	bool boardModified;
	MoveGenerator board;
	MoveHistory moveHistory;
	uint32_t playedMovesInGame;
	ComputingInfo computingInfo;
	ClockSetting curClock;
	IterativeDeepening iterativeDeepening;
	// WhatIf whatIf;
	uint32_t _workerCount;

};


#endif //__BOARDADAPTER_H