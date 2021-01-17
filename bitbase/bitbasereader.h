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
 * Reads all bitbases to memory
 *
 */

#include "bitbase.h"

namespace ChessBitbase {

	class BitBaseReader
	{
	public:
		static void loadBitBase() {
			kpk.readFromFile("KPK.btb");
		}

		static void loadRelevant3StoneBitBase() {
			loadBitBase("KPK");
		}

		static void loadRelevant4StoneBitBase() {
			loadBitBase("KPKP");
			loadBitBase("KPKN");
			loadBitBase("KPKB");
			loadBitBase("KPPK");
			loadBitBase("KNPK");
			loadBitBase("KBPK");
			loadBitBase("KBNK");
			loadBitBase("KBBK");
			loadBitBase("KRKP");
			loadBitBase("KRKN");
			loadBitBase("KRKB");
			loadBitBase("KRKR");
			loadBitBase("KQKP");
			loadBitBase("KQKN");
			loadBitBase("KQKB");
			loadBitBase("KQKR");
			loadBitBase("KQKQ");
		}

		static void load5StoneBitBase() {
			loadBitBase("KQQKQ");
		}

		static value_t getValueFromBitBase(GenBoard& board, PieceSignature signature, value_t currentValue) {
			value_t result = currentValue;
			auto it = bitBases.find(signature.getSignature());
			if (it != bitBases.end()) {
				uint64_t index = BoardAccess::getIndex(board);
				bool wins = it->second.getBit(index);
				result = wins ? currentValue + WINNING_BONUS : 0;
			}
			return result;
		}

		static const value_t getValueFromBitBase(GenBoard& board, value_t currentValue) {
			PieceSignature signature = board.pieceSignature;
			value_t result = getValueFromBitBase(board, signature, currentValue);
			if (result == currentValue) {
				signature.switchSide();
				result = -getValueFromBitBase(board, signature, -currentValue);
			}
			return result;
		}

		static void setBitBase(std::string pieceString, const BitBase& bitBase) {
			PieceSignature signature;
			signature.setFromString(pieceString.c_str());
			bitBases[signature.getSignature()] = bitBase;
		}

		static void loadBitBase(std::string pieceString) {
			PieceSignature signature;
			signature.setFromString(pieceString.c_str());
			FileClass fileClass;
			fileClass.open(pieceString + ".btb", "rb");
			if (fileClass.isOpen()) {
				bitBases[signature.getSignature()].readFromFile(fileClass);
			}
		}

		static bool isBitBaseAvailable(std::string pieceString) {
			PieceSignature signature;
			signature.setFromString(pieceString.c_str());
			auto it = bitBases.find(signature.getSignature());
			return it != bitBases.end();
		}

		static std::map<pieceSignature_t, BitBase> bitBases;

	private:
		BitBaseReader() {};
	};

}

