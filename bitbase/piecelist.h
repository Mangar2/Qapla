#pragma once

#include "MoveConverter.h"
#include "../movegenerator/movegenerator.h"
#include <string>

using namespace std;

class PieceList {
public:
	PieceList(const PieceList& pieceList) {
		operator=(pieceList);
	}

	PieceList(const char* pieceString) {
		bool whitePiece = true;
		piece_t piece;
		uint32_t stringPos;
		pieceAmount = 0;
		pawnAmount = 0;
		if (pieceString[0] == 'K') {
			for (stringPos = 1; pieceString[stringPos] != 0; stringPos++) {
				if (pieceString[stringPos] == 'K') {
					whitePiece = false;
				} else {
					piece = MoveConverter::charToPiece(whitePiece, pieceString[stringPos]); 
					addPiece(piece);
				}
			}
		}
		bubbleSort();
	}

	PieceList(const Board& board) {
		pieceAmount = 0;
		pawnAmount = 0;
		addPiecesFromBitBoard(board.getPieceBitBoard(WHITE_KING), WHITE_KING);
		addPiecesFromBitBoard(board.getPieceBitBoard(BLACK_KING), BLACK_KING);
		for (piece_t piece = WHITE_PAWN; piece <= BLACK_QUEEN; piece ++) {
			addPiecesFromBitBoard(board.getPieceBitBoard(piece), piece);
		}
	}

	PieceList& operator=(const PieceList& pieceList) {
		pieceAmount = 0;
		pawnAmount = 0;
		for(uint32_t pieceNo = 0; pieceNo < pieceList.getPieceAmount(); pieceNo++) {
			addPiece(pieceList.getPiece(pieceNo));
		}
		return *this;
	}

	template<piece_t COLOR>
	string getPieceStringOfColor() {
		string result = "K";
		for (int32_t pieceNo = getPieceAmount() - 1;  pieceNo >= 0; pieceNo --) {
			if (getPieceColor(getPiece(pieceNo)) == COLOR) {
				result += MoveConverter::pieceToChar(getPiece(pieceNo) - COLOR);
			}
		}
		return result;
	}

	string getPieceString() {
		string result = getPieceStringOfColor<WHITE>() + getPieceStringOfColor<BLACK>();
		return result;
	}


	void addPiece(piece_t piece) {
		pieces[pieceAmount] = piece;
		pieceAmount++;
		if (isPawn(piece)) {
			pawnAmount++;
		}
	}

	void removePiece(uint32_t pieceNo) {
		if (pieceNo < pieceAmount) {
			pieceAmount--;
			if (isPawn(pieces[pieceNo])) {
				pawnAmount--;
			}
			for (; pieceNo < pieceAmount; pieceNo ++) {
				pieces[pieceNo] = pieces[pieceNo + 1];
			}
		}
	}

	void promotePawn(uint32_t pieceNo, piece_t promotePieceType = QUEEN) {
		if (pieceNo < pieceAmount && isPawn(pieces[pieceNo])) {
			pieces[pieceNo] += promotePieceType - PAWN;
			pawnAmount--;
			bubbleSort();
		}
	}

	piece_t getPiece(uint32_t pieceNo) const {
		return pieces[pieceNo];
	}

	pos_t getPos(uint32_t pieceNo) const {
		return piecePos[pieceNo];
	}

	uint32_t getPawnAmount() const { return pawnAmount; }
	uint32_t getPieceAmount() const { return pieceAmount; }
	uint32_t getPieceAmountWithoutPawns() const { return pieceAmount - pawnAmount; }

private:
	void addPiecesFromBitBoard(bitBoard_t piecesBitBoard, piece_t piece) {
		for (;piecesBitBoard != 0; piecesBitBoard &= piecesBitBoard - 1) {
			piecePos[pieceAmount] = BitBoardMasks::lsb(piecesBitBoard);
			addPiece(piece);
		}
	}

	void swap(uint32_t index1, uint32_t index2) {
		piece_t tmp = pieces[index1];
		pieces[index1] = pieces[index2];
		pieces[index2] = tmp;
	}

	void bubbleSort() {
		for (uint32_t outerLoop = pieceAmount - 1; outerLoop > 0; outerLoop--) {
			for (uint32_t innerLoop = 1; innerLoop <= outerLoop; innerLoop++) {
				if (pieces[innerLoop - 1] > pieces[innerLoop]) {
					swap(innerLoop - 1, innerLoop);
				}
			}
		}
	}

	static const uint32_t MAX_PIECES = 16;
	uint32_t pieceAmount;
	uint32_t pawnAmount;
	piece_t pieces[MAX_PIECES];
	pos_t piecePos[MAX_PIECES];
};