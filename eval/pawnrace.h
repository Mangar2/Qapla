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
 * Trial functionality to compute pawn races
 */

#include "../basics/board.h"
#include "../movegenerator/movegenerator.h"
#include "../movegenerator/bitboardmasks.h"

using namespace QaplaMoveGenerator;

namespace ChessEval {

	class PawnRace
	{
	public:
		PawnRace();

		value_t runnerRace(const MoveGenerator& board, bitBoard_t whitePassedPawns, bitBoard_t blackPassedPawns);

		template<Piece COLOR>
		bool hasSideRunner() {
			return hasRunner[COLOR];
		}

		template<Piece COLOR>
		bool hasTempoSafeRunner() {
			return hasRunner[COLOR] && isRunnerTempoSafe[COLOR];
		}

		template<Piece COLOR>
		bool hasTempoCriticalPawn() {
			return !hasRunner[COLOR] && hasTempoCriticalPassedPawn[COLOR];
		}


	private:
		template<Piece COLOR>
		bool inFrontOfPawn(Square piecePos, Square pawnPos);

		template<Piece COLOR>
		uint32_t computePawnDistance(Square ownKingPos, Square pawnPos);

		template<Piece COLOR>
		void computeFastestCandidate(const Board& board);

		template<Piece COLOR>
		void computeLegalPositions(const MoveGenerator& board);

		template<Piece COLOR>
		void initRace(const MoveGenerator& board);

		template<Piece COLOR>
		void updateCandidate(const MoveGenerator& board);

		template<Piece COLOR>
		bool pawnPromoted();

		template<Piece COLOR>
		void setPawnPromoted();

		template<Piece COLOR>
		void makeKingMove();

		template<Piece COLOR>
		bool moveKingAway(Square ownKingPosition);

		template<Piece COLOR>
		void makePawnMove();

		template<Piece COLOR>
		bool isPawnCapturedByKing();

		template<Piece COLOR>
		void setPawnCapturedByKing();

		template<Piece COLOR>
		void checkIfCandidateIsRunner(const MoveGenerator& board);

		template<Piece COLOR>
		bool hasPromisingCandidate();

		template<Piece COLOR>
		value_t computeBonus();

		bitBoard_t legalPositions[COLOR_COUNT];
		bitBoard_t kingPositions[COLOR_COUNT];
		bitBoard_t formerPositions[COLOR_COUNT];
		bitBoard_t kingAttack[COLOR_COUNT];
		bitBoard_t passedPawns[COLOR_COUNT];
		bitBoard_t pawnPositions[COLOR_COUNT];
		uint32_t bestPassedPawnDistanceInHalfmoves[COLOR_COUNT];
		uint32_t bestRunnerDistanceInHalfmoves[COLOR_COUNT];
		Square candidatePawnSquare[COLOR_COUNT];
		bool hasRunner[COLOR_COUNT];
		bool isRunnerTempoSafe[COLOR_COUNT];
		bool hasTempoCriticalPassedPawn[COLOR_COUNT];

		static constexpr bitBoard_t PROMOTE_BIT_MASK[COLOR_COUNT] = { BitBoardMasks::RANK_8_BITMASK, BitBoardMasks::RANK_1_BITMASK };
		static const value_t RUNNER_FACTOR = 15;
	};

}