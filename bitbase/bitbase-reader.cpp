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
 */

#include <map>
#include "../search/clockmanager.h"
#include "../eval/evalendgame.h"

#include "boardaccess.h"
#include "KPK.h"
#include "KRKP.h"
#include "bitbase-reader.h"

using namespace QaplaBitbase;

map<pieceSignature_t, Bitbase> BitbaseReader::_bitbases;

void BitbaseReader::loadBitbase() {
	QaplaSearch::ClockManager clock;
	clock.setStartTime();
	registerBitbaseFromHeader("KPK", KPK, KPK_size);
	// registerBitbaseFromHeader("KRKP", KRKP, KRKP_size);
	//loadBitbaseRec("K*K");
	//loadBitbaseRec("KK*");
	//loadBitbaseRec("K*K*");
	//loadBitbaseRec("K**K");
	//loadBitbaseRec("K**K*");
	//cout << "Time spent to load bitbases: " << clock.computeTimeSpentInMilliseconds() << " Milliseconds " << endl;
}

void BitbaseReader::registerBitbaseFromHeader(std::string pieceString, const uint32_t data[], uint32_t sizeInBytes) {
	PieceSignature signature;
	signature.set(pieceString);
	pieceSignature_t sig = signature.getPiecesSignature();
	if (_bitbases.find(sig) != _bitbases.end()) {
		return;
	}
	PieceList list(pieceString);
	BitbaseIndex index(list);
	Bitbase bitbase;
	_bitbases[sig] = bitbase;
	_bitbases[sig].loadFromEmbeddedData(data, sizeInBytes, index.getSizeInBit());
	ChessEval::EvalEndgame::registerBitbase(pieceString);
}

void BitbaseReader::loadBitbaseRec(string name, bool force) {
	auto pos = name.find('*');
	if (pos != string::npos) {
		for (auto ch : string("QRBNP")) {
			string next = name;
			next[pos] = ch;
			loadBitbaseRec(next, force);
		}
	}
	else {
		if (force || !isBitbaseAvailable(name)) {
			loadBitbase(name);
		}
	}
}

void BitbaseReader::loadRelevant3StoneBitbase() {
	loadBitbase("KPK");
}

void BitbaseReader::loadRelevant4StoneBitbase() {
	loadBitbase("KPKP");
	loadBitbase("KPKN");
	loadBitbase("KPKB");
	loadBitbase("KPPK");
	loadBitbase("KNPK");
	loadBitbase("KBPK");
	loadBitbase("KBNK");
	loadBitbase("KBBK");
	loadBitbase("KRKP");
	loadBitbase("KRKN");
	loadBitbase("KRKB");
	loadBitbase("KRKR");
	loadBitbase("KQKP");
	loadBitbase("KQKN");
	loadBitbase("KQKB");
	loadBitbase("KQKR");
	loadBitbase("KQKQ");
}

void BitbaseReader::load5StoneBitbase() {
	loadBitbase("KQQKQ");
}

Result BitbaseReader::getValueFromSingleBitbase(const MoveGenerator& position) {
	PieceSignature signature = PieceSignature(position.getPiecesSignature());
	if (!position.hasAnyMaterial<WHITE>()) {
		return Result::DrawOrLoss;
	}

	const Bitbase* bitbase = getBitbase(signature);
	if (bitbase != 0) {
		uint64_t index = BoardAccess::getIndex<0>(position);
		return bitbase->getBit(index) ? Result::Win : Result::DrawOrLoss;
	}
	return Result::Unknown;
}

Result BitbaseReader::getValueFromBitbase(const MoveGenerator& position) {
	PieceSignature signature = PieceSignature(position.getPiecesSignature());

	// A bitbase contains winning information for white only. White wins or does not win.
	// We can use the same bitbase to see, if black will win by switching the side. (second case).
	const Bitbase* whiteBitbase = getBitbase(signature);
	if (whiteBitbase != 0) {
		uint64_t index = BoardAccess::getIndex<0>(position);
		// Check if white wins
		if (whiteBitbase->getBit(index)) {
			// Bitbase indicates that side to move is winning - result is calculated on white view
			return position.isWhiteToMove() ? Result::Win : Result::Loss;
		}
		// If we are here, then white will not win. If black may not win, it is draw
		if (!signature.hasEnoughMaterialToMate<BLACK>()) return Result::Draw;
	}

	// Now switching the side to test, if black may win
	signature.changeSide();
	const Bitbase* blackBitbase = getBitbase(signature);
	if (blackBitbase != 0) {
		uint64_t index = BoardAccess::getIndex<1>(position);
		if (blackBitbase->getBit(index)) {
			return position.isWhiteToMove() ? Result::Loss : Result::Win;
		}
		// If we are here, then black will not win. If white may not win, it is draw
		if (!signature.hasEnoughMaterialToMate<WHITE>()) return Result::Draw;
	}

	// If both bitbases are available and we did not find a win, it is a draw. 
	return whiteBitbase != 0 && blackBitbase != 0 ? Result::Draw : Result::Unknown;
}

value_t BitbaseReader::getValueFromBitbase(const MoveGenerator& position, value_t currentValue) {
	const Result bitbaseResult = getValueFromBitbase(position);
	value_t value = currentValue;
	if (bitbaseResult == Result::Win) value = currentValue + WINNING_BONUS;
	else if (bitbaseResult == Result::Loss) value = currentValue - WINNING_BONUS;
	else if (bitbaseResult == Result::Draw) value = 1;
	return value;
}

void BitbaseReader::loadBitbase(std::string pieceString) {
	PieceSignature signature;
	signature.set(pieceString);
	pieceSignature_t sig = signature.getPiecesSignature();
	if (_bitbases.find(sig) != _bitbases.end()) {
		return;
	}
	Bitbase bitbase;
	_bitbases[sig] = bitbase;
	if (!_bitbases[sig].readFromFile(pieceString)) {
		_bitbases.erase(sig);
	}
	ChessEval::EvalEndgame::registerBitbase(pieceString);
}

bool BitbaseReader::isBitbaseAvailable(std::string pieceString) {
	PieceSignature signature;
	signature.set(pieceString.c_str());
	auto it = _bitbases.find(signature.getPiecesSignature());
	return (it != _bitbases.end() && it->second.isLoaded());
}

void BitbaseReader::setBitbase(std::string pieceString, const Bitbase& bitBase) {
	PieceSignature signature;
	signature.set(pieceString.c_str());
	_bitbases[signature.getPiecesSignature()] = bitBase;
}

const Bitbase* BitbaseReader::getBitbase(PieceSignature signature) {
	Bitbase* bitbase = 0;
	auto it = _bitbases.find(signature.getPiecesSignature());
	if (it != _bitbases.end() && it->second.isLoaded()) {
		bitbase = &it->second;
	}
	return bitbase;
}



