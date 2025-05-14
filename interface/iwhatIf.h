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
 * Interface to a "what if" debugging interface for chess engines
 * This is a special Qapla feature not typically implemented in chess engines.
 * It is used to ask the engine questions on the search tree like
 * what I play "e4 d6" ?
 */

#ifndef __IWHATIF_H
#define __IWHATIF_H

namespace QaplaInterface {

	class IWhatIf {
	public:
		virtual void setSearchDepht(int32_t depth) = 0;
		virtual void setMove(int32_t ply, char movingPiece,
			uint32_t departureRank, uint32_t depatureFile,
			uint32_t destinationRank, uint32_t destinationFile,
			char promotePiece) = 0;
		virtual void setNullmove(int32_t ply) = 0;
		virtual void clear() = 0;
	};
}

#endif // __IWHATIF_H