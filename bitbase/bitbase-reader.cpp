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

#include <map>
#include <memory>
#include <format>

#include "../search/clockmanager.h"
#include "../eval/evalendgame.h"

#include "boardaccess.h"
#include "KPK.h"
#include "bitbase-reader.h"
#include "bitbase-repairfile.h"

using namespace QaplaBitbase;

// File-local map of open .qwdl files registered by the generator.
// Kept here (not in the header) so that bitbase-repairfile.h and <windows.h>
// are not pulled into every file that includes bitbase-reader.h.
static std::map<pieceSignature_t, std::unique_ptr<BitbaseRePairFile>> qwdlFiles;

std::vector<std::string> BitbaseReader::loadBitbase() {
	QaplaSearch::ClockManager clock;
	clock.setStartTime();
	vector<std::string> toLoad = {
		"K*K", "K*K*", "K**K", 
		"K*K**","K**K*",  "K***K",
		"K*K***", "K**K**", "K***K*"
	};
	vector <std::string> messages;
	for (const auto& name : toLoad) {
		auto subErrors = loadBitbaseRec(name, true);
		for (const auto& error : subErrors) {
			if (messages.size() < 5) {
				messages.push_back(error);
			}
			else if (messages.size() == 5) {
				messages.push_back("Too many errors, skipping further messages");
			}
		}
	}
	messages.push_back(std::format("Read bitbases from directory: {} - {} bitbases read", bitbasePath.string(), _bitbases.size() - 1));
	messages.push_back(std::format("Time spent to load bitbases: {} milliseconds", clock.computeTimeSpentInMilliseconds()));
	return messages;
}

void BitbaseReader::registerBitbaseFromHeader() {
	registerBitbaseFromHeader("KPK", KPK, KPK_size);
}

bool BitbaseReader::setBitbasePath(const std::string& path) {
	try {
		const std::filesystem::path p(path);
		bitbasePath = "";
		if (!std::filesystem::is_directory(p)) {
			return false;
		}

		bitbasePath = std::filesystem::canonical(p).string();
	}
	catch (...) {
		return false;
	}
	return true;
}

void BitbaseReader::registerBitbaseFromHeader(std::string pieceString, const uint32_t data[], uint32_t sizeInBytes) {
	PieceSignature signature;
	signature.set(pieceString);
	pieceSignature_t sig = signature.getPiecesSignature();
	if (_bitbases.find(sig) != _bitbases.end()) {
		return;
	}
	BitbaseIndex index(pieceString);
	Bitbase bitbase(index, 1, sig);
	_bitbases[sig] = bitbase;
	_bitbases[sig].loadFromEmbeddedData(data);
	ChessEval::EvalEndgame::registerBitbase(pieceString);
}

std::vector<std::string> BitbaseReader::loadBitbaseRec(std::string name, bool force) {
	std::vector<std::string> errors;

	auto pos = name.find('*');
	if (pos != std::string::npos) {
		for (char ch : std::string("QRBNP")) {
			std::string next = name;
			next[pos] = ch;

			auto subErrors = loadBitbaseRec(next, force);
			errors.insert(errors.end(), subErrors.begin(), subErrors.end());
		}
	}
	else if (!isBitbaseAvailable(name)) {
		try {
			loadBitbase(name, true);
		}
		catch (const std::runtime_error& e) {
			errors.push_back("[" + name + "]: " + e.what());
		}
	}

	return errors;
}

BitbaseResult BitbaseReader::getValueFromSingleBitbase(const MoveGenerator& position) {
	PieceSignature signature = PieceSignature(position.getPiecesSignature());
	// KK (no material on either side) is always a draw - no bitbase needed.
	// Do NOT short-circuit when only white has no material (e.g. KKQ): the mirror
	// fallback below correctly returns Loss in that case.
	if (!position.hasAnyMaterial<WHITE>() && !position.hasAnyMaterial<BLACK>()) {
		return BitbaseResult::Draw;
	}

	Bitbase* bitbase = getBitbase(signature);
	if (bitbase != nullptr) {
		uint64_t index = BoardAccess::getIndex<0>(position);
		if (bitbase->getBitsPerEntry() >= 2) {
			return bitbase->get2Bits(index);
		}
		// 1-bit encoding: only win vs. draw-or-loss
		return bitbase->getBit(index) ? BitbaseResult::Win : BitbaseResult::DrawOrLoss;
	}

	// Check .qwdl file registered by the generator (full 2-bit WDL, memory-mapped).
	{
		auto iq = qwdlFiles.find(signature.getPiecesSignature());
		if (iq != qwdlFiles.end() && iq->second->isOpen()) {
			uint64_t index = BoardAccess::getIndex<0>(position);
			return iq->second->probe(index);
		}
	}

	// Fallback: try the color-swapped bitbase (e.g. KRKQ via KQKR) and invert Win<->Loss.
	// Works for both 2-bit (full Win/Draw/Loss) and 1-bit (bit=1 is mirrored Win → Loss here;
	// bit=0 is DrawOrLoss, but since the strong side has no material it can only be Draw).
	PieceSignature mirroredSig = signature;
	mirroredSig.changeSide();
	Bitbase* mirroredBitbase = getBitbase(mirroredSig);
	if (mirroredBitbase != nullptr) {
		uint64_t index = BoardAccess::getIndex<1>(position);  // <1> swaps colors + side-to-move
		if (mirroredBitbase->getBitsPerEntry() >= 2) {
			BitbaseResult result = mirroredBitbase->get2Bits(index);
			if (result == BitbaseResult::Win)  return BitbaseResult::Loss;
			if (result == BitbaseResult::Loss) return BitbaseResult::Win;
			return result;  // Draw or Unknown unchanged
		}
		// 1-bit mirror: bit=1 means the mirrored (strong) side wins → Loss for current view; bit=0 → Draw
		return mirroredBitbase->getBit(index) ? BitbaseResult::Loss : BitbaseResult::Draw;
	}

	// Mirrored .qwdl fallback.
	{
		auto iq = qwdlFiles.find(mirroredSig.getPiecesSignature());
		if (iq != qwdlFiles.end() && iq->second->isOpen()) {
			uint64_t index = BoardAccess::getIndex<1>(position);
			BitbaseResult result = iq->second->probe(index);
			if (result == BitbaseResult::Win)  return BitbaseResult::Loss;
			if (result == BitbaseResult::Loss) return BitbaseResult::Win;
			return result;
		}
	}

	return BitbaseResult::Unknown;
}

BitbaseResult BitbaseReader::getValueFromBitbase(const MoveGenerator& position) {
	PieceSignature signature = PieceSignature(position.getPiecesSignature());

	// A bitbase contains winning information for white only. White wins or does not win.
	// We can use the same bitbase to see, if black will win by switching the side. (second case).
	Bitbase* whiteBitbase = getBitbase(signature);
	if (whiteBitbase != 0) {
		uint64_t index = BoardAccess::getIndex<0>(position);
		// Check if white wins
		if (whiteBitbase->getBit(index)) {
			// Bitbase indicates that side to move is winning - result is calculated on white view
			return position.isWhiteToMove() ? BitbaseResult::Win : BitbaseResult::Loss;
		}
		// If we are here, then white will not win. If black may not win, it is draw
		if (!signature.hasEnoughMaterialToMate<BLACK>()) return BitbaseResult::Draw;
	}

	// Now switching the side to test, if black may win
	signature.changeSide();
	Bitbase* blackBitbase = getBitbase(signature);
	if (blackBitbase != 0) {
		uint64_t index = BoardAccess::getIndex<1>(position);
		if (blackBitbase->getBit(index)) {
			return position.isWhiteToMove() ? BitbaseResult::Loss : BitbaseResult::Win;
		}
		// If we are here, then black will not win. If white may not win, it is draw
		// We use "BLACK" as template argument as we changed the sigature to have "white view"
		if (!signature.hasEnoughMaterialToMate<BLACK>()) return BitbaseResult::Draw;
	}

	// If both bitbases are available and we did not find a win, it is a draw. 
	return whiteBitbase != 0 && blackBitbase != 0 ? BitbaseResult::Draw : BitbaseResult::Unknown;
}

value_t BitbaseReader::getValueFromBitbase(const MoveGenerator& position, value_t currentValue) {
	const BitbaseResult bitbaseResult = getValueFromBitbase(position);
	value_t value = currentValue;
	if (bitbaseResult == BitbaseResult::Win) value = currentValue + WINNING_BONUS;
	else if (bitbaseResult == BitbaseResult::Loss) value = currentValue - WINNING_BONUS;
	else if (bitbaseResult == BitbaseResult::Draw) value = 1;
	return value;
}

bool BitbaseReader::tryLoadBitbaseFile(const std::string& pieceString, bool onlyHeader) {
	PieceSignature signature;
	signature.set(pieceString);
	BitbaseIndex index(pieceString);
	Bitbase bitbase(index, 1, signature.getPiecesSignature());
	if (!bitbase.attachFromFile(pieceString, ".btb", bitbasePath)) {
		return false;
	}
	ChessEval::EvalEndgame::registerBitbase(pieceString);
	if (!onlyHeader) {
		auto [success, errorMessage] = bitbase.readAll();
		if (!success) {
			std::cout << "info string loaded bitbase " << pieceString << " " << errorMessage << std::endl;
			return false;
		}
	}
	_bitbases.emplace(signature.getPiecesSignature(), std::move(bitbase));
	return true;
}

void BitbaseReader::loadBitbase(std::string pieceString, bool onlyHeader) {
	if (isBitbaseAvailable(pieceString)) {
		return;
	}
	if (tryLoadBitbaseFile(pieceString, onlyHeader)) {
		return;
	}
	// If the direct file doesn't exist, try the color-swapped name (e.g. KKN → KNK).
	PieceList list(pieceString);
	std::string mirrorName = list.getPieceStringOfColor<BLACK>() + list.getPieceStringOfColor<WHITE>();
	if (mirrorName != pieceString && !isBitbaseAvailable(mirrorName)) {
		tryLoadBitbaseFile(mirrorName, onlyHeader);
	}
}

bool BitbaseReader::isBitbaseAvailable(std::string pieceString) {
	PieceSignature signature;
	signature.set(pieceString.c_str());
	auto it = _bitbases.find(signature.getPiecesSignature());
	if (it != _bitbases.end() && it->second.isHeaderLoaded()) return true;
	// Also check .qwdl files registered by the generator.
	auto iq = qwdlFiles.find(signature.getPiecesSignature());
	if (iq != qwdlFiles.end() && iq->second->isOpen()) return true;
	// Also treat as available if the color-swapped bitbase exists (any bit width).
	// getValueFromSingleBitbase handles the Win<->Loss inversion transparently.
	signature.changeSide();
	auto itMirror = _bitbases.find(signature.getPiecesSignature());
	if (itMirror != _bitbases.end() && itMirror->second.isHeaderLoaded()) return true;
	auto iqMirror = qwdlFiles.find(signature.getPiecesSignature());
	return iqMirror != qwdlFiles.end() && iqMirror->second->isOpen();
}

void BitbaseReader::registerQwdlFile(const std::string& pieceString,
                                     const std::string& filePath) {
	PieceSignature signature;
	signature.set(pieceString.c_str());
	auto file = std::make_unique<BitbaseRePairFile>();
	if (file->open(filePath)) {
		qwdlFiles[signature.getPiecesSignature()] = std::move(file);
	}
}

void BitbaseReader::setBitbase(std::string pieceString, const Bitbase& bitBase) {
	PieceSignature signature;
	signature.set(pieceString.c_str());
	_bitbases[signature.getPiecesSignature()] = bitBase;
}

Bitbase* BitbaseReader::getBitbase(PieceSignature signature) {
	Bitbase* bitbase = 0;
	auto it = _bitbases.find(signature.getPiecesSignature());
	if (it != _bitbases.end() && it->second.isHeaderLoaded()) {
		bitbase = &it->second;
	}
	return bitbase;
}



