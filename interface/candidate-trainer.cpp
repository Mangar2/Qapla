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

#include "candidate-trainer.h"

std::pair<double, double> Candidate::getConfidenceInterval() const {
    double z = Z98; // 98% confidence interval, for 95% use 1.96, for 99% use 2.58
    int n = numGames();
    double p = score();
    auto stddev = std::sqrt(p * (1 - p) / n);
    double upperBound = p + z * stddev;
    double lowerBound = p - z * stddev;
    return { lowerBound, upperBound };
}

bool Candidate::isProbablyNeutral() const {
    double z = Z98; // 98% confidence interval, for 95% use 1.96, for 99% use 2.58
    uint32_t n = numGames();
    if (n >= MAX_GAMES) return true;
    if (n <= MIN_GAMES) return false;
    double p = score();
    auto [lowerBound, upperBound] = getConfidenceInterval();
    double p_extreme = (std::abs(lowerBound - bestValue) > std::abs(upperBound - bestValue)) ? lowerBound : upperBound;
    double p_future = (p * n + p_extreme * (MAX_GAMES - n)) / (MAX_GAMES * 1.0);

    double stddev_future = std::sqrt(p_future * (1 - p_future) / (MAX_GAMES * 1.0));
    double lower_future = p_future - z * stddev_future;
    double upper_future = p_future + z * stddev_future;

    return (lower_future <= bestValue && upper_future >= bestValue);
}

bool Candidate::isBetter() const {
    const auto [lowerBound, upper_bound] = getConfidenceInterval();
    return lowerBound > bestValue;
}
bool Candidate::isWorse() const {
    const auto [lowerBound, upper_bound] = getConfidenceInterval();
    return upper_bound < bestValue;
}
bool Candidate::isUnknown() const {
    if (numGames() < MIN_GAMES) return true;
    const auto [lowerBound, upper_bound] = getConfidenceInterval();
    return lowerBound <= bestValue && upper_bound >= bestValue && !isProbablyNeutral() && numGames() < MAX_GAMES;
}

void CandidateTrainer::nextStepOnOptimizer() {
    if (currentCandidate) {
        optimizer.addPoint(currentCandidate->scaling, currentCandidate->score());
        optimizer.printBest(std::cout);
        currentCandidate->printShort(std::cout);
    }
    currentCandidate = std::make_unique<MobilityCandidate>();
    currentCandidate->scaling = optimizer.nextX();
    const uint32_t index = 2;
    currentCandidate->rescaleWeightVector(index, currentCandidate->scaling);
    currentCandidate->setId("Mobility weights scaled by " + std::to_string(currentCandidate->scaling) + " index: " + std::to_string(index));
    finishedFlag = optimizer.goodEnough();
    
}

bool CandidateTrainer::shallTerminate() {
    const Candidate& c = getCurrentCandidate();
    if (!c.isUnknown()) return true;
    return c.numGames() >= 10000;
}

void CandidateTrainer::nextStepOnPopulation() {
    std::cout << std::endl;
    getCurrentCandidate().printShort(std::cout);
    ++currentIndex;
    if (currentIndex >= population.size()) {
        finishedFlag = true;
    }
    currentCandidate = std::move(population[currentIndex]);
}

