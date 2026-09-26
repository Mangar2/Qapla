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
 * @copyright Copyright (c) 2026 Volker Böhm
 * @Overview
 * Reads and writes a net file, see nnue-arch.h
 */

#include <fstream>
#include <iostream>

#include "nnue-arch.h"

using namespace QaplaNnue;

namespace {

	template <typename ARRAY>
	bool readArray(std::istream& stream, ARRAY& array) {
		stream.read(reinterpret_cast<char*>(array.data()),
			std::streamsize(array.size() * sizeof(typename ARRAY::value_type)));
		return bool(stream);
	}

	template <typename ARRAY>
	void writeArray(std::ostream& stream, const ARRAY& array) {
		stream.write(reinterpret_cast<const char*>(array.data()),
			std::streamsize(array.size() * sizeof(typename ARRAY::value_type)));
	}

	/** A small generator, so that a test net does not depend on a library. */
	class Random {
	public:
		explicit Random(uint64_t seed) : _state(seed != 0 ? seed : 0x9E3779B97F4A7C15ull) {}

		int32_t next(int32_t limit) {
			_state ^= _state << 13;
			_state ^= _state >> 7;
			_state ^= _state << 17;
			return int32_t(_state % uint64_t(2 * limit + 1)) - limit;
		}

	private:
		uint64_t _state;
	};
}

std::unique_ptr<Network> QaplaNnue::readNetwork(const std::string& path) {
	std::ifstream stream(path, std::ios::binary);
	if (!stream) {
		std::cout << "Error (cannot read net): " << path << std::endl;
		return nullptr;
	}
	char magic[sizeof(NNUE_MAGIC)] = {};
	uint32_t identifier = 0;
	stream.read(magic, sizeof(magic));
	stream.read(reinterpret_cast<char*>(&identifier), sizeof(identifier));
	if (!stream || std::string(magic, sizeof(magic)) != std::string(NNUE_MAGIC, sizeof(NNUE_MAGIC))) {
		std::cout << "Error (not a net file): " << path << std::endl;
		return nullptr;
	}
	if (identifier != architectureId()) {
		std::cout << "Error (net of another shape): " << path << ", file says " << identifier
			<< ", this build wants " << architectureId() << std::endl;
		return nullptr;
	}

	auto network = std::make_unique<Network>();
	const bool complete = readArray(stream, network->featureBias)
		&& readArray(stream, network->featureWeight)
		&& readArray(stream, network->l1Bias)
		&& readArray(stream, network->l1Weight)
		&& readArray(stream, network->l2Bias)
		&& readArray(stream, network->l2Weight)
		&& bool(stream.read(reinterpret_cast<char*>(&network->outputBias), sizeof(int32_t)))
		&& readArray(stream, network->outputWeight);
	if (!complete) {
		std::cout << "Error (net file is too short): " << path << std::endl;
		return nullptr;
	}
	return network;
}

bool QaplaNnue::writeNetwork(const std::string& path, const Network& network) {
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	if (!stream) {
		std::cout << "Error (cannot write net): " << path << std::endl;
		return false;
	}
	const uint32_t identifier = architectureId();
	stream.write(NNUE_MAGIC, sizeof(NNUE_MAGIC));
	stream.write(reinterpret_cast<const char*>(&identifier), sizeof(identifier));
	writeArray(stream, network.featureBias);
	writeArray(stream, network.featureWeight);
	writeArray(stream, network.l1Bias);
	writeArray(stream, network.l1Weight);
	writeArray(stream, network.l2Bias);
	writeArray(stream, network.l2Weight);
	stream.write(reinterpret_cast<const char*>(&network.outputBias), sizeof(int32_t));
	writeArray(stream, network.outputWeight);
	return bool(stream);
}

std::unique_ptr<Network> QaplaNnue::randomNetwork(uint64_t seed) {
	auto network = std::make_unique<Network>();
	Random random(seed);
	// Small weights on purpose: a column of the feature transformer is added up to
	// thirty times, and the activation saturates at QA. With weights of this size
	// the accumulator lands in the range the net will later be trained into
	// instead of being clipped everywhere.
	for (int16_t& value : network->featureBias) value = int16_t(random.next(QA / 4));
	for (int16_t& value : network->featureWeight) value = int16_t(random.next(QA / 16));
	for (int32_t& value : network->l1Bias) value = random.next(QA * QB / 4);
	for (int8_t& value : network->l1Weight) value = int8_t(random.next(QB / 2));
	for (int32_t& value : network->l2Bias) value = random.next(QA * QB / 4);
	for (int8_t& value : network->l2Weight) value = int8_t(random.next(QB / 2));
	network->outputBias = random.next(QA * QB / 4);
	for (int8_t& value : network->outputWeight) value = int8_t(random.next(QB / 2));
	return network;
}
