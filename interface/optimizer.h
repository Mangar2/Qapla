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
 * Implements a UCI - Interface
 */

#pragma once

#include <vector>
#include <memory>
#include <iostream>
#include <string>
#include <cmath>
#include <cassert>
#include <algorithm>

class Optimizer {
    struct Point {
        double x;          
        double pMeasured;  
        double pEstimated; 
        double confidence; 
    };
public:
	void clear() {
		points.clear();
	}
	void addPoint(double x, double pMeasured, double radius = 0.1);

    void updateEstimates(double radius = 0.1);

	std::pair<uint32_t, Point> getBest();

	void printBest(std::ostream& os);

	bool goodEnough() {
		auto [index, best] = getBest();
		return best.confidence > CONFIDENCE_THRESHOLD;
	}

	double nextX(double min, double max);

	void print(std::ostream& os) {
		for (const auto& point : points) {
			os << point.x << " " << point.pMeasured << " " << (point.pEstimated * 100.0) << "% " << point.confidence << std::endl;
		}
	}

	size_t numPoints() const {
		return points.size();
	}

	const double CONFIDENCE_THRESHOLD = 5;
	std::vector<Point> points;
};