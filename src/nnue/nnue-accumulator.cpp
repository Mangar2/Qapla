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
 * @copyright Copyright (c) 2026 Volker Böhm
 * @Overview
 * The accumulators of a search, see nnue-accumulator.h
 */

#include <cstring>
#include <iostream>
#include <memory>

#include "nnue-accumulator.h"
#include "nnue-evaluator.h"
#include "nnue-features.h"
#include "../../basics/bits.h"

using namespace QaplaNnue;
using QaplaBasics::Board;
using QaplaBasics::Move;
using QaplaBasics::NO_PIECE;
using QaplaBasics::Piece;
using QaplaBasics::Square;
using QaplaBasics::value_t;

namespace {
	std::unique_ptr<Network> theNetwork;
}

thread_local AccumulatorStack QaplaNnue::accumulators;

bool QaplaNnue::loadNetwork(const std::string& path) {
	auto loaded = readNetwork(path);
	if (loaded == nullptr) return false;
	theNetwork = std::move(loaded);
	return true;
}

const Network* QaplaNnue::network() {
	return theNetwork.get();
}

void QaplaNnue::reportMissingNetwork() {
	static bool reported = false;
	if (reported) return;
	reported = true;
	std::cout << "info string this build plays with a net and none is loaded, "
		<< "the hand written evaluation is used instead - set the option NnueFile"
		<< std::endl;
}

void KingSquareCache::clear(const Network& network) {
	_entries.resize(2 * SQUARE_COUNT);
	for (Entry& entry : _entries) {
		std::memcpy(entry.accumulator, network.featureBias.data(),
			ACCUMULATOR_SIZE * sizeof(int16_t));
		// An empty board: the accumulator is the bias and holds no piece at all.
		entry.pieces.fill(0);
	}
}

template <Piece PERSPECTIVE>
void KingSquareCache::refresh(const Network& network, const Board& board, int16_t* accumulator) {
	const Square kingSquare = squareOf<PERSPECTIVE>(PERSPECTIVE == QaplaBasics::WHITE
		? board.getKingSquare<QaplaBasics::WHITE>() : board.getKingSquare<QaplaBasics::BLACK>());
	Entry& entry = _entries[size_t(PERSPECTIVE) * SQUARE_COUNT + size_t(kingSquare)];

	for (Piece piece = QaplaBasics::MIN_PIECE; piece < QaplaBasics::PIECE_AMOUNT; ++piece) {
		const uint32_t plane = pieceePlaneOf<PERSPECTIVE>(piece);
		// The own king has no plane of its own - its square is the first part of every
		// index here - so it is not in the accumulator and not in the difference.
		if (plane == NO_PIECE_PLANE) continue;
		const QaplaBasics::bitBoard_t current = board.getPieceBB(piece);
		QaplaBasics::bitBoard_t added = current & ~entry.pieces[piece];
		QaplaBasics::bitBoard_t removed = entry.pieces[piece] & ~current;
		while (added != 0) {
			const Square square = QaplaBasics::popLSB(added);
			addFeature(network, entry.accumulator,
				featureIndex(kingSquare, plane, squareOf<PERSPECTIVE>(square)));
		}
		while (removed != 0) {
			const Square square = QaplaBasics::popLSB(removed);
			removeFeature(network, entry.accumulator,
				featureIndex(kingSquare, plane, squareOf<PERSPECTIVE>(square)));
		}
		entry.pieces[piece] = current;
	}
	std::memcpy(accumulator, entry.accumulator, ACCUMULATOR_SIZE * sizeof(int16_t));
}

void AccumulatorStack::computeBoth(const Board& board, Entry& entry) {
	_cache.refresh<QaplaBasics::WHITE>(*theNetwork, board, entry.accumulator[QaplaBasics::WHITE]);
	_cache.refresh<QaplaBasics::BLACK>(*theNetwork, board, entry.accumulator[QaplaBasics::BLACK]);
}

void AccumulatorStack::reset(const Board& board) {
	_valid = false;
	if (theNetwork == nullptr) return;
	if (_entries.empty()) _entries.resize(MAX_PLIES);
	if (_cachedNetwork != theNetwork.get()) {
		_cache.clear(*theNetwork);
		_cachedNetwork = theNetwork.get();
	}
	_top = 0;
	computeBoth(board, _entries[0]);
	_valid = true;
}

void AccumulatorStack::push(const Board& board, Move move) {
	if (!_valid) return;
	if (_top + 1 >= MAX_PLIES) {
		// Deeper than the stack goes. Saying so is the only safe answer: every
		// evaluation below this point computes everything from nothing.
		_valid = false;
		return;
	}
	const Entry& parent = _entries[_top];
	Entry& child = _entries[_top + 1];
	_top++;

	if (move.isNullMove()) {
		// The side to move changes, the pieces do not. Which accumulator is the own
		// one is read off the board when the value is computed.
		child = parent;
		return;
	}

	const Piece piece = move.getMovingPiece();
	if (QaplaBasics::isKing(piece) || move.isCastleMove()) {
		// The own king square is part of every feature index of its perspective.
		computeBoth(board, child);
		return;
	}

	const Square departure = move.getDeparture();
	const Square destination = move.getDestination();
	const Piece captured = move.getCapture();
	// An en passant capture takes a pawn that does not stand on the destination:
	// it is on the file of the destination and the rank of the departure.
	const Square captureSquare = move.isEPMove()
		? Square((int32_t(departure) & ~7) | (int32_t(destination) & 7)) : destination;
	const Piece placed = move.isPromote() ? move.getPromotion() : piece;

	for (const Piece perspective : { QaplaBasics::WHITE, QaplaBasics::BLACK }) {
		int16_t* accumulator = child.accumulator[perspective];
		std::memcpy(accumulator, parent.accumulator[perspective],
			ACCUMULATOR_SIZE * sizeof(int16_t));
		// No king has moved, so the king squares are the ones of the parent.
		const Square kingSquare = perspective == QaplaBasics::WHITE
			? squareOf<QaplaBasics::WHITE>(board.getKingSquare<QaplaBasics::WHITE>())
			: squareOf<QaplaBasics::BLACK>(board.getKingSquare<QaplaBasics::BLACK>());
		const auto planeOf = [perspective](Piece candidate) {
			return perspective == QaplaBasics::WHITE
				? pieceePlaneOf<QaplaBasics::WHITE>(candidate)
				: pieceePlaneOf<QaplaBasics::BLACK>(candidate);
		};
		const auto squareFor = [perspective](Square square) {
			return perspective == QaplaBasics::WHITE
				? squareOf<QaplaBasics::WHITE>(square) : squareOf<QaplaBasics::BLACK>(square);
		};
		removeFeature(*theNetwork, accumulator,
			featureIndex(kingSquare, planeOf(piece), squareFor(departure)));
		if (captured != NO_PIECE) {
			removeFeature(*theNetwork, accumulator,
				featureIndex(kingSquare, planeOf(captured), squareFor(captureSquare)));
		}
		addFeature(*theNetwork, accumulator,
			featureIndex(kingSquare, planeOf(placed), squareFor(destination)));
	}
}

void AccumulatorStack::pop() {
	if (_top > 0) _top--;
}

value_t AccumulatorStack::evaluate(const Board& board) {
	if (theNetwork == nullptr) return 0;
	if (!_valid) {
		// Not standing at the position, so there is nothing to build on.
		return Evaluator(*theNetwork).evaluate(board);
	}
	const Entry& entry = _entries[_top];
	const bool whiteToMove = board.isWhiteToMove();
	const int16_t* own = entry.accumulator[whiteToMove ? QaplaBasics::WHITE : QaplaBasics::BLACK];
	const int16_t* opponent = entry.accumulator[whiteToMove ? QaplaBasics::BLACK : QaplaBasics::WHITE];

#ifdef QAPLA_VERIFY_NNUE_INCREMENTAL
	Entry fresh;
	refreshAccumulator<QaplaBasics::WHITE>(*theNetwork, board, fresh.accumulator[QaplaBasics::WHITE]);
	refreshAccumulator<QaplaBasics::BLACK>(*theNetwork, board, fresh.accumulator[QaplaBasics::BLACK]);
	for (const Piece perspective : { QaplaBasics::WHITE, QaplaBasics::BLACK }) {
		for (uint32_t index = 0; index < ACCUMULATOR_SIZE; index++) {
			if (entry.accumulator[perspective][index] == fresh.accumulator[perspective][index]) {
				continue;
			}
			std::cerr << "nnue: the accumulator kept up to date move by move differs from a "
				<< "fresh one at ply " << _top << ", perspective " << int(perspective)
				<< ", element " << index << ": " << entry.accumulator[perspective][index]
				<< " against " << fresh.accumulator[perspective][index] << std::endl;
			std::abort();
		}
	}
#endif

	return forward(*theNetwork, own, opponent);
}
