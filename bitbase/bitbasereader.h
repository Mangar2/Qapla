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
 * @author Volker B�hm
 * @copyright Copyright (c) 2021 Volker B�hm
 * @Overview
 * Reads all bitbases to memory
 *
 */

#ifndef __BITBASEREADER_H
#define __BITBASEREADER_H

#include <map>
#include "../movegenerator/movegenerator.h"
#include "../eval/evalendgame.h"
#include "bitbase.h"
#include "piecelist.h"
#include "boardaccess.h"

using namespace ChessMoveGenerator;
using namespace std;

namespace QaplaBitbase {

	enum class Result {
		Unknown, Loss, Draw, Win
	};

	class BitbaseReader
	{
	public:
		static void loadBitbase() {
			loadBitbaseRec("K*K");
			loadBitbaseRec("K*K*");
			loadBitbaseRec("K**K");
		}

		static void loadBitbaseRec(string name) {
			auto pos = name.find('*');
			if (pos != string::npos) {
				for (auto ch : string("QRBNP")) {
					string next = name;
					next[pos] = ch;
					loadBitbaseRec(next);
				}
			}
			else {
				loadBitbase(name);
			}
		}

		static void loadRelevant3StoneBitbase() {
			loadBitbase("KPK");
			loadBitbase("KQK");
		}

		static void loadRelevant4StoneBitbase() {
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

		static void load5StoneBitbase() {
			loadBitbase("KQQKQ");
		}

		/**
		 * Reads a value from bitboard having the position
		 */
		static Result getValueFromBitbase(const MoveGenerator& position) {
			PieceSignature signature = PieceSignature(position.getPiecesSignature());

			// 1. Test if the bitbase has a win for white view (example KRKP)
			const Bitbase* whiteBitbase = getBitbase(signature);
			if (whiteBitbase != 0) {
				uint64_t index = BoardAccess::computeIndex<0>(position);
				if (whiteBitbase->getBit(index)) {
					return position.isWhiteToMove() ? Result::Win : Result::Loss;
				}
			}

			// 2. Test if the bitbase has a win for black (example KPKR using the KRKP bitbase)
			signature.changeSide();
			const Bitbase* blackBitbase = getBitbase(signature);
			if (blackBitbase != 0) {
				uint64_t index = BoardAccess::computeIndex<1>(position);
				if (blackBitbase->getBit(index)) {
					return position.isWhiteToMove() ? Result::Loss : Result::Win;
				}
			}
			return (whiteBitbase != 0 && blackBitbase != 0) ? Result::Draw : Result::Unknown;
		}

		/**
		 * Reads a value from bitboard having the position and a current value
		 */
		static value_t getValueFromBitbase(const MoveGenerator& position, value_t currentValue) {
			const Result bitbaseResult = getValueFromBitbase(position);
			value_t value = currentValue;
			if (bitbaseResult == Result::Win) value = currentValue + WINNING_BONUS;
			else if (bitbaseResult == Result::Loss) value = currentValue - WINNING_BONUS;
			else if (bitbaseResult == Result::Draw) value = 1;
			return value;
		}

		/**
		 * Loads a bitbase and stores it for later use
		 */
		static void loadBitbase(std::string pieceString) {
			PieceSignature signature;
			signature.set(pieceString.c_str());
			Bitbase bitbase;
			if (bitbase.readFromFile(pieceString)) {
				_bitbases[signature.getPiecesSignature()] = bitbase;
			}
		}

		/**
		 * Tests, if a bitbase is available
		 */
		static bool isBitbaseAvailable(std::string pieceString) {
			PieceSignature signature;
			signature.set(pieceString.c_str());
			auto it = _bitbases.find(signature.getPiecesSignature());
			return (it != _bitbases.end() && it->second.isLoaded());
		}

		/**
		 * Sets a bitbase
		 */
		static void setBitbase(std::string pieceString, const Bitbase& bitBase) {
			PieceSignature signature;
			signature.set(pieceString.c_str());
			_bitbases[signature.getPiecesSignature()] = bitBase;
		}

	private:

		/**
		 * Gets a bitbase using a pice signature
		 */
		static const Bitbase* getBitbase(PieceSignature signature) {
			Bitbase* bitbase = 0;
			auto it = _bitbases.find(signature.getPiecesSignature());
			if (it != _bitbases.end() && it->second.isLoaded()) {
				bitbase = &it->second;
			}
			return bitbase;
		}

		BitbaseReader() {};
		static map<pieceSignature_t, Bitbase> _bitbases;

	};

}

#endif // __BITBASEREADER_H