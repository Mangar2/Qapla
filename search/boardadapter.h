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

class BoardAdapter: public IChessBoard {
public:
	BoardAdapter(/* ISendSearchInfo* sendInfo */) 
		: boardModified(true) //, thinkingInfo(sendInfo) 
	{
		// GenBoard::initStatics();
	}

	virtual IWhatIf* getWhatIf() { 
		// WhatIf::whatIf.setBoard(genBoard);
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
				// moveHistory.setStartPosition(genBoard);
				boardModified = false;
			}
			genBoard.doMove(move);
			// moveHistory.doMove(move);
			playedMovesInGame++;
		}

		//genBoard.printBoard();

		return !move.isEmpty();
	};

	virtual void undoMove() {
		/*
		genBoard = moveHistory.undoMove();
		if (playedMovesInGame > 0) {
			playedMovesInGame--;
		}
		*/
	}
	
	// Clears the genBoard, setting everything to empty.
	virtual void clearBoard() {
		genBoard.clear();
		boardModified = true;
		playedMovesInGame = 0;
	}

	virtual void setWhiteToMove(bool whiteToMove) {
		genBoard.setWhiteToMove(whiteToMove);
	}

	virtual bool isWhiteToMove() {
		return genBoard.isWhiteToMove();
	}

	/**
	 * Sets a piece to a square
	 */
	virtual void setPiece(uint32_t file, uint32_t rank, char piece) {
		const File castFile = static_cast<File>(file);
		const Rank castRank = static_cast<Rank>(rank);
		if (isFileInBoard(castFile) && isRankInBoard(castRank)) {
			const Square square = computeSquare(castFile, castRank);
			genBoard.setPiece(square, charToPiece(piece));
		}
	}

	virtual void setWhiteQueenSideCastlingRight(bool allow) {
		genBoard.setCastlingRight(WHITE, false, allow);
	}

	virtual void setWhiteKingSideCastlingRight(bool allow) {
		genBoard.setCastlingRight(WHITE, true, allow);
	}
	
	virtual void setBlackQueenSideCastlingRight(bool allow) {
		genBoard.setCastlingRight(BLACK, false, allow);
	}
	
	virtual void setBlackKingSideCastlingRight(bool allow) {
		genBoard.setCastlingRight(BLACK, true, allow);
	}

	virtual void setHalfmovesWithouthPawnMoveOrCapture(uint16_t moves) { 
		//genBoard.setHalfmovesWithoutPawnMoveOrCapture(moves);
	}

	virtual void setPlayedMovesInGame(uint16_t moves) {
		playedMovesInGame = moves;
	}

	virtual uint64_t perft(uint16_t depth, uint32_t verbose = 1) { 
		// uint64_t res = ChessSearch::doPerftIter(genBoard, depth, verbose);
		uint64_t res = ChessSearch::doPerftRec(genBoard, depth, _workerCount, true);
		return res;
	}

	virtual GameResult getGameResult() {
		/*
		int8_t result = isMate(genBoard);
		if (result == GameResult::GAME_NOT_ENDED) {
			if (moveHistory.isDrawByRepetition(genBoard)) {
				result = DRAW_BY_REPETITION;
			} else if (genBoard.boardInfo.getHalfMovesWithoutPawnMovedOrPieceCaptured() > 100) {
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
		iterativeDeepening.searchByIterativeDeepening(genBoard, curClock, thinkingInfo, moveHistory);
		*/
	}

	virtual ComputingInfo getComputingInfo() {
		ComputingInfo computingInfo;
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
		eval.printEval(genBoard);
	}

	/**
	 * Sets the amount of worker threads working in parallel to the main thread
	 */
	void setWorkerAmount(uint32_t workerCount) {
		_workerCount = workerCount;
	}

	/*
	GenBoard& getBoard() {
		return genBoard;
	}
	*/

private:
	Move findMove(char movingPieceChar, 
		uint32_t departureFile, uint32_t departureRank,
		uint32_t destinationFile, uint32_t destinationRank, char promotePieceChar)
	{
		MoveList moveList;
		Move foundMove;

		uint16_t moveNoFound = 0;
		genBoard.genMovesOfMovingColor(moveList);
		const bool whiteToMove = genBoard.isWhiteToMove();
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
	GameResult isMate(GenBoard& genBoard) {
		GameResult result = GameResult::NOT_ENDED;
		MoveList moveList;
		genBoard.genMovesOfMovingColor(moveList);
		
		if (moveList.getMoveAmount() == 0) {
			if (genBoard.whiteToMove) {
				if (genBoard.isInCheck()) {
					result = GameResult::BLACK_WINS_BY_MATE;
				} else {
					result = GameResult::DRAW_BY_STALEMATE;
				}
			} else {
				if (genBoard.isInCheck()) {
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
	MoveGenerator genBoard;
	// MoveHistory moveHistory;
	uint32_t playedMovesInGame;
	// ThinkingInformation thinkingInfo;
	ClockSetting curClock;
	// IterativeDeepening iterativeDeepening;
	// WhatIf whatIf;
	uint32_t _workerCount;

};


#endif //__BOARDADAPTER_H