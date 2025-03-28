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

MobilityCandidate::MobilityCandidate() {
    // knight mobility
    addWeight({ { -30, -30 }, { -20, -20 }, { -10, -10 }, { 0, 0 }, { 10, 10 },
        { 20, 20 }, { 25, 25 }, { 25, 25 }, { 25, 25 } });
    // bishop mobility
    addWeight({ { -15, -25 }, { -10, -15 }, { 0, 0 }, { 5, 5 }, { 8, 8 }, { 13, 13 }, { 16, 16 }, { 18, 18 },
        { 20, 20 }, { 22, 22 }, { 24, 24 }, { 25, 25 }, { 25, 25 }, { 25, 25 }, { 25, 25 } });
    // Rook mobility
    addWeight({
        { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 8, 8 }, { 12, 12 }, { 16, 16 },
        { 20, 20 }, { 25, 25 }, { 25, 25 }, { 25, 25 }, { 25, 25 }, { 25, 25 }, { 25, 25 } });
    // Queen mobility
    addWeight({
        -10, -10, -10, -5, 0, 2, 4, 5, 6, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10
        });
}


PropertyCandidate::PropertyCandidate() {
    // knight
    addWeight({ { 0,  0}, { 20,   0}, {  0,   0}, { 20,   0} });
	// bishop
    addWeight({ { 0,  0}, { 10,   5}, {  0,   0}, { 10,   5} });
    // rook
    addWeight({ 
          {  0,   0}, {-50,   0}, { 10,   0}, {-40,   0}, { 10,   0}, {-40,   0}, { 20,   0}, {-30,   0},
          { 20,   0}, {-30,   0}, { 30,   0}, {-20,   0}, { 30,   0}, {-20,   0}, { 40,   0}, {-10,   0},
          {  0,   0}, {-50,   0}, { 10,   0}, {-40,   0}, { 10,   0}, {-40,   0}, { 20,   0}, {-30,   0},
          { 20,   0}, {-30,   0}, { 30,   0}, {-20,   0}, { 30,   0}, {-20,   0}, { 40,   0}, {-10,   0},
          { 10,   0}, {-40,   0}, { 20,   0}, {-30,   0}, { 20,   0}, {-30,   0}, { 30,   0}, {-20,   0},
          { 30,   0}, {-20,   0}, { 40,   0}, {-10,   0}, { 40,   0}, {-10,   0}, { 50,   0}, {  0,   0},
          { 10,   0}, {-40,   0}, { 20,   0}, {-30,   0}, { 20,   0}, {-30,   0}, { 30,   0}, {-20,   0},
          { 30,   0}, {-20,   0}, { 40,   0}, {-10,   0}, { 40,   0}, {-10,   0}, { 50,   0}, {  0,   0},
          { 10,   0}, {-40,   0}, { 20,   0}, {-30,   0}, { 20,   0}, {-30,   0}, { 30,   0}, {-20,   0},
          { 30,   0}, {-20,   0}, { 40,   0}, {-10,   0}, { 40,   0}, {-10,   0}, { 50,   0}, {  0,   0},
          { 10,   0}, {-40,   0}, { 20,   0}, {-30,   0}, { 20,   0}, {-30,   0}, { 30,   0}, {-20,   0},
          { 30,   0}, {-20,   0}, { 40,   0}, {-10,   0}, { 40,   0}, {-10,   0}, { 50,   0}, {  0,   0},
          {  0,   0}, {-50,   0}, { 10,   0}, {-40,   0}, { 10,   0}, {-40,   0}, { 20,   0}, {-30,   0},
          { 20,   0}, {-30,   0}, { 30,   0}, {-20,   0}, { 30,   0}, {-20,   0}, { 40,   0}, {-10,   0},
          {  0,   0}, {-50,   0}, { 10,   0}, {-40,   0}, { 10,   0}, {-40,   0}, { 20,   0}, {-30,   0},
          { 20,   0}, {-30,   0}, { 30,   0}, {-20,   0}, { 30,   0}, {-20,   0}, { 40,   0}, {-10,   0},
          {  0,   0}, {-50,   0}, { 10,   0}, {-40,   0}, { 10,   0}, {-40,   0}, { 20,   0}, {-30,   0},
          { 20,   0}, {-30,   0}, { 30,   0}, {-20,   0}, { 30,   0}, {-20,   0}, { 40,   0}, {-10,   0},
          {  0,   0}, {-50,   0}, { 10,   0}, {-40,   0}, { 10,   0}, {-40,   0}, { 20,   0}, {-30,   0},
          { 20,   0}, {-30,   0}, { 30,   0}, {-20,   0}, { 30,   0}, {-20,   0}, { 40,   0}, {-10,   0},
          { 20,   0}, {-30,   0}, { 30,   0}, {-20,   0}, { 30,   0}, {-20,   0}, { 40,   0}, {-10,   0},
          { 40,   0}, {-10,   0}, { 50,   0}, {  0,   0}, { 50,   0}, {  0,   0}, { 60,   0}, { 10,   0},
          { 20,   0}, {-30,   0}, { 30,   0}, {-20,   0}, { 30,   0}, {-20,   0}, { 40,   0}, {-10,   0},
          { 40,   0}, {-10,   0}, { 50,   0}, {  0,   0}, { 50,   0}, {  0,   0}, { 60,   0}, { 10,   0},
          { 20,   0}, {-30,   0}, { 30,   0}, {-20,   0}, { 30,   0}, {-20,   0}, { 40,   0}, {-10,   0},
          { 40,   0}, {-10,   0}, { 50,   0}, {  0,   0}, { 50,   0}, {  0,   0}, { 60,   0}, { 10,   0},
          { 20,   0}, {-30,   0}, { 30,   0}, {-20,   0}, { 30,   0}, {-20,   0}, { 40,   0}, {-10,   0},
          { 40,   0}, {-10,   0}, { 50,   0}, {  0,   0}, { 50,   0}, {  0,   0}, { 60,   0}, { 10,   0},
          {  0,   0}, {-50,   0}, { 10,   0}, {-40,   0}, { 10,   0}, {-40,   0}, { 20,   0}, {-30,   0},
          { 20,   0}, {-30,   0}, { 30,   0}, {-20,   0}, { 30,   0}, {-20,   0}, { 40,   0}, {-10,   0},
          {  0,   0}, {-50,   0}, { 10,   0}, {-40,   0}, { 10,   0}, {-40,   0}, { 20,   0}, {-30,   0},
          { 20,   0}, {-30,   0}, { 30,   0}, {-20,   0}, { 30,   0}, {-20,   0}, { 40,   0}, {-10,   0}
     });
    // queen
    addWeight({ { 0,   0 }, { 0,   0 } });
}

int countBits(size_t number) {
    int count = 0;
    while (number > 0) {
        count++;
        number >>= 1; // Rechtsverschiebung um 1 Bit
    }
    return count;
}

void PropertyCandidate::scaleIndex(uint32_t index, double scale, bool noScale) {
	const std::string indexNames[4] = { "knight", "bishop", "rook", "queen" };
	bool isMidgame = index % 2 == 0;
	int32_t remainingIndex = index / 2;
	for (uint32_t vectorIndex = 0; vectorIndex < weights.size() && remainingIndex >= 0; ++vectorIndex) {
        uint64_t propertyBit = 1ULL << remainingIndex;
        if (propertyBit < weights[vectorIndex].size()) {
			std::cout << "Scaling vector " << indexNames[vectorIndex] << " property " << remainingIndex << " phase " << (isMidgame ? "midgame" : "endgame") << " with " << scale;
            for (uint32_t valueIndex = 0; valueIndex < weights[vectorIndex].size(); ++valueIndex) {
                if (propertyBit & valueIndex) {
                    EvalValue propertyWeight = originalWeights[vectorIndex][propertyBit];
                    EvalValue baseWeight = weights[vectorIndex][valueIndex & (~propertyBit)];
					EvalValue currentValue = weights[vectorIndex][valueIndex];
                    weights[vectorIndex][valueIndex] = EvalValue(
                        static_cast<value_t>(isMidgame && !noScale ? scaleValue(baseWeight.midgame(), propertyWeight.midgame(), scale) : currentValue.midgame()),
						static_cast<value_t>(!isMidgame && !noScale ? scaleValue(baseWeight.endgame(), propertyWeight.endgame(), scale) : currentValue.endgame())
                    );
                }
				std::cout << " " << weights[vectorIndex][valueIndex];
            }
			std::cout << std::endl;
            break;
        }
		remainingIndex -= countBits(weights[vectorIndex].size() - 1);
	}
}

void PropertyCandidateTemplate::scaleIndex(uint32_t index, double scale, bool noScale) {
    bool isMidgame = index % 2 == 0;
    const uint32_t vectorIndex = 0;
    int32_t remainingIndex = index / 2;
    uint64_t propertyBit = 1ULL << remainingIndex;
    if (propertyBit < weights[vectorIndex].size()) {
        std::cout << "Scaling vector " << pieceName << " property " << remainingIndex << " phase " << (isMidgame ? "midgame" : "endgame") << " with " << scale << std::endl;
        for (uint32_t valueIndex = 0; valueIndex < weights[vectorIndex].size(); ++valueIndex) {
            if (propertyBit & valueIndex) {
                EvalValue propertyWeight = originalWeights[vectorIndex][propertyBit];
                EvalValue baseWeight = weights[vectorIndex][valueIndex & (~propertyBit)];
                EvalValue currentValue = weights[vectorIndex][valueIndex];
                weights[vectorIndex][valueIndex] = EvalValue(
                    static_cast<value_t>(isMidgame && !noScale ? scaleValue(baseWeight.midgame(), propertyWeight.midgame(), scale) : currentValue.midgame()),
                    static_cast<value_t>(!isMidgame && !noScale ? scaleValue(baseWeight.endgame(), propertyWeight.endgame(), scale) : currentValue.endgame())
                );
            }
        }
    }
}

void Candidate::addWeight(const std::vector<EvalValue>& initial) {
    weights.push_back(initial);
    originalWeights.push_back(initial);
}

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

// Rescale weight vector to preserve average evaluation (based on getValue(50))
void Candidate::rescaleWeightVector(size_t index, double factor) {
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

void Candidate::rescaleWeightPhase(size_t index, double factor) {
	assert(index < weights.size());
	std::vector<EvalValue>& vec = weights[index / 2];
    if (vec.empty()) return;
    bool isMidgame = index % 2 == 0;
    double originalSum = 0.0;
    for (const auto& v : vec) originalSum += isMidgame ? v.midgame() : v.endgame();

	double midgameFactor = isMidgame ? factor : 1.0;
	double endgameFactor = !isMidgame ? factor : 1.0;
	if (vec.empty()) return;
	for (size_t i = 0; i < vec.size(); ++i) {
		EvalValue scaled(
			static_cast<value_t>(vec[i].midgame() * midgameFactor),
			static_cast<value_t>(vec[i].endgame() * endgameFactor)
		);
		vec[i] = scaled;
	}
	if (isMidgame) correctAverageMidgame(vec, originalSum);
	else correctAverageEndgame(vec, originalSum);
}

// Adjusts the vector so its average (getValue(50)) matches the target average
void Candidate::correctAverage(std::vector<EvalValue>& vec, double targetAverage) {
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

void Candidate::correctAverageMidgame(std::vector<EvalValue>& vec, double targetAverage) {
    if (vec.empty()) return;

    double currentSum = 0.0;
    for (const auto& v : vec) currentSum += v.midgame();

    double currentAverage = currentSum / vec.size();
    double delta = (targetAverage - currentAverage) / vec.size();

    for (auto& v : vec) {
        value_t mid = static_cast<value_t>(v.midgame() + delta + 0.5);
        value_t end = static_cast<value_t>(v.endgame());
        v = EvalValue(mid, end);
    }
}

void Candidate::correctAverageEndgame(std::vector<EvalValue>& vec, double targetAverage) {
    if (vec.empty()) return;

    double currentSum = 0.0;
    for (const auto& v : vec) currentSum += v.endgame();

    double currentAverage = currentSum / vec.size();
    double delta = (targetAverage - currentAverage) / vec.size();

    for (auto& v : vec) {
        value_t mid = static_cast<value_t>(v.midgame());
        value_t end = static_cast<value_t>(v.endgame() + delta + 0.5);
        v = EvalValue(mid, end);
    }
}

void CandidateTrainer::initializePopulation() {
    optimizerIndex = 0;
    candidateIndex = 0;
	currentCandidate = nullptr;
    /*
    addCandidate(std::make_unique<PropertyCandidateTemplate>(
        "Bishop",
        std::vector<EvalValue>({ { 0,  0}, { 10,   5}, {  0,   0}, { 10,   5} })
    ));
    */
	addCandidate(std::make_unique<PropertyCandidateTemplate>(
        "Queen",
		std::vector<EvalValue>({ { 0,   0 }, { 0,   0 } })
    ));
    nextStep();
}


void CandidateTrainer::nextStepOnOptimizer() {
    if (!currentCandidate) {
        cout << "No candidate to optimize" << std::endl;
        return;
    }
    if (currentCandidate->numGames() > 0) {
        optimizer.addPoint(currentCandidate->scaling, currentCandidate->score(), currentCandidate->getRadius());
        optimizer.printBest(std::cout);
        currentCandidate->printShort(std::cout);
    }

    if (optimizer.goodEnough()) {
		auto best = optimizer.getBest().second;
		if (best.pEstimated < currentCandidate->getLastBestValue() + 0.004) {
            currentCandidate->scaleIndex(optimizerIndex, 1, true);
        }
        else {
            currentCandidate->scaleIndex(optimizerIndex, best.x);
        }
        optimizer.clear();
        currentCandidate->print(std::cout);
        if (optimizerIndex >= currentCandidate->getNumIndex()) {
			currentCandidate = nullptr;
			cout << "Candidate optimized" << endl;
            return;
        }
        optimizerIndex++;
    }
    else {
        // optimizer index too high, no map to optimize
        if (optimizerIndex >= currentCandidate->getNumIndex()) {
            cout << "No more vectors to optimize for the current candidate. Shouldn´t come here! " << std::endl;
            return;
        }
    }

    currentCandidate->clear();
    currentCandidate->scaling = optimizer.nextX(-10, 10);
	currentCandidate->setBestValue(std::max(0.5, optimizer.getBest().second.pEstimated));
	currentCandidate->scaleIndex(optimizerIndex, currentCandidate->scaling);
    getCurrentCandidate().print(std::cout);
    currentCandidate->setId("Weights scaled by " + std::to_string(currentCandidate->scaling) + " Index: " + std::to_string(optimizerIndex));
}

bool CandidateTrainer::shallTerminate() {
    const Candidate& c = getCurrentCandidate();
    if (c.isWorse() && c.numGames() > 1000) return true;
	if (c.score() > c.getLastBestValue()) return c.numGames() >= 10000;
	auto numPoints = optimizer.numPoints();
    return c.numGames() >= 10000;
}

void CandidateTrainer::nextStepOnPopulation() {
	cout << "Candidate " << (candidateIndex + 1) << " of " << population.size() << endl;
    if (candidateIndex >= population.size()) {
		cout << "Finished" << endl;
        finishedFlag = true;
    }
    else
    {
        currentCandidate = std::move(population[candidateIndex]);
        ++candidateIndex;
    }
}

