#ifndef __BITBASEGENERATOR_H
#define __BITBASEGENERATOR_H

#include "BitBase.h"
#include "BitBaseIndex.h"

class PositionGenerator {
public:	
	PositionGenerator() {
		pawnAmount = 0;
		pieceAmount = 0;
	}

	void setToFirstPosition() {
		for (currentPieceNo = 0;currentPieceNo < getTotalPieceAmount(); currentPieceNo++) {
			setCurrentPieceToStartPos();
		}
		currentPieceNo = getTotalPieceAmount() - 1;
		bitBaseIndex = 0;
	}

	static const uint32_t MAX_PAWN_AMOUNT = 4;
	static const uint32_t MAX_PIECE_AMOUNT = 4;
	static const uint32_t KING_AMOUNT = 2;
	static const uint32_t FIELD_SIZE = KING_AMOUNT + MAX_PAWN_AMOUNT + MAX_PIECE_AMOUNT;

	uint32_t getTotalPieceAmount() {
		return KING_AMOUNT + pawnAmount + pieceAmount;
	}

	bool nextPosition() {
		bool result = true;
		piecePos[currentPieceNo] = nextPosOfCurrentPiece();
		while (piecePos[currentPieceNo] == BOARD_SIZE) {
			if (currentPieceNo > 0) {
				currentPieceNo--;
				piecePos[currentPieceNo] = nextPosOfCurrentPiece();
			} else {
				result = false;
				break;
			}
		}
		while (currentPieceNo < getTotalPieceAmount() - 1) {
			currentPieceNo++;
			setCurrentPieceToStartPos();
		}
		return result;
	}

	void setPiecesToBoard(GenBoard& board) {
		board.setPieceToPosition(getWhiteKingPos(), WHITE_KING);
		board.setPieceToPosition(getBlackKingPos(), BLACK_KING);
		for (uint32_t index = 2; index < getTotalPieceAmount(); index++) {
			board.setPieceToPosition(getPiecePos(index), pieceTable[index - 2]);
		}
	}

	void removePiecesFromBoard(GenBoard& board) {
		board.removePieceFromPosition(getWhiteKingPos());
		board.removePieceFromPosition(getBlackKingPos());
		for (uint32_t index = 2; index < getTotalPieceAmount(); index++) {
			board.removePieceFromPosition(getPiecePos(index));
		}
	}

	void addPiece(piece_t piece) {
		pieceTable[getTotalPieceAmount() - 2] = piece;
		if (piece == WHITE_PAWN || piece == BLACK_PAWN) {
			pawnAmount++;
		} else {
			pieceAmount++;
		}
	}

	pos_t getWhiteKingPos() { return piecePos[0]; }
	pos_t getBlackKingPos() { return piecePos[1]; }
	pos_t getPiecePos(uint32_t index) { return piecePos[index]; }
	uint64_t getIndex() { return bitBaseIndex; }
	piece_t pieceTable[MAX_PAWN_AMOUNT + MAX_PIECE_AMOUNT];

private:
	bool isPositionUsed(pos_t position) {
		bool result = false;
		for (uint32_t index = 0; index < currentPieceNo; index++) {
			if (piecePos[index] == position) {
				result = true;
				break;
			}
		}
		return result;
	}

	pos_t nextPawnPos(pos_t currentPos) {
		do {
			if (currentPos == BOARD_SIZE) {
				currentPos = POS_A2;
			} else if (currentPos == POS_H7) {
				currentPos = BOARD_SIZE;
				if (currentPieceNo == getTotalPieceAmount() - 1) bitBaseIndex += BitBaseIndex::COLORS;
			} else {
				currentPos ++;
				if (currentPieceNo == getTotalPieceAmount() - 1) bitBaseIndex += BitBaseIndex::COLORS;
			}
		} while (currentPos < BOARD_SIZE && isPositionUsed(currentPos));
		return currentPos;
	}

	pos_t nextPiecePos(pos_t currentPos) {
		do {
			if (currentPos >= BOARD_SIZE) {
				currentPos = POS_A1;
			} else {
				currentPos++;
				if (currentPieceNo == getTotalPieceAmount() - 1) bitBaseIndex += BitBaseIndex::COLORS;
			}
		} while(currentPos < BOARD_SIZE && isPositionUsed(currentPos));
		return currentPos;
	}

	pos_t nextFirstKingPos(pos_t currentPos) {
		if (currentPos >= BOARD_SIZE) {
			currentPos = POS_A1;
		} else {
			currentPos = BitBaseIndex::computeNextKingPosForPositionsWithPawn(currentPos); 
		}
		return currentPos;
	}

	pos_t nextSecondKingPos(pos_t currentPos) {
		if (currentPos >= BOARD_SIZE) {
			currentPos = POS_A1;
		} else {
			currentPos++;
		}
		while (currentPos <= POS_H8 && BitBaseIndex::isAdjacent(getWhiteKingPos(), currentPos)) {
			currentPos++;
		}
		return currentPos;
	}

	pos_t nextPosOfCurrentPiece() {
		pos_t currentPos = piecePos[currentPieceNo];
		pos_t result = BOARD_SIZE;
		if (currentPieceNo == 0) {
			result = nextFirstKingPos(currentPos); 
		} else if (currentPieceNo == 1) {
			result = nextSecondKingPos(piecePos[currentPieceNo]);
		} else if (currentPieceNo < pawnAmount + KING_AMOUNT) {
			result = nextPawnPos(currentPos);
		} else {
			result = nextPiecePos(currentPos); 
		}
		return result;
	}

	pos_t setCurrentPieceToStartPos() {
		pos_t currentPos = piecePos[currentPieceNo];
		pos_t result = BOARD_SIZE;
		if (currentPieceNo == 0) {
			piecePos[currentPieceNo] = POS_A1;
		} else if (currentPieceNo == 1) {
			piecePos[currentPieceNo] = nextSecondKingPos(BOARD_SIZE);
		} else if (currentPieceNo < pawnAmount + KING_AMOUNT) {
			piecePos[currentPieceNo] = nextPawnPos(BOARD_SIZE);
		} else {
			piecePos[currentPieceNo] = nextPiecePos(BOARD_SIZE);
		}
		return result;
	}

	pos_t piecePos[FIELD_SIZE];

	uint32_t currentPieceNo;
	uint32_t pawnAmount;
	uint32_t pieceAmount;
	uint64_t bitBaseIndex;

};

class BitBaseGenerator {
public:
	BitBaseGenerator() {
	}

	static value_t search(GenBoard& board, Eval& eval, move_t lastMove, int32_t depth, value_t alpha, value_t beta) 
	{
		MoveProvider moveProvider;
		move_t move;
		BoardInfo boardInfo = board.boardInfo;
		if (lastMove != 0) {
			board.doMove(lastMove);
		}
		value_t newValue;
		value_t curValue = eval.evaluateBoardPosition(board, -MAX_VALUE);
		if (!board.whiteToMove) {
			curValue = -curValue;
		}

		if (depth > 0) {
			moveProvider.computeMoves(board, Move::EMPTY_MOVE, 0);
			if (moveProvider.getTotalMoveAmount() == 0) {
				curValue = moveProvider.checkForGameEnd(board, 0);
			}
		} else {
			moveProvider.computeCaptures(board, Move::EMPTY_MOVE);
		}
		
		if (curValue < beta) {
			if (curValue > alpha) {
				alpha = curValue;
			}

			while (move = moveProvider.selectNextMove(board)) {
				
				if (Move::getCapturedPiece(move) == BLACK_KING) {
					curValue = 0;
					break;
				}
				newValue = -search(board, eval, move, -beta, -alpha, depth - 1);
				
				if (newValue > curValue) {
					curValue = newValue;
					if (newValue >= beta) {
						break;
					}
				}
			}
		}

		if (lastMove != 0) {
			board.undoMove(lastMove);
			board.boardInfo = boardInfo;
		}
		return curValue;

	}


	bool computeValue(GenBoard& board, BitBase& bitBase) {
		MoveList moveList;
		move_t move;
		bool result = board.whiteToMove ? false : true;
		BoardInfo info;
		bool value;

		board.genMovesOfMovingColor(moveList);
		if (moveList.getMoveAmount() == 0 && !board.isInCheck()) {
			result = false;
		}
		for (uint32_t moveNo = 0; moveNo < moveList.getMoveAmount(); moveNo++) {
			move = moveList[moveNo];
			if (Move::getCapturedPiece(move) != WHITE_KING && Move::getCapturedPiece(move) != BLACK_KING) {
				info = board.boardInfo;
				board.doMove(move);
				value = false;
				if (board.getPieceBitBoard(WHITE_PAWN) != 0) {
					uint64_t index = BitBaseIndex::kpkIndex(board);
					value = bitBase.getBit(index);
				}
				board.undoMove(move);
				board.boardInfo = info;
				if (board.whiteToMove && value) {
					result = true;
					break;
				}
				if (!board.whiteToMove && !value) {
					result = false;
					break;
				}
			}
		}
		return result;
	}

	bool isPawnPromoting(pos_t whiteKingPos, pos_t blackKingPos, pos_t whitePawnPos) {
		bool result = false;
		if (whitePawnPos >= POS_A7) {
			result = !BitBaseIndex::isAdjacent(whitePawnPos + BOARD_COL_AMOUNT, blackKingPos) || 
				BitBaseIndex::isAdjacent(whitePawnPos + BOARD_COL_AMOUNT, whiteKingPos);
		}
		return result;
	}


	uint32_t computePosition(GenBoard& board, BitBase& bitBase, uint64_t index) {
		uint32_t result = 0;
		board.setWhiteToMove(true);
		if (!bitBase.getBit(index) && computeValue(board, bitBase)) { 
			bitBase.setBit(index);
			result++;
		}
		board.setWhiteToMove(false);
		if (!bitBase.getBit(index + 1) && computeValue(board, bitBase)) {
			bitBase.setBit(index + 1);
			result++;
		}
		return result;
	}



	uint32_t initalSetBitbase(GenBoard& board, Eval& eval, BitBase& bitBase, uint64_t index, PositionGenerator& positionGenerator) {
		uint32_t bitsChanged = 0;
		if (!bitBase.getBit(index))
		{
			//bool won = isPawnPromoting(positionGenerator.getWhiteKingPos(), positionGenerator.getBlackKingPos(), positionGenerator.getPiecePos(2));
			bool won = search(board, eval, 0, 1, 0, WINNING_BONUS) >= WINNING_BONUS;
			if (won) {
				bitBase.setBit(index);
				bitsChanged++;
			}
		}
		return bitsChanged;
	}

	void printInfo(uint32_t loop, uint64_t index, uint64_t size, uint32_t bitsChanged) {
		if (index % 100000 == 0) {
			printf("loop %ld, index %lld/%lld, bits changed %ld\n", loop, index, size, bitsChanged);
		}
	}

	void computeKKPBitbase() {
		GenBoard board;
		Eval eval;
		uint64_t index;
		uint32_t bitsChanged;
		uint64_t bitBaseSize = BitBaseIndex::computeKRKPSize();
		BitBase bitBase(bitBaseSize);
		PositionGenerator positionGenerator;
		positionGenerator.addPiece(BLACK_PAWN);
		positionGenerator.addPiece(WHITE_ROOK);
		board.clear();
		for (uint32_t loopCount = 0; loopCount < 50; loopCount++) {
			positionGenerator.setToFirstPosition();
			bitsChanged = 0;
			do {
				positionGenerator.setPiecesToBoard(board);
				index = positionGenerator.getIndex();
				printInfo(loopCount, index, bitBaseSize, bitsChanged);
				if (loopCount == 0) {
					bitsChanged += initalSetBitbase(board, eval, bitBase, index, positionGenerator);
				} else {
					bitsChanged += computePosition(board, bitBase, index);
				}
				positionGenerator.removePiecesFromBoard(board);
			} while (positionGenerator.nextPosition());

			printf("Loop: %ld, positions set: %ld\n", loopCount, bitsChanged);
			if (bitsChanged == 0) {
				break;
			}
		}
		bitBase.storeToFile("KRKP.btb");
	}


};


#endif // __BITBASEGENERATOR_H