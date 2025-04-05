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

#include <vector>
#include <cmath>
#include <algorithm>

std::vector<int> generateSmoothStepCurve(int size, int minValue, int maxValue, int kinkLow, int kinkHigh) {
    std::vector<int> values(size);
    if (size < 2 || kinkLow >= kinkHigh || kinkLow < 0 || kinkHigh >= size)
        return values;

    auto smoothStep = [](double t) {
        return t * t * (3 - 2 * t);
        };

    for (int i = 0; i < size; ++i) {
        double norm = 0.0;

        if (i < kinkLow)
            norm = 0.0;
        else if (i > kinkHigh)
            norm = 1.0;
        else {
            double t = double(i - kinkLow) / double(kinkHigh - kinkLow);
            norm = smoothStep(t);
        }

        double scaled = minValue + norm * (maxValue - minValue);
        values[i] = std::clamp(int(std::round(scaled)), minValue, maxValue);
    }

    return values;
}


std::vector<int> generateSigmoidCurve(int size, int minValue, int maxValue, int kinkLow, int kinkHigh) {
    std::vector<int> values(size);
    if (size < 2 || kinkLow >= kinkHigh || kinkLow < 0 || kinkHigh >= size)
        return values;

    auto sigmoid = [](double x) {
        return 1.0 / (1.0 + std::exp(-x));
        };

    double startX = -6.0;
    double endX = 6.0;

    for (int i = 0; i < size; ++i) {
        double norm = 0.0;

        if (i < kinkLow)
            norm = 0.0;
        else if (i > kinkHigh)
            norm = 1.0;
        else {
            double t = double(i - kinkLow) / double(kinkHigh - kinkLow); // 0..1
            double x = startX + t * (endX - startX);                     
            double y = sigmoid(x);

            // Normiere so, dass sigmoid(-6) = 0 und sigmoid(6) = 1
            norm = (y - sigmoid(startX)) / (sigmoid(endX) - sigmoid(startX));
        }

        double scaled = minValue + norm * (maxValue - minValue);
        values[i] = std::clamp(int(std::round(scaled)), minValue, maxValue);
    }

    return values;
}



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

KingAttackCandidate::KingAttackCandidate() {
    addWeight(std::vector<QaplaBasics::EvalValue>{
        0, 0, 0, 0, 0, -2, 0, -7, -2, -15, -9, -26, -20, -39, -34, -55,
        -52, -73, -73, -93, -96, -115, -121, -137, -148, -161, -176, -186, -205, -211, -235, -237,
        -265, -263, -295, -289, -324, -314, -352, -339, -379, -363, -404, -385, -427, -407, -448, -427,
        -466, -445, -480, -461, -491, -474, -498, -485, -500, -493, -500, -498, -500, -500, -500, -500
    });
    addWeight(std::vector<QaplaBasics::EvalValue>{
        0, 0, 0, 0, -1, -3, -5, -8, -11, -15, -19, -24, -29, -35, -41, -48, 0, 0, 0, 0, -1, -3, -5, -8, -11, -15, -19, -24, -29, -35, -41, -48
    });
    addWeight(std::vector<QaplaBasics::EvalValue>{
        0, 0, 0, -1, -1, -2, -3, -3, -4, -5, -7, -11, -14, -18, -23, -28, 0, -1, -1, -2, -3, -4, -5, -7, -9, -11, -16, -21, -27, -34, -41, -49
    });
    addWeight(std::vector<QaplaBasics::EvalValue>{
        0, 0, 0, -1, -1, -3, -4, -4, -6, -7, -10, -15, -19, -25, -31, -38, 0, -1, -1, -2, -3, -4, -5, -8, -10, -12, -18, -24, -31, -39, -48, -57
    });
    addWeight(std::vector<QaplaBasics::EvalValue>{
        0, -1, -1, -2, -2, -4, -5, -5, -7, -9, -12, -18, -23, -30, -38, -46, 0, -1, -1, -2, -4, -5, -7, -9, -12, -15, -22, -29, -37, -47, -57, -68
    });
	addWeight(std::vector<QaplaBasics::EvalValue>{
        237, 90, 117, 136, 110, 114, 171, 140, 56, 113, 100, 152, 79, 92, 122, 70
	});
    startIndex = 0;
    numIndex = 26;
    setRadius(0.1);
    minScale = 0.4;
    maxScale = 1.6;
    maxGames = 100000;
    /*
    auto curve0 = generateSmoothStepCurve(32, 0, 500, 3, 28);
    auto curve1 = generateSmoothStepCurve(32, 0, 500, 1, 30);
	for (uint32_t i = 0; i < weights[0].size() / 2; ++i) {
		originalWeights[0][i * 2] = EvalValue(
			static_cast<value_t>(-curve0[i]),
			static_cast<value_t>(0)
		);
		originalWeights[0][i * 2 + 1] = EvalValue(
			static_cast<value_t>(-curve1[i]),
			static_cast<value_t>(0)
		);
	}
	weights[0] = originalWeights[0];
    */
}



void KingAttackCandidate::scaleIndex(uint32_t index, double scale, bool noScale) {
    uint32_t baseIndex = 0;
    uint32_t weightIndex = 0;
    if (index >= 0 && index <= 1) {
		weightIndex = 0;
		scaleType(weightIndex, index % 2, 2, weights[0].size(), scale, noScale);
	}
    else if (index >= 2 && index <= 2) {
		weightIndex = 1;
        scaleType(weightIndex, 0, 1, weights[1].size(), scale, noScale);
    }
    else if (index >= 3 && index <= 9) {
		weightIndex = 2 + ((index - 3) / 2);
		const size_t step = (index - 3) % 2;
		const size_t weightSize = weights[weightIndex].size();
		scaleType(weightIndex, uint32_t(step * (weightSize / 2)), 1, weightSize / 2, scale, noScale);
    }
    else if (index >= 10 && index <= 25) {
        setRadius(0.1);
		weightIndex = 5;
		scaleType(weightIndex, index - 10, 1, 1, scale, noScale);
    }
    else if (index == 26) {
        // do nothing
    }
    for (auto& weight : weights[weightIndex]) {
        std::cout << weight.midgame() << ",";
    }
    std::cout << std::endl;
}

void KingAttackCandidate::print(std::ostream& os) const {
    printShort(os);
    for (auto& vec : weights) {
        os << "{";
        std::string sep = "";
        for (const auto& v : vec) {
            os << sep << v.midgame();
            sep = ", ";
        }
        os << "}" << std::endl;
    }
    os << std::endl;
}

PawnShieldCandidate::PawnShieldCandidate() {
    addWeight(std::vector<QaplaBasics::EvalValue>{
        -8, -9, -9, -5, -9, -4, 5, 10
    });
    startIndex = 8;
    numIndex = 9;
    setRadius(0.2);
    minScale = -10;
    maxScale = +10;
    maxGames = 10000;
}

void PawnShieldCandidate::scaleIndex(uint32_t index, double scale, bool noScale) {
    uint32_t baseIndex = 0;
    uint32_t weightIndex = 0;
    if (index >= 0 && index <= 7) {
        weightIndex = 0;
        scaleType(weightIndex, index, 1, 1, scale, noScale);
	}
	else if (index == 8) {
        weightIndex = 0;
		scaleType(weightIndex, 0, 1, weights[weightIndex].size(), scale, noScale);
	}
    for (auto& weight : weights[weightIndex]) {
        std::cout << weight.midgame() << ",";
    }
    std::cout << std::endl;
}

PawnCandidate::PawnCandidate() {
    startIndex = 0;
    numIndex = 1;
    setRadius(0.1);
    minScale = 1;
    maxScale = 1;
    maxGames = 300000;
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
    if (n >= maxGames) return true;
    if (n <= MIN_GAMES) return false;
    double p = score();
    auto [lowerBound, upperBound] = getConfidenceInterval();
    double p_extreme = (std::abs(lowerBound - bestValue) > std::abs(upperBound - bestValue)) ? lowerBound : upperBound;
    double p_future = (p * n + p_extreme * (maxGames - n)) / (maxGames * 1.0);

    double stddev_future = std::sqrt(p_future * (1 - p_future) / (maxGames * 1.0));
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
    return lowerBound <= bestValue && upper_bound >= bestValue && !isProbablyNeutral() && numGames() < maxGames;
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

void Candidate::scaleType(uint32_t weightIndex, uint32_t itemBaseIndex, size_t loopStep, size_t loopMax, double scale, bool noScale) {
    auto originalWeight = originalWeights[weightIndex];
    auto weight = weights[weightIndex];
    if (noScale) {
        weights[weightIndex] = originalWeight;
        for (auto& weight : weights[weightIndex]) {
            weight.endgame() = 0;
        }
        return;
    }
    for (size_t i = 0; i < loopMax; i += loopStep) {
        const auto itemIndex = itemBaseIndex + uint32_t(i);
        weights[weightIndex][itemIndex] = EvalValue(
            static_cast<value_t>(originalWeight[itemIndex].midgame() * scale),
            static_cast<value_t>(0)
        );
    }
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
    addCandidate(std::make_unique<PawnCandidate>());
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

    if (optimizer.goodEnough() || optimizer.unrelevant()) {
		auto best = optimizer.getBest().second;
		if (best.pEstimated < currentCandidate->getLastBestValue() + 0.002) {
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
    currentCandidate->scaling = optimizer.nextX(currentCandidate->minScale, currentCandidate->maxScale);
	currentCandidate->setBestValue(std::max(0.5, optimizer.getBest().second.pEstimated));
	currentCandidate->scaleIndex(optimizerIndex, currentCandidate->scaling);
    //getCurrentCandidate().print(std::cout);
    currentCandidate->setId("Weights scaled by " + std::to_string(currentCandidate->scaling) + " Index: " + std::to_string(optimizerIndex));
}

bool CandidateTrainer::shallTerminate() {
    const Candidate& c = getCurrentCandidate();
    if (c.isWorse() && c.numGames() > 1000) return true;
	if (c.score() > c.getLastBestValue()) return c.numGames() >= c.maxGames;
	auto numPoints = optimizer.numPoints();
    return c.numGames() >= c.maxGames;
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
        optimizerIndex = currentCandidate->startIndex;
        ++candidateIndex;
    }
}

