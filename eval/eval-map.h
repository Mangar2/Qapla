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
 * @copyright Copyright (c) 2024 Volker Böhm
 * @Overview
 * Implements helper functions for eval
 */

#ifndef __EVAL_MAP_H
#define __EVAL_MAP_H

#include <array>
#include "../basics/types.h"
#include "../basics/evalvalue.h"

using namespace QaplaBasics;

template <uint32_t size, uint32_t buckets>
class alignas(8) EvalMap {
public:
	EvalMap() {
		std::fill(values.begin(), values.end(), 0);
	}
	void setValue(uint32_t index, value_t value[buckets]) {
		std::copy(value, value + buckets, values.begin() + index * buckets);
	}
	void setValue(uint32_t index, array<value_t, buckets> value) {
		std::copy(value.begin(), value.end(), values.begin() + index * buckets);
	}
	void setValue(uint32_t index, uint32_t bucket, value_t value) {
		values[index * buckets + bucket] = value;
	}

	array<value_t, buckets> getValue(uint32_t index) const {
		array<value_t, buckets> result;
		std::copy(values.begin() + index * buckets, values.begin() + (index + 1) * buckets, result.begin());
		return result;
	}

	value_t getValue(uint32_t index, uint32_t bucket) const {
		return values[index * buckets + bucket];
	}

private: 
	std::array<value_t, size* buckets> values;

};

#endif
