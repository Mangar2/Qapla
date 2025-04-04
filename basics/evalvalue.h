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
 * Implements a value class containing midgame and endgame evaluation components
 */

#ifndef __EVALVALUE_H
#define __EVALVALUE_H

#include <cstdint>
#include <ostream>
#include "../basics/types.h"

using namespace std;

namespace QaplaBasics {

	typedef int32_t value_t;

	const value_t MAX_VALUE = 30000;
	const value_t NO_VALUE = -30001;
	const value_t MIN_MATE_VALUE = MAX_VALUE - 1000;
	const value_t MAX_BONUS = MAX_VALUE - 5000;
	const value_t WINNING_BONUS = 5000;

	// the draw value is reseved and signales a forced draw (stalemate, repetition)
	const value_t DRAW_VALUE = 1;

	class EvalValue {
	public:
		constexpr EvalValue() : _midgame(0), _endgame(0) {}
		constexpr EvalValue(value_t value) : _midgame(value), _endgame(value) {}
		constexpr EvalValue(value_t midgame, value_t endgame) : _midgame(midgame), _endgame(endgame) {}
		constexpr EvalValue(const value_t value[2]) : _midgame(value[0]), _endgame(value[1]) {}
		constexpr EvalValue(const array<value_t, 2> value) : _midgame(value[0]), _endgame(value[1]) {}

		/**
		 * Returns the evaluation value based on the game state
		 * @param midgameInPercent weight for the midgame, endgame weight is 100-midgameInPercent
		 */
		constexpr value_t getValue(value_t midgameInPercent) const {
			return (value_t(_midgame) * midgameInPercent + value_t(_endgame) * (100 - midgameInPercent)) / 100;
		}

		constexpr array<value_t, 2> getValue() const {
			const array<value_t, 2> result = { _midgame, _endgame };
			return result;
		}

		constexpr value_t midgame() const { return _midgame; }
		constexpr value_t endgame() const { return _endgame; }
		value_t& midgame() { return _midgame; }
		value_t& endgame() { return _endgame; }

		constexpr  EvalValue& operator+=(EvalValue add) { _midgame += add._midgame; _endgame += add._endgame; return *this; }
		constexpr  EvalValue& operator-=(EvalValue sub) { _midgame -= sub._midgame; _endgame -= sub._endgame; return *this; }
		constexpr  EvalValue& operator*=(EvalValue mul) { _midgame *= mul._midgame; _endgame *= mul._endgame; return *this; }
		constexpr  EvalValue& operator/=(EvalValue div) { _midgame /= div._midgame; _endgame /= div._endgame; return *this; }
		constexpr  EvalValue abs() const { return EvalValue(std::abs(_midgame), std::abs(_endgame)); }
		// EvalValue operator*(value_t mul) { _midgame *= mul; _endgame *= mul; return *this; }

		constexpr friend EvalValue operator+(EvalValue a, EvalValue b);
		constexpr friend EvalValue operator-(EvalValue a, EvalValue b);
		constexpr friend EvalValue operator-(EvalValue a);
		constexpr friend EvalValue operator*(EvalValue a, EvalValue b);
		constexpr friend EvalValue operator/(EvalValue a, EvalValue b);


		inline std::string toString() const {
			return std::to_string(_midgame) + ", " + std::to_string(_endgame);
		}

		//This method handles all the outputs.    
		friend ostream& operator<<(ostream&, const EvalValue&);
	private:
		value_t _midgame;
		value_t _endgame;
	};

	inline ostream& operator<<(ostream& o, const EvalValue& v) {
		o << "{" << std::right << std::setw(3) << v._midgame << ", " 
			<< std::right << std::setw(3) << v._endgame << "}";
		return o;
	}

	constexpr EvalValue operator+(EvalValue a, EvalValue b) {
		return EvalValue(value_t(a._midgame + b._midgame), value_t(a._endgame + b._endgame));
	}

	constexpr EvalValue operator-(EvalValue a, EvalValue b) {
		return EvalValue(value_t(a._midgame - b._midgame), value_t(a._endgame - b._endgame));
	}

	constexpr EvalValue operator-(EvalValue a) {
		return EvalValue(-a._midgame, -a._endgame);
	}

	constexpr EvalValue operator*(EvalValue a, EvalValue b) {
		return EvalValue(value_t(a._midgame * b._midgame), value_t(a._endgame * b._endgame));
	}

	constexpr EvalValue operator/(EvalValue a, EvalValue b) {
		return EvalValue(value_t(a._midgame / b._midgame), value_t(a._endgame / b._endgame));
	}

}





#endif