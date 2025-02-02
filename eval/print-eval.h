#ifndef PRINT_EVAL_H
#define PRINT_EVAL_H

#include "../basics/types.h"

static void printValueSys(EvalValue cur, EvalValue sum, size_t valueTab) {
	cout
		<< ":"
		<< std::right
		<< std::setw(valueTab)
		<< cur
		<< " sum:"
		<< std::setw(12)
		<< cur
		<< endl;
}

static void printValueSys(EvalValue cur, size_t valueTab) {
	cout
		<< ":"
		<< std::right
		<< std::setw(valueTab)
		<< cur
		<< endl;
}

static void printValue(std::string topic, Piece color, EvalValue cur) {
	const size_t valueTab = 33;
	cout << colorToString(color) + " " + topic;
	printValueSys(cur, valueTab - topic.length() - 7);
}

static void printValue(std::string topic, Piece color, EvalValue cur, Square square) {
	const size_t valueTab = 33;
	cout << colorToString(color) + " " + topic + " (" + squareToString(square) + ")";
	printValueSys(cur, valueTab - topic.length() - 12);
}

static void printValue(std::string topic, value_t sum, value_t cur) {
	const size_t valueTab = 33;
	cout
		<< topic
		<< ":"
		<< std::right
		<< std::setw(valueTab - topic.length() - 1)
		<< cur
		<< " sum:"
		<< std::setw(6)
		<< sum
		<< endl;
}

static void printEvalStep(string topic, value_t sum, EvalValue cur, value_t midgame) {
	const int32_t valueTab = 33;
	cout
		<< topic
		<< ":"
		<< std::right
		<< std::setw(valueTab - topic.length() - 1)
		<< cur.getValue(midgame)
		<< " sum:"
		<< std::setw(6)
		<< sum
		<< " phase: "
		<< cur
		<< endl;
}

#endif