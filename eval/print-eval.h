#ifndef PRINT_EVAL_H
#define PRINT_EVAL_H

#include "../basics/types.h"

template <bool PRINT>
inline static void printEvalStep(string topic, value_t sum, EvalValue cur, value_t midgame) {
	const int32_t valueTab = 33;
	if (PRINT) cout
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

template <bool PRINT>
inline static value_t sumEvalStep(string topic, value_t sum, value_t cur) {
	const int32_t valueTab = 33;
	if (PRINT) cout
		<< topic
		<< ":"
		<< std::right
		<< std::setw(valueTab - topic.length() - 1)
		<< cur
		<< " sum:"
		<< std::setw(6)
		<< ( sum + cur )
		<< endl;
	return sum + cur;
}

template <Piece COLOR, bool PRINT>
inline static value_t sumEvalStep(string topic, value_t sum, value_t cur, Square square) {
	return sumEvalStep<PRINT>(colorToString(COLOR) + " " + topic + " (" + squareToString(square) + ")", sum, cur);
}

template <Piece COLOR, bool PRINT>
inline static void printEvalStep(string topic, value_t sum, value_t cur, Square square) {
	sumEvalStep<COLOR, PRINT>(topic, sum, cur, square);
}

template <bool PRINT>
inline static EvalValue sumEvalStep(string topic, EvalValue sum, EvalValue cur, Square square) {
	const int32_t valueTab = 33;
	if (PRINT) cout
		<< topic
		<< " (" << squareToString(square) << ")"
		<< std::right
		<< std::setw(valueTab - topic.length() - 5)
		<< cur
		<< " sum:"
		<< std::setw(12)
		<< (sum + cur)
		<< endl;
	return sum + cur;
}


#endif