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

#pragma once

#include "../basics/types.h"
#include "../basics/move.h"
#include "../basics/movelist.h"
#include "../basics/board.h"
#include "magics.h"



// ----------------------------------------------------------------------------
// Implements move generator
// ----------------------------------------------------------------------------

using namespace QaplaBasics;

namespace QaplaMoveGenerator {

	class MoveGenerator : public Board
	{
	public:
		MoveGenerator(void);

		/**
		 * Checks if the king of the side to move is currently under attack.
		 *
		 * This uses the precomputed attack masks and tests whether the moving side's
		 * king is on any square attacked by the opponent.
		 *
		 * @return True if the side to move is in check, false otherwise.
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
		 * Checks if the board state is legal: both kings exist and the king
		 * not on move is not in check (used after move application).
		 *
		 * @return True if the position is legal.
		 */
		bool isLegal() {
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
		 * Applies a move and updates internal attack masks.
		 * Null moves are handled separately and do not change attack masks.
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
		 * Undoes a move and restores the board state.
		 * Handles null moves as well.
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
		 * Sets this board to a mirrored version of the input board (white <-> black).
		 * This can be useful for evaluation symmetry testing or engine self-play balance.
		 */
		void setToSymetricBoard(const MoveGenerator& board) {
			Board::setToSymetricBoard(board);
			computeAttackMasksForBothColors();
		}

		// ------------------------------------------------------------------------
		// ---------------------- Move generation ---------------------------------
		// ------------------------------------------------------------------------

		/*
		* Returns an array of bitboards holding all squares, where pieces can check the king
		*/


		std::array<bitBoard_t, Piece::PIECE_AMOUNT / 2> computeCheckBitmapsForMovingColor() const;

		/**
		 * Determines if a move results in check. This includes:
		 * - Regular direct checks
		 * - Discovered checks
		 * - Special moves (en passant, castling, promotions)
		 *
		 * Uses the precomputed checkBitmaps from computeCheckBitmaps().
		 */
		bool isCheckMove(Move move, const std::array<bitBoard_t, Piece::PIECE_AMOUNT / 2>& checkingBitmaps);

		/**
		 * Generates all legal moves that evade a check for the side to move.
		 * Includes captures, king moves and interpositions.
		 */
		void genEvadesOfMovingColor(MoveList& moveList);

		/**
		 * Generates all moves (silent and non silent) of the color to move
		 */
		void genMovesOfMovingColor(MoveList& moveList);

		/**
		 * Generates all non-silent moves for the moving side.
		 * These include captures, promotions, en passant and castling.
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
		 * Computes the pinned piece mask for the given color.
		 * A pinned piece may only move along the ray between the king and the pinning piece.
		 *
		 * This method works by:
		 * - Identifying all rays from the king to potential blockers.
		 * - Removing pinned-side pieces from the occupancy temporarily.
		 * - Finding all sliding enemy pieces (rooks, bishops, queens) that now attack the king.
		 * - For each such piece, the full ray to the king is added to the pinned mask.
		 *
		 * The result includes both the pinned piece and the allowed movement direction.
		 */
		template <Piece COLOR>
		void computePinnedMask();

		// ------------------------------------------------------------------------
		// ---------------------- Gives check -------------------------------------
		// ------------------------------------------------------------------------

	private:
		enum moveGenType_t { SILENT, NON_SILENT, ALL };

		/**
		 * Generates moves for a single piece on a given square.
		 *
		 * @param piece The piece type to generate for.
		 * @param startPos The square the piece is on.
		 * @param destinationBB Bitboard of legal destination squares.
		 * @param moveList Output list of generated moves.
		 */
		void genMovesSinglePiece(uint32_t piece, Square startPos, bitBoard_t destinationBB, MoveList& moveList);

		/**
		 * Generates moves for all pieces of a certain type that move via a single step pattern,
		 * such as pawns or knights. Often used for mass generation.
		 *
		 * @param piece The piece to move (e.g. WHITE_PAWN).
		 * @param aStep Direction offset for move computation.
		 * @param destinationBB Bitboard of target squares.
		 * @param moveList Output list.
		 */
		void genMovesMultiplePieces(uint32_t piece, int32_t aStep, bitBoard_t destinationBB, MoveList& moveList);

		/**
		 * Generates en passant move if legal. Handles the edge case where the capture
		 * may cause a discovered check (removing two pieces from a rank).
		 *
		 * The legality is verified by simulating the resulting position and checking
		 * for sliding attacks on the king.
		 */
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

		/**
		 * Generates all possible moves that evade a check:
		 * - Captures the checking piece
		 * - Interposes along the ray
		 * - Moves the king to safety
		 *
		 * If more than one piece gives check, only king moves are possible.
		 * Handles all special cases including promotions and en passant.
		 */
		template <Piece COLOR>
		void genEvades(MoveList& moveList);

		template <Piece COLOR>
		void genNonSilentMoves(MoveList& moveList);

		template <Piece COLOR>
		void genMoves(MoveList& moveList);

		/**
		 * Computes the attack mask of a single piece (excluding any blocking logic).
		 *
		 * Used for each square individually during attack map generation.
		 * The result is stored in the per-square attack mask array.
		 */
		template<Piece PIECE>
		bitBoard_t computeAttackMaskForPiece(Square square, bitBoard_t allPiecesWithoutKing);

		template<Piece PIECE>
		bitBoard_t computeAttackMaskForPieces(bitBoard_t pieceBB, bitBoard_t allPiecesWithoutKing);

		template <Piece COLOR>
		bitBoard_t computeAttackMask();

		template <Piece COLOR>
		void computeCastlingMasksForMoveGeneration();

		/**
		 * Computes the set of squares from which each piece type could check the given king.
		 *
		 * This includes:
		 * - Direct check vectors (pawn, knight, rook, bishop, queen)
		 * - Discovered check masks: squares that, if vacated, would expose a check
		 *
		 * These bitboards are used to detect if a move delivers check.
		 */
		template <Piece COLOR>
		std::array<bitBoard_t, Piece::PIECE_AMOUNT / 2> computeCheckBitmaps() const;

		static const int32_t ONE_COLUMN = 1;

	public:

		/**
		 * Bitboards representing all squares attacked by each side.
		 * Indexed by side color: attackMask[WHITE] or attackMask[BLACK].
		 */
		array<bitBoard_t, 2> attackMask;

		/**
		 * Bitboards marking all pinned pieces for each side.
		 * A pinned piece cannot legally move in arbitrary directions.
		 */
		array<bitBoard_t, 2> pinnedMask;

		// Squares attacked by pawns
		array<bitBoard_t, 2> pawnAttack;

		array<bitBoard_t, BOARD_SIZE> pieceAttackMask;
		
		/**
		 * Bitboards used to check if the king passes through attacked squares when castling.
		 * Required for castling legality checks.
		 */
		array<bitBoard_t, 2> castleAttackMaskKingSide;
		array<bitBoard_t, 2> castleAttackMaskQueenSide;

		/**
		 * Bitboards used to verify that the castling path is free of pieces.
		 */
		array<bitBoard_t, 2> castlePieceMaskKingSide;
		array<bitBoard_t, 2> castlePieceMaskQueenSide;
	};

}
