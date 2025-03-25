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
#include "../basics/evalvalue.h"
#include "optimizer.h"

using namespace QaplaBasics;

class Candidate {
public:
	Candidate() :wins(0), draws(0), losses(0), bestValue(0.5) {}

    // Access the i-th weight vector
    const std::vector<EvalValue>& getWeightVector(size_t index) const {
        assert(index < weights.size());
        return weights[index];
    }

	void setBestValue(double value) {
		bestValue = value;
	}

    std::vector<EvalValue>& getWeightVector(size_t index) {
        assert(index < weights.size());
        return weights[index];
    }

    // Initialize weights with external source (e.g., current engine config)
    void addWeight(const std::vector<EvalValue>& initial) {
		weights.push_back(initial);
    }

    // Returns number of weight vectors
    size_t weightVectorCount() const {
        return weights.size();
    }

    // Rescale all weight vectors 
	void rescaleAllWeightVectors(double factor) {
		for (size_t i = 0; i < weights.size(); ++i) {
			rescaleWeightVector(i, factor);
		}
	}

    // Rescale weight vector to preserve average evaluation (based on getValue(50))
    void rescaleWeightVector(size_t index, double factor) {
        assert(index < weights.size());
        std::vector<EvalValue>& vec = weights[index];
        if (vec.empty()) return;

        double originalSum = 0.0;
        for (const auto& v : vec) originalSum += v.getValue(50);

        for (size_t i = 0; i < vec.size(); ++i) {
            EvalValue scaled(
                static_cast<value_t>(vec[i].midgame() * factor),
                static_cast<value_t>(vec[i].endgame() * factor)
            );
            vec[i] = scaled;
        }

		correctAverage(vec, originalSum);
    }

    // Adjusts the vector so its average (getValue(50)) matches the target average
    void correctAverage(std::vector<EvalValue>& vec, double targetAverage) {
        if (vec.empty()) return;

        double currentSum = 0.0;
        for (const auto& v : vec) currentSum += v.getValue(50);

        double currentAverage = currentSum / vec.size();
        double delta = (targetAverage - currentAverage) / vec.size();

        for (auto& v : vec) {
            value_t mid = static_cast<value_t>(v.midgame() + delta + 0.5);
            value_t end = static_cast<value_t>(v.endgame() + delta + 0.5);
            v = EvalValue(mid, end);
        }
    }

	size_t numWeights() const {
		return weights.size();
	}

	uint32_t numGames() const {
		return wins + draws + losses;
	}

    double score() const {
        return (wins + 0.5 * draws) / (wins + draws + losses);
    }

	void setId(const std::string& id) {
		this->id = id;
	}

    std::pair<double, double> getConfidenceInterval() const;

    bool isProbablyNeutral() const;

    bool isBetter() const;
    bool isWorse() const;
    bool isUnknown() const;

    void setGameResult(bool win, bool draw) {
        if (win) wins++;
        else if (draw) draws++;
        else losses++;
    }

    int wins = 0;
    int draws = 0;
    int losses = 0;

	void printShort(std::ostream& os) const {
		os << id 
            << " score: " << std::setprecision(3) << (score() * 100.0) << "%" 
            << " games: " << numGames() 
            << " (" << wins << "W, " << draws << "D, " << losses << "L)" 
            << std::endl;
	}

	void print(std::ostream& os) const {
		printShort(os);
		for (auto& vec : weights) {
            os << "{";
			std::string sep = "";
			for (const auto& v : vec) {
				os << sep << v;
				sep = ", ";
			}
			os << "}" << std::endl;
		}
        os << std::endl;
	}

protected:
    bool done = false;
    std::string id;
    std::vector<std::vector<EvalValue>> weights; // Indexed lookup tables
    const double Z98 = 2.3263;
    double bestValue;
    const uint32_t MAX_GAMES = 10000;
    const uint32_t MIN_GAMES = 2000;
public:
    double scaling;
};

class MobilityCandidate : public Candidate {
public:
	MobilityCandidate() {
        // Queen mobility
        addWeight({
            -10, -10, -10, -5, 0, 2, 4, 5, 6, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10
            });
        // Rook mobility
        addWeight({
            { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 8, 8 }, { 12, 12 }, { 16, 16 },
            { 20, 20 }, { 25, 25 }, { 25, 25 }, { 25, 25 }, { 25, 25 }, { 25, 25 }, { 25, 25 } });
        // bishop mobility
        addWeight({ { -15, -25 }, { -10, -15 }, { 0, 0 }, { 5, 5 }, { 8, 8 }, { 13, 13 }, { 16, 16 }, { 18, 18 },
            { 20, 20 }, { 22, 22 }, { 24, 24 }, { 25, 25 }, { 25, 25 }, { 25, 25 }, { 25, 25 } });
		// knight mobility
        addWeight({ { -30, -30 }, { -20, -20 }, { -10, -10 }, { 0, 0 }, { 10, 10 },
            { 20, 20 }, { 25, 25 }, { 25, 25 }, { 25, 25 } });

        //rescaleAllWeightVectors(0.5);
	}

};

class CandidateTrainer {
public:
    static void initializePopulation(int count) {
		//buildScaledPopulation(0.5, 1.5, 0.1);
		currentCandidate = nullptr;
        nextStep();
    }

    static void buildScaledPopulation(double min, double max, double step) {
        population.clear();
		MobilityCandidate c;
		for (size_t i = 0; i < c.numWeights(); i++) {
			for (double scale = min; scale <= max; scale += step) {
				if (scale == 1.0) continue;
                auto c = make_unique<MobilityCandidate>();
				c->rescaleWeightVector(i, scale);
				c->setId("Mobility " + std::to_string(i) + " scaled by " + std::to_string(scale));
                population.push_back(std::move(c));
			}
		}
        for (double scale = min; scale <= max; scale += step) {
            if (scale == 1.0) continue;
            auto c = make_unique<MobilityCandidate>();
            c->rescaleAllWeightVectors(scale);
			c->setId("Mobility weights scaled by " + std::to_string(scale));
            population.push_back(std::move(c));
        }
        currentIndex = 0;
    }

    static void nextStep() {
		nextStepOnOptimizer();
    }

    static void nextStepOnOptimizer();

    static void nextStepOnPopulation();

    static bool finished() {
        return finishedFlag;
    }

    static std::pair<double, double> getConfidenceInterval() {
        const Candidate& c = getCurrentCandidate();
		return c.getConfidenceInterval();
    }

    static bool shallTerminate();

    static Candidate& getCurrentCandidate() {
        return *currentCandidate;
    }

    static void setGameResult(bool win, bool draw) {
        Candidate& c = getCurrentCandidate();
		c.setGameResult(win, draw);
    }

	static double getScore() {
		return getCurrentCandidate().score() * 100.0;
	}

    static void printBest(std::ostream& os) {
        os << "Best candidate after training:\n";
        uint32_t bestIndex = -1;
		for (uint32_t i = 1; i < population.size(); ++i) {
			if (population[i]->score() > population[bestIndex]->score()) {
				bestIndex = i;
			}
		}
        if (bestIndex >= 0) {
			population[bestIndex]->print(std::cout);
        }
    }

    static void sort() {
        std::sort(population.begin(), population.end(), [](const std::unique_ptr<Candidate>& a, const std::unique_ptr<Candidate>& b) {
            return a->score() > b->score();
            });
    }

	static void printAll(std::ostream& os) {
		sort();
        cout << std::endl;
		for (const auto& c : population) {
            c->print(std::cout);
            if (c->score() <= 0.5) break;
		}
	}

private:
    static inline std::vector<std::unique_ptr<Candidate>> population;
    static inline std::unique_ptr<Candidate> currentCandidate;
    static inline size_t currentIndex = 0;
    static inline bool finishedFlag = false;
	static inline Optimizer optimizer;
};
