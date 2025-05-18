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
 * Implements a value class containing midgame and endgame evaluation components
 */

#pragma once

#include <cstdint>
#include <ostream>
#include <cmath>
#include <array>
#include "../basics/types.h"

using namespace std;

namespace QaplaBasics {

	using value_t = int32_t;

	const value_t MAX_VALUE = 30000;
	const value_t NO_VALUE = -30001;
	const value_t MIN_MATE_VALUE = MAX_VALUE - 1000;
	const value_t NON_MATE_VALUE_LIMIT = MAX_VALUE - 5000;
	const value_t WINNING_BONUS = 10000;

	// the draw value is reseved and signales a forced draw (stalemate, repetition)
	const value_t DRAW_VALUE = 1;

	template <Piece COLOR> value_t whiteValue(value_t value) {
		if constexpr (COLOR == WHITE) {
			return value;
		}
		else {
			return -value;
		}
	}

	constexpr std::array<int, 9> props = { -100, -76, -61, -31, 0, 31, 61, 76, 100 };
	constexpr std::array<int, 9> values = { -700, -300, -200, -100, 0, 100, 200, 300, 700 };
	constexpr std::array<double, 9> slopes = {
		5.0, 3.0, 2.5, 1.8, 0.0, 1.8, 2.5, 3.0, 5.0
	};

	/** 
	 * Hermite - Interpolation
	 * @param x the x value to interpolate
	 * @param x0 the first x value
	 * @param x1 the second x value
	 * @param y0 the first y value
	 * @param y1 the second y value
	 * @param m0 the slope at x0
	 * @param m1 the slope at x1
	 * @returns the rounded interpolated value as int
	 */
	constexpr value_t interpolate(int x, int x0, int x1, int y0, int y1, double m0, double m1) {
		double t = double(x - x0) / double(x1 - x0);
		double t2 = t * t;
		double t3 = t2 * t;

		double h00 = 2 * t3 - 3 * t2 + 1;
		double h10 = t3 - 2 * t2 + t;
		double h01 = -2 * t3 + 3 * t2;
		double h11 = t3 - t2;

		double result = h00 * y0 + h10 * (x1 - x0) * m0 + h01 * y1 + h11 * (x1 - x0) * m1;
		return static_cast<value_t>(result + (result >= 0 ? 0.5 : -0.5));
	}

	/**
	 * Converts a win probability into a centipawn evaluation value.
	 *
	 * Scale: -100 = black always wins, +100 = white always wins.
	 * Symmetric around 0. Values are mapped using Hermite interpolation.
	 *
	 * @param prop Probability in range [-100, 100]
	 * @return Evaluation in centipawns
	 */
	constexpr value_t propToValue(int prop) {
		if (prop <= props.front()) return values.front();
		if (prop >= props.back())  return values.back();

		for (size_t i = 1; i < props.size(); ++i) {
			if (prop <= props[i]) {
				return interpolate(
					prop,
					props[i - 1], props[i],
					values[i - 1], values[i],
					slopes[i - 1], slopes[i]
				);
			}
		}
		return 0;
	}


	/**
	 * Stores an evaluation value with midgame and endgame components.
	 *
	 * This allows for tapered evaluation based on the game phase.
	 */
	class EvalValue {
	public:
		constexpr EvalValue() : _midgame(0), _endgame(0) {}
		constexpr EvalValue(value_t value) : _midgame(value), _endgame(value) {}
		constexpr EvalValue(value_t midgame, value_t endgame) : _midgame(midgame), _endgame(endgame) {}
		constexpr EvalValue(const value_t value[2]) : _midgame(value[0]), _endgame(value[1]) {}
		constexpr EvalValue(const array<value_t, 2> value) : _midgame(value[0]), _endgame(value[1]) {}

		/**
		 * Returns a tapered evaluation value depending on the given game phase.
		 * @param midgameInPercent Percentage weight of the midgame component (0..100).
		 * @return Combined tapered evaluation value.
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


		constexpr friend EvalValue operator+(EvalValue a, EvalValue b);
		constexpr friend EvalValue operator-(EvalValue a, EvalValue b);
		constexpr friend EvalValue operator-(EvalValue a);

		inline friend EvalValue operator*(EvalValue v, value_t scale);
		inline friend EvalValue operator*(value_t scale, EvalValue v);
		inline friend EvalValue operator/(EvalValue v, value_t divisor);
		
		inline friend EvalValue operator*(EvalValue a, EvalValue b);

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

	/**
	 * Component-wise multiplication. 
	 */
	inline EvalValue operator*(EvalValue a, EvalValue b) {
		return EvalValue(value_t(a._midgame * b._midgame), value_t(a._endgame * b._endgame));
	}
	
	inline EvalValue operator*(EvalValue v, value_t scale) {
		return EvalValue(v.midgame() * scale, v.endgame() * scale);
	}
	inline EvalValue operator*(value_t scale, EvalValue v) {
		return v * scale;
	}
	inline EvalValue operator/(EvalValue v, value_t divisor) {
		return EvalValue(v.midgame() / divisor, v.endgame() / divisor);
	}

}

