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

 /*
#include "MoveHistory.h"
#include "IterativeDeepening.h"
#include "MoveConverter.h"
#include "WhatIf.h"
*/

using namespace ChessMoveGenerator;
using namespace ChessInterface;
using namespace ChessEval;
using namespace ChessSearch;

class BoardAdapter: public IChessBoard {
public:
	BoardAdapter(/* ISendSearchInfo* sendInfo */) 
		: boardModified(true) //, thinkingInfo(sendInfo) 
	{
		// board::initStatics();
	}

	virtual IWhatIf* getWhatIf() { 
		// WhatIf::whatIf.setBoard(board);
		// return &WhatIf::whatIf;
		return 0;
	}
		
    virtual bool doMove(char movingPiece,
			uint32_t departureFile, uint32_t departureRank,
			uint32_t destinationFile, uint32_t destinationRank,
			char promotePiece)
	{
		Move move = findMove(movingPiece, departureFile, departureRank, destinationFile, destinationRank, promotePiece);
		
		if (!move.isEmpty()) {
			if (boardModified) {
				// moveHistory.setStartPosition(board);
				boardModified = false;
			}
			board.doMove(move);
			// moveHistory.doMove(move);
			playedMovesInGame++;
		}

		//board.printBoard();

		return !move.isEmpty();
	};

	virtual void undoMove() {
		/*
		board = moveHistory.undoMove();
		if (playedMovesInGame > 0) {
			playedMovesInGame--;
		}
		*/
	}
	
	// Clears the board, setting everything to empty.
	virtual void clearBoard() {
		board.clear();
		boardModified = true;
		playedMovesInGame = 0;
	}

	virtual void setWhiteToMove(bool whiteToMove) {
		board.setWhiteToMove(whiteToMove);
	}

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

	virtual void setWhiteQueenSideCastlingRight(bool allow) {
		board.setCastlingRight(WHITE, false, allow);
	}

	virtual void setWhiteKingSideCastlingRight(bool allow) {
		board.setCastlingRight(WHITE, true, allow);
	}
	
	virtual void setBlackQueenSideCastlingRight(bool allow) {
		board.setCastlingRight(BLACK, false, allow);
	}
	
	virtual void setBlackKingSideCastlingRight(bool allow) {
		board.setCastlingRight(BLACK, true, allow);
	}

	virtual void setHalfmovesWithouthPawnMoveOrCapture(uint16_t moves) { 
		//board.setHalfmovesWithoutPawnMoveOrCapture(moves);
	}

	virtual void setPlayedMovesInGame(uint16_t moves) {
		playedMovesInGame = moves;
	}

	virtual uint64_t perft(uint16_t depth, uint32_t verbose = 1) { 
		// uint64_t res = ChessSearch::doPerftIter(board, depth, verbose);
		uint64_t res = ChessSearch::doPerftRec(board, depth, _workerCount, true);
		return res;
	}

	virtual GameResult getGameResult() {
		/*
		int8_t result = isMate(board);
		if (result == GameResult::GAME_NOT_ENDED) {
			if (moveHistory.isDrawByRepetition(board)) {
				result = DRAW_BY_REPETITION;
			} else if (board.boardInfo.getHalfMovesWithoutPawnMovedOrPieceCaptured() > 100) {
				result = DRAW_BY_50_MOVES_RULE;
			}
		}

		return result;
		*/
		return GameResult::NOT_ENDED;
	}

	virtual void moveNow() {
		// iterativeDeepening.stopSearch();
	}

	virtual void computeMove(const ClockSetting& clockSetting, bool verbose = true) {
		/*
		curClock = clockSetting;
		curClock.setPlayedMovesInGame(playedMovesInGame);
		thinkingInfo.setVerbose(verbose);
		iterativeDeepening.searchByIterativeDeepening(board, curClock, thinkingInfo, moveHistory);
		*/
	}

	virtual ComputingInfoExchange getComputingInfo() {
		ComputingInfoExchange computingInfo;
		/*
		computingInfo.currentConsideredMove = move.getLAN(thinkingInfo.pvMovesStore[0]);
		computingInfo.nodesSearched = thinkingInfo.nodesSearched;
		computingInfo.searchDepth = thinkingInfo.searchDepth;
		computingInfo.elapsedTimeInMilliseconds = thinkingInfo.timeControl.getTimeSpentInMilliseconds();
		computingInfo.totalAmountOfMovesToConcider = thinkingInfo.amountOfMovesToSearch;
		computingInfo.movesLeftToConcider = thinkingInfo.amountOfMovesToSearch - thinkingInfo.currentMoveSearched;
		*/
		return computingInfo;
	}

	virtual void requestPrintSearchInfo() {
		// thinkingInfo.requestPrintSearchInfo();
	}

	virtual void printEvalInfo() {
		Eval eval;
		eval.printEval(board);
	}

	void callSearch() {
		Search search;
		ComputingInfoExchange exchange;
		ComputingInfo computingInfo(0);
		ClockManager clockManager;

		// search.searchRec(board, computingInfo, clockManager);
	}

	/**
	 * Sets the amount of worker threads working in parallel to the main thread
	 */
	void setWorkerAmount(uint32_t workerCount) {
		_workerCount = workerCount;
	}

	MoveGenerator& getBoard() {
		return board;
	}

private:
	Move findMove(char movingPieceChar, 
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

	/*
	GameResult isMate(board& board) {
		GameResult result = GameResult::NOT_ENDED;
		MoveList moveList;
		board.genMovesOfMovingColor(moveList);
		
		if (moveList.getMoveAmount() == 0) {
			if (board.whiteToMove) {
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
	*/
	bool boardModified;
	MoveGenerator board;
	// MoveHistory moveHistory;
	uint32_t playedMovesInGame;
	// ThinkingInformation thinkingInfo;
	ClockSetting curClock;
	// IterativeDeepening iterativeDeepening;
	// WhatIf whatIf;
	uint32_t _workerCount;

};


#endif //__BOARDADAPTER_H