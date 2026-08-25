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
 * @copyright Copyright (c) 2025 Volker Böhm
 */

#include <optional>
#include <limits>
#include "rootmoves.h"
#include "moveprovider.h"
#include "../src/syzygy/tbposition-builder.h"
#include "../eval/tablebase-value.h"

using namespace QaplaSearch;
using QaplaSyzygy::Wdl;

namespace {

	// The whole probing layer follows plan/syzygy-probe.md, which holds the rules extracted from
	// Ronald de Man's own probing tool.

	/** The same outcome seen from the other side. Wdl is symmetric around Draw, so it is a flip. */
	constexpr Wdl negateWdl(Wdl wdl) {
		return Wdl(-int8_t(wdl));
	}

	/**
	 * Distance a position gets when a zeroing move is the best move: the counter zeroes after that
	 * one ply. The outcomes the fifty move rule turns into a draw carry its hundred plies on top,
	 * which is what puts them out of reach of a real win or loss.
	 */
	constexpr int32_t zeroingDistance(Wdl wdl) {
		switch (wdl) {
		case Wdl::Win:         return 1;
		case Wdl::CursedWin:   return 101;
		case Wdl::Draw:        return 0;
		case Wdl::BlessedLoss: return -101;
		case Wdl::Loss:        return -1;
		}
		return 0;
	}

	/** A win/draw/loss value together with the note whether a zeroing move reaches it. */
	struct WdlValue {
		Wdl value;
		bool zeroingIsBest;
	};

	/**
	 * Resolves the captures of `position` and returns the best value reachable from it. The stored
	 * entry is a lower bound - it may sit below the true value where a capture already reaches it,
	 * never above - so the answer is the better of the entry and the captures.
	 *
	 * Must not be called on a position that has an en passant capture available; the tables do not
	 * model that right. No descendant of a capture has one, so only the entry point has to care.
	 *
	 * @returns the value, or std::nullopt if a table needed along the way is missing.
	 */
	std::optional<Wdl> resolveCaptures(MoveGenerator& position, Wdl alpha, Wdl beta) {
		MoveList moveList;
		position.genMovesOfMovingColor(moveList);

		for (uint32_t i = 0; i < moveList.getTotalMoveAmount(); ++i) {
			const Move move = moveList[i];
			if (!move.isCapture()) continue;

			BoardState state = position.getBoardState();
			IncrementalState incremental = position.getIncrementalState();
			position.doMove(move);
			const std::optional<Wdl> replyValue =
				resolveCaptures(position, negateWdl(beta), negateWdl(alpha));
			position.undoMove(move, state, incremental);
			// undoMove does not restore the attack masks, and the move generation of the caller
			// reads them - see the note at the end of negaMaxPreSearch in search.cpp.
			position.computeAttackMasksForBothColors();

			if (!replyValue) return std::nullopt;
			alpha = std::max(alpha, negateWdl(*replyValue));
			if (alpha >= beta) return alpha;
		}

		QaplaSyzygy::TbPosition tbPos;
		if (!QaplaSyzygy::buildTbPosition(position, tbPos)) return std::nullopt;
		const QaplaSyzygy::WdlEntry entry = QaplaSyzygy::probeWdlEntry(tbPos);
		if (entry.status != QaplaSyzygy::Status::Ok) return std::nullopt;
		return std::max(alpha, entry.value);
	}

	/**
	 * Computes the win/draw/loss value of `position`, en passant included.
	 *
	 * @returns the value, or std::nullopt if a table needed along the way is missing.
	 */
	std::optional<WdlValue> computeWdl(MoveGenerator& position) {
		MoveList moveList;
		position.genMovesOfMovingColor(moveList);

		// The three kinds of move are counted for their own sake. Every case below asks whether
		// one of them is present, and reading that off a resolved value instead would hide which
		// property is actually being tested.
		uint32_t quietMoveAmount = 0;
		uint32_t ordinaryCaptureAmount = 0;
		uint32_t enPassantCaptureAmount = 0;
		for (uint32_t i = 0; i < moveList.getTotalMoveAmount(); ++i) {
			const Move move = moveList[i];
			if (!move.isCapture()) ++quietMoveAmount;
			else if (move.isEPMove()) ++enPassantCaptureAmount;
			else ++ordinaryCaptureAmount;
		}

		// En passant captures get a maximum of their own: the stored entry describes the position
		// without the en passant right, so only the ordinary captures may be held against it.
		// Each maximum means something only where its amount above is not zero.
		Wdl bestCapture = Wdl::Loss;
		Wdl bestOrdinaryCapture = Wdl::Loss;
		Wdl bestEnPassantCapture = Wdl::Loss;

		for (uint32_t i = 0; i < moveList.getTotalMoveAmount(); ++i) {
			const Move move = moveList[i];
			if (!move.isCapture()) continue;

			BoardState state = position.getBoardState();
			IncrementalState incremental = position.getIncrementalState();
			position.doMove(move);
			const std::optional<Wdl> replyValue =
				resolveCaptures(position, Wdl::Loss, negateWdl(bestCapture));
			position.undoMove(move, state, incremental);
			position.computeAttackMasksForBothColors();

			if (!replyValue) return std::nullopt;
			const Wdl value = negateWdl(*replyValue);

			bestCapture = std::max(bestCapture, value);
			if (move.isEPMove()) bestEnPassantCapture = std::max(bestEnPassantCapture, value);
			else bestOrdinaryCapture = std::max(bestOrdinaryCapture, value);
		}

		// A capture wins. A win is the best outcome there is, so no move can beat it and the entry
		// is not needed at all. It zeroes the counter, and nothing converts faster than one ply.
		if (bestCapture == Wdl::Win) {
			return WdlValue{ Wdl::Win, true };
		}

		// No capture wins, so a quiet move may be the best one - that is what the entry answers.
		QaplaSyzygy::TbPosition tbPos;
		if (!QaplaSyzygy::buildTbPosition(position, tbPos)) return std::nullopt;
		const QaplaSyzygy::WdlEntry entry = QaplaSyzygy::probeWdlEntry(tbPos);
		if (entry.status != QaplaSyzygy::Status::Ok) return std::nullopt;
		const Wdl stored = entry.value;

		// An en passant capture beats every ordinary capture and the entry as well. The entry
		// covers all quiet moves, so this move is strictly better than everything else there is -
		// it is the only best move, and the next zeroing move is therefore its own, one ply away.
		if (enPassantCaptureAmount > 0
			&& bestEnPassantCapture > bestOrdinaryCapture
			&& bestEnPassantCapture > stored) {
			return WdlValue{ bestEnPassantCapture, true };
		}

		// A capture reaches the value of the position. That makes it a best move, but only for a
		// win does it also settle the distance: one ply is the shortest there is. For a loss the
		// distance has to come out as long as possible, and a quiet move may reach the same value
		// and hold out longer - the entry is a lower bound and does not rule that out.
		if (bestCapture >= stored) {
			return WdlValue{ bestCapture, bestCapture > Wdl::Draw };
		}

		// Every legal move is an en passant capture, and the side to move is not in check. Without
		// the en passant right the position would have no move at all, so the entry describes a
		// stalemate - a draw that cannot happen here, because the capture is playable and must be
		// played. The entry is discarded instead of being taken as a lower bound.
		if (enPassantCaptureAmount > 0 && ordinaryCaptureAmount == 0 && quietMoveAmount == 0
			&& !position.isInCheck()) {
			return WdlValue{ bestEnPassantCapture, true };
		}

		// No capture reaches the entry, so the best move is a quiet one and the entry is its value.
		return WdlValue{ stored, false };
	}

	/**
	 * Computes the distance to the next zeroing move of `position`, counted in plies and signed
	 * like the win/draw/loss value: positive wins, negative loses, magnitude above 100 is a value
	 * the fifty move rule turns into a draw. A mate answers -1.
	 *
	 * The figure assumes a halfmove counter of zero and may be one too low; see the usability
	 * rules in plan/syzygy-probe.md.
	 *
	 * @returns the distance, or std::nullopt if a table needed along the way is missing.
	 */
	std::optional<int32_t> computeDtz(MoveGenerator& position) {
		const std::optional<WdlValue> wdl = computeWdl(position);
		if (!wdl) return std::nullopt;
		if (wdl->value == Wdl::Draw) return 0;
		if (wdl->zeroingIsBest) return zeroingDistance(wdl->value);

		MoveList moveList;
		position.genMovesOfMovingColor(moveList);

		// A quiet pawn move that holds the value zeroes the counter after a single ply.
		if (wdl->value > Wdl::Draw) {
			for (uint32_t i = 0; i < moveList.getTotalMoveAmount(); ++i) {
				const Move move = moveList[i];
				if (move.isCapture() || !isPawn(move.getMovingPiece())) continue;

				BoardState state = position.getBoardState();
				IncrementalState incremental = position.getIncrementalState();
				position.doMove(move);
				const std::optional<WdlValue> replyWdl = computeWdl(position);
				position.undoMove(move, state, incremental);
				position.computeAttackMasksForBothColors();

				if (!replyWdl) return std::nullopt;
				if (negateWdl(replyWdl->value) == wdl->value) return zeroingDistance(wdl->value);
			}
		}

		// The best move is known not to be an en passant capture now, so the value belongs to the
		// position without that right and the distance table may be asked with it.
		QaplaSyzygy::TbPosition tbPos;
		if (!QaplaSyzygy::buildTbPosition(position, tbPos)) return std::nullopt;
		const QaplaSyzygy::DtzEntry entry = QaplaSyzygy::probeDtzEntry(tbPos, wdl->value);
		if (entry.status == QaplaSyzygy::Status::Ok) {
			// The entry already carries the one ply that zeroingDistance stands for - see
			// the trailing "value + 1" in mapScore, src/syzygy/tbprobe.cpp. Only the hundred plies
			// that mark a value the fifty move rule turns into a draw have to be added here.
			const int32_t cursed =
				(wdl->value == Wdl::CursedWin || wdl->value == Wdl::BlessedLoss) ? 100 : 0;
			return (entry.distance + cursed) * (wdl->value > Wdl::Draw ? 1 : -1);
		}
		if (entry.status != QaplaSyzygy::Status::OtherSideToMove) return std::nullopt;

		// The table holds this material for the other side to move only. One ply of quiet non pawn
		// moves recovers the figure - pawn moves are already accounted for above.
		int32_t best = wdl->value > Wdl::Draw ? std::numeric_limits<int32_t>::max()
			: zeroingDistance(wdl->value);

		for (uint32_t i = 0; i < moveList.getTotalMoveAmount(); ++i) {
			const Move move = moveList[i];
			if (move.isCapture() || isPawn(move.getMovingPiece())) continue;

			BoardState state = position.getBoardState();
			IncrementalState incremental = position.getIncrementalState();
			position.doMove(move);

			const std::optional<int32_t> replyDtz = computeDtz(position);
			bool isMate = false;
			if (replyDtz && -*replyDtz == 1 && position.isInCheck()) {
				MoveList replies;
				position.genMovesOfMovingColor(replies);
				isMate = replies.getTotalMoveAmount() == 0;
			}

			position.undoMove(move, state, incremental);
			position.computeAttackMasksForBothColors();

			if (!replyDtz) return std::nullopt;
			const int32_t value = -*replyDtz;

			if (isMate) best = 1;
			else if (wdl->value > Wdl::Draw) {
				if (value > 0 && value + 1 < best) best = value + 1;
			}
			else if (value - 1 < best) best = value - 1;
		}

		return best;
	}

	/**
	 * Halfmove counter from which the distance stops being a tie-break and becomes the criterion.
	 * That close to the fifty move draw there are too few plies left to convert a win any other
	 * way, so a move that merely also wins can no longer be afforded and the search must not pick
	 * among the winning moves any more.
	 *
	 * Deliberately a constant and not a tunable: the situation is far too rare in a game for a
	 * tuning run to ever see enough of it.
	 */
	constexpr uint32_t DISTANCE_DECIDES_FROM_HALFMOVES = 80;

	/**
	 * The outcome group of a root move, from its distance and the halfmove counter active at the
	 * root - the rank rule of plan/syzygy-probe.md, reduced to the five groups the root move list
	 * sorts by. A nominal win whose distance no longer fits the fifty move budget is a cursed win.
	 */
	inline Wdl rootBucket(int32_t dtz, uint32_t cnt50) {
		if (dtz > 0) return dtz + int32_t(cnt50) <= 99 ? Wdl::Win : Wdl::CursedWin;
		if (dtz < 0) return -dtz * 2 + int32_t(cnt50) < 100 ? Wdl::Loss : Wdl::BlessedLoss;
		return Wdl::Draw;
	}

}

void RootMove::init() {
	_valueOfLastSearch = -MAX_VALUE;

	_alphaOfLastSearch = -MAX_VALUE;
	_betaOfLastSearch = MAX_VALUE;
	_depthOfLastSearch = 0;
	_isPVSearched = false;

	_nodeCountOfLastSearch = 0;
	_totalNodeCount = 0;
	_totalTableBaseHits = 0;
	_totalBitbaseHits = 0;
	_timeSpendToSearchMoveInMilliseconds = 0;

	_pvString = "";
	_isExcluded = false;

	_hasTbInfo = false;
	_tbWdl = Wdl::Draw;
	_tbDtz = 0;
	_tbDistanceKnown = false;
	_tbDistanceDecides = false;
}

bool RootMove::computeTablebaseInfo(MoveGenerator& rootPosition, uint32_t cnt50,
	bool distanceDecides) {
	_hasTbInfo = false;

	if (!QaplaSyzygy::Tablebase::isProbeable(rootPosition)) return false;

	const bool isZeroingMove = _move.isCapture() || isPawn(_move.getMovingPiece());

	BoardState state = rootPosition.getBoardState();
	IncrementalState incremental = rootPosition.getIncrementalState();
	rootPosition.doMove(_move);

	// No value means the move could not be classified - a single source of truth, instead of
	// tracking success alongside the figure.
	std::optional<int32_t> dtz;

	if (isZeroingMove) {
		// The counter restarts, so there is no distance left to measure beyond the value itself.
		const std::optional<WdlValue> replyWdl = computeWdl(rootPosition);
		if (replyWdl) dtz = zeroingDistance(negateWdl(replyWdl->value));
	}
	else {
		const std::optional<int32_t> replyDtz = computeDtz(rootPosition);
		if (replyDtz) {
			const int32_t value = -*replyDtz;
			dtz = value + (value > 0 ? 1 : value < 0 ? -1 : 0);
		}
	}

	// A mating move is worth distance 1, whatever the raw answer says - otherwise the fastest
	// mate is not recognised as the fastest.
	if (dtz && (*dtz == 2 || *dtz == 3) && rootPosition.isInCheck()) {
		MoveList replies;
		rootPosition.genMovesOfMovingColor(replies);
		if (replies.getTotalMoveAmount() == 0) dtz = 1;
	}

	// Where no distance file answers, the win/draw/loss tables alone still place the move. That
	// order holds whatever the halfmove counter is: the fifty move rule can turn a win into a
	// draw and a loss into a draw, never a win into a loss.
	std::optional<Wdl> outcome;
	if (!dtz) {
		const std::optional<WdlValue> replyWdl = computeWdl(rootPosition);
		if (replyWdl) outcome = negateWdl(replyWdl->value);
	}

	rootPosition.undoMove(_move, state, incremental);
	rootPosition.computeAttackMasksForBothColors();

	if (dtz) {
		_tbWdl = rootBucket(*dtz, cnt50);
		_tbDtz = *dtz;
		_tbDistanceKnown = true;
	}
	else if (outcome) {
		_tbWdl = *outcome;
		_tbDtz = 0;
		_tbDistanceKnown = false;
	}
	else {
		return false;
	}

	_hasTbInfo = true;
	_tbDistanceDecides = distanceDecides;
	return true;
}

void RootMove::set(value_t searchResult, const SearchStack& stack, bool isPVSearched)
{
	_valueOfLastSearch = searchResult;
	_alphaOfLastSearch = stack[0].alpha;
	_betaOfLastSearch = stack[0].beta;
	_isPVSearched = isPVSearched;
	_depthOfLastSearch = stack[0].remainingDepth;
	_pvLine.setMove(0, Move::EMPTY_MOVE);
	if (_isPVSearched) {
		// We cannot directly set stack[0].pv, because stack[0] is not yet updated
		_pvLine.setMove(0, _move);
		_pvLine.copyFromPV(stack[1].pv, 1);
	}
}

value_t RootMove::getReportedValue() const {
	if (!_hasTbInfo || _tbWdl != Wdl::Win) return _valueOfLastSearch;
	// With a distance the value says how far the conversion still is, which is what makes one
	// win better than another. Without one only the outcome is known.
	return _tbDistanceKnown
		? ChessEval::tablebaseDtzToValue(_tbDtz)
		: ChessEval::tablebaseWdlToValue(Wdl::Win);
}

bool RootMove::doSearch(const SearchNode& variables) const {
	if (_isExcluded) {
		return false;
	}
	if (_depthOfLastSearch < variables.getRemainingDepth()) {
		return true;
	}
	// If evaluated value was outside the window and is now inside the window, we need to search again
	if (_valueOfLastSearch >= _betaOfLastSearch && variables.beta > _betaOfLastSearch) {
		return true;
	}
	if (_valueOfLastSearch <= _alphaOfLastSearch && variables.alpha < _alphaOfLastSearch) {
		return true;
	}
	return false;
}

bool RootMove::operator<(const RootMove& other) const {
	if (_hasTbInfo && other._hasTbInfo && _tbWdl != other._tbWdl) {
		return _tbWdl < other._tbWdl;
	}
	const bool bothHaveDistance = _hasTbInfo && other._hasTbInfo
		&& _tbDistanceKnown && other._tbDistanceKnown;
	if (_hasTbInfo && other._hasTbInfo && _tbWdl == QaplaSyzygy::Wdl::Win
		&& _tbDistanceKnown != other._tbDistanceKnown) {
		// A win with a distance is forceable within the fifty move budget - that is what the
		// distance was checked against. A win without one may still run into the rule.
		return !_tbDistanceKnown;
	}
	if (_tbDistanceDecides && bothHaveDistance && _tbDtz != other._tbDtz) {
		// A position has repeated since the last zeroing move, so the game is no longer making
		// progress and the search value must not pick among equally classified moves any more -
		// only a strictly shorter distance gets out of the repetition.
		return _tbDtz > other._tbDtz;
	}
	if (_depthOfLastSearch != other._depthOfLastSearch) {
		return _depthOfLastSearch < other._depthOfLastSearch;
	}
	if (!other.isPVSearched() && !isPVSearched()) {
		// Neither has a real search result yet this run: within a tied tablebase bucket, order by
		// distance to zero, shorter first. Once either move has a search value this tie-break no
		// longer applies - the real search result decides below instead.
		if (bothHaveDistance && _tbDtz != other._tbDtz) {
			return _tbDtz > other._tbDtz;
		}
		return false;
	}
	if (!other.isPVSearched()) return false;
	if (!isPVSearched()) return true;

	if (other.isFailLow()) return false;
	if (isFailLow()) return true;

	return _valueOfLastSearch < other._valueOfLastSearch;
}

void RootMove::print() const {
	cout << _move.getLAN()
		<< " [v:" << std::right << std::setw(5) << _valueOfLastSearch << "]"
		<< " [d:" << std::right << std::setw(2) << _depthOfLastSearch << "]"
		<< " [" << std::right << std::setw(5) << _alphaOfLastSearch << ", " 
		<< std::right << std::setw(5) << _betaOfLastSearch << "]"
		<< (isPVSearched() ? (" " + _pvLine.toString()) : "")
		<< endl;
}

RootMove& RootMoves::findMove(Move move) {
	int32_t result = -1;
	for (int32_t i = 0; i < _moves.size(); ++i) {
		if (_moves[i].getMove() == move) {
			result = i;
			break;
		}
	}
	return _moves[result];
}

void RootMoves::setMoves(MoveGenerator& position, const std::vector<Move>& searchMoves, ButterflyBoard& butterflyBoard) {
	MoveProvider moveProvider;
	position.computeAttackMasksForBothColors();
	moveProvider.computeMoves(position, butterflyBoard, Move::EMPTY_MOVE, Move::EMPTY_MOVE);
	_moves.clear();
	Move move;
	while (!(move = moveProvider.selectNextMove(position)).isEmpty()) {
		if (!searchMoves.empty() && std::find(searchMoves.begin(), searchMoves.end(), move) == searchMoves.end()) {
			continue;
		}
		RootMove rootMove;
		rootMove.setMove(move);
		_moves.push_back(rootMove);
	}
}

bool RootMoves::computeTablebaseInfo(MoveGenerator& position, bool hasRepeatedPosition) {
	if (!QaplaSyzygy::Tablebase::isProbeable(position)) return false;

	const uint32_t cnt50 = position.getTotalHalfmovesWithoutPawnMoveOrCapture();

	// Two situations leave no room to convert a win any way other than the shortest: the game has
	// already stopped making progress, or the fifty move draw is close enough that it will.
	const bool distanceDecides =
		hasRepeatedPosition || cnt50 >= DISTANCE_DECIDES_FROM_HALFMOVES;

	// A partially classified list must never be sorted: an unanswered move carries no bucket and
	// would fall behind every classified one, reading as "loses" where nothing is known at all.
	for (RootMove& move : _moves) {
		if (move.computeTablebaseInfo(position, cnt50, distanceDecides)) continue;
		for (RootMove& toClear : _moves) {
			toClear.clearTbInfo();
		}
		return false;
	}

	if (!_moves.empty()) bubbleSort(0);
	return getTablebaseWinCount() > 0;
}

void RootMoves::bubbleSort(uint32_t first) {
	uint32_t last = uint32_t(_moves.size()) - 1;

	for (uint32_t i = first; i < last; ++i)
	{
		for (uint32_t j = last; j > i; --j)
		{
			if (_moves[j - 1] < _moves[j])
			{
				std::swap(_moves[j], _moves[j - 1]);
			}
		}
	}
}

void RootMoves::print() {
	for (auto& move : _moves) {
		move.print();
	}
}

