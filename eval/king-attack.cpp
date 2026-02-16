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

#include "king-attack.h"

namespace ChessEval {

#ifdef PARAM_OPTIMIZE_KING_ATTACK

/**
 * Generates attackWeight array using B-spline interpolation
 */
static std::array<value_t, KingAttack::MAX_WEIGHT_COUNT + 1> generateAttackWeight(
	value_t p0,
	value_t p4,
	value_t p10,
	value_t p15,
	value_t p32)
{
	std::vector<std::pair<int, double>> points = {
		{ 0, p0 },
		{ 4, p4 },
		{ 10, p10 },
		{ 15, p15 },
		{ 32, p32 }
	};
	return generateArrayBSpline<KingAttack::MAX_WEIGHT_COUNT + 1>(points, false);
}

/**
* UCI parameter access implementation for KingAttack
*/
class UciAccess : public UciParameterProvider {
public:
	std::vector<UciParam> getUciParameters() const override {
		return {
			{ .name = "kAttackP0", .defaultValue = p0_ },
			{ .name = "kAttackP4", .defaultValue = p4_ },
			{ .name = "kAttackP10", .defaultValue = p10_ },
			{ .name = "kAttackP15", .defaultValue = p15_ },
			{ .name = "kAttackP32", .defaultValue = p32_ }
		};
	}

	bool setUciParameter(const std::string& name, int32_t value) override {
		if (name == "kAttackP0") {
			p0_ = static_cast<value_t>(value);
		} else if (name == "kAttackP4") {
			p4_ = static_cast<value_t>(value);
		} else if (name == "kAttackP10") {
			p10_ = static_cast<value_t>(value);
		} else if (name == "kAttackP15") {
			p15_ = static_cast<value_t>(value);
		} else if (name == "kAttackP32") {
			p32_ = static_cast<value_t>(value);
		} else {
			return false;
		}

		KingAttack::attackWeight = generateAttackWeight(p0_, p4_, p10_, p15_, p32_);

		printArray("attackWeight", KingAttack::attackWeight);

		return true;
	}

private:
	value_t p0_ = KingAttack::ATTACK_WEIGHT_DEFAULT[0];
	value_t p4_ = KingAttack::ATTACK_WEIGHT_DEFAULT[4];
	value_t p10_ = KingAttack::ATTACK_WEIGHT_DEFAULT[10];
	value_t p15_ = KingAttack::ATTACK_WEIGHT_DEFAULT[15];
	value_t p32_ = KingAttack::ATTACK_WEIGHT_DEFAULT[32];
};
#endif

UciParameterProvider& KingAttack::getUciAccess() {
#ifdef PARAM_OPTIMIZE_KING_ATTACK
	static UciAccess instance;
#else
	static EmptyParameterProvider instance;
#endif
	return instance;
}

}
