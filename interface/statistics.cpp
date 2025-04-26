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
 */


#include "statistics.h"
#include "../training/piece-signature-statistic.h"
#include "../training/signature-eval-adjuster.h"
#include "../training/position-filter.h"
#include "../eval/eval.h"
#include "winboardprintsearchinfo.h"
#include <thread>
#include <vector>
#include <algorithm>

using namespace std;

using namespace QaplaInterface;

std::map<std::string, std::vector<uint64_t>> createIndexLookupCount(const ChessEval::IndexLookupMap& original) {
	std::map<std::string, std::vector<uint64_t>> copy;

	for (const auto& [key, values] : original) {
		copy[key] = std::vector<uint64_t>(values.size(), 0);
	}

	return copy;
}

std::ostream& formatIndexLookupMap(const std::map<std::string, std::vector<uint64_t>>& map, std::ostream& os) {
	for (const auto& [key, values] : map) {
		os << key << ": ";
		std::string spacer = "";
		for (size_t i = 0; i < values.size(); ++i) {
			if (i % 8 == 0) {
				os << std::endl << "  (" << i << ") ";
				spacer = "";
			}
			os << spacer << values[i];
			spacer = ", ";
		}
		os << std::endl;
	}
	return os;
}

std::ostream& formatMultiplyIndexLookupMap(const ChessEval::IndexLookupMap& map, std::ostream& os) {
	for (const auto& [key, values] : map) {
		os << "static constexpr std::array<EvalValue, " << values.size() << "> " << key << "{ {";
		std::string spacer = "";
		std::string lineEnd = "";
		for (size_t i = 0; i < values.size(); ++i) {
			if (i % 8 == 0) {
				os << lineEnd << std::endl << "  ";
				lineEnd = ",";
				spacer = "";
			}
			os << spacer << (values[i] / 1000);
			spacer = ", ";
		}
		os << std::endl << "} };" << std::endl;
	}
	return os;
}

ChessEval::IndexLookupMap multiplyIndexLookupMap(const ChessEval::IndexLookupMap& original) {
	ChessEval::IndexLookupMap result;
	for (const auto& [key, vec] : original) {
		std::vector<EvalValue> newVec;
		newVec.reserve(vec.size());
		for (const auto& val : vec) {
			newVec.push_back(val * 1000);
		}
		result[key] = std::move(newVec);
	}
	return result;
}


bool Statistics::handleMove(string move) {
	move = move == "" ? getCurrentToken() : move;
	bool legalMove = false;
	if (!setMove(move)) {
		println("Illegal move: " + move);
	}
	else {
		legalMove = true;
	}
	return legalMove;
}

void Statistics::printGameResult(GameResult result) {
	switch (result) {
	case GameResult::DRAW_BY_REPETITION: println("1/2-1/2 {Draw by repetition}"); break;
	case GameResult::DRAW_BY_50_MOVES_RULE: println("1/2-1/2 {Draw by 50 moves rule}"); break;
	case GameResult::DRAW_BY_STALEMATE: println("1/2-1/2 {Stalemate}"); break;
	case GameResult::DRAW_BY_NOT_ENOUGHT_MATERIAL: println("1/2-1/2 {Not enough material to win}"); break;
	case GameResult::BLACK_WINS_BY_MATE: println("0-1 {Black mates}"); break;
	case GameResult::WHITE_WINS_BY_MATE: println("1-0 {White mates}"); break;
	case GameResult::NOT_ENDED: break; // do nothing
	}
}

Statistics::Statistics() : _sendSearchInfo(0) {
	_mode = Mode::WAIT;
	_computerIsWhite = false;
	_xBoardMode = false;
};

void Statistics::handleRemove() {
	if (_computerIsWhite != getBoard()->isWhiteToMove()) {
		getBoard()->undoMove();
		getBoard()->undoMove();
	}
}

void Statistics::analyzeMove() {
	_mode = Mode::ANALYZE;
	GameResult result = getBoard()->getGameResult();
	if (result != GameResult::NOT_ENDED) {
		printGameResult(result);
	}
	else {
		_clock.setAnalyseMode();
		setInfiniteSearch(true);
		getBoard()->setClock(_clock);
		getWorkerThread().startTask([this]() {
			getBoard()->computeMove();
			waitIfInfiniteSearchFinishedEarly();
		});
	}
}

void Statistics::WMTest() {
	uint32_t numThreads = 16;
	uint32_t depthLimit = 10;
	uint64_t totalNodesSearched = 0;
	while (getNextTokenNonBlocking() != "") {
		if (getCurrentToken() == "threads") {
			if (getNextTokenNonBlocking() != "") {
				numThreads = (uint32_t)getCurrentTokenAsUnsignedInt();
			}
		}
		else if (getCurrentToken() == "sd") {
			if (getNextTokenNonBlocking() != "") {
				depthLimit = (uint32_t)getCurrentTokenAsUnsignedInt();
			}
		}

	}
	loadEPD("wmtest.epd");
	_clock.setSearchDepthLimit(depthLimit);
	getBoard()->setClock(_clock);
	StdTimeControl timeControl;
	timeControl.storeStartTime();
	for (auto& epd : _startPositions) {
		getBoard()->newGame();
		ChessInterface::setPositionByFen(epd, getBoard());
		getBoard()->computeMove();
		auto info = getBoard()->getComputingInfo();
		totalNodesSearched += info.nodesSearched;
		std::cout << epd << " nodes: " << info.nodesSearched << " total: " << totalNodesSearched << std::endl;
	}
	std::cout << "Positions searched: " << _startPositions.size() 
		<< " Total nodes searched: " << totalNodesSearched 
		<< " Time used (s): " << (timeControl.getTimeSpentInMilliseconds() * 1.0 / 1000.0) << std::endl;
}

void Statistics::loadGamesFromFile(const std::string& filename) {
	std::ifstream file(filename);
	if (!file) {
		std::cerr << "Error: Could not open file " << filename << "\n";
		return;
	}
	_games.clear();

	std::string line;
	uint32_t count = 0;
	while (std::getline(file, line)) {
		std::istringstream ss(line);
		std::string fen, move;
		int eval;

		if (!std::getline(ss, fen, ',')) {
			continue;
		}

		ChessGame game(fen);
		while (std::getline(ss, move, ',')) {
			if (!(ss >> eval)) {
				break;
			}
			ss.ignore(); // Ignore comma separator
			game.addMove(move, eval);
		}
		count++;
		if (count % 5000 == 0) {  // Update nur alle 1000 Zeilen
			std::cout << "\rGames loaded: " << count << std::flush;
		}
		_games.push_back(game);
#ifdef _DEBUG
		if (count > 1000) break;
#endif
	}
	std::cout << "\rGames loaded: " << count << std::endl;
}

std::tuple<EvalValue, value_t> Statistics::computeEval(
	ChessEval::IndexLookupMap& lookupMap, std::map<std::string, std::vector<uint64_t>>& lookupCount, bool verbose) {
	auto indexVector = getBoard()->computeEvalIndexVector();
	std::map<std::string, EvalValue> aggregatedValues;

	EvalValue evalCalculated = 0;
	int32_t midgame = indexVector[0].index;
	int32_t midgameV2 = indexVector[1].index;
	assert(indexVector[0].name == "midgame" && indexVector[1].name == "midgamev2");
	for (auto indexInfo : indexVector) {
		if (indexInfo.name == "midgame" || indexInfo.name == "midgamev2") {
			continue;
		}
		auto index = indexInfo.index;
		assert(index < lookupMap[indexInfo.name].size());
		auto value = lookupMap[indexInfo.name][index];
		lookupCount[indexInfo.name][indexInfo.index]++;
		auto valueColor = indexInfo.color == WHITE ? value : -value;
		evalCalculated += valueColor;
		if (verbose) {
			cout << "sum: " << evalCalculated << " " << indexInfo.name << " value: " << value << " index: " << indexInfo.index << " color: " << (indexInfo.color == WHITE ? "white" : "black") << endl;
			std::string modifiedName = indexInfo.name.substr(1);
			aggregatedValues[modifiedName] += valueColor;
			aggregatedValues[indexInfo.name] += valueColor;
		}
	}
	return std::make_tuple(evalCalculated, midgameV2);
}

void Statistics::trainPosition(ChessEval::IndexLookupMap& lookupMap, int32_t evalDiff) {
	auto indexVector = getBoard()->computeEvalIndexVector();

	int32_t midgame = indexVector[0].index;
	int32_t midgameV2 = indexVector[1].index;
	assert(indexVector[0].name == "midgame" && indexVector[1].name == "midgamev2");
	int32_t eta = std::clamp(evalDiff, -100, 100);
	EvalValue etaEval(eta * midgameV2 / 100, eta * (100 - midgameV2) / 100);

	for (auto indexInfo : indexVector) {
		if (indexInfo.name == "midgame" || indexInfo.name == "midgamev2") {
			continue;
		}
		auto index = indexInfo.index;
		assert(index < lookupMap[indexInfo.name].size());
		auto& lookup = lookupMap[indexInfo.name];
		auto valueEta = (lookup[index].abs() * etaEval)/ 10000;
		if (indexInfo.color == WHITE) {
			lookup[index] += etaEval + valueEta;
		}
		else {
			lookup[index] -= etaEval + valueEta;
		}
	}
}

void Statistics::train() {
	loadGamesFromFile("games.txt");
	auto lookupMap = getBoard()->computeEvalIndexLookupMap();
	auto trainMap = multiplyIndexLookupMap(lookupMap);
	formatMultiplyIndexLookupMap(trainMap, std::cout);
	auto lookupCount = createIndexLookupCount(lookupMap);
	StdTimeControl timeControl;
	timeControl.storeStartTime();
	for (int epoch = 0; epoch < 100; epoch++) {
		int64_t difference = 0;
		int64_t differenceTest = 0;
		int64_t moveCountTrained = 0;
		int64_t moveCountTest = 0;
		uint32_t gameIndex = 0;
		for (auto& game : _games) {
			gameIndex++;
			setPositionByFen(game.fen);
			for (auto& movePair : game.moves) {
				auto [evalCalculated, midgameV2] = computeEval(lookupMap, lookupCount);
				auto [evalTrained, midgameV3] = computeEval(trainMap, lookupCount);
				int32_t eval = getBoard()->eval();
				int32_t positionValue = movePair.second;
				std::string move = movePair.first;

				// Eval is from side to move perspective, we need the eval always from white perspective
				if (!getBoard()->isWhiteToMove()) {
					eval = -eval;
					positionValue = -positionValue;
				}
				int32_t evalC = evalCalculated.getValue(midgameV2);

				if (std::abs(eval - evalC) > 3) {
					// We got into special endgame evaluation what is not supported here.
					break;
				}
				if (!getBoard()->isInCheck() && !isCapture(move)) {
					int32_t evalT = evalTrained.getValue(midgameV2);
					int32_t diff = (positionValue * 1000) - evalT;
					/*
					if (std::abs(diff) > 200000) {
						getBoard()->printEvalInfo();
						std::cout << "diff: " << diff << " move: " << move << " eval: " << eval << " evalC: " << evalC << " evalT: " << evalT << " positionValue: " << positionValue << std::endl;
					}
					*/
					if (gameIndex > _games.size() - 20000) {
						differenceTest += std::abs(diff);
						moveCountTest++;
					}
					else {
						trainPosition(trainMap, diff / 1000);
						difference += std::abs(diff);
						moveCountTrained++;
					}
				}
				if (!handleMove(move)) {
					break;
				}
			}
			// print the number of game and the total games as releation to cout e.g. 5/123
			if (gameIndex % 1000 == 0 && moveCountTrained > 0) {
				std::cout << "\rEpoch: " << epoch 
					<< " Games trained: " << gameIndex << "/" << _games.size() << " diff: " << (difference / moveCountTrained)
					<< " diff test: " << (differenceTest / std::max(int64_t(1), moveCountTest))
					<< " time spent: " << timeControl.getTimeSpentInMilliseconds() / 1000;
			}
		}
		std::cout 
			<< "\rEpoch: " << epoch << " Games trained: " << gameIndex << "/" << _games.size()
			<< " diff: " << (difference / std::max(int64_t(1), moveCountTrained))
			<< " diff test: " << (differenceTest / std::max(int64_t(1), moveCountTest)) 
			<< " time spent: " << timeControl.getTimeSpentInMilliseconds() / 1000
			<< std::endl;
		if (epoch % 10 == 0) {
			formatMultiplyIndexLookupMap(trainMap, std::cout);
		}
	}
	formatIndexLookupMap(lookupCount, std::cout);
	formatMultiplyIndexLookupMap(trainMap, std::cout);
}

void Statistics::playEpdGames(uint32_t numThreads) {
	uint32_t games = 0;
	uint64_t gpe = 2;
	// command line: epd file <epd-filename> output <output-filename> threads <numThreads> games <numGames> gpe <games per epd>
	while (getNextTokenNonBlocking() != "") {
		if (checkClockCommands()) {
			continue;
		}
		if (getCurrentToken() == "threads") {
			if (getNextTokenNonBlocking() != "") {
				numThreads = (uint32_t)getCurrentTokenAsUnsignedInt();
			}
		}
		else if (getCurrentToken() == "file") {
			if (getNextTokenNonBlocking() != "") {
				loadEPD(getCurrentToken());
			}
		}
		else if (getCurrentToken() == "output") {
			if (getNextTokenNonBlocking() != "") {
				epdTasks.setOutputFile(getCurrentToken()); 
			}
		}
		else if (getCurrentToken() == "games") {
			if (getNextTokenNonBlocking() != "") {
				games = static_cast<uint32_t>(getCurrentTokenAsUnsignedInt());
			}
		}
		else if (getCurrentToken() == "gpe") {
			if (getNextTokenNonBlocking() != "") {
				gpe = static_cast<uint64_t>(getCurrentTokenAsUnsignedInt());
			}
		}
	}
	epdTasks.start(numThreads, _clock, _startPositions, getBoard(), games, gpe);
}

/**
 * Improved evaulation weights by candidate pool selection
 */
void Statistics::trainCandidates(uint32_t numThreads) {
	if (getNextTokenNonBlocking() != "") {
		if (getCurrentToken() == "threads") {
			if (getNextTokenNonBlocking() != "") {
				numThreads = (uint32_t)getCurrentTokenAsUnsignedInt();
			}
		}
	}
	//
	CandidateTrainer::initializePopulation();

	while (!CandidateTrainer::finished()) {
		Candidate& c = CandidateTrainer::getCurrentCandidate();
		epdTasks.start(numThreads, _clock, _startPositions, getBoard());
		epdTasks.waitForEnd();
		CandidateTrainer::nextStep();
	}

	CandidateTrainer::printAll(std::cout);

}

void Statistics::handleWhatIf(std::string whatif) {
	IWhatIf* whatIf = getBoard()->getWhatIf();
	whatIf->clear();
	std::vector<std::string> tokens;
	std::string token;
	for (size_t i = 0; i < whatif.size(); ++i) {
		if (whatif[i] == ' ') {
			if (!token.empty()) {
				tokens.push_back(token);
				token.clear();
			}
		}
		else {
			token += whatif[i];
		}
	}
	if (!token.empty()) tokens.push_back(token);
	
	int32_t searchDepth = 5;
	for (int32_t ply = 0; getNextTokenNonBlocking() != ""; ply++) {
		if (getCurrentToken() == "null") {
			whatIf->setNullmove(ply);
		}
		else {
			MoveScanner moveScanner(getCurrentToken());
			if (moveScanner.isLegal()) {
				whatIf->setMove(ply, moveScanner.piece,
					moveScanner.departureFile, moveScanner.departureRank,
					moveScanner.destinationFile, moveScanner.destinationRank, moveScanner.promote);
			}
		}
	}
	whatIf->clear();
}

void Statistics::playStatistic(uint32_t numThreads) {
	uint32_t numGames = 0;
	while (getNextTokenNonBlocking() != "") {
		if (getCurrentToken() == "threads") {
			if (getNextTokenNonBlocking() != "") {
				numThreads = (uint32_t)getCurrentTokenAsUnsignedInt();
			}
		}
		else if (getCurrentToken() == "file") {
			if (getNextTokenNonBlocking() != "") {
				loadGamesFromFile(getCurrentToken());
			}
		}
		else if (getCurrentToken() == "games") {
			if (getNextTokenNonBlocking() != "") {
				numGames = (uint32_t)getCurrentTokenAsUnsignedInt();
			}
		}
		else {
			break;
		}
	}
	_clock.setAnalyseMode();
	epdTasks.start(numThreads, _clock, _startPositions, getBoard(), numGames);
}

void Statistics::setBoard() {
	string fen = getToEOLBlocking();
	bool success = setPositionByFen(fen);
	if (!success) {
		printf("Error (illegal fen): %s \n", fen.c_str());
		setPositionByFen();
	}
}

void Statistics::readLevelCommand() {
	uint8_t infoPos = 0;
	uint64_t curValue;
	uint64_t timeToThinkInSeconds = 0;

	while (getNextTokenNonBlocking(":") != "" && infoPos <= 4) {
		curValue = getCurrentTokenAsUnsignedInt();
		switch (infoPos) {
		case 0: _clock.setMoveAmountForClock(int32_t(curValue)); break;
		case 1: timeToThinkInSeconds = curValue * 60; break;
		case 2:
			if (getCurrentToken()[0] != ':') {
				_clock.setTimeIncrementPerMoveInMilliseconds(curValue * 1000);
				infoPos = 4;
			}
			break;
		case 3: timeToThinkInSeconds += curValue; break;
		case 4: _clock.setTimeIncrementPerMoveInMilliseconds(curValue * 1000); break;
		}
		infoPos++;
	}
	_clock.setTimeToThinkForAllMovesInMilliseconds(timeToThinkInSeconds * 1000);

}

bool Statistics::checkClockCommands() {
	bool commandProcessed = true;
	string token = getCurrentToken();
	if (token == "sd") {
		if (getNextTokenNonBlocking() != "") {
			_clock.setSearchDepthLimit((uint32_t)getCurrentTokenAsUnsignedInt());
		}
	}
	else if (token == "time") {
		if (getNextTokenNonBlocking() != "") {
			_clock.setComputerClockInMilliseconds(getCurrentTokenAsUnsignedInt() * 10);
		}
	}
	else if (token == "otim") {
		if (getNextTokenNonBlocking() != "") {
			_clock.setUserClockInMilliseconds(getCurrentTokenAsUnsignedInt() * 10);
		}
	}
	else if (token == "level")
	{
		readLevelCommand();
	}
	else if (token == "st") {
		if (getNextTokenNonBlocking() != "") {
			_clock.setExactTimePerMoveInMilliseconds(getCurrentTokenAsUnsignedInt() * 1000ULL);
		}
	}
	else {
		commandProcessed = false;
	}
	return commandProcessed;
}


void Statistics::computeMaterialDifference() {
	QaplaTraining::SignatureEvalAdjuster adjuster;
	int32_t minAdjust = 0;
	bool run = false;
	std::string binaryGamesFile = "epdGames.bin";
	while (getNextTokenNonBlocking() != "") {
		if (getCurrentToken() == "epd") {
			if (getNextTokenNonBlocking() != "") {
				loadEPD(getCurrentToken());
			}
		}
		if (getCurrentToken() == "games-file") {
			if (getNextTokenNonBlocking() != "") {
				binaryGamesFile = getCurrentToken();
			}
		}
		else if (getCurrentToken() == "min") {
			if (getNextTokenNonBlocking() != "") {
				minAdjust = (uint32_t)getCurrentTokenAsUnsignedInt();
			}
		}
		else if (getCurrentToken() == "run") {
			run = true;
		}
		else {
			break;
		}
	}
	QaplaTraining::PositionFilter positionFilter(1023);
	QaplaTraining::GameReplayEngine engine(getBoard(), _startPositions);
	positionFilter.analyzeGames(engine, binaryGamesFile);

	/*
	if (run) {
		adjuster.run(_startPositions, getBoard(), binaryGamesFile);
	}
	else {
		adjuster.computeFromFile("signature-eval-adjuster.bin", minAdjust);
	}
	*/
}

void Statistics::loadEPD() {
	std::string fileName = getNextTokenNonBlocking();
	if (fileName == "") {
		std::cerr << "Error: No EPD file specified." << std::endl;
		return;
	}
	loadEPD(fileName);
}
void Statistics::loadEPD(const std::string & filename) {
	std::ifstream file(filename);

	if (!file) {
		std::cerr << "Error: Could not open file " << filename << std::endl;
		return;
	}

	_startPositions.clear();
	std::string line;
	while (std::getline(file, line)) {
		if (!line.empty()) {
			_startPositions.push_back(line);
		}
	}

	file.close();
	std::cout << "Loaded " << _startPositions.size() << " positions from EPD file." << std::endl;
}

/**
 * Processes any input from stdio
 */
void Statistics::runLoop() {
	_mode = Mode::WAIT;
	string token = "";
	getBoard()->initialize();
	while (token != "quit" && _mode != Mode::QUIT) {
		switch (_mode) {
		case Mode::COMPUTE: handleInputWhileComputingMove(); break;
		default: 
		{
				waitForComputingThreadToEnd();
				handleInput();
		}
		}
		token = getNextTokenBlocking();
	}
	stopCompute();
	waitForComputingThreadToEnd();
	epdTasks.stop();
}

/**
 * Processes input while computing a move
 */
void Statistics::handleInputWhileComputingMove() {
	const string token = getCurrentToken();
	if (token == "?") stopCompute();
	else if (token == ".") getBoard()->requestPrintSearchInfo();
	else println("Error (command not supported in computing mode): " + token);
}

void Statistics::handleInput() {
	const string token = getCurrentToken();
	if (token == "analyze") analyzeMove();
	else if (token == "new") setPositionByFen();
	else if (token == "setboard") setBoard();
	else if (token == "eval") getBoard()->printEvalInfo();
	else if (token == "wmtest") WMTest();
	else if (token == "cores") readCores();
	else if (token == "memory") readMemory();
	else if (token == "playepd") playEpdGames();
	else if (token == "playstat") playStatistic();
	else if (token == "train") train();
	else if (token == "ct") trainCandidates();
	else if (token == "epd") loadEPD();
	else if (token == "material") computeMaterialDifference();
	else if (checkClockCommands()) {}
}
