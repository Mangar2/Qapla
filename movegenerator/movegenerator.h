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
 * Bitboard based move generator using magic numbers
 * Generates only fully legal moves
 */

#ifndef __MOVEGENERATOR_H
#define __MOVEGENERATOR_H

#include "../basics/types.h"
#include "../basics/move.h"
#include "../basics/movelist.h"
#include "../basics/board.h"
#include "magics.h"



// ----------------------------------------------------------------------------
// Implements move generator
// ----------------------------------------------------------------------------

using namespace ChessBasics;

namespace ChessMoveGenerator {

	class MoveGenerator : public Board
	{
	public:
		MoveGenerator(void);

		/**
		 * Returns true, if the side to move is in check
		 */
		inline bool isInCheck() {
			bool result;
			if (isWhiteToMove()) {
				result = (bitBoardsPiece[WHITE_KING] & attackMask[BLACK]) != 0;
			}
			else {
				result = (bitBoardsPiece[BLACK_KING] & attackMask[WHITE]) != 0;
			}
			return result;
		}

		/**
		 * Checks, if the king not on move is in check
		 */
		bool isLegalPosition() {
			computeAttackMasksForBothColors();
			bool hasKingOfBothColors = 
				(bitBoardsPiece[WHITE_KING] != 0) && (bitBoardsPiece[BLACK_KING] != 0);
			bool result = hasKingOfBothColors;
			if (!isWhiteToMove()) {
				result = result && (bitBoardsPiece[WHITE_KING] & attackMask[BLACK]) == 0;
			}
			else {
				result = result && (bitBoardsPiece[BLACK_KING] & attackMask[WHITE]) == 0;
			}
			return result;
		}

		/**
		 * Clears/empties the board
		 */
		void clear(); 

		/**
		 * Initializes masks for castling move generator
		 */
		void initCastlingMasksForMoveGeneration();

		/**
		 * Sets a move
		 */
		void doMove(Move move) {
			if (move.isNullMove()) {
				Board::doNullmove();
				// Attacks are identical after a nullmove
			}
			else {
				Board::doMove(move);
				computeAttackMasksForBothColors();
			}
		}

		/**
		 * Unset a move
		 */
		void undoMove(Move move, BoardState boardState) {
			if (move.isNullMove()) {
				Board::undoNullmove(boardState);
			}
			else {
				Board::undoMove(move, boardState);
			}
		}

		/**
	     * Creates a symetric board exchanging black/white side
	     */
		void setToSymetricBoard(const MoveGenerator& board) {
			Board::setToSymetricBoard(board);
			computeAttackMasksForBothColors();
		}

		// ------------------------------------------------------------------------
		// ---------------------- Move generation ---------------------------------
		// ------------------------------------------------------------------------

		/**
		 * Generates all check evade moves (silent and non silent) of the color to move
		 */
		void genEvadesOfMovingColor(MoveList& moveList);

		/**
		 * Generates all moves (silent and non silent) of the color to move
		 */
		void genMovesOfMovingColor(MoveList& moveList);

		/**
		 * Generates all non silent moves (captures and promotes) of the color to move
		 */
		void genNonSilentMovesOfMovingColor(MoveList& moveList);

		/**
		 * Sets a new piece to the board
		 */
		void setPiece(Square square, Piece piece) {
			Board::setPiece(square, piece);
			computeAttackMasksForBothColors();
		}
		
		/**
		 * Sets a piece, you need to call compute attack masks before move generation
		 */
		void unsafeSetPiece(Square square, Piece piece) {
			Board::setPiece(square, piece);
		}

		/**
		 * Computes all attack masks for WHITE and BLACK
		 */
		void computeAttackMasksForBothColors();

		/**
		 * Computes a mask of pinned pieces
		 */
		template <Piece COLOR>
		void computePinnedMask();

	private:
		enum moveGenType_t { SILENT, NON_SILENT, ALL };

		void genMovesSinglePiece(uint32_t piece, Square startPos, bitBoard_t destinationBB, MoveList& moveList);

		/**
		 * Generates all moves of multiple pieces (usually pawns) 
		 * @param piece Piece to move
		 * @param step destination - step = departure
		 * @param destinationBB holding every destination bit
		 * @param moveList Output parameter: list holding all moves
		 */ 
		void genMovesMultiplePieces(uint32_t piece, int32_t aStep, bitBoard_t destinationBB, MoveList& moveList);

		template<Piece COLOR>
		void genEPMove(Square startPos, Square epPos, MoveList& moveList);

		template<Piece COLOR>
		void genPawnPromotions(bitBoard_t targetBitBoard, int32_t moveDirection, MoveList& moveList);

		template <Piece COLOR>
		void genPawnCaptures(bitBoard_t targetBitBoard, int32_t moveDirection, MoveList& moveList);

		template <Piece COLOR>
		void genSilentSinglePawnMoves(Square startPos, bitBoard_t allowedPositionMask, MoveList& moveList);

		template <Piece COLOR>
		void genPawnCaptureSinglePiece(Square startPos, bitBoard_t targetBitBoard, MoveList& moveList);

		template <Piece COLOR>
		void genSilentPawnMoves(MoveList& moveList);

		template <Piece COLOR>
		void genNonSilentPawnMoves(MoveList& moveList, Square epPos);

		template<Piece PIECE, moveGenType_t TYPE>
		void genMovesForPiece(MoveList& moveList);

		template<MoveGenerator::moveGenType_t TYPE, Piece COLOR>
		void genNonPinnedMovesForAllPieces(MoveList& moveList);

		template<Piece COLOR>
		void genPinnedMovesForAllPieces(MoveList& moveList, Square epPos);

		template<Piece COLOR>
		void genPinnedCapturesForAllPieces(MoveList& moveList, Square epPos);

		template <Piece PIECE>
		void genEvadesByBlocking(MoveList& moveList, 
			bitBoard_t removePinnedPiecesMask,
			bitBoard_t blockingPositions);

		template <Piece COLOR>
		void genEvades(MoveList& moveList);

		template <Piece COLOR>
		void genNonSilentMoves(MoveList& moveList);

		template <Piece COLOR>
		void genMoves(MoveList& moveList);

		template<Piece PIECE>
		bitBoard_t computeAttackMaskForPiece(Square square, bitBoard_t allPiecesWithoutKing);

		template<Piece PIECE>
		bitBoard_t computeAttackMaskForPieces(bitBoard_t pieceBB, bitBoard_t allPiecesWithoutKing);

		template <Piece COLOR>
		bitBoard_t computeAttackMask();

		template <Piece COLOR>
		void computeCastlingMasksForMoveGeneration();

		static const int32_t ONE_COLUMN = 1;

	public:

		// Squares attacked by any piece
		array<bitBoard_t, 2> attackMask;

		// Squares where pieces are pinned
		array<bitBoard_t, 2> pinnedMask;

		// Squares attacked by pawns
		array<bitBoard_t, 2> pawnAttack;

		array<bitBoard_t, BOARD_SIZE> pieceAttackMask;
		
		// Bits to check against attack mask to see, if castling is possible
		array<bitBoard_t, 2> castleAttackMaskKingSide;
		array<bitBoard_t, 2> castleAttackMaskQueenSide;

		// Bits to check agains piece mask to see, if castling is possible
		array<bitBoard_t, 2> castlePieceMaskKingSide;
		array<bitBoard_t, 2> castlePieceMaskQueenSide;
	};

}

#endif // __MOVEGENERATOR_H
