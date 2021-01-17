#ifndef __BITBASEGENERATOR_H
#define __BITBASEGENERATOR_H

#include "BitBase.h"
#include "BitBaseIndex.h"
#include "QuiescenceSearch.h"
#include "ThinkingTimeManager.h"
#include <iostream>
using namespace std;

class BitBaseGenerator {
public:
	//static const uint64_t debugIndex =  2194203;
	static const uint64_t debugIndex = 0xFFFFFFFFFFFFFFFF; 
	BitBaseGenerator() {
	}

	static value_t search(GenBoard& board, Eval& eval, move_t lastMove, value_t alpha, value_t beta, int32_t depth) 
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

	value_t initialSearch(GenBoard& board)
	{
		MoveProvider moveProvider;
		move_t move;

		value_t positionValue = 0;
		value_t result = 0;
		BoardInfo boardInfo = board.boardInfo;
		
		board.computeAttackMask();
		moveProvider.computeCaptures(board, Move::EMPTY_MOVE);

		while (move = moveProvider.selectNextMove(board)) {
			if (!Move::isCaptureOrPromoteMove(move)) {
				continue;
			}
			board.doMove(move);
			positionValue = BitBaseReader::getValueFromBitBase(board, board.pieceSignature, 0);
			board.undoMove(move);
			board.boardInfo = boardInfo;
				
			if (board.whiteToMove && positionValue >= WINNING_BONUS) {
				result = 1;
				break;
			}
			if (!board.whiteToMove && positionValue < WINNING_BONUS) {
				result = -1;
				break;
			}
		}
		return result;
	}


	value_t quiescense(GenBoard& board) {
		value_t result = 0;
		ThinkingInformation thinkingInfo(NULL);
		board.computeAttackMask();
		value_t positionValue = QuiescenceSearch::search(board, eval,  thinkingInfo, Move::EMPTY_MOVE, -MAX_VALUE, MAX_VALUE, 0);
		if (!board.whiteToMove) {
			positionValue = -positionValue;
		}
		if (positionValue > WINNING_BONUS) { 
			result = 1;
		} 
	
		else if (positionValue <= 1) {
			result = -1;
		} else {
			result = 0;
		}
		
		return result;
	}


	bool computeValue(GenBoard& board, BitBase& bitBase, bool verbose) {
		MoveList moveList;
		move_t move;
		bool whiteToMove = board.whiteToMove;
		bool result = board.whiteToMove ? false : true;
		uint64_t index = 0;
		PieceList pieceList(board);

		if (verbose) {
			board.printBoard();
			printf("%s\n", board.whiteToMove ? "white" : "black");
		}

		board.genMovesOfMovingColor(moveList);

		for (uint32_t moveNo = 0; moveNo < moveList.getMoveAmount(); moveNo++) {
			move = moveList[moveNo];
			if (!Move::isCaptureOrPromoteMove(move)) {
				index = BoardAccess::getIndex(!whiteToMove, pieceList, move);
				result = bitBase.getBit(index);
				if (verbose) {
					printf("%s, index: %lld, value: %s\n", Move::getLAN(move).getCharBuffer(), index, result ? "win" : "draw or unknown");
				}
			} 
			if (whiteToMove && result) {
				break;
			}
			if (!whiteToMove && !result) {
				break;
			}

		}
		return result;
	}

	uint32_t computePosition(uint64_t index, GenBoard& board, BitBase& bitBase, BitBase& computedPositions) {
		uint32_t result = 0;
		if (computeValue(board, bitBase, false)) { 
			if (index == debugIndex) {
				computeValue(board, bitBase, true);
			}
			bitBase.setBit(index);
			computedPositions.setBit(index);
			result++;
		}

		return result;
	}

	static bool isKing(piece_t piece) {
		return (piece & ~COLOR_MASK) == KING;
	}

	bool kingCaptureFound(MoveList& moveList) {
		bool result = false;
		for (uint32_t index = 0; index < moveList.getMoveAmount(); index++) {
			if (isKing(Move::getCapturedPiece(moveList[index]))) {
				return true;
				break;
			}
		}
		return result;
	}

	uint32_t initialComputePosition(uint64_t index, GenBoard& board, Eval& eval, BitBase& bitBase, BitBase& computedPositions) {
		uint32_t result = 0;
		MoveList moveList;
		bool kingInCheck = false;
		if (index == debugIndex) {
			kingInCheck = false;
		}

		if (board.isIllegalPosition2()) {
			amountOfIllegalPositions++;
			computedPositions.setBit(index);
			return 0;
		}

		board.genMovesOfMovingColor(moveList);
		if (moveList.getMoveAmount() > 0) {
			value_t positionValue;
			
			positionValue = initialSearch(board);
			if (positionValue == 1) {
				bitBase.setBit(index);
				computedPositions.setBit(index);
				result++;
			} else if (positionValue == -1) {
				computedPositions.setBit(index);
				amountOfDirectDrawOrLoss++;
			}
		} else {
			computedPositions.setBit(index);
			if (!board.whiteToMove && board.isInCheck()) {
				if (index == debugIndex) {
					board.printBoard();
					printf("index: %lld, mate detected\n", index);
				}
				bitBase.setBit(index);
				result++;
			} else {
				amountOfDirectDrawOrLoss++;
			}
		}
		return result;
	}


	void printInfo(uint64_t index, uint64_t size, uint32_t bitsChanged, uint32_t step = 1) {
		uint64_t onePercent = size / 100;
		if (index % onePercent >= onePercent - step) {
			printf(".");
		}
	}

	void addPiecesToBoard(GenBoard& board, const BitBaseIndex& bitBaseIndex, const PieceList& pieceList) {
		board.setPieceToPosition(bitBaseIndex.getPiecePos(0), WHITE_KING);
		board.setPieceToPosition(bitBaseIndex.getPiecePos(1), BLACK_KING);
		for (uint32_t pieceNo = 0; pieceNo < pieceList.getPieceAmount(); pieceNo ++) {
			board.setPieceToPosition(bitBaseIndex.getPiecePos(2 + pieceNo), pieceList.getPiece(pieceNo));
		}
		board.whiteToMove = bitBaseIndex.getWhiteToMove();
	}

	void removePiecesFromBoard(GenBoard& board, const BitBaseIndex& bitBaseIndex) {
		for (uint32_t pieceNo = 0; pieceNo < bitBaseIndex.getPieceAmount(); pieceNo++) {
			board.removePieceFromPosition(bitBaseIndex.getPiecePos(pieceNo));
		}
	}

	void compareBitbases(const char* pieceString, BitBase& bitbase1, BitBase& bitbase2) {
		GenBoard board;
		PieceList pieceList(pieceString);
		uint64_t sizeInBit = bitbase1.getSizeInBit();
		for (uint64_t index = 0; index < sizeInBit; index++) {
			bool newResult = bitbase1.getBit(index);
			bool oldResult = bitbase2.getBit(index);
			if (bitbase1.getBit(index) != bitbase2.getBit(index)) {
				BitBaseIndex bitBaseIndex;
				bitBaseIndex.setPiecePositionsByIndex(index, pieceList);
				addPiecesToBoard(board, bitBaseIndex, pieceList);
				printf("new: %s, old: %s\n", newResult ? "won" : "not won", oldResult ? "won" : "not won");
				printf("index: %lld\n", index);
				board.printBoard();
				board.clear();
			}
		}
	}

	void compareFiles(string pieceString) {
		BitBase bitBase1;
		BitBase bitBase2;
		bitBase1.readFromFile(pieceString);
		bitBase2.readFromFile("generated\\" + pieceString);
		compareBitbases(pieceString.c_str(), bitBase1, bitBase2);
	}

	void printTimeSpent(ThinkingTimeManager& timeManager) {
		uint64_t timeInMilliseconds = timeManager.getTimeSpentInMilliseconds();
		printf("Time spend: %lld:%lld:%lld.%lld\n", 
			timeInMilliseconds / (60 * 60 * 1000), 
			(timeInMilliseconds / (60 * 1000)) % 60, 
			(timeInMilliseconds / 1000) % 60, 
			timeInMilliseconds % 1000);
	}


	void computeBitbase(const char* pieceString) {
		PieceList pieceList(pieceString);
		computeBitbaseRec(pieceList);
	}

	void computeBitbaseRec(PieceList& pieceList) {
		string pieceString = pieceList.getPieceString();
		if (!BitBaseReader::isBitBaseAvailable(pieceString)) {		
			BitBaseReader::loadBitBase(pieceString);
		}
		if (!BitBaseReader::isBitBaseAvailable(pieceString)) {		
			if (pieceList.getPieceAmount() >= 1) {
				for(uint32_t pieceNo = 0; pieceNo < pieceList.getPieceAmount(); pieceNo++) {
					PieceList newPieceList(pieceList);
					if (isPawn(newPieceList.getPiece(pieceNo))) {
						for (piece_t piece = QUEEN; piece >= KNIGHT; piece -= 2) {
							newPieceList.promotePawn(pieceNo, piece);
							computeBitbaseRec(newPieceList);
							newPieceList = pieceList;
						}
					}
					newPieceList.removePiece(pieceNo);
					computeBitbaseRec(newPieceList);
				}
			}
			computeBitbase(pieceList);
		}
	}

	void computeBitbase(PieceList& pieceList) {
		GenBoard board;
		string pieceString = pieceList.getPieceString();
		printf("Computing bitbase for %s\n", pieceString.c_str());
		if (BitBaseReader::isBitBaseAvailable(pieceString)) {
			printf("Bitbase already loaded\n");
			return;
		}
		BitBaseIndex bitBaseIndex;
		uint32_t bitsChanged = 0;
		bitBaseIndex.setPiecePositionsByIndex(0, pieceList);
		uint64_t sizeInBit = bitBaseIndex.getSizeInBit();
		BitBase bitBase(bitBaseIndex);
		BitBase computedPositions(bitBaseIndex);
		ThinkingTimeManager timeManager;

		timeManager.startTimer();
		board.clear();
		amountOfIllegalPositions = 0;
		amountOfDirectDrawOrLoss = 0;
		for (uint64_t index = 0; index < sizeInBit; index ++) {
			printInfo(index, sizeInBit, bitsChanged);
			if (bitBaseIndex.setPiecePositionsByIndex(index, pieceList)) {
				addPiecesToBoard(board, bitBaseIndex, pieceList);
				bitsChanged += initialComputePosition(index, board, eval, bitBase, computedPositions);
				removePiecesFromBoard(board, bitBaseIndex);
			}
		} 
		printf("\nInitial, positions set: %ld\n", bitsChanged);
		printTimeSpent(timeManager);

		for (uint32_t loopCount = 0; loopCount < 100; loopCount++) {
			bitsChanged = 0;
			for (uint64_t index = 0; index < sizeInBit; index ++) {
				printInfo(index, sizeInBit, bitsChanged);

				if (!computedPositions.getBit(index) && bitBaseIndex.setPiecePositionsByIndex(index, pieceList)) {
					addPiecesToBoard(board, bitBaseIndex, pieceList);
					assert(index == BoardAccess::getIndex(board));
					bitsChanged += computePosition(index, board, bitBase, computedPositions);
					//if (bitsChanged == 0) { board.printBoard(); }
					removePiecesFromBoard(board, bitBaseIndex);
				}
			} 

			printf("\nLoop: %ld, positions set: %ld\n", loopCount, bitsChanged);
			printTimeSpent(timeManager);
			if (bitsChanged == 0) {
				break;
			}
		}
		//compareToFile(pieceString, bitBase);
		bitBase.printStatistic();
		printf("Illegal positions: %lld, draw or loss found in quiescense: %lld, sum: %lld \n", 
			amountOfIllegalPositions, amountOfDirectDrawOrLoss, amountOfDirectDrawOrLoss + amountOfIllegalPositions);
		printTimeSpent(timeManager);
		string fileName = pieceString + string(".btb");
		bitBase.storeToFile(fileName.c_str());
		BitBaseReader::setBitBase(pieceString, bitBase);
		//testAllKPK(bitBase);
	}

	void testBoard(GenBoard& board, pos_t wk, pos_t bk, bool wtm, bool expected, BitBase& bitBase) {
		board.setPieceToPosition(wk, WHITE_KING);
		board.setPieceToPosition(bk, BLACK_KING);
		board.whiteToMove = wtm;
		uint64_t checkIndex = BoardAccess::getIndex(board);
		if (bitBase.getBit(checkIndex) == expected) {
			printf("Test OK\n"); 
		}
		else {
			printf("%s, index:%lld\n", board.whiteToMove ? "white" : "black", checkIndex);
			printf("Test failed\n");
			board.printBoard();
			computeValue(board, bitBase, true);
			showDebugIndex("KRKP");
		}
	}

	struct piecePos {
		piece_t piece;
		pos_t pos;
	};

	void testKQKR(pos_t wk, pos_t bk, pos_t q, pos_t r, bool wtm, bool expected, BitBase& bitBase) {
		GenBoard board;
		board.setPieceToPosition(q, WHITE_QUEEN);
		board.setPieceToPosition(r, BLACK_ROOK);
		testBoard(board, wk, bk, wtm, expected, bitBase);
	}

	void testKRKQ(pos_t wk, pos_t bk, pos_t r, pos_t q, bool wtm, bool expected, BitBase& bitBase) {
		GenBoard board;
		board.setPieceToPosition(q, BLACK_QUEEN);
		board.setPieceToPosition(r, WHITE_ROOK);
		testBoard(board, wk, bk, wtm, expected, bitBase);
	}


	void testAllKQKR() {
		BitBase bitBase;
		bitBase.readFromFile("KQKR.btb_generated");
		testKQKR(POS_B7, POS_B1, POS_C8, POS_A2, false, false, bitBase);
		testKQKR(POS_B7, POS_B1, POS_D8, POS_A2, false, true, bitBase);
		testKQKR(POS_B7, POS_B1, POS_C8, POS_A3, false, true, bitBase);
		testKQKR(POS_B7, POS_B1, POS_C8, POS_A2, true, true, bitBase);
	}


	void testKPK(pos_t wk, pos_t bk, pos_t p, bool wtm, bool expected, BitBase& bitBase) {
		GenBoard board;
		board.setPieceToPosition(p, WHITE_PAWN);
		testBoard(board, wk, bk, wtm, expected, bitBase);
	}

	void testAllKPK(BitBase& bitBase) {
		testKPK(POS_E5, POS_E8, POS_E7, false, false, bitBase);
		testKPK(POS_D6, POS_E8, POS_E7, true, false, bitBase);
		testKPK(POS_A2, POS_A5, POS_E2, true, true, bitBase);
		testKPK(POS_A2, POS_A5, POS_E2, false, false, bitBase);
		testKPK(POS_A3, POS_A5, POS_E2, true, false, bitBase);
		testKPK(POS_A3, POS_A5, POS_E2, false, true, bitBase);
	}

	void testAllKPK() {
		BitBase bitBase;
		bitBase.readFromFile("KPK");
		testAllKPK(bitBase);
	}

	void testKRKP(pos_t wk, pos_t bk, pos_t p, pos_t r, bool wtm, bool expected, BitBase& bitBase) {
		GenBoard board;
		board.setPieceToPosition(wk, WHITE_KING);
		board.setPieceToPosition(bk, BLACK_KING);
		board.setPieceToPosition(p, BLACK_PAWN);
		board.setPieceToPosition(r, WHITE_ROOK);
		board.whiteToMove = wtm;
		uint64_t checkIndex = BoardAccess::getIndex(board);
		if (bitBase.getBit(checkIndex) == expected) {
			printf("Test OK\n"); 
		}
		else {
			board.printBoard();
			printf("%s, index:%lld\n", board.whiteToMove ? "white" : "black", checkIndex);
			printf("Test failed\n");
			computeValue(board, bitBase, true);
			showDebugIndex("KRKP");
			
		}
	}

	void showDebugIndex(string pieceString) {
		GenBoard board;
		BitBase bitBase;
		bitBase.readFromFile(pieceString);
		PieceList pieceList(pieceString.c_str());
		BitBaseIndex bitBaseIndex;
		uint32_t index;
		while (true) {
			cin >> index;
			printf("%ld\n", index);		
			if (bitBaseIndex.setPiecePositionsByIndex(index, pieceList)) {
				board.clear();
				addPiecesToBoard(board, bitBaseIndex, pieceList);
				printf("index:%lld, result for white %s\n", index, bitBase.getBit(index) ? "win" : "draw");
				
				computeValue(board, bitBase, true);
			} else {
				break;
			}
		} 
	}


	void testAllKRKP() {
		BitBase bitBase;
		bitBase.readFromFile("KRKP");
		testKRKP(POS_F2, POS_D4, POS_F6, POS_F3, true, true, bitBase);
		testKRKP(POS_F2, POS_D4, POS_F6, POS_F3, false, true, bitBase);
		testKRKP(POS_A7, POS_G4, POS_F2, POS_B5, false, false, bitBase);
		testKRKP(POS_A7, POS_G4, POS_F2, POS_B5, true, false, bitBase);
		testKRKP(POS_A7, POS_G4, POS_F5, POS_B5, true, true, bitBase);
		testKRKP(POS_A7, POS_G4, POS_F5, POS_B5, false, false, bitBase);
		testKRKP(POS_C3, POS_C1, POS_D2, POS_A2, false, false, bitBase);
	}

	void testAllKRKQ() {
		BitBase bitBase;
		bitBase.readFromFile("KRKQ");
		testKRKQ(POS_C3, POS_C1, POS_A2, POS_D1, false, false, bitBase);
	}


	Eval eval;
	uint64_t amountOfIllegalPositions;
	uint64_t amountOfDirectDrawOrLoss;
};


#endif // __BITBASEGENERATOR_H